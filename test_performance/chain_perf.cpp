#include "../src/cache/cache.h"
#include "utils/measurements.h"
#include "../test/testcommon.h"
#include "../test/nodes/common.h"
#include "../src/cache/policies/fifo.h"
#include "../src/service_locator/service_initializer.h"
#include <gtest/gtest.h>
#include <requests_manager/configurable_header_filter.h>
#include <requests_manager/error_producer.h>
#include <requests_manager/request_stats.h>
#include <requests_manager/gzip_filter.h>
#include <requests_manager/date_setter.h>
#include <requests_manager/header_filter.h>
#include <requests_manager/method_filter.h>
#include <requests_manager/franco_host.h>
#include <requests_manager/sys_filter.h>
#include <requests_manager/interreg_filter.h>
#include <requests_manager/error_file_provider.h>
#include <requests_manager/id_header_adder.h>
#include <chain_of_responsibility/chain_of_responsibility.h>
#include <requests_manager/cache_manager/cache_manager.h>


using namespace test_utils;
/** Test on the performance of the chain without gzip and cache overhead.
 *
 * */

static std::string generate_random_path(size_t length) {
	static const char alphanum[] =
			"0123456789"
					"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
					"abcdefghijklmnopqrstuvwxyz";
	std::string s;
	s.reserve(length);

	for (size_t i = 0; i < length; ++i) {
		char randomlySelected = alphanum[rand() % (sizeof(alphanum) - 1)];
		s += randomlySelected;
	}

	return s;
}
http::http_response make_res_header()
{
	http::http_response r;
	r.status(200);
	r.header(("content-type"), ("application/json; charset=utf-8"));
	r.date(("Fri, 28 Oct 2016 15:15:37 GMT"));
	r.header(("etag"), ("rrfuiyr4589roihhjgduihuiydauioidhasuigh"));
	r.header(("x-debug-cyn-domain"), ("cydev1.com"));
	r.header(("x--debug-cyn-ep-trace-info"), ("61|61"));
	r.header(("x-debug-cyn-hash"), ("e251616b542b3b81838d49136dc47d89b758948f"));
	r.header(("x-debug-cyn-microserver-ip"), ("2a01:84a0:1001:a001::2:1"));
	r.header(("x-debug-cyn-region-id"), ("1"));
	r.header(("x-debug-cyn-version"), ("11.501"));
	r.header(("x-debug-rp-request-time"), ("0.173"));
	r.header(("x-powered-by"), ("Express"));

	return r;
}
struct analizing_node : public node_interface
{
	using node_interface::node_interface;

	analizing_node() {}

	void on_request_preamble(http::http_request&& message)
	{
		++managed_requests;
		subscribe(this);
	}

	void on_request_finished()
	{
		++managed_requests;
	}

	static int managed_requests;

	static std::vector<analizing_node *> subscribed;

	static void subscribe(analizing_node *subscriber)
	{
		subscribed.emplace_back(subscriber);
	}

	static void trigger()
	{
		for(auto &s: subscribed)
		{
			s->on_header(make_res_header());
			s->on_body(("HELLO, WORLD!"));
			s->on_end_of_message();
		}
	}
};


int analizing_node::managed_requests = 0;
std::vector<analizing_node *> analizing_node::subscribed;

struct nocache_mock_conf : test_utils::preset::mock_conf {

	bool cache_enabled() const noexcept override {
		return false;
	}

	std::string cache_path() const noexcept override
	{
		return "/dev/null/";
	}

};

struct req_representation
{
	http::http_request req;
	std::string body;
};

struct res_representation
{
	http::http_response res;
	std::string body;
};
std::unordered_map<std::string, std::pair<req_representation, res_representation>> morphcast_requests;


struct smart_node : public node_interface
{
	using node_interface::node_interface;

	smart_node() {std::cout << "creating" << std::endl; }

	void on_request_preamble(http::http_request&& message)
	{
		url = "https://"+std::string(message.urihost())+"/"+std::string(message.path());
		if(message.query()) url += "?"+ std::string(message.query());
		++managed_requests;
		subscribe(this);
	}

	void on_request_finished()
	{
		++managed_requests;
	}

	static int managed_requests;

	static std::vector<smart_node *> subscribed;

	static void subscribe(smart_node *subscriber)
	{
		subscribed.emplace_back(subscriber);
	}

	static void trigger()
	{
		std::cout << "need to send back" << subscribed.size() << "responses" << std::endl;
		for(auto &s: subscribed)
		{
			auto response = morphcast_requests.find(s->url);
			if(response != morphcast_requests.end())
			{
				http::http_response res = response->second.second.res;
				s->on_header(std::move(res));

				if(response->second.second.body.size())
					s->on_body(response->second.second.body.c_str());
				s->on_end_of_message();
				std::cout << "sending back along the chain!" << std::endl;
			}
			else
			{
				std::cout << "not found string " << s->url << std::endl;
			}
		}
	}
	std::string url;
};

