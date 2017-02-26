#ifndef DOORMAT_CACHE_H
#define DOORMAT_CACHE_H

#include "../service_locator/service_locator.h"
#include "../io_service_pool.h"
#include <unordered_map>
#include <string>
#include <cstdint>
#include <mutex>
#include <functional>
#include "../http/http_response.h"
#include <cynnypp/bloom_filters.hpp>
#include <functional>
#include <citycrc.h>
#include <io/async/fs/fs_manager_interface.h>
#include "translator/translation_unit.h"
#include "cache_element.h"
#include "ttl_cleaner.h"
#include "tag.h"
#include "../fs/fs_manager_wrapper.h"
#include "../utils/log_wrapper.h"


/** \brief the cache class provides a basic cache functionality in the form of key-value interrogations
 * on a data structure. It interacts with the disk in order to store the value information.
 * It is basically a mediator between different indexes and the disk.
 *
 * \tparam replacement_policy the policy used to evict elements in case cache space is exhausted 4
 * \tparam key_t the type of the key
 * \tparam max_elements_size the maximum number of elements to be stored in the cache (defaults to 2^20)
 * */
template<template<class> class replacement_policy, typename key_t = std::string, size_t max_elements_size = 1024 * 1024>
class cache
{
	static bool _has_instance;
	static std::mutex singleton_mutex;
	static std::unique_ptr<cache> instance;

	/** Structure used for fast lookup*/
	using bloom_filter = cynny::cynnypp::bloom_filters::CountingBloomFilter<max_elements_size * 2, 3, 4, key_t>;
	/** Structure used to store and return back the values stored in the cache */
	using output_data_t = std::vector<uint8_t>;
	/** Type of the read callback */
	using read_callback = std::function<void( output_data_t )>;

	/** \brief temp_cache_representation is a temporary representation of an element in the cache; it is used
	 *  before the value associated to a certain key has been provided completely to the cache, hence before
	 *  storing the data in permanent memory.
	 * */
	struct temp_cache_representation
	{
		/** creates a temp_cache representation object
		 * \param ttl the time to live of the resource
		 * \param tag the tag of the resource
		 * \param etag the etag of the resource
		 * */
		temp_cache_representation( uint32_t ttl, std::chrono::system_clock::time_point creation_time,
			const std::string& tag, const std::string& etag )
			: data(), ttl{ ttl }, creation_time{ creation_time }, tag{ tag }, etag{ etag } {}

		/** creates a temp_cache_representation object
		 * \param ttl user provided time to live of the resource
		 * \param tag user provided category (or tag) for the element; used for selective, user mandated eviction*/
		temp_cache_representation( uint32_t ttl, std::chrono::system_clock::time_point creation_time,
			const std::string& tag ) : data(), ttl{ ttl }, creation_time{ creation_time }, tag{ tag }, etag{} {}

		temp_cache_representation( uint32_t ttl, std::chrono::system_clock::time_point creation_time )
			: data(), ttl{ ttl }, creation_time{ creation_time }, tag{}, etag{} {}

		/* Data */
		std::vector<uint8_t> data;
		/** TimeToLive*/
		unsigned int ttl;
		std::chrono::system_clock::time_point creation_time;
		bool finished{ false };
		bool invalidated{ false };
		/** Tag: can be empty (and normally is)*/
		std::string tag;
		std::string etag;


		void invalidate()
		{
			finished = true;
			invalidated = true;
		}
	};

	/** City hash wrapper used by auxiliary data structures. */
	struct city_hash
	{
		std::size_t operator ()( const key_t& k ) const
		{
			const uint128 seed{ 87598975460497, 1266683016321 };
			return CityHashCrc128WithSeed( k.data(), k.size(), seed ).second;
		}
	};

