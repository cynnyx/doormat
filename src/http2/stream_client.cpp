#include "stream_client.h"
#include "session_client.h"
#include "http2alloc.h"
#include "../utils/log_wrapper.h"
#include "../utils/utils.h"
#include "../http/http_structured_data.h"
#include "../http/http_commons.h"
#include "../protocol/http_handler.h"
#include "../http/client/request.h"
#include "../http/client/response.h"


#define MAKE_NV(NAME, VALUE) \
{ \
	(uint8_t *)NAME.data(), (uint8_t *)VALUE.data(), NAME.size(), VALUE.size(), \
		NGHTTP2_NV_FLAG_NO_COPY_NAME | NGHTTP2_NV_FLAG_NO_COPY_VALUE \
}

namespace http2
{

stream_client::stream_client ( stream_client && o) noexcept
{
	id_ = o.id_;
	response = std::move( o.response );
	destructor = std::move( o.destructor );
}

stream_client& stream_client::operator=( stream_client&& o ) noexcept
{
	if ( this != &o )
	{
		id_ = o.id_;
		response = std::move( o.response );
		destructor = std::move( o.destructor );
	}
	return *this;
}

stream_client::stream_client(std::shared_ptr<server::http_handler> s, std::function<void(stream_client*, session_client*)> des, std::int16_t prio ):
	s_owner{std::static_pointer_cast<session_client>(s)}, destructor{des}
{
	assert(s_owner);
	prd.source.ptr =  static_cast<void*> ( this );
	prd.read_callback = stream_client::data_source_read_callback;
	s_owner->subscribe(this);
	//todo: check the following comment:
	// not all streams are supposed to send data - it would be better to have a lazy creation
	response.protocol( http::proto_version::HTTP11 );
	response.channel( http::proto_version::HTTP20 );
	response.origin(s_owner->find_origin() );
}

void stream_client::on_response_header_complete()
{
	res->headers(std::move(response));
	_headers_sent = true;
}

void stream_client::add_header(std::string key, std::string value)
{
	if(!_headers_sent) return response.header(std::move(key), std::move(value));

	res->trailer(std::move(key), std::move(value));
}

void stream_client::on_response_body( data_t d, size_t size )
{
	//managed_chain->on_request_body( std::move( c ) );
   res->body(std::move(d), size);
}

void stream_client::on_response_finished()
{
	LOGTRACE("stream ", id_, " request end detected");
	//managed_chain->on_request_finished();
	res->finished();
	// something is wrong here!
	//std::cout << "setting req "<< this << " to nullptr [1]" << std::endl;
	res = nullptr; //we don't grant anymore the existence of the request after the on_finished callback has been triggered.
	//if you want it, keep it.
}

void stream_client::on_eom()
{
	LOGTRACE("on_eom");
	eof_ = true;

	if ( closed_ ) die();
	else
	{
		if(auto request = req)
		{
			s_owner->add_pending_callbacks([request](){
				request->cleared();
			}, [request](){
				request->error(http::connection_error{http::error_code::write_error});
			});
		}
		flush();
	}
}

bool stream_client::body_empty() const noexcept
{
	return body.size() == 0 || ( body.size() == 1 && body.front().size() == body_index );
}

bool stream_client::defer() const noexcept
{
	return ! eof_ && ! has_trailers() && body_empty();
}

std::size_t stream_client::body_length() const noexcept
{
	std::size_t r{0};
	bool first{true};
	for ( const auto& chunk : body )
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

bool stream_client::is_last_frame( std::size_t length ) const noexcept
{
	return eof_ && ! has_trailers() && body_length() == length;
}

bool stream_client::body_eof() const noexcept
{
	return body_empty() && ( eof_ || has_trailers() );
}

ssize_t stream_client::data_source_read_callback ( nghttp2_session *session_, std::int32_t stream_id, std::uint8_t *buf,
	std::size_t length, std::uint32_t *data_flags, nghttp2_data_source *source, void *user_data )
{
	LOGTRACE("stream_client::data_source_read_callback - ", stream_id );
	stream_client* s_this = static_cast<stream_client*>( source->ptr );

	if ( s_this->defer() )
	{
		LOGTRACE("stream_client::data_source_read_callback deferred");
		s_this->resume_needed_ = true;
		return NGHTTP2_ERR_DEFERRED;
	}

	// STATE MACHINE!
	//https://nghttp2.org/documentation/types.html#c.nghttp2_data_source_read_callback
	if ( s_this->body_eof() || s_this->is_last_frame( length ) )
	{
		LOGTRACE("stream_client::data_source_read_callback EOF in stream ", stream_id );
		s_this->body_sent = true;
		*data_flags |= NGHTTP2_DATA_FLAG_EOF;
	}

	if ( s_this->has_trailers() )
	{
		LOGTRACE("stream_client::data_source_read_callback NO END stream ", stream_id );
		*data_flags |= NGHTTP2_DATA_FLAG_NO_END_STREAM;
	}

	if ( s_this->body_empty() )
		return 0;

	//*data_flags |= NGHTTP2_DATA_FLAG_NO_COPY; needed sooner or later
	auto& first = s_this->body.front();

	std::size_t len = first.size() - s_this->body_index;
	ssize_t r = std::min( len, length );
	s_this->body_index += r;
	std::copy(first.begin(), first.begin() + r, buf);

	if ( s_this->body_index == first.size() )
	{
		s_this->body.pop_front();
		s_this->body_index = 0;
	}

	return r;
}

void stream_client::flush() noexcept
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
			id_ = nghttp2_submit_request( s_owner->next_layer(), nullptr, nva, nvlen, nullptr, this ); // TODO: handle return value < 0
			LOGTRACE( "id: ", id_, " nghttp2_submit_request :: ", nghttp2_strerror(id_) );
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

void stream_client::on_body( data_t data, size_t size )
{
	LOGTRACE("stream_client::on_body");

	body.emplace_back( data.get(), size );
	flush();
}

void stream_client::create_headers( nghttp2_nv** a ) noexcept
{
	*a = reinterpret_cast<nghttp2_nv*> (
		static_cast<http2_allocator<std::uint8_t>*>( s_owner->next_layer_allocator()->mem_user_data )
			->allocate( sizeof(nghttp2_nv) * nvlen ) );
}

void stream_client::destroy_headers( nghttp2_nv** d ) noexcept
{
	static_cast<http2_allocator<std::uint8_t>*>( s_owner->next_layer_allocator()->mem_user_data )
		->deallocate( reinterpret_cast<uint8_t*>( *d ), 0 );
	*d = nullptr;
}

void stream_client::die() noexcept
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
void stream_client::on_header(http::http_request&& req )
{
	req.filter ( []( const http::http_request::header_t& h ) -> bool
	{
		return
			utils::icompare ( h.first, "connection" ) || // Mandatory
			utils::icompare ( h.first, "Keep-Alive" ) || // SHOULD be removed
			utils::icompare ( h.first, "Upgrade" ) || // Ditto //
			utils::icompare ( h.first, "Proxy-Connection" /* Ditto */ );
	});
	if ( req.has("Transfer-Encoding") )
	{
		req.remove_header( "Transfer-Encoding" );
		req.header( "Transfer-Encoding", "trailers" );
	}

	for ( auto&& it : req.headers() )
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

	prepared_headers.clear();
	nvlen = prepared_headers.size() + 4;
	create_headers( &nva );

	std::size_t i = 0;
	for ( auto&& it : prepared_headers )
	{
		LOGTRACE( "Name:", static_cast<std::string>( it.first ),
			" Value:", static_cast<std::string>( it.second ), "-"  );
		nva[i++] = MAKE_NV( it.first, it.second );
	}

	// TODO: this is an hack!!! drop dstring instead!
	static const std::string method = ":method";
	static const std::string scheme = ":scheme";
	static const std::string authority = ":authority";
	static const std::string path = ":path";

	static std::string app1 = req.method();
	static std::string app2 = req.schema();
	static std::string app3 = req.hostname();
	static std::string app4 = req.path();

	nva[i++] = MAKE_NV(method, app1);
	nva[i++] = MAKE_NV(scheme, app2);
	nva[i++] = MAKE_NV(authority, app3);
	nva[i++] = MAKE_NV(path, app4);

	flush();
}

void stream_client::on_trailer( std::string&& key, std::string&& value )
{
	LOGTRACE("stream:", this, " on_trailer");
	trailers.emplace( http::http_structured_data::header_t{key, value} );
}

void stream_client::notify_error(http::error_code ec)
{
	if(res) res->error(ec);
	if(auto request = req) request->error(ec);
}

stream_client::~stream_client()
{
	//std::cout << "destroying stream: " << this << std::endl;
	s_owner->unsubscribe(this);
	LOGTRACE("Stream ", this, " destroyed");
	if ( nva ) destroy_headers( &nva );
	if ( trailers_nva ) destroy_headers( &trailers_nva );
	//destructor(this, s_owner.get());
}

void stream_client::uri_host( const std::string &p ) noexcept
{
	response.hostname( p );
}

void stream_client::set_handlers(std::shared_ptr< http::client_request > req_handler, std::shared_ptr< http::client_response > res_handler)
{
	res = res_handler;
	req = req_handler;
}

}
