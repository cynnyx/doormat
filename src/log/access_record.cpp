#include "access_record.h"
#include "log.h"
#include "../service_locator/service_locator.h"
#include "inspector_serializer.h"
#include "../stats/stats_manager.h"
#include "../utils/base64.h"
#include "../http/http_commons.h"
#include "../utils/log_wrapper.h"

#include <cctype>
#include <sstream>
#include <set>

namespace logging
{

access_recorder::access_recorder( const access_recorder & o )
{
	method = o.method.clone();
	urihost = o.urihost.clone();
	port = o.port.clone();
	query = o.query.clone();
	fragment = o.fragment.clone();
	protocol = o.protocol.clone();
	schema = o.schema.clone();

	for ( auto&& p : o.req_headers )
		req_headers.insert ( std::pair<dstring, dstring>{ p.first.clone(), p.second.clone() } );
	for ( auto&& b : o.req_body )
		req_body.emplace_back( b.clone() );
	for ( auto&& p : o.req_trailers )
		req_trailers.insert ( std::pair<dstring, dstring>{ p.first.clone(), p.second.clone() } );
	for ( auto&& p : o.resp_headers )
		resp_headers.insert ( std::pair<dstring, dstring>{ p.first.clone(), p.second.clone() } );
	for ( auto&& b : o.resp_body )
		req_body.emplace_back( b.clone() );
	for ( auto&& p : o.resp_trailers )
		req_trailers.insert ( std::pair<dstring, dstring>{ p.first.clone(), p.second.clone() } );
}

void access_recorder::set_request_start()
{
	ar_.start = clock::now();
}

void access_recorder::set_request_end()
{
	ar_.end = clock::now();
}

void access_recorder::set_pipe(bool pipe) noexcept
{
	ar_.pipe = pipe;
}

void access_recorder::append_response_body( const dstring& body )
{
	resp_body.push_back( body );
	resp_body_len += body.size();
}

void access_recorder::append_request_body( const dstring& body )
{
	req_body.push_back( body );
	req_body_len += body.size();
}

void access_recorder::cancel() noexcept
{
	cancelled_ = true;
}

void access_recorder::error( int err ) noexcept
{
	err_ = err;
}

void access_recorder::commit() noexcept
{
	try
	{
		if ( committed_ )
			return;

		using namespace std::chrono;

		auto start = clock::now();
		service::locator::access_log().log( ar_ );
		committed_ = true;

		auto total = clock::now() - start;

		// add the time spent in the log function to the stats
		service::locator::stats_manager().enqueue( stats::access_log_time,
			std::chrono::duration_cast<std::chrono::nanoseconds>( total ).count());

		if ( service::locator::inspector_log().active() )
			service::locator::inspector_log().log( std::move( *this ));
		else
			LOGDEBUG("Inspector log not active in commit");
	}
	catch ( ... )
	{
		LOGERROR( "Access recorder commit exploded!" );
	}
}

void access_recorder::request( const http::http_request& r )
{
	const auto& tmp = r.origin().to_string();
	ar_.remote_addr = dstring{tmp.data(), tmp.size()};

	auto h0 = r.headers("Authorization");
	if ( ! h0.empty() )
	{
		static const dstring tag_{"basic "};
		auto auth = utils::base64_decode(h0.front());
		if( auth.substr(0, tag_.size(), ::tolower) == tag_ )
		{
			//dstringify
			auto found = std::find_if(auth.cbegin() + tag_.size(), auth.cend(), [](char c){ return c != ' '; });
			if(found != auth.cend())
				ar_.remote_user = utils::base64_decode(std::string(found, auth.cend()));
		}
	}
	using namespace http;

	if ( service::locator::inspector_log().active() )
	{
		method = r.method();
		schema = r.schema();
		urihost = r.urihost();
		port = r.port();
		query = r.query();
		fragment = r.fragment();
		protocol = http::proto_to_string( r.channel() );
	}

	dstring line;
	line.append(r.method())
		.append(space);

	auto&& _schema = r.schema();
	if(_schema.is_valid())
		line.append(_schema)
			.append(colon)
			.append(slash)
			.append(slash);

	//TODO: DRM-207 userinfo not yet supported
	//if(_userinfo.valid())
	//	chunks.push_back(_userinfo);

	auto _host = r.urihost();
	if(_host.is_valid())
		line.append( _host);

	auto _port = r.port();
	if(_port.is_valid())
		line.append( colon ).append( _port );

	auto _path = r.path();
	if(_path.is_valid())
		line.append( _path);

	auto _query = r.query();
	if(_query.is_valid())
		line.append( questionmark )
			.append( _query );

	auto _fragment = r.fragment();
	if(_fragment.is_valid())
		line.append( hash )
			.append( _fragment );

	line.append( space )
		.append( http::proto_to_string( r.channel() ) );

	ar_.req = line;
	ar_.req_length += line.size();

	const http::http_structured_data::headers_map& headers = r.headers();
	std::set<dstring> keys;
	for ( const http::http_structured_data::header_t& value : headers )
	{
		keys.insert( value.first );
		req_headers.insert( std::make_pair<>( value.first, value.second ) );
		if ( keys.find( value.first) == keys.end() )
			ar_.req_length += value.first.size();
		ar_.req_length += value.second.size();
		ar_.req_length += 2;
	}

	auto h = r.headers("referer");
	if( ! h.empty() )
		ar_.referer = h.front();

	auto h2 = r.headers("user-agent");
	if( ! h2.empty() )
		ar_.user_agent = h2.front();

	if ( r.hostname().empty() )
		ar_.host = r.urihost();
	else
		ar_.host = r.hostname();
}

void access_recorder::response( const http::http_response& r)
{
	ar_.status = r.status_code();
	auto h = r.headers("x-debug-cyn-ip");
	if ( ! h.empty() )
		ar_.x_debug_cyn_ip = h.front();

	if ( service::locator::inspector_log().active() )
	{
		const http::http_structured_data::headers_map& headers = r.headers();
		for ( const http::http_structured_data::header_t& value : headers )
		{
			resp_headers.insert( std::make_pair<>( value.first, value.second ) );
		}
	}
}

void access_recorder::append_request_trailer(const dstring& k, const dstring& v)
{
	req_trailers.insert( std::make_pair<>( k, v ) );
}

void access_recorder::append_response_trailer(const dstring& k, const dstring& v)
{
	resp_trailers.insert( std::make_pair<>( k, v ) );
}

void access_recorder::continued() noexcept
{
	continued_ = true;
}

} // namespace logging
