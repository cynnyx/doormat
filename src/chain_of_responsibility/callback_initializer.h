#pragma once

#include "node_interface.h"

template<class T>
void callback_cor_initializer( std::unique_ptr<node_interface>& cor, T* handler )
{
	header_callback hcb = [handler](http::http_response&& headers)
	{
		handler->on_header(std::move(headers));
	};
	body_callback bcb = [handler](dstring &&chunk)
	{ 
		handler->on_body(std::move(chunk));
	};
	trailer_callback tcb = [handler](dstring&& k, dstring&& v)
	{
		handler->on_trailer(std::move(k),std::move(v));
	};
	end_of_message_callback eomcb = [handler]()
	{
		handler->on_eom();
	};
	error_callback ecb = [handler](const errors::error_code& ec)
	{
		handler->on_error(ec.code());
	};
	response_continue_callback rccb = [handler]()
	{
		handler->on_response_continue();
	};

	cor->initialize_callbacks(hcb,bcb,tcb,eomcb,ecb,rccb);
}
