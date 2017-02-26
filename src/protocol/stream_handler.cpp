#include "stream_handler.h"
#include "handler_http2.h"
#include "../connector.h"
#include "../utils/log_wrapper.h"
#include "../http/http_structured_data.h"
#include "../log/inspector_serializer.h"
#include "../service_locator/service_locator.h"
#include <string>

//TODO: DRM-123
#include <nghttp2/../../../src/asio_server_http2_handler.h>

using namespace std;

namespace server
{

stream_handler::stream_handler(std::unique_ptr<node_interface>&& mc)
	: managed_chain{std::move(mc)}
{
	_access.set_request_start();

	LOGTRACE( "Constructor", this );

	// @todo to refactor
	header_callback hcb = [this](http::http_response&& headers){on_header(move(headers));};
	body_callback bcb = [this](dstring&& chunk){ on_body(std::move(chunk));};
	trailer_callback tcb = [this](dstring&& k, dstring&& v){on_trailer(move(k),move(v));};
	end_of_message_callback eomcb = [this](){on_eom();};
	error_callback ecb = [this](const errors::error_code& ec) { on_error( ec.code() ); };
	response_continue_callback rccb = [this](){on_response_continue();};

	managed_chain->initialize_callbacks(hcb, bcb, tcb, eomcb, ecb, rccb);
}

void stream_handler::on_request_preamble(http::http_request&& message)
{
	LOGTRACE("stream_handler:", this, " processing preamble");
	_access.request(message);
	managed_chain->on_request_preamble(std::move(message));
}

void stream_handler::on_request_body(dstring&& c)
{
	if ( service::locator::inspector_log().active() )
		_access.append_request_body( c );

	LOGTRACE("stream_handler:", this, " processing ", c.size(), " bytes");
	managed_chain->on_request_body(std::move(c));
}

void stream_handler::on_request_finished()
{
	LOGTRACE("stream_handler ", this, " request end detected");
	managed_chain->on_request_finished();
}

void stream_handler::on_request_canceled(const errors::error_code &ec)
{
	LOGTRACE("stream_handler:", this, "on_req_canceled");
	managed_chain->on_request_canceled(ec);
}

void stream_handler::on_header(http::http_response&& preamble)
{
	LOGTRACE("stream_handler:", this, " on_header");
	if (!_response)
	{
		LOGTRACE("stream_handler:", this, " Response is gone already!");
		return;
	}

	// add needed information to access log
	_access.response(preamble);

	nghttp2::asio_http2::header_map headers;
	preamble.filter([&headers](const http::http_structured_data::header_t& h)
	{
		//Filter function provides a mean to loop over headers
		//and apply predicate function on them.
		//It's intended to filter out unwanted headers
		//but here we use it to copy all but some into a different data_structure
		if(h.first != "connection" && h.first != "transfer-encoding" && h.first != "date")
		{
			nghttp2::asio_http2::header_value value{h.second, false};
			headers.insert( std::make_pair( h.first, std::move( value ) ) );
		}
		return false;
	});

	_response->write_head(preamble.status_code(), std::move(headers));

	//response could have been destroyed after previous call
	if(_response)
	{
		auto self = this->shared_from_this();
		_response->end([this,self](uint8_t* buffer, size_t len, uint32_t *data_flags)
		{
			ssize_t bytes{0};
			ssize_t copy_size{0};
			size_t space_left{0};

			while(buf_chain.size())
			{
				const auto& curr = buf_chain.front();
				space_left = len - bytes;

				if(!space_left)
					break;

				copy_size = std::min(curr.size() - partial_read_offset,space_left);
				memcpy(buffer+bytes, curr.cdata()+partial_read_offset, copy_size);
				bytes += copy_size;

				if(partial_read_offset + copy_size == curr.size())
				{
					partial_read_offset = 0;
					buf_chain.pop_front();
				}
				else
				{
					partial_read_offset += copy_size;
				}
			}

			if( buf_chain.empty() &&  _terminated )
			{
				LOGTRACE("stream_handler:", this, " finalizing response");
				*data_flags |= NGHTTP2_DATA_FLAG_EOF;
			}
			else if( !bytes )
			{
				LOGTRACE("stream_handler:", this, " response's not over, defer");
				bytes = NGHTTP2_ERR_DEFERRED;
			}
			return bytes;
		});
	}
}

void stream_handler::on_body(dstring&& chunk)
{
	LOGTRACE("stream_handler:", this, " on_body: received ", chunk.size(), " bytes");
	if(_response)
	{
		if ( service::locator::inspector_log().active())
			_access.append_response_body( chunk );
		_access.add_request_size( chunk.size() );
		buf_chain.push_back(std::move(chunk));
		_response->resume();
	}
	else
	{
		LOGTRACE("stream_handler:", this, " on_body: response is already gone");
	}
}

void stream_handler::on_trailer(dstring&& k, dstring&& v)
{
	LOGTRACE("stream_handler:", this, " on_trailer");

	/*
	if (!_response)
	{
		LOGTRACE("stream_handler:", this, " Response is gone already!");
		return;
	}

	if(k == "Connection")
		return;

	nghttp2::asio_http2::header_map trailer;
	nghttp2::asio_http2::header_value value = {std::string(v.begin(), v.end()), false};
	trailer.insert(std::make_pair(std::string(k.begin(), k.end()), std::move(value)));
	 */
	if ( service::locator::inspector_log().active()  )
		_access.append_response_trailer( k, v );
}

void stream_handler::on_eom()
{
	LOGTRACE("stream_handler:", this, " on_eom");
	_terminated = true;
	if(_response)
		_response->resume();

	_access.set_request_end();
	_listener->on_eom();
}

void stream_handler::on_error(const int &ec)
{
	LOGTRACE("stream_handler:", this, " on_error");

	_access.error( ec );
	_terminated = true;
	if (_response)
		_response->cancel(ec);

	_listener->on_error(ec);
}

void stream_handler::on_response_continue()
{
	LOGDEBUG("on_response_continue received");
	//Todo;
}

}