	/** Cache base dir, used to store the contents.*/
	std::string base_dir;
	/** Translation unit: transforms key_t (usually an expensive type) in rather unexpensive ids of type uint32_t */
	translation_unit<key_t, cache_element, city_hash> elements;
	/** Index of the elements ordered by ttl, used to evict expired elements*/
	ttl_cleaner<max_elements_size * 2, 32> ttl;
	/** Data structure used to store the <key,value> pairs before "commit" on diks*/
	std::unordered_map<key_t, temp_cache_representation, city_hash> tmp;
	/** Policy manager, decides which between the valid elements should be evicted. */
	replacement_policy<int> rp;
	/** Tag index, stores elements by tag*/
	tagger<std::string, int> tag_indexes;
	std::mutex global_mutex{};
	/** Total size of the cache. */
	uint64_t total_size = 0;
	/** appproximate membership structure*/
	bloom_filter approximate_membership;
	/** Maximum allowed size of the cache (in terms of the total size occupied by the *values*) */
	size_t max_bytes_size = 0;
	/** Temporary used to store how much of the data was freed during the cleaning operation triggered by evict*/
	size_t freed_size;

	/** Verifies if an element associated to a certain key is contained in the cache, without performing locks.
	 * Used only for the inner logic.
	 *
	 * \param key the key of the element for which an existence verification is required
	 * \return true if the key is stored in the cache with an associated value, false otherwise.
	 *
	 * */
	bool lockless_has( const key_t& key )
	{
		bool approximate_has = approximate_membership.has( key );
		if ( !approximate_has ) return false;
		return elements.has( key );
	}

	/** Returns the filename on disk for a given content
	 *  \param base_dir the cache base directory
	 *  \param key the key of the content whose filename is retrieved
	 *  \param tag the tag under which the content is indexed
	 * \returns a pair containing in the first element the directory path, in the second one the whole filename.
	 *
	 * */
	static std::pair<std::string, std::string>
	get_fs_name( const std::string& base_dir, const key_t& key, const std::string& tag )
	{
		auto res = CityHashCrc128WithSeed( key.data(), key.size(), { 5684564328, 7795678956778 } );
		std::stringstream ss;
		ss << base_dir;
		if ( tag.size()) ss << "tags/" << CityHash32( tag.data(), tag.size()) << "/";
		ss << std::hex << std::setfill( '0' );
		ss << std::setw( 2 ) << (uint16_t) ((res.first & 0xFF00000000000000) >> 56);
		ss << "/";
		auto dir = ss.str();
		ss << std::setw( 16 ) << res.first;
		std::string out = ss.str();

		return std::make_pair( dir, out );
	}

	/** Returns the name of the directory on which the tags are stored.
	 * \param base_dir the base dir of the cache
	 * \param tag the tag whose directory we want to retrieve.
	 *
	 * \returns the name of the directory on which the content indexed by a tag are stored.
	 * */
	static std::string get_tag_dir( const std::string& base_dir, const std::string& tag )
	{
		std::stringstream ss;
		ss << base_dir;
		if ( tag.size()) ss << "tags/" << CityHash32( tag.data(), tag.size()) << "/";
		return ss.str();
	}

public:
	/** Creates a new cache
	 *  \param max_bytes_size the maximum size occupied by the cache
	 *  \param base_dir the base directory used to write the files on disk.
	 * */

	static std::unique_ptr<cache>&
	get_instance( unsigned int max_bytes_size = UINT32_MAX, std::string base_dir = "/tmp/cache/" )
	{
		std::lock_guard<std::mutex> s{ singleton_mutex };
		if ( _has_instance ) return instance;
		instance = std::unique_ptr<cache>{ new cache( max_bytes_size, base_dir ) };
		_has_instance = true;
		return instance;
	}