int smart_node::managed_requests = 0;
std::vector<smart_node *> smart_node::subscribed;

struct chain_perf_basic : public preset::test {
protected:
	virtual void SetUp() override
	{
		preset::setup(new nocache_mock_conf{});
	}

	virtual void TearDown() override
	{
		preset::test::TearDown();
	}


	std::unique_ptr<node_interface> make_chain()
	{

		return make_unique_chain<node_interface,
				nodes::error_producer,
				nodes::request_stats,
				nodes::gzip_filter,
				nodes::date_setter,
				nodes::header_filter,
				nodes::method_filter,
				nodes::franco_host,
				nodes::sys_filter,
				nodes::interreg_filter,
				nodes::error_file_provider,
				nodes::id_header_adder,
				nodes::configurable_header_filter,
				nodes::cache_manager,
				analizing_node>();
	}

	std::unique_ptr<node_interface> make_smart_chain()
	{
		return make_unique_chain<node_interface,
				/*
				nodes::error_producer,
				nodes::request_stats,
				nodes::redirector,
				nodes::gzip_filter,
				nodes::date_setter,
				nodes::header_filter,
				nodes::method_filter,
				nodes::franco_host,
				nodes::sys_filter,
				nodes::interreg_filter,
				nodes::error_file_provider,
				nodes::id_header_adder,
				nodes::configurable_header_filter,
				nodes::cache_manager,*/
				smart_node>();

	}


	void add_headers(http::http_structured_data &container, nlohmann::json &headers)
	{
		for(auto &header : headers)
		{
			std::string name;
			std::string value;
			for(auto &h : header)
			{
				if(!name.size())
				{
					name = h;
					continue;
				}
				value = h;
			}
			container.header(name.c_str(), value.c_str());
		}
	}


	void load_morphcast_requests()
	{
		std::string morphcast_requests_har = "./etc/doormat/testing/morphcast1.har";
		std::filebuf fb;
		fb.open(morphcast_requests_har, std::ios::in);
		nlohmann::json har;
		nlohmann::json::parse(std::istream{&fb},[&](int depth, nlohmann::json::parse_event_t event, nlohmann::json& parsed){
			if(depth == 1) har = parsed;
			return true;
		});
		auto entries = har.find("entries");
		if(entries == har.cend()) { std::cout << "could not find entries in morphcast" << std::endl; }
		const boost::regex extract_url_fields{"https:\\/\\/([^\\/]+)\\/([^?]+)[?]?(.*)"};
		auto all_entries = entries.value();
		for(auto &c: all_entries)
		{
			auto request = c.find("request").value();
			auto response = c.find("response").value();
			std::string url = request.find("url").value();
			boost::smatch URI_match;
			if(!boost::regex_search(url, URI_match, extract_url_fields))
			{
				FAIL() << "Invalid url fetched!";
			}
			/** prepare request*/
			http::http_request reqs{};
			if(URI_match.size() < 3) FAIL() << "invalid url fetched.";
			std::string method = request.find("method").value();
			reqs.method(method);
			ASSERT_TRUE((std::string(reqs.method()) == method));
			reqs.urihost(URI_match[1].str().c_str());
			reqs.hostname(URI_match[1].str().c_str());
			reqs.path(URI_match[2].str().c_str());

			if(URI_match.size() == 4)
				reqs.query(URI_match[4].str().c_str());

			int body_size = request.find("bodySize").value();
			reqs.content_len(body_size);
			std::string reqstring = request.find("httpVersion").value();
			if(reqstring.find("2.0") != std::string::npos) reqs.protocol(http::proto_version::HTTP20);
			reqs.protocol(http::proto_version::HTTP11);
			auto headers = request.find("headers").value();
			add_headers(reqs, headers);
			req_representation rr;
			rr.req = reqs;
			if(request.find("content") != request.cend())
				rr.body = request.find("content").value().find("text").value();

			/** prepare response*/
			int status = response.find("status").value();
			http::http_response res{};
			res.status(status);
			std::string resstring = response.find("httpVersion").value();
			if(reqstring.find("2.0") != std::string::npos) res.protocol(http::proto_version::HTTP20);
			res.protocol(http::proto_version::HTTP11);
			headers = response.find("headers").value();
			add_headers(res, headers);
			res_representation rs;
			rs.res = res;
			if(response.find("content") != response.cend())
				rs.body = response.find("content").value().find("text").value();
			morphcast_requests.insert(std::pair<std::string, std::pair<req_representation, res_representation>>{url, std::pair<req_representation, res_representation>{rr, rs}});
		}
	}
};


