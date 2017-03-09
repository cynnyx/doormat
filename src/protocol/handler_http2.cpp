#include "handler_http2.h"
#include "../connector.h"
#include "../service_locator/service_locator.h"
#include "../utils/log_wrapper.h"
#include "../http/http_structured_data.h"
#include <string>
#include <string.h>
#include <utility>

//TODO: DRM-123
#include <nghttp2/../../../src/asio_server_http2_handler.h>
#include <nghttp2/../../../src/asio_server_serve_mux.h>

namespace server
{
handler_http2::handler_http2()
	: handler_interface()
	, mux(new ng_mux())
{
	LOGINFO("HTTP2 OLD selected");
	mux->handle("/", [this](const nghttp2::asio_http2::server::request &req,
		const nghttp2::asio_http2::server::response &res)
	{
		auto sh = std::make_shared<stream_handler>(handler_interface::make_chain());
		sh->listener(this);
		sh->response(&res);

		//Register response finalization cb (needed to couple sh and res lifecycles)
		res.on_close([sh](uint32_t ec) mutable
		{
			LOGTRACE("stream_handler ", sh.get(), " reponse::on_close");
			sh->response(nullptr);
		});

		http::http_request request{true};

		//Tranlate URI
		const auto &uri = req.uri();
		request.channel(http::proto_version::HTTP20);
		request.protocol(http::proto_version::HTTP11);

		if(uri.raw_path.size())
			request.path(dstring{uri.raw_path.data(), uri.raw_path.size()});

		if(uri.raw_query.size())
			request.query(dstring{uri.raw_query.data(), uri.raw_query.size()});

		if(uri.fragment.size())
			request.fragment(dstring{uri.fragment.data(), uri.fragment.size()});

		if(uri.host.size())
			request.hostname(dstring{uri.host.data(), uri.host.size()});//fixme

		request.method(req.method());
		request.origin(find_origin());

		//Translate headers
		for (auto &&hd:req.header())
		{
			dstring key{hd.first.data(), hd.first.size()};
			dstring value{hd.second.value.data(), hd.second.value.size(), hd.second.sensitive};
			request.header(key, value);
		}

		//Register callback for streamed content
		req.on_data([this,sh](const uint8_t *buffer, std::size_t len)
		{
			if(len)
				sh->on_request_body(dstring{buffer, len});
			else
				sh->on_request_finished();
		});

		sh->on_request_preamble(std::move(request));
		streams.emplace_back(sh);
	});
}

handler_http2::~handler_http2()
{
	//needed to allow forwarding defs of unique_pointed objects
	error_code_distruction = INTERNAL_ERROR_LONG(408);
	for(auto&& s: streams)
	{
		s->listener(nullptr);
		s->on_request_canceled(error_code_distruction);
	}

	LOGTRACE(this, " Destructor");
}

bool handler_http2::start() noexcept
{
	boost::system::error_code ec;
	if (connector() == nullptr) return false;
	auto &socket = static_cast<server::connector<ssl_socket> *>(connector())->socket();
	handler.reset(new ng_h2h(socket.get_io_service(), socket.lowest_layer().remote_endpoint(ec),
		[this]() { notify_write(); }, *mux));

	return (!ec && handler->start() == 0);
}

bool handler_http2::should_stop() const noexcept
{
	return local_should_stop && streams.empty();
}

bool handler_http2::on_read(const char* data, size_t len)
{
	if (connector())
	{
		assert( len <= inbuf::size());
		LOGTRACE(this, " Received chunk of size:", len);

		std::copy_n(data, len, _inbuf.begin());
		auto rv = handler->on_read(_inbuf, len);
		connector()->_rb.consume(data + len);
		local_should_stop = handler->should_stop();
		if(rv == 0)
		{
			handler->initiate_write();
			return true;
		}
		LOGDEBUG(this," Error during inner on_read:", rv);
	}
	//FIXME:Could this happen?
	else
		LOGDEBUG(this," Connector already gone while on_read");
	return false;
}

bool handler_http2::on_write(dstring& data)
{
	size_t size{0};
	auto rv = handler->on_write(_outbuf, size);
	local_should_stop = handler->should_stop();
	if(rv != 0)
	{
		LOGDEBUG(this, " Error during inner on_write:", rv);
		return false;
	}

	LOGTRACE(this, " Wrote a chunk of size:", size);
	//FIXME:useless copy takes place here
	data = dstring{_outbuf.data(), size};
	return true;
}

void handler_http2::on_eom()
{
	LOGTRACE(this, " on_eom");
	remove_terminated();
	if(!connector() && streams.empty())
		delete this;
}

void handler_http2::on_error(const int &ec)
{
	LOGTRACE(this, " on_error:", ec);
	remove_terminated();
	if(!connector() && streams.empty())
		delete this;
}

void handler_http2::remove_terminated()
{
	auto it = streams.begin();
	while(it != streams.end())
	{
		if((*it)->terminated())
			it = streams.erase(it);
		else
			it++;
	}
}

void handler_http2::do_write()
{
	if(connector())
		connector()->do_write();
	else
	{
		LOGERROR(this, " connector already destroyed");
		// go on with handler work, so that nghttp2 can call the generate_cb
		dstring str;
		on_write(str);
	}
}

void handler_http2::on_connector_nulled()
{
	remove_terminated();
	if(handler_http2::should_stop())
		delete this;
	else
		for(auto &s: streams) s->on_request_canceled(error_code_distruction);
}

}