	cache( unsigned int max_bytes_size = UINT32_MAX, std::string base_dir = "/tmp/cache/" ) :
		base_dir{ base_dir },
		approximate_membership
			{
				{
					[]( const key_t& c )
					{
						const uint128 seed{ 58966895434, 23289789237897 };
						return CityHashCrc128WithSeed( c.data(), c.size(), seed ).first;
					},
					[]( const key_t& c )
					{
						const uint128 seed{ 574542563448, 8679805476824 };
						return CityHashCrc128WithSeed( c.data(), c.size(), seed ).first;
					},
					[]( const key_t& c )
					{
						const uint128 seed{ 2574657455567, 878679086346 };
						return CityHashCrc128WithSeed( c.data(), c.size(), seed ).first;
					}
				}
			},
		max_bytes_size{ max_bytes_size }
	{
		//initialization of the callbacks for the elements which could trigger spontaneous remove operations.
		try {
			cynny::cynnypp::filesystem::removeDirectory( base_dir );
		}
		catch ( const cynny::cynnypp::filesystem::ErrorCode& ec ) {
			LOGERROR( "[Cache] could not clean the base dir", base_dir, " on startup because of ", ec.what());
		}
		ttl.set_remove_callback( [ this ]( const unsigned int id )
			{
				auto& k = elements.get_key( id );
				approximate_membership.remove( k );

				rp.remove( id );
				std::string tag = tag_indexes.remove( id );
				auto name = get_fs_name( this->base_dir, k, tag );
				try {
					cynny::cynnypp::filesystem::removeFile( name.second );
				}
				catch ( const cynny::cynnypp::filesystem::ErrorCode& ec ) {
					LOGERROR( "[CACHE] error while removing file ", name.second, ": ", ec.what());
				}
				size_t element_size = elements.remove( id );
				freed_size += element_size;
			}
		);

		elements.set_remove_callback( [ this ]( const std::string& str, const unsigned int id,
				std::chrono::system_clock::time_point expiry_time, size_t size )
			{
				ttl.thorough_remove_element( id, expiry_time );
				approximate_membership.remove( str );
				rp.remove( id );
				total_size -= size;
				std::string tag = tag_indexes.remove( id );
				auto name = get_fs_name( this->base_dir, str, tag );
				try {
					cynny::cynnypp::filesystem::removeFile( name.second );
				}
				catch ( const cynny::cynnypp::filesystem::ErrorCode& ec ) {
					LOGERROR( "[CACHE] error while removing file ", name.second, ": ", ec.what());
				}
			}
		);

		rp.set_remove_callback( [ this ]( unsigned int id )
			{
				auto& k = elements.get_key( id );
				approximate_membership.remove( k );
				ttl.thorough_remove_element( id );
				std::string tag = tag_indexes.remove( id );

				auto name = get_fs_name( this->base_dir, k, tag );
				try {
					cynny::cynnypp::filesystem::removeFile( name.second );
				}
				catch ( const cynny::cynnypp::filesystem::ErrorCode& ec ) {
					LOGERROR( "[CACHE] error while removing file ", name.second, ": ", ec.what());
				}
				elements.remove( id );
			}
		);
	}

	/** Checks whether a key is stored in cache.
	 * \param k the key of the element to verify
	 * \return true in case a pair with key = k is stored in the permanent memory of the cache; in case the content has
	 * not been committed yet or the element is not stored, the result will be false.
	 * */
	inline bool has( const key_t& k )
	{
		bool approximate_has = approximate_membership.has( k );
		if ( !approximate_has ) return false;
		std::lock_guard<std::mutex> g{ global_mutex };
		return elements.has( k );
	}

	/** Gets the age of the current content, if it exists.
	 *
	 * */
	inline uint32_t get_age( const key_t& k )
	{
		auto el = elements.get_element( k );
		if ( el == nullptr ) return 0;
		return std::chrono::duration_cast<std::chrono::seconds>( std::chrono::system_clock::now() - el->creation_time
		).count();
	}

	/** Total size (in bytes) of the cach
	 * \return the total size (in bytes) of the cache.
	 * */
	inline size_t size()
	{
		return total_size;
	}


	/** Gets an element from the cache (if it exists)
	 * \param key the key for whcih the user wants to get the content
	 * \param rcb the callback used to send back the content to the requestor. In case the key is not found, an empty data structure is returned.
	 * */
	bool get( const key_t& key, read_callback rcb )
	{
		bool ret = false;
		std::lock_guard<std::mutex> g{ global_mutex };
		auto found = elements.get_element( key );
		if ( found != nullptr ) {
			auto fs_name = get_fs_name( base_dir, key, found->tag ).second;
			std::basic_ifstream<uint8_t>* fdptr = new std::basic_ifstream<uint8_t>( fs_name,
				std::ios::in | std::ios::binary | std::ios::ate
			);
			if ( !(*fdptr)) {
				service::locator::service_pool().get_thread_io_service().post( std::bind( rcb, output_data_t{} ));
				return false;
			}
			ret = true;
			std::unique_ptr<std::basic_ifstream<uint8_t>> file_descriptor( fdptr );
			auto retrieved_data = std::make_shared<output_data_t>();
			service::locator::fs_manager().async_read( std::move( file_descriptor ), *retrieved_data, [ retrieved_data, rcb ](
					const cynny::cynnypp::filesystem::ErrorCode& ec, size_t size )
				{
					if ( !ec ) rcb( std::move( *retrieved_data )); //enhance this.
					else rcb( output_data_t{} );
				}
			);
		} else {
			service::locator::service_pool().get_thread_io_service().post( std::bind( rcb, output_data_t{} ));
		}

		return ret;
	}

