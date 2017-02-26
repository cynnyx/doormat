#include "inspector_serializer.h"
#include "../configuration/configuration_wrapper.h"
#include "../utils/base64.h"
#include "../utils/log_wrapper.h"

#include <fstream>

namespace logging
{

class fs_writer : public writer
{
	std::fstream writer;
public:
	void open( const std::string &name ) override
	{
		writer.open( name, std::ios::app );
	}

	fs_writer& operator<<( const std::string &stuff ) override
	{
		writer << stuff << std::endl;
		return *this;
	}

	virtual ~fs_writer() = default;
};

inspector_log::inspector_log( const std::string &log_dir, const std::string &file_prefix,
  bool active, writer *w )
{
	std::string val;
	if ( active )
		LOGDEBUG("Inspector log running");
	else
	{
		LOGDEBUG("Inspector log not active!");
		return;
	}

	if ( w )
		logfile.reset( w );
	else
	{
		logfile = std::unique_ptr<writer>{ new fs_writer() };
		// Append thread id to avoid to open the same file
		std::thread::id id = std::this_thread::get_id();
		std::stringstream ss;
		ss << id;
		val = ss.str();
	}

	logfile->open( log_dir + "/" + file_prefix + val + ".json" );
}

void inspector_log::jsonize_body( nlohmann::json &json, const std::list<dstring> &body )
{
	if ( body.size() == 0 || body.front().size() == 0 )
		return;
	std::list<std::string> encoded_body;
	std::size_t len{0};
	for ( const dstring& chunk : body )
	{
		std::string ebody = utils::base64_encode( reinterpret_cast<const unsigned char*>( chunk.cdata() ),
			chunk.size() );
		encoded_body.emplace_back( std::move( ebody ) );
	}

	std::string built_body;
	built_body.reserve( len );
	for ( const std::string& chunk: encoded_body )
		built_body += chunk;

	json["body"] = built_body;
}

void inspector_log::jsonize_headers( nlohmann::json &json, const http::http_structured_data::headers_map &hrs )
{
	if ( hrs.size() == 0 )
		return;

	std::vector<std::string> multi_values;
	std::string multi_key;
	for ( const http::http_structured_data::header_t& values : hrs )
	{
		dstring key = values.first;
		dstring value = values.second;
		if ( ! multi_key.empty() && multi_key != static_cast<std::string>( key ) )
		{
			json[multi_key] = multi_values;
			multi_values.clear();
			multi_key.clear();
		}
		std::size_t c = hrs.count( key );
		if ( c == 1 )
			json[ static_cast<std::string>( key )]  = static_cast<std::string>( value );
		else
		{
			if ( multi_key.empty() )
			{
				multi_key = static_cast<std::string>( key );
				multi_values.reserve( c );
			}
			multi_values.emplace_back( static_cast<std::string>( value ) );
		}
	}
}

void inspector_log::jsonize( access_recorder& r ) noexcept
{
	try
	{
		nlohmann::json entry;
		nlohmann::json req;

		req["method"] = std::string( r.method );
		req["schema"] = std::string( r.schema );
		req["urihost"] = std::string( r.urihost);
		req["port"] = std::string( r.port );
		req["query"] = std::string( r.query );
		req["fragment"] = std::string( r.fragment );
		req["protocol"] = std::string( r.protocol );
		nlohmann::json headers;
		jsonize_headers( headers, r.req_headers );
		req["headers"] = headers;
		jsonize_body( req, r.req_body );
		nlohmann::json trailers;
		jsonize_headers( trailers, r.req_trailers );
		req["trailers"] = trailers;

  		nlohmann::json resp;
		resp["status"] = r.ar_.status;
		nlohmann::json rheaders;
		jsonize_headers( rheaders, r.resp_headers );
		resp["headers"] = rheaders;
		jsonize_body( resp, r.resp_body );
		nlohmann::json rtrailers;
		jsonize_headers( rtrailers, r.resp_trailers );
		resp["trailers"] = rtrailers;

		entry["request"] = req;
		entry["response"] = resp;

		*logfile << entry.dump( 4 );
	}
	catch ( ... )
	{
		LOGERROR("Inspector JSON Error!");
	}
}

void inspector_log::log( access_recorder&& r ) noexcept
{
	if ( active() ) jsonize( r );
}

inspector_log::~inspector_log() = default;

bool inspector_log::active() noexcept
{
	return static_cast<bool>( logfile );
}

}