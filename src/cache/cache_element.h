#ifndef DOORMAT_CACHE_ELEMENT_H
#define DOORMAT_CACHE_ELEMENT_H

#include <string>
#include <chrono>

struct cache_element 
{
	cache_element(const std::string&ref, const unsigned int translated_id, const std::string&tag, std::chrono::system_clock::time_point creation_time,  std::chrono::system_clock::time_point t, size_t size, const unsigned int ttl, std::string etag)
		: key{ref}
		, tag{tag}
		, creation_time{creation_time}
		, expiry_time{t}
		, _size{size}
		, id{translated_id}
		, ttl{ttl}
		, etag{std::move(etag)}
	{}

	bool valid() const
	{
		return std::chrono::system_clock::now() < expiry_time;
	}
	std::string key;
	std::string tag;
	std::chrono::system_clock::time_point creation_time;
	std::chrono::system_clock::time_point expiry_time;
	size_t _size;
	const unsigned int id;
	unsigned int ttl;
	std::string etag;

	size_t size() const noexcept
	{
		return _size;
	}

	bool verify_version(const std::string &version) const
	{
		return version == etag;
	}
};


#endif //DOORMAT_CACHE_ELEMENT_H