	/** Starts to put content for a given key
	 * \tparam T... list of additional parameters to fetch to the cache representation
	 * \param key the key used to identify the content
	 * \param ttl the duration (in seconds) of the content in the cache
	 * \param tag the tag used to index the content
	 * \return true in case the content can be put successfully in the cache; false otherwise
	 * */

	template<typename... T>
	bool begin_put( const key_t& key, unsigned int ttl, T&& ... t )
	{
		std::lock_guard<std::mutex> g{ global_mutex };
		if ( ttl <= 0 || tmp.find( key ) != tmp.end() || lockless_has( key )) return false;
		tmp.emplace( key, temp_cache_representation{ ttl, std::forward<T>( t )... } );
		return true;
	}

	/** Puts the content for a given key
	 * \param key the identifier of the content on which the append operation will be performed
	 * \param cl the data to append
	 * \return true in case the operation is successfully; false otherwise
	 * */
	bool put( const key_t& key, const std::string& cl )
	{
		std::lock_guard<std::mutex> g{ global_mutex };
		total_size += cl.size();
		evict();
		auto where_to_put = tmp.find( key );
		if ( where_to_put != tmp.end() && !where_to_put->second.finished ) {
			auto& container = where_to_put->second.data;
			container.insert( container.end(), cl.begin(), cl.end());
			return true;
		}
		total_size -= cl.size();
		return false;
	}

	/** Stores an element in the cache (if ok == true) or discards it, preventing other put operations on it
	 * \param key the identifier of the data
	 * \param ok commit or rollback flag
	 * */
	void end_put( const key_t& key, bool ok = true )
	{
		std::lock_guard<std::mutex> g{ global_mutex };
		auto data = tmp.find( key );
		if ( data != tmp.end() && !data->second.finished ) {
			data->second.finished = true;
			if ( ok ) {
				auto fs_name = get_fs_name( base_dir, key, data->second.tag );
				try {
					cynny::cynnypp::filesystem::createDirectory( fs_name.first, true );
				}
				catch ( const cynny::cynnypp::filesystem::ErrorCode& ec ) {
					LOGERROR( "[CACHE]", "Could not store ", key, " contents because creation of directory ", fs_name.first, " failed with error: ", ec.what());
					return;
				}
				service::locator::fs_manager().async_write( fs_name.second, data->second.data, [ this, key ](
						const cynny::cynnypp::filesystem::ErrorCode& ec, size_t size )
					{
						std::lock_guard<std::mutex> g{ global_mutex };
						auto data = tmp.find( key ); //already checked: don't do it again.
						if ( !ec && data->second.invalidated == false ) {
							auto expiry_time = data->second.creation_time + std::chrono::seconds( data->second.ttl );
							auto translation_id = elements.insert( key, data->second.tag, data->second.creation_time, expiry_time, size, data->second.ttl, data->second.etag );
							approximate_membership.set( key );
							ttl.insert( translation_id, expiry_time );
							rp.put( translation_id, size );
							if ( data->second.tag.size()) tag_indexes.insert( data->second.tag, translation_id );
							tmp.erase( data );
						} else {
							if ( data->second.invalidated ) {
								auto path = get_fs_name( this->base_dir, key, data->second.tag );
								try {
									cynny::cynnypp::filesystem::removeFile( path.second );
								}
								catch ( const cynny::cynnypp::filesystem::ErrorCode& ec ) {
									LOGERROR( "[CACHE] File was not created; hence i have not deleted it.", ec.what());
								}
							}
							LOGERROR( "[CACHE]", "Could not store ", key, " contents because of a filesystem error: ", ec.what());
							total_size -= data->second.data.size();
							tmp.erase( data );
						}
					}
				);
				return;
			} else {
				total_size -= data->second.data.size();
				tmp.erase( data );
			}

			return;
		}
	}