void add_req_headers(http::http_request &req)
{
	req.urihost(("cy.cynnt.com"));
	req.header((":authority"), ("cy1.cydev1.com"));
	req.header((":method"), ("GET"));
	req.header((":scheme"), ("https"));
	req.header(("accept"), ("text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8"));
	req.header(("accept-encoding"), ("deflate, sdch, br"));
	req.header(("accept-language"), ("it-IT,it;q=0.8,en-US;q=0.6,en;q=0.4"));
	req.header(("cache-control"), ("max-age=0"));
	req.header(("upgrade-insecure-requests"), ("1"));
	req.header(("user-agent"), ("Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/54.0.2840.71 Safari/537.36"));
}

TEST_F(chain_perf_basic, simplest_get)
{
	std::string universal_path{"http://www.ciaon.org"};
	std::vector<std::unique_ptr<node_interface>> chain_pool;
	std::vector<http::http_request> requests_pool{};
	std::vector<std::string> body_pool{};
	size_t test_repetitions = 10000;
	measurement allocation("[CHAIN] Allocation (us)");
	measurement forward("[CHAIN] Basic HTTPGET forward (us)");
	measurement backward("[CHAIN] Basic HTTPGET backward (us)");
	chain_pool.reserve(test_repetitions);
	analizing_node::subscribed.reserve(test_repetitions);
	auto chain_creation = std::chrono::high_resolution_clock::now();
	for(size_t i = 0; i < test_repetitions; i++)
	{
		chain_pool.push_back(make_chain());
	}
	auto chain_creation_end = std::chrono::high_resolution_clock::now();
	allocation.put(std::chrono::duration_cast<std::chrono::microseconds>(chain_creation_end - chain_creation).count() / test_repetitions);
	for(size_t i = 0; i < test_repetitions; i++)
	{
		requests_pool.emplace_back(true);
		requests_pool[i].method(http_method::HTTP_GET);
		requests_pool[i].path(universal_path.c_str());
		requests_pool[i].protocol(http::proto_version::HTTP11);
		requests_pool[i].hostname(("cjaohsauishauishuoashuias"));
		add_req_headers(requests_pool[i]);
		body_pool.emplace_back(generate_random_path(600));
	}

	auto begin_forward = std::chrono::high_resolution_clock::now();
	for(size_t i = 0; i < test_repetitions; ++i)
	{
		auto &chain = chain_pool[i];
		chain->on_request_preamble(std::move(requests_pool[i]));
		chain->on_request_body(body_pool[i].c_str());
		chain->on_request_finished();
	}
	auto end_forward = std::chrono::high_resolution_clock::now();
	forward.put(std::chrono::duration_cast<std::chrono::microseconds>(end_forward - begin_forward).count()/test_repetitions);

	auto begin_bw = std::chrono::high_resolution_clock::now();
	analizing_node::trigger();
	auto end_bw = std::chrono::high_resolution_clock::now();

	backward.put(std::chrono::duration_cast<std::chrono::microseconds>(end_bw - begin_bw).count()/test_repetitions);

	allocation.push_average();
	forward.push_average();
	backward.push_average();
	ASSERT_EQ(analizing_node::managed_requests, 2*test_repetitions);
}



TEST_F(chain_perf_basic, morphcast_1)
{
	return; //todo: remove
	if(morphcast_requests.size() == 0) load_morphcast_requests();
	std::vector<std::unique_ptr<node_interface>> chain_pool;

	measurement forward("[CHAIN] DivinitaELavoratoriMorphcast forward(us)");
	measurement backward("[CHAIN] DivinitaELavoratoriMorphcast backward(us)");
	size_t test_repetitions = 1;

	for(size_t i = 0; i < test_repetitions*morphcast_requests.size(); i++)
	{
		chain_pool.push_back(make_smart_chain());
	}
	auto begin_forward = std::chrono::high_resolution_clock::now();
	for(size_t i = 0; i < test_repetitions; ++i)
	{
		int subindex = 0;
		for(auto &req : morphcast_requests)
		{
			auto index = i * morphcast_requests.size() + subindex;
			auto &chain = chain_pool[index];
			auto rcpy = req.second.first.req;
			/** reassemble url */
			std::string url = "https://"+std::string(rcpy.urihost())+"/"+std::string(rcpy.path());
			if(rcpy.query()) url += "?"+ std::string(rcpy.query());
			std::cout << "url is" << url << std::endl;
			chain->on_request_preamble(std::move(rcpy));
			if(req.second.first.body.size())
				chain->on_request_body(req.second.first.body.c_str());
			chain->on_request_finished();
			++subindex;
		}

	}
	auto end_forward = std::chrono::high_resolution_clock::now();
	forward.put(std::chrono::duration_cast<std::chrono::microseconds>(end_forward - begin_forward).count()/test_repetitions);
	auto begin_bw = std::chrono::high_resolution_clock::now();
	smart_node::trigger();
	auto end_bw = std::chrono::high_resolution_clock::now();
	backward.put(std::chrono::duration_cast<std::chrono::microseconds>(end_bw - begin_bw).count()/test_repetitions);
	//allocation.push_average();
	forward.push_average();
	backward.push_average();
	//ASSERT_EQ(analizing_node::managed_requests, 2*test_repetitions);

}
