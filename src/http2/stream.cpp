#include "stream.h"
#include "http2alloc.h"
#include "../utils/log_wrapper.h"
#include "../utils/utils.h"
#include "../http/http_structured_data.h"
#include "../http/http_commons.h"
#include "../http/server/request.h"
#include "../http/server/response.h"
#include "../protocol/http_handler.h"


#define MAKE_NV(NAME, VALUE) \
{ \
	(uint8_t *)NAME.data(), (uint8_t *)VALUE.data(), NAME.size(), VALUE.size(), \
		NGHTTP2_NV_FLAG_NO_COPY_NAME | NGHTTP2_NV_FLAG_NO_COPY_VALUE \
}

namespace http2
{

stream::stream ( stream && o) noexcept
{
	id_ = o.id_;
	request = std::move( o.request );
	destructor = std::move( o.destructor );
}

stream& stream::operator=( stream&& o ) noexcept
{
	if ( this != &o )
	{
		id_ = o.id_;
		request = std::move( o.request );
		destructor = std::move( o.destructor );
	}
	return *this;
}

stream::stream(std::shared_ptr<server::http_handler> s, std::function<void(stream*, session*)> des, std::int16_t prio ):
	s_owner{std::static_pointer_cast<session>(s)}, destructor{des}
{
	assert(s_owner);
	prd.source.ptr =  static_cast<void*> ( this );
	prd.read_callback = stream::data_source_read_callback;
	s_owner->subscribe(this);
	//todo: check the following comment:
	// not all streams are supposed to send data - it would be better to have a lazy creation
	request.protocol( http::proto_version::HTTP11 );
	request.channel( http::proto_version::HTTP20 );
	request.origin(s_owner->find_origin() );
}

void stream::on_request_header_complete()
{
	req->headers(std::move(request));
	_headers_sent = true;
}

void stream::add_header(std::string key, std::string value)
{
	if(!_headers_sent) return request.header(std::move(key), std::move(value));

	req->trailer(std::move(key), std::move(value));
}

void stream::on_request_body( data_t d, size_t size )
{
	//managed_chain->on_request_body( std::move( c ) );
   req->body(std::move(d), size);
}

void stream::on_request_finished()
{
	LOGTRACE("stream ", this, " request end detected");
	//managed_chain->on_request_finished();
    req->finished();
	// something is wrong here!
	//std::cout << "setting req "<< this << " to nullptr [1]" << std::endl;
	req = nullptr; //we don't grant anymore the existence of the request after the on_finished callback has been triggered.
	//if you want it, keep it.
}

void stream::on_eom()
{
	LOGTRACE("on_eom");
	eof_ = true;

	if ( closed_ ) 
		die();
	else
	{
		if(auto response = res.lock())
		{
			s_owner->add_pending_callbacks([response]()
			{
				response->cleared();
			}, [response]()
			{
				response->error(http::connection_error{http::error_code::write_error});
			});
		}
		flush();
	}
}

bool stream::body_empty() const noexcept
{
	return body.size() == 0 || ( body.size() == 1 && body.front().size() == body_index );
}

bool stream::defer() const noexcept
{
	return ! eof_ && ! has_trailers() && body_empty();
}

std::size_t stream::body_length() const noexcept
{
	std::size_t r{0};
	bool first{true};
	for ( const std::string& chunk : body )
	{
		r += chunk.size();
		if ( first )
		{
			first = !first;
			r -= body_index;
		}
	}
	return r;
}

bool stream::is_last_frame( std::size_t length ) const noexcept
{
	return eof_ && ! has_trailers() && body_length() == length;
}

bool stream::body_eof() const noexcept
{
	return body_empty() && ( eof_ || has_trailers() );
}

ssize_t stream::data_source_read_callback ( nghttp2_session *session_, std::int32_t stream_id, std::uint8_t *buf,
	std::size_t length, std::uint32_t *data_flags, nghttp2_data_source *source, void *user_data )
{
	LOGTRACE("stream::data_source_read_callback - ", stream_id );
	stream* s_this = static_cast<stream*>( source->ptr );

	if ( s_this->defer() )
	{
		LOGTRACE("stream::data_source_read_callback deferred");
		s_this->resume_needed_ = true;
		return NGHTTP2_ERR_DEFERRED;
	}

	// STATE MACHINE!
	//https://nghttp2.org/documentation/types.html#c.nghttp2_data_source_read_callback
	if ( s_this->body_eof() || s_this->is_last_frame( length ) )
	{
		LOGTRACE("stream::data_source_read_callback EOF in stream ", stream_id );
		s_this->body_sent = true;
		*data_flags |= NGHTTP2_DATA_FLAG_EOF;
	}

	if ( s_this->has_trailers() )
	{
		LOGTRACE("stream::data_source_read_callback NO END stream ", stream_id );
		*data_flags |= NGHTTP2_DATA_FLAG_NO_END_STREAM;
	}

	if ( s_this->body_empty() )
		return 0;

	//*data_flags |= NGHTTP2_DATA_FLAG_NO_COPY; needed sooner or later
	auto& first = s_this->body.front();

	std::size_t len = first.size() - s_this->body_index;
	ssize_t r = std::min( len, length );
	s_this->body_index += r;
	std::memcpy( buf, first.data(), r );

	if ( s_this->body_index == first.size() )
	{
		s_this->body.pop_front();
		s_this->body_index = 0;
	}

	return r;
}

void stream::flush() noexcept
{
	if ( closed_ ) return;

	if ( status != 101 )
	{
		if ( resume_needed_ )
		{
			int r = nghttp2_session_resume_data( s_owner->next_layer(), id_ );
			LOGTRACE("nghttp2_session_resume_data :: ", nghttp2_strerror(r) );
// 			assert( r == 0);
			resume_needed_ = false;
		}
		else if ( ! headers_sent )
		{
			int r = nghttp2_submit_response( s_owner->next_layer(), id_, nva, nvlen, &prd );
			LOGTRACE( "id: ", id_, " nghttp2_submit_response :: ", nghttp2_strerror(r) );
			assert( r == 0 );
			headers_sent = true;
		}
		else if ( body_sent && eof_ )
		{
			trailers_nvlen = trailers.size();
			create_headers( &trailers_nva );

			std::size_t i = 0;
			for ( auto&& it : trailers )
			{
				LOGTRACE( "Name:", static_cast<std::string>( it.first ),
					" Value:", static_cast<std::string>( it.second ), "-"  );
				nva[i++] = MAKE_NV( it.first, it.second );
			}

			int r = nghttp2_submit_trailer( s_owner->next_layer(), id_, trailers_nva, trailers_nvlen );
			LOGTRACE( "id: ", id_, " nghttp2_submit_trailer :: ", nghttp2_strerror(r) );
			// Todo
		}
		else
			LOGTRACE("Flush not useful");
	}
	else
	{
		uint8_t flags = 0;
		int r = nghttp2_submit_headers(  s_owner->next_layer(), flags, id_,
			nullptr, nva, nvlen, static_cast<void*>( this ) );
		LOGTRACE("nghttp2_submit_headers :: ", r );
		assert( r == 0 );
	}
	s_owner->do_write();
}

void stream::on_body( data_t data, size_t size )
{
	LOGTRACE("stream::on_body");

	body.emplace_back( data.get(), size );
	flush();
}

void stream::create_headers( nghttp2_nv** a ) noexcept
{
	*a = reinterpret_cast<nghttp2_nv*> (
		static_cast<http2_allocator<std::uint8_t>*>( s_owner->next_layer_allocator()->mem_user_data )
			->allocate( sizeof(nghttp2_nv) * nvlen ) );
}

void stream::destroy_headers( nghttp2_nv** d ) noexcept
{
	static_cast<http2_allocator<std::uint8_t>*>( s_owner->next_layer_allocator()->mem_user_data )
		->deallocate( reinterpret_cast<uint8_t*>( *d ), 0 );
	*d = nullptr;
}

void stream::die() noexcept
{
	//std::cout << "calling die " << this << std::endl;
	if ( eof_ || errored )
	{
		if ( destructor )
			destructor(this, s_owner.get());
		//else
	//		LOGERROR("No destructor set up! BUG!");
	}
	else
	{
		closed_ = true;
	}

}

// https://tools.ietf.org/html/rfc7540#section-8.1.2
// 8.1.2.2 : no header connection specific is allowed
// Transfer Encoding is allowed to appear but it *must* contain only "trailers"
void stream::on_header(  http::http_response && resp )
{
	resp.filter ( []( const http::http_request::header_t& h ) -> bool
	{
		return
			utils::icompare ( h.first, "connection" ) || // Mandatory
			utils::icompare ( h.first, "Keep-Alive" ) || // SHOULD be removed
			utils::icompare ( h.first, "Upgrade" ) || // Ditto //
			utils::icompare ( h.first, "Proxy-Connection" /* Ditto */ );
	});
	if ( resp.has("Transfer-Encoding") )
	{
		resp.remove_header( "Transfer-Encoding" );
		resp.header( "Transfer-Encoding", "trailers" );
	}

	status = resp.status_code();
	http::http_structured_data::header_t _status{ ":status", std::to_string( status ) };
	prepared_headers.emplace( _status );

	for ( auto&& it : resp.headers() )
	{
		http::http_structured_data::headers_map::iterator found = prepared_headers.find( it.first );
		if ( found != prepared_headers.end() )
		{
			auto cval = found->second;
			cval.append( ", " );
			cval.append( it.second );
			prepared_headers.emplace( http::http_structured_data::header_t{ it.first, cval } );
			prepared_headers.erase( found );
		}
		else
			prepared_headers.emplace( http::http_structured_data::header_t{ it.first, it.second } );
	}

	nvlen = prepared_headers.size();
	create_headers( &nva );

	std::size_t i = 0;
	for ( auto&& it : prepared_headers )
	{
		LOGTRACE( "Name:", static_cast<std::string>( it.first ),
			" Value:", static_cast<std::string>( it.second ), "-"  );
		nva[i++] = MAKE_NV( it.first, it.second );
	}

	flush();
}

void stream::on_trailer( std::string&& key, std::string&& value )
{
	LOGTRACE("stream:", this, " on_trailer");
	trailers.emplace( http::http_structured_data::header_t{key, value} );
}

void stream::notify_error(http::error_code ec)
{
	if(req) req->error(ec);
	if(auto response = res.lock()) response->error(ec);
}

stream::~stream()
{
	//std::cout << "destroying stream: " << this << std::endl;
	s_owner->unsubscribe(this);
	LOGTRACE("Stream ", this, " destroyed");
	if ( nva ) destroy_headers( &nva );
	if ( trailers_nva ) destroy_headers( &trailers_nva );
	//destructor(this, s_owner.get());
}

void stream::uri_host( const std::string &p ) noexcept
{
//	From RFC!
//	Clients that generate HTTP/2 requests directly SHOULD use the ":authority"
//	pseudo-header field instead of the Host header field.  An
//	intermediary that converts an HTTP/2 request to HTTP/1.1 MUST
//	create a Host header field if one is not present in a request by
//	copying the value of the ":authority" pseudo-header field.
//
//	From Me:
//	If our client uses Host, it should be managed normally. If it uses both,
//  every behaviour is fine.
	request.hostname( p );
	request.urihost( p );
}

void stream::set_handlers(std::shared_ptr< http::request > req_handler, std::shared_ptr< http::response > res_handler)
{
    req = req_handler;
    res = res_handler;
}

}