	/** Clears all elements under a given tag
	 * \param tag the identifier of the tag
	 * \return number of bytes cleared
	 * */
	size_t clear_tag( const std::string& tag )
	{
		std::lock_guard<std::mutex> g{ global_mutex };
		size_t clearedBytes = 0;
		tag_indexes.clear( tag, [ this, &clearedBytes ]( int id )
			{
				auto& k = elements.get_key( id );
				approximate_membership.remove( k );
				size_t element_size = elements.remove( id );
				total_size -= element_size;
				clearedBytes += element_size;
				ttl.thorough_remove_element( id );
				rp.remove( id );
			}
		);

		auto name = get_tag_dir( base_dir, tag );
		try {
			cynny::cynnypp::filesystem::removeDirectory( name );
		}
		catch ( const cynny::cynnypp::filesystem::ErrorCode& ec ) {
			LOGERROR( "[CACHE] error while removing tag directory", name, ": ", ec.what());
		}

		return clearedBytes;
	}


	/** Manually removes from the cache a certain element.
	 *
	 * \param key the key of the element to remove.
	 * */
	void invalidate( const key_t& key )
	{
		std::lock_guard<std::mutex> g{ global_mutex };
		auto el = elements.get_element( key );
		if ( el == nullptr ) {
			/** The element is not in cache... Yet, it might be in temporary and we're writing it right now. Make it work as if it doesnt.*/
			auto d = tmp.find( key );
			if ( d != tmp.end()) {
				d->second.invalidate();
			}
			return;
		}
		total_size -= el->_size;
		int id = el->id;
		ttl.thorough_remove_element( id );
		tag_indexes.remove( id );
		rp.remove( id );
		approximate_membership.remove( el->key );
		auto path = get_fs_name( this->base_dir, el->key, el->tag );
		elements.remove( id );
		try {
			cynny::cynnypp::filesystem::removeFile( path.second );
		}
		catch ( const cynny::cynnypp::filesystem::ErrorCode& ec ) {
			LOGERROR( "[CACHE] could not delete file", path.second, " from disk; error is: ", ec.what());
		}

	}

	/** Verifies the version associated to a certain key.
	 * \tparam the type of the version tag.
	 *  \param key the key on which the version will be checked
	 *  \param t the value against which we want to compare
	 *  \return true in case the version is the same
	 * */
	template<typename T>
	bool verify_version( const key_t& key, const T& t )
	{
		std::lock_guard<std::mutex> g{ global_mutex };
		auto el = elements.get_element( key );
		return el != nullptr && el->verify_version( t );
	}


	/** Cache eviction: frees space. NOT THREAD SAFE
	 * */
	void evict()
	{
		if ( total_size < max_bytes_size ) return;
		auto min_size_to_evict = 2 * (total_size - max_bytes_size);
		freed_size = 0;
		ttl.remove_expired();
		total_size -= freed_size;
		if ( freed_size >= min_size_to_evict ) return;
		//call policy maker.
		total_size -= rp.free_space( min_size_to_evict - freed_size );
	}

	/** Clears all elements in cache
	 * \returns the number of bytes freed.
	 * */
	size_t clear_all()
	{
		std::lock_guard<std::mutex> g{ global_mutex };
		freed_size = 0;
		auto old_total = total_size;
		ttl.remove_expired();
		total_size -= freed_size;
		total_size -= rp.free_space( total_size );
		return old_total;
	}
};

template<template<class> class replacement_policy, typename key_t, size_t max_elements_size>
bool cache<replacement_policy, key_t, max_elements_size>::_has_instance{ false };

template<template<class> class replacement_policy, typename key_t, size_t max_elements_size>
std::unique_ptr<cache<replacement_policy, key_t, max_elements_size>> cache<replacement_policy, key_t, max_elements_size>::instance
{
	nullptr
};

template<template<class> class replacement_policy, typename key_t, size_t max_elements_size>
std::mutex cache<replacement_policy, key_t, max_elements_size>::singleton_mutex;
#endif //DOORMAT_CACHE_H
