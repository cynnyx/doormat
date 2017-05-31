#include "common.h"

#include "src/network/communicator/dns_communicator_factory.h"

namespace test_utils
{

std::vector<std::pair<bool, std::string>> preset::mock_conf::cache_domain_config;
bool preset::mock_conf::compression_enabled{false};

//first_node impl
boost::optional<http::http_request> first_node::request{};
boost::optional<http::http_response> first_node::response{};
uint first_node::req_body{0};
uint first_node::req_trailer{0};
uint first_node::req_eom{0};
uint first_node::res_body{0};
uint first_node::res_trailer{0};
uint first_node::res_eom{0};
uint first_node::res_continue{0};

std::string first_node::res_body_str{};
errors::error_code first_node::err{};

std::function<void(void)> first_node::res_start_fn{};
std::function<void(void)> first_node::res_stop_fn{};

void first_node::reset()
{
	request.reset();
	response.reset();
	res_body_str.clear();
	err = {};
	res_start_fn = {};
	res_stop_fn = {};

	req_body = 0;
	req_trailer = 0;
	req_eom = 0;
	res_body = 0;
	res_trailer = 0;
	res_eom = 0;
	res_continue = 0;
}

void first_node::on_request_preamble(http::http_request&& r)
{
	request = r;
	base::on_request_preamble(std::move(r));
}

void first_node::on_request_body(data_t data, size_t len)
{
	++req_body;
	base::on_request_body(std::move(data), len);
}

void first_node::on_request_trailer(std::string&& k, std::string&& v)
{
	++req_trailer;
	base::on_request_trailer(std::move(k), std::move(v));
}

void first_node::on_request_finished()
{
	++req_eom;
	base::on_request_finished();
}

void first_node::on_header(http::http_response&& r)
{
	response = r;
}

void first_node::on_body(data_t data, size_t len)
{
	res_body_str.append(data.get(), len);
	++res_body;
}

void first_node::on_trailer(std::string&&, std::string&&)
{
	++res_trailer;
}

void first_node::on_end_of_message()
{
	++res_eom;
	if(res_stop_fn)
		res_stop_fn();

	service::locator::service_pool().allow_graceful_termination();
}


void first_node::on_error(const errors::error_code& ec)
{
	err = ec;
	if(res_stop_fn)res_stop_fn();
	service::locator::service_pool().allow_graceful_termination();
}

void first_node::on_response_continue()
{
	++res_continue;
}

//last_node impl
boost::optional<http::http_request> last_node::request{};
boost::optional<http::http_response> last_node::response{};

uint last_node::req_body{0};
uint last_node::req_trailer{0};
uint last_node::req_eom{0};
errors::error_code last_node::err{};

std::list<std::string> last_node::body_parts;
std::string last_node::trailer_key{};
std::string last_node::trailer_value{};

void last_node::reset()
{
	request.reset();
	req_body = 0;
	req_trailer = 0;
	req_eom = 0;
	err = errors::error_code{};

	body_parts.clear();
	body_parts.emplace_back(std::string(1024, 'b').c_str());
	trailer_key = std::string{"trailer-key"};
	trailer_value = std::string{"trailer-value"};

	response.reset({});
	response->protocol(http::proto_version::HTTP11);
	response->status(200);
	response->chunked(true);
}

void last_node::on_request_preamble(http::http_request&& r)
{
	request = r;
}

void last_node::on_request_body(data_t, size_t)
{
	++req_body;
}

void last_node::on_request_trailer(std::string&& k, std::string&& v)
{
	++req_trailer;
}

void last_node::on_request_finished()
{
	++req_eom;

	if( response )
	{
		auto chunked = response->chunked();

		on_header(std::move(response.get()));
		response.reset();
		for( auto&& b : body_parts ) {
			auto ptr = std::make_unique<char[]>(b.size());
			std::copy(b.begin(), b.end(), ptr.get());
			on_body(std::move(ptr), b.size());
		}

		if( chunked )
			on_trailer(std::move(trailer_key),std::move(trailer_value));
		on_end_of_message();
	}
}

void last_node::on_request_canceled(const errors::error_code&ec)
{
	err=ec;
	on_error(ec);
}

namespace preset
{

/// @note if this grow up in parameters let's use a fluent builder!
void setup( configuration::configuration_wrapper* cw, logging::inspector_log* il )
{
	service::initializer::set_configuration(cw);
	service::initializer::set_service_pool(new server::io_service_pool{ cw->get_thread_number() });
	service::initializer::set_stats_manager(new stats::stats_manager{ cw->get_thread_number() });
	service::initializer::set_access_log(new logging::access_log_c{ cw->get_log_path() + "access_logs", "access"});
	if ( il == nullptr )
	{
		service::initializer::set_inspector_log
			(
				new logging::inspector_log{ cw->get_log_path() + "inspector_logs", "inspector", false }
			);
	}
	else
		service::initializer::set_inspector_log( il );
}

void teardown()
{
	service::locator::service_pool().stop();
	service::initializer::set_access_log(nullptr);
	service::initializer::set_inspector_log(nullptr);
	service::initializer::set_stats_manager(nullptr);
	service::initializer::set_service_pool(nullptr);
	service::initializer::set_configuration(nullptr);
}

void teardown( std::shared_ptr<node_interface>& chain )
{
	chain.reset();
	teardown();
}

void init_thread_local()
{

	service::locator::stats_manager().register_handler();
	service::initializer::set_communicator_factory(
				new network::dns_connector_factory(service::locator::service_pool().get_thread_io_service(),
												   std::chrono::milliseconds{100}));
}

void stop_thread_local()
{

}

}

}

