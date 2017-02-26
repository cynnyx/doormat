#include "../../src/requests_manager/cache_manager/cache_manager.h"
#include "common.h"
#include "../../src/service_locator/service_locator.h"
#include "../../src/io_service_pool.h"
#include "../testcommon.h"
#include "../../src/requests_manager/gzip_filter.h"
#include <functional>

using namespace test_utils;
struct cache_manager_test : public ::preset::test
{
	void TearDown() override
	{
		preset::test::TearDown();
		try {
			cynny::cynnypp::filesystem::removeDirectory("/tmp/cache/");
		} catch(cynny::cynnypp::filesystem::ErrorCode &ec)
		{
			std::cout << "that's ok, an error happened with ec" << ec.what() << std::endl;
		}
	}
};

//FIXME:
struct last_node_normal : node_interface
{
	using node_interface::node_interface;
	last_node_normal() {}
	~last_node_normal() noexcept { reset(); }
	static boost::optional<http::http_request> request;
	static uint req_body;
	static uint req_trailer;
	static uint req_eom;
	static errors::error_code err;
	static std::string _body;
	static std::string etag;
	static int status_code;
	static std::vector<std::pair<std::string, std::string>> headers;

	static void reset()
	{
		request.reset();
		req_body = 0;
		req_trailer = 0;
		req_eom = 0;
		err = errors::error_code{};
	}

	void on_request_preamble(http::http_request&& r)
	{
		request = r;
	}

	void on_request_body(dstring&&)
	{
		++req_body;
	}

	void on_request_trailer(dstring&&, dstring&&)
	{
		++req_trailer;
	}

	void on_request_finished()
	{
		++req_eom;
		http::http_response res;
		res.protocol(http::proto_version::HTTP11);
		res.status(status_code);
		res.content_len(_body.size());
		res.header("etag", etag.c_str());
		for(auto & v : headers)
			res.header(v.first.c_str(), v.second.c_str());

		on_header(std::move(res));
		on_body(_body.c_str());
		on_end_of_message();
	}

	void on_request_canceled(const errors::error_code&ec)
	{
		err=ec;
		on_error(ec);
	}
};
std::string last_node_normal::_body{"thebody"};
std::string last_node_normal::etag{"asjoaisausyhuiasguisgahjasg"};
int last_node_normal::status_code{200};
std::vector<std::pair<std::string, std::string>> last_node_normal::headers{};
boost::optional<http::http_request> last_node_normal::request{};
uint last_node_normal::req_body{0};
uint last_node_normal::req_trailer{0};
uint last_node_normal::req_eom{0};
errors::error_code last_node_normal::err{};



TEST_F(cache_manager_test, first_goes_through_second_is_cached)
{
	auto ch = make_unique_chain<node_interface, first_node, nodes::cache_manager, last_node_normal>();
	auto ch2 = make_unique_chain<node_interface, first_node, nodes::cache_manager, last_node_normal>();
	http::http_request first, second;

	first.method(http_method::HTTP_GET);
	dstring hostname = "www.ciaociao.org";
	dstring uri = "/prova0.txt";
	first.hostname(hostname);
	first.path(uri);
	first.protocol(http::proto_version::HTTP11);
	second = first;
//	dstring first_serialized;
//	first.serialize(first_serialized);
	ch->on_request_preamble(std::move(first));
	auto &io =service::locator::service_pool().get_thread_io_service();
	boost::asio::deadline_timer t{io};
	boost::asio::deadline_timer put_waiter{io};
	boost::asio::deadline_timer content_waiter{io};

	t.expires_from_now(boost::posix_time::milliseconds(1));
	t.async_wait([&](const boost::system::error_code &ec)
	{
		ASSERT_FALSE(ec) << "an unknown error stopped the timer" << ec.message();
		/** postponed message termination to make sure io_service is ok*/
		ch->on_request_finished();
		ASSERT_TRUE(last_node_normal::request);
		ASSERT_TRUE(last_node_normal::req_eom);
		ASSERT_TRUE(last_node_normal::request->path() == uri);
		ASSERT_TRUE(last_node_normal::request->hostname() == hostname);
		ASSERT_TRUE(first_node::response->header("x-cache-status") == "MISS");
		first_node::reset();
		last_node_normal::reset();
		put_waiter.expires_from_now(boost::posix_time::seconds(1)); //await writing on disk.
		put_waiter.async_wait([&](const boost::system::error_code &ec)
							  {
								  ASSERT_FALSE(ec);
								  ch2->on_request_preamble(std::move(second));
								  ch2->on_request_finished();
								  ASSERT_FALSE(last_node_normal::request);
								  ASSERT_FALSE(last_node_normal::req_eom);
								  ASSERT_FALSE(first_node::response);
								  content_waiter.expires_from_now(boost::posix_time::seconds(1));
								  content_waiter.async_wait([&](const boost::system::error_code &ec){

									  ASSERT_FALSE(ec);
									  /** By now get should have been terminate; hence*/
									  ASSERT_TRUE(first_node::response);
									  ASSERT_TRUE(first_node::response->status_code() == 200);
									  ASSERT_TRUE(std::string(first_node::response->header("x-cache-status")) == "HIT");
								  });

							  });
	});
	service::locator::service_pool().get_thread_io_service().run();
	ASSERT_TRUE(std::string(first_node::response->header("x-cache-status")) == "HIT");
}



TEST_F(cache_manager_test, not_modified_content) //todo: manage case in which, instead, the etag does not match
{
	auto ch = make_unique_chain<node_interface, first_node, nodes::cache_manager, last_node_normal>();
	auto ch2 = make_unique_chain<node_interface, first_node, nodes::cache_manager, last_node_normal>();
	http::http_request first, second;

	first.method(http_method::HTTP_GET);
	std::string hostname = "www.ciaociao.org";
	std::string uri = "/prova1.txt";
	first.hostname(hostname.c_str ());
	first.path(uri.c_str());
	first.protocol(http::proto_version::HTTP11);
	second = first;
	second.header("if-none-match", last_node_normal::etag.c_str());
	ch->on_request_preamble(std::move(first));
	auto &io =service::locator::service_pool().get_thread_io_service();
	boost::asio::deadline_timer t{io};
	boost::asio::deadline_timer put_waiter{io};
	boost::asio::deadline_timer content_waiter{io};

	t.expires_from_now(boost::posix_time::milliseconds(5));
	t.async_wait([&](const boost::system::error_code &ec)
				 {
					 ASSERT_FALSE(ec) << "an unknown error stopped the timer" << ec.message();
					 /** postponed message termination to make sure io_service is ok*/
					 ch->on_request_finished();
					 ASSERT_TRUE(last_node_normal::request);
					 ASSERT_TRUE(last_node_normal::req_eom);
					 ASSERT_TRUE(std::string(last_node_normal::request->path()) == uri);
					 ASSERT_TRUE(std::string(last_node_normal::request->hostname()) == hostname);
					 ASSERT_TRUE(std::string(first_node::response->header("x-cache-status")) == "MISS");
					 first_node::reset();
					 last_node_normal::reset();
					 put_waiter.expires_from_now(boost::posix_time::seconds(2)); //await writing on disk.
					 put_waiter.async_wait([&](const boost::system::error_code &ec)
										   {
											   ASSERT_FALSE(ec);
											   ch2->on_request_preamble(std::move(second));
											   ch2->on_request_finished();
											   ASSERT_FALSE(last_node_normal::request);
											   ASSERT_FALSE(last_node_normal::req_eom);
											   content_waiter.expires_from_now(boost::posix_time::seconds(1));
											   content_waiter.async_wait([&](const boost::system::error_code &ec){

												   ASSERT_FALSE(ec);
												   /** By now get should have been terminate; hence*/
												   ASSERT_TRUE(first_node::response);
												   ASSERT_TRUE(first_node::response->status_code() == 304);
												   //ASSERT_TRUE(std::string(first_node::response->header("x-cache-status")) == "HIT");
											   });

										   });
				 });
	service::locator::service_pool().get_thread_io_service().run();
	ASSERT_TRUE(first_node::response->status_code() == 304);
	//ASSERT_TRUE(std::string(first_node::response->header("x-cache-status")) == "HIT");
}



TEST_F(cache_manager_test, not_matching_etag) //todo: manage case in which, instead, the etag does not match
{
	auto ch = make_unique_chain<node_interface, first_node, nodes::cache_manager, last_node_normal>();
	auto ch2 = make_unique_chain<node_interface, first_node, nodes::cache_manager, last_node_normal>();
	http::http_request first, second;

	first.method(http_method::HTTP_GET);
	std::string hostname = "www.ciaociao.org";
	std::string uri = "/prova4.txt";
	first.hostname(hostname.c_str());
	first.path(uri.c_str());
	first.protocol(http::proto_version::HTTP11);
	second = first;
	second.header("if-none-match", "invalidetagiamsendingtoprovethatitworks");
	ch->on_request_preamble(std::move(first));
	auto &io =service::locator::service_pool().get_thread_io_service();
	boost::asio::deadline_timer t{io};
	boost::asio::deadline_timer put_waiter{io};
	boost::asio::deadline_timer content_waiter{io};

	t.expires_from_now(boost::posix_time::milliseconds(1));
	t.async_wait([&](const boost::system::error_code &ec)
				 {
					 ASSERT_FALSE(ec) << "an unknown error stopped the timer" << ec.message();
					 /** postponed message termination to make sure io_service is ok*/
					 ch->on_request_finished();
					 ASSERT_TRUE(last_node_normal::request);
					 ASSERT_TRUE(last_node_normal::req_eom);
					 ASSERT_TRUE(std::string(last_node_normal::request->path()) == uri);
					 ASSERT_TRUE(std::string(last_node_normal::request->hostname()) == hostname);
					 ASSERT_TRUE(std::string(first_node::response->header("x-cache-status")) == "MISS");
					 first_node::reset();
					 last_node_normal::reset();
					 put_waiter.expires_from_now(boost::posix_time::seconds(1)); //await writing on disk.
					 put_waiter.async_wait([&](const boost::system::error_code &ec)
					 {
						 ASSERT_FALSE(ec);
						 ch2->on_request_preamble(std::move(second));
						 ch2->on_request_finished();
						 ASSERT_FALSE(last_node_normal::request);
						 ASSERT_FALSE(last_node_normal::req_eom);
						 content_waiter.expires_from_now(boost::posix_time::seconds(1));
						 content_waiter.async_wait([&](const boost::system::error_code &ec)
						 {
							 ASSERT_FALSE(ec);
							 /** By now get should have been terminate; hence*/
							 ASSERT_TRUE(first_node::response);
							 ASSERT_TRUE(first_node::response->status_code() == 200);
							 ASSERT_TRUE(std::string(first_node::response->header("x-cache-status")) == "HIT");
							 ASSERT_TRUE(std::string(first_node::response->header("etag")) == last_node_normal::etag);
							 ASSERT_FALSE(last_node_normal::request);
							 ASSERT_FALSE(last_node_normal::req_eom);
						 });

					 });
				 });
	service::locator::service_pool().get_thread_io_service().run();
	ASSERT_TRUE(first_node::response->status_code() == 200);
}


TEST_F(cache_manager_test, invalidate_on_modification_put_complete)
{
	auto ch = make_unique_chain<node_interface, first_node, nodes::cache_manager, last_node_normal>();
	auto ch2 = make_unique_chain<node_interface, first_node, nodes::cache_manager, last_node_normal>();
	auto ch3 = make_unique_chain<node_interface, first_node, nodes::cache_manager, last_node_normal>();
	http::http_request first, second, third;

	first.method(http_method::HTTP_GET);
	std::string hostname = "www.ciaociao.org";
	std::string uri = "/prova3.txt";
	first.hostname(hostname.c_str());
	first.path(uri.c_str());
	first.protocol(http::proto_version::HTTP11);
	second = third = first;
	second.method(http_method::HTTP_DELETE);
	ch->on_request_preamble(std::move(first));
	auto &io =service::locator::service_pool().get_thread_io_service();
	boost::asio::deadline_timer t{io};
	boost::asio::deadline_timer put_waiter{io};
	boost::asio::deadline_timer content_waiter{io};
	std::string cache_expected_status{"invalid stuff"};
	t.expires_from_now(boost::posix_time::milliseconds(1));
	t.async_wait([&](const boost::system::error_code &ec)
				 {
					 ch->on_request_finished();
					 ASSERT_TRUE(last_node_normal::request);
					 ASSERT_TRUE(last_node_normal::req_eom);
					 ASSERT_TRUE(std::string(last_node_normal::request->path()) == uri);
					 ASSERT_TRUE(std::string(last_node_normal::request->hostname()) == hostname);
					 ASSERT_TRUE(std::string(first_node::response->header("x-cache-status")) == "MISS");
					 first_node::reset();
					 last_node_normal::reset();
					 /** First the element gets saved on cache.*/
					 put_waiter.expires_from_now(boost::posix_time::seconds(1));
					 put_waiter.async_wait([&](const boost::system::error_code &ec)
					 {
						/** Now that the element has been inserted...*/
						 ch2->on_request_preamble(std::move(second));  /** This would cause invaldiation. */
						 ch2->on_request_finished();
						 ASSERT_TRUE(last_node_normal::request);
						 ASSERT_TRUE(last_node_normal::req_eom);
						 ASSERT_TRUE(std::string(last_node_normal::request->path()) == uri);
						 ASSERT_TRUE(std::string(last_node_normal::request->hostname()) == hostname);
						 ASSERT_TRUE(first_node::response);
						 ASSERT_TRUE(first_node::response->status_code() == 200);
						 first_node::reset();
						 last_node_normal::reset();
						 /** Now send another get and see that it is a miss!*/
						ch3->on_request_preamble(std::move(third));
						ch3->on_request_finished(); /** This will give a miss; */
						content_waiter.expires_from_now(boost::posix_time::seconds(1));
						content_waiter.async_wait(
								[&](const boost::system::error_code &ec){
									/** Now check everything.*/
									ASSERT_TRUE(last_node_normal::request);
									ASSERT_TRUE(last_node_normal::req_eom);
									ASSERT_TRUE(std::string(last_node_normal::request->path()) == uri);
									ASSERT_TRUE(std::string(last_node_normal::request->hostname()) == hostname);
									ASSERT_TRUE(first_node::response);
									ASSERT_TRUE(first_node::response->status_code() == 200);
									cache_expected_status = "MISS";
								}
						);
					 });
				 });
	service::locator::service_pool().get_thread_io_service().run();
	ASSERT_TRUE(first_node::response->status_code() == 200);
	ASSERT_TRUE(std::string(first_node::response->header("x-cache-status")) == cache_expected_status);
}


TEST_F(cache_manager_test, status_codes_allowed)
{
	std::vector<int> allowed_status_codes{410, 203, 300, 301, 200};
	std::vector<int> some_forbidden_status_codes{500, 303, 501, 505, 404};
	auto current_chain = make_unique_chain<node_interface, first_node, nodes::cache_manager, last_node_normal>();
	boost::asio::deadline_timer single_request_spawner{service::locator::service_pool().get_thread_io_service()};
	boost::asio::deadline_timer wait_for_caching{service::locator::service_pool().get_thread_io_service()};
	boost::asio::deadline_timer wait_for_cached{service::locator::service_pool().get_thread_io_service()};
	std::function<void(const boost::system::error_code &ec)> spawn_request;
	std::string hostname = "www.ciaociao.org";
	std::string base_uri = "/prova_statuscode";
	std::string uri;
	spawn_request = [&](const boost::system::error_code &ec)
	{
		int current = 0;
		bool should_cache = false;
		if(allowed_status_codes.size())
		{
			should_cache = true;
			current = allowed_status_codes.back();
			allowed_status_codes.pop_back();
		} else if(some_forbidden_status_codes.size()){
			current = some_forbidden_status_codes.back();
			some_forbidden_status_codes.pop_back();
		} else return;
		/** Now that we have the status code, we can just go on generating our request.*/
		http::http_request request, verification_request;
		request.method(http_method::HTTP_GET);
		request.hostname(hostname.c_str());
		uri = base_uri + std::to_string(current);
		request.path(uri.c_str());
		request.protocol(http::proto_version::HTTP11);
		verification_request = request;
		last_node_normal::status_code = current;  /** Tell the status node what we wish to receive back!*/
		current_chain->on_request_preamble(std::move(request));
		current_chain->on_request_finished();
		wait_for_caching.expires_from_now(boost::posix_time::milliseconds(100));
		/** 100 ms should suffice; in case of failure, however, please check this first*/
		wait_for_caching.async_wait(
				[verification_request, current, should_cache, &current_chain, &wait_for_cached, &uri, &single_request_spawner, &spawn_request](const boost::system::error_code &ec)
				{
					ASSERT_TRUE(last_node_normal::request);
					ASSERT_TRUE(std::string(last_node_normal::request->path()) == uri);
					ASSERT_TRUE(first_node::response);
					ASSERT_TRUE(std::string(first_node::response->header("x-cache-status")) == "MISS");  /** This one is always miss!*/
					ASSERT_EQ(first_node::response->status_code(),current);

					last_node_normal::reset();
					first_node::reset();
					http::http_request req = verification_request;
					/** Prepare and send another request... That's the only way to verify that it was in cache*/
					current_chain = make_unique_chain<node_interface, first_node, nodes::cache_manager, last_node_normal>();
					current_chain->on_request_preamble(std::move(req));
					current_chain->on_request_finished();
					/** Now that the request is over, we just wait for the data*/
					wait_for_cached.expires_from_now(boost::posix_time::milliseconds(100));
					wait_for_cached.async_wait(
							[&, should_cache](const boost::system::error_code &ec)
							{
								/** Response should be arrived*/
								ASSERT_TRUE(first_node::response);
								if(should_cache)
								{
									ASSERT_EQ(std::string(first_node::response->header("x-cache-status")), "HIT"); /** Found in cache*/
									ASSERT_FALSE(last_node_normal::request);
								} else {
									ASSERT_TRUE(std::string(first_node::response->header("x-cache-status")) == "MISS"); /** Not found in cache*/
									ASSERT_TRUE(last_node_normal::request);
								}

								/** And now that everything was right... Schedule another one!*/
								last_node_normal::status_code = 200; /* reset for other tests.*/
								/** schedule another call*/
								single_request_spawner.expires_from_now(boost::posix_time::milliseconds(100));
								single_request_spawner.async_wait(spawn_request);
								first_node::reset();
								last_node_normal::reset();
							});

				}
		);

	};
	single_request_spawner.expires_from_now(boost::posix_time::milliseconds(5));
	single_request_spawner.async_wait(spawn_request);
	service::locator::service_pool().get_thread_io_service().run();
	ASSERT_TRUE(allowed_status_codes.empty());
	ASSERT_TRUE(some_forbidden_status_codes.empty());
}


TEST_F(cache_manager_test, request_max_age_0)
{
/** Given a request with max_age == 0, we will never retrieve it from the cache!*/
	http::http_request cached_request;
	std::string hostname = "www.ciaociao.org";
	std::string uri = "/prova_req_max_age";
	auto first_chain = make_unique_chain<node_interface, first_node, nodes::cache_manager, last_node_normal>();
	cached_request.hostname(hostname.c_str());
	cached_request.path(uri.c_str());
	cached_request.method(http_method::HTTP_GET);
	cached_request.protocol(http::proto_version::HTTP11);
	first_chain->on_request_preamble(http::http_request{cached_request});
	boost::asio::deadline_timer first_insert{service::locator::service_pool().get_thread_io_service()};
	service::locator::service_pool().get_thread_io_service().post([&](){
		first_chain->on_request_finished();
	});
	first_insert.expires_from_now(boost::posix_time::milliseconds(100));


	first_insert.async_wait([&](const boost::system::error_code &ec){
		/** Now it should be allright*/
		ASSERT_TRUE(last_node_normal::request);
		ASSERT_TRUE(first_node::response);
		ASSERT_TRUE(std::string(first_node::response->header("x-cache-status")) == "MISS"); /** Not found in cache*/
		/** Now we send a new one*/
		last_node_normal::reset();
		first_node::reset();
		first_chain = make_unique_chain<node_interface, first_node, nodes::cache_manager, last_node_normal>();
		http::http_request max_age_0{cached_request};
		ASSERT_FALSE(first_node::response);
		ASSERT_FALSE(last_node_normal::request);
		max_age_0.header("cache-control", "public, max-age=0");
		first_chain->on_request_preamble(std::move(max_age_0));
		first_chain->on_request_finished();
		ASSERT_TRUE(last_node_normal::request);
		ASSERT_TRUE(last_node_normal::req_eom);
		ASSERT_TRUE(first_node::response);
		ASSERT_TRUE(std::string(first_node::response->header("x-cache-status")) == "MISS"); /** Not found in cache*/
	});
	service::locator::service_pool().get_thread_io_service().run();
}

TEST_F(cache_manager_test, request_smaxage_0)
{
/** Given a request with max_age == 0, we will never retrieve it from the cache!*/
	http::http_request cached_request;
	std::string hostname = "www.ciaociao.org";
	std::string uri = "/prova_req_smax-age";
	auto first_chain = make_unique_chain<node_interface, first_node, nodes::cache_manager, last_node_normal>();
	cached_request.hostname(hostname.c_str());
	cached_request.path(uri.c_str());
	cached_request.method(http_method::HTTP_GET);
	cached_request.protocol(http::proto_version::HTTP11);
	first_chain->on_request_preamble(http::http_request{cached_request});
	boost::asio::deadline_timer first_insert{service::locator::service_pool().get_thread_io_service()};
	service::locator::service_pool().get_thread_io_service().post([&](){
		first_chain->on_request_finished();
	});
	first_insert.expires_from_now(boost::posix_time::milliseconds(100));


	first_insert.async_wait([&](const boost::system::error_code &ec){
		/** Now it should be allright*/
		ASSERT_TRUE(last_node_normal::request);
		ASSERT_TRUE(first_node::response);
		ASSERT_TRUE(std::string(first_node::response->header("x-cache-status")) == "MISS"); /** Not found in cache*/
		/** Now we send a new one*/
		last_node_normal::reset();
		first_node::reset();
		first_chain = make_unique_chain<node_interface, first_node, nodes::cache_manager, last_node_normal>();
		http::http_request max_age_0{cached_request};
		ASSERT_FALSE(first_node::response);
		ASSERT_FALSE(last_node_normal::request);
		max_age_0.header("cache-control", "s-maxage=0");
		first_chain->on_request_preamble(std::move(max_age_0));
		first_chain->on_request_finished();
		ASSERT_TRUE(last_node_normal::request);
		ASSERT_TRUE(last_node_normal::req_eom);
		ASSERT_TRUE(first_node::response);
		ASSERT_TRUE(std::string(first_node::response->header("x-cache-status")) == "MISS"); /** Not found in cache*/
	});
	service::locator::service_pool().get_thread_io_service().run();
}


TEST_F(cache_manager_test, request_no_cache)
{
/** Given a request with max_age == 0, we will never retrieve it from the cache!*/
	http::http_request cached_request;
	std::string hostname = "www.ciaociao.org";
	std::string uri = "/prova_req_nocache";
	auto first_chain = make_unique_chain<node_interface, first_node, nodes::cache_manager, last_node_normal>();
	cached_request.hostname(hostname.c_str());
	cached_request.path(uri.c_str());
	cached_request.method(http_method::HTTP_GET);
	cached_request.protocol(http::proto_version::HTTP11);
	first_chain->on_request_preamble(http::http_request{cached_request});
	boost::asio::deadline_timer first_insert{service::locator::service_pool().get_thread_io_service()};
	service::locator::service_pool().get_thread_io_service().post([&](){
		first_chain->on_request_finished();
	});
	first_insert.expires_from_now(boost::posix_time::milliseconds(100));


	first_insert.async_wait([&](const boost::system::error_code &ec){
		/** Now it should be allright*/
		ASSERT_TRUE(last_node_normal::request);
		ASSERT_TRUE(first_node::response);
		ASSERT_TRUE(std::string(first_node::response->header("x-cache-status")) == "MISS"); /** Not found in cache*/
		/** Now we send a new one*/
		last_node_normal::reset();
		first_node::reset();
		first_chain = make_unique_chain<node_interface, first_node, nodes::cache_manager, last_node_normal>();
		http::http_request max_age_0{cached_request};
		ASSERT_FALSE(first_node::response);
		ASSERT_FALSE(last_node_normal::request);
		max_age_0.header("cache-control", "no-cache");
		first_chain->on_request_preamble(std::move(max_age_0));
		first_chain->on_request_finished();
		ASSERT_TRUE(last_node_normal::request);
		ASSERT_TRUE(last_node_normal::req_eom);
		ASSERT_TRUE(first_node::response);
		ASSERT_TRUE(std::string(first_node::response->header("x-cache-status")) == "MISS"); /** Not found in cache*/
	});
	service::locator::service_pool().get_thread_io_service().run();
}


TEST_F(cache_manager_test, request_legacy_no_cache)
{
/** Given a request with max_age == 0, we will never retrieve it from the cache!*/
	http::http_request cached_request;
	std::string hostname = "www.ciaociao.org";
	std::string uri = "/prova_req_pragmanocache";
	auto first_chain = make_unique_chain<node_interface, first_node, nodes::cache_manager, last_node_normal>();
	cached_request.hostname(hostname.c_str());
	cached_request.path(uri.c_str());
	cached_request.method(http_method::HTTP_GET);
	cached_request.protocol(http::proto_version::HTTP11);
	first_chain->on_request_preamble(http::http_request{cached_request});
	boost::asio::deadline_timer first_insert{service::locator::service_pool().get_thread_io_service()};
	service::locator::service_pool().get_thread_io_service().post([&](){
		first_chain->on_request_finished();
	});
	first_insert.expires_from_now(boost::posix_time::milliseconds(100));


	first_insert.async_wait([&](const boost::system::error_code &ec){
		/** Now it should be allright*/
		ASSERT_TRUE(last_node_normal::request);
		ASSERT_TRUE(first_node::response);
		ASSERT_TRUE(std::string(first_node::response->header("x-cache-status")) == "MISS"); /** Not found in cache*/
		/** Now we send a new one*/
		last_node_normal::reset();
		first_node::reset();
		first_chain = make_unique_chain<node_interface, first_node, nodes::cache_manager, last_node_normal>();
		http::http_request max_age_0{cached_request};
		ASSERT_FALSE(first_node::response);
		ASSERT_FALSE(last_node_normal::request);
		max_age_0.header("pragma", "no-cache");
		first_chain->on_request_preamble(std::move(max_age_0));
		first_chain->on_request_finished();
		ASSERT_TRUE(last_node_normal::request);
		ASSERT_TRUE(last_node_normal::req_eom);
		ASSERT_TRUE(first_node::response);
		ASSERT_TRUE(std::string(first_node::response->header("x-cache-status")) == "MISS"); /** Not found in cache*/
	});
	service::locator::service_pool().get_thread_io_service().run();
}

TEST_F(cache_manager_test, response_max_age_0)
{
	/** Last node won't allow you to cache. Let us respect him*/
	last_node_normal::headers.emplace_back("cache-control", "max-age=0");

	auto ch = make_unique_chain<node_interface, first_node, nodes::cache_manager, last_node_normal>();
	auto ch2 = make_unique_chain<node_interface, first_node, nodes::cache_manager, last_node_normal>();
	http::http_request first, second;
	first.method(http_method::HTTP_GET);
	std::string hostname = "www.ciaociao.org";
	std::string uri = "/prova_response_no_cache.txt";
	first.hostname(hostname.c_str());
	first.path(uri.c_str());
	first.protocol(http::proto_version::HTTP11);
	second = first;
//	dstring first_serialized;
//	first.serialize(first_serialized);
	ch->on_request_preamble(std::move(first));
	auto &io =service::locator::service_pool().get_thread_io_service();
	boost::asio::deadline_timer t{io};
	boost::asio::deadline_timer put_waiter{io};
	boost::asio::deadline_timer content_waiter{io};

	t.expires_from_now(boost::posix_time::milliseconds(1));
	t.async_wait([&](const boost::system::error_code &ec)
	{
		ASSERT_FALSE(ec) << "an unknown error stopped the timer" << ec.message();
		/** postponed message termination to make sure io_service is ok*/
		ch->on_request_finished();
		ASSERT_TRUE(last_node_normal::request);
		ASSERT_TRUE(last_node_normal::req_eom);
		ASSERT_TRUE(std::string(last_node_normal::request->path()) == uri);
		ASSERT_TRUE(std::string(last_node_normal::request->hostname()) == hostname);
		ASSERT_TRUE(std::string(first_node::response->header("x-cache-status")) == "MISS");
		first_node::reset();
		last_node_normal::reset();
		put_waiter.expires_from_now(boost::posix_time::seconds(1)); //await, just in case it wants to write on disk.
		put_waiter.async_wait([&](const boost::system::error_code &ec)
		{
			ASSERT_FALSE(ec);
			ch2->on_request_preamble(std::move(second));
			ch2->on_request_finished();
			ASSERT_TRUE(last_node_normal::request);
			ASSERT_TRUE(last_node_normal::req_eom);
			content_waiter.expires_from_now(boost::posix_time::seconds(1)); //second, identical request was sent; now should do something about it
			content_waiter.async_wait([&](const boost::system::error_code &ec){

				ASSERT_FALSE(ec);
				/** By now get should have been terminate; hence*/
				ASSERT_TRUE(first_node::response);
				ASSERT_TRUE(first_node::response->status_code() == 200);
				ASSERT_TRUE(std::string(first_node::response->header("x-cache-status")) == "MISS");
			});

		});
	});
	service::locator::service_pool().get_thread_io_service().run();
	ASSERT_TRUE(std::string(first_node::response->header("x-cache-status")) == "MISS");
	last_node_normal::headers.clear();
	last_node_normal::reset();
	first_node::reset();
}


TEST_F(cache_manager_test, response_no_cache)
{
	/** Last node won't allow you to cache. Let us respect him*/
	last_node_normal::headers.emplace_back("cache-control", "no-cache");

	auto ch = make_unique_chain<node_interface, first_node, nodes::cache_manager, last_node_normal>();
	auto ch2 = make_unique_chain<node_interface, first_node, nodes::cache_manager, last_node_normal>();
	http::http_request first, second;
	first.method(http_method::HTTP_GET);
	std::string hostname = "www.ciaociao.org";
	std::string uri = "/prova_response_ccno_cache.txt";
	first.hostname(hostname.c_str());
	first.path(uri.c_str());
	first.protocol(http::proto_version::HTTP11);
	second = first;
//	dstring first_serialized;
//	first.serialize(first_serialized);
	ch->on_request_preamble(std::move(first));
	auto &io =service::locator::service_pool().get_thread_io_service();
	boost::asio::deadline_timer t{io};
	boost::asio::deadline_timer put_waiter{io};
	boost::asio::deadline_timer content_waiter{io};

	t.expires_from_now(boost::posix_time::milliseconds(1));
	t.async_wait([&](const boost::system::error_code &ec)
				 {
					 ASSERT_FALSE(ec) << "an unknown error stopped the timer" << ec.message();
					 /** postponed message termination to make sure io_service is ok*/
					 ch->on_request_finished();
					 ASSERT_TRUE(last_node_normal::request);
					 ASSERT_TRUE(last_node_normal::req_eom);
					 ASSERT_TRUE(std::string(last_node_normal::request->path()) == uri);
					 ASSERT_TRUE(std::string(last_node_normal::request->hostname()) == hostname);
					 ASSERT_TRUE(std::string(first_node::response->header("x-cache-status")) == "MISS");
					 first_node::reset();
					 last_node_normal::reset();
					 put_waiter.expires_from_now(boost::posix_time::seconds(1)); //await, just in case it wants to write on disk.
					 put_waiter.async_wait([&](const boost::system::error_code &ec)
					 {
						 ASSERT_FALSE(ec);
						 ch2->on_request_preamble(std::move(second));
						 ch2->on_request_finished();
						 ASSERT_TRUE(last_node_normal::request);
						 ASSERT_TRUE(last_node_normal::req_eom);
						 content_waiter.expires_from_now(boost::posix_time::seconds(1)); //second, identical request was sent; now should do something about it
						 content_waiter.async_wait([&](const boost::system::error_code &ec){

							 ASSERT_FALSE(ec);
							 /** By now get should have been terminate; hence*/
							 ASSERT_TRUE(first_node::response);
							 ASSERT_TRUE(first_node::response->status_code() == 200);
							 ASSERT_TRUE(std::string(first_node::response->header("x-cache-status")) == "MISS");
						 });

					 });
				 });
	service::locator::service_pool().get_thread_io_service().run();
	ASSERT_TRUE(std::string(first_node::response->header("x-cache-status")) == "MISS");
	last_node_normal::headers.clear();
	last_node_normal::reset();
	first_node::reset();
}


TEST_F(cache_manager_test, response_max_age_verify)
{
	/** Last node won't allow you to cache. Let us respect him*/
	last_node_normal::headers.emplace_back("cache-control", "max-age=20");

	auto ch = make_unique_chain<node_interface, first_node, nodes::cache_manager, last_node_normal>();
	auto ch2 = make_unique_chain<node_interface, first_node, nodes::cache_manager, last_node_normal>();
	http::http_request first, second;
	first.method(http_method::HTTP_GET);
	std::string hostname = "www.ciaociao.org";
	std::string uri = "/prova_response_ttl.txt";
	first.hostname(hostname.c_str());
	first.path(uri.c_str());
	first.protocol(http::proto_version::HTTP11);
	second = first;
//	dstring first_serialized;
//	first.serialize(first_serialized);
	ch->on_request_preamble(std::move(first));
	auto &io =service::locator::service_pool().get_thread_io_service();
	boost::asio::deadline_timer t{io};
	boost::asio::deadline_timer put_waiter{io};
	boost::asio::deadline_timer content_waiter{io};

	t.expires_from_now(boost::posix_time::milliseconds(1));
	t.async_wait([&](const boost::system::error_code &ec)
	{
		ASSERT_FALSE(ec) << "an unknown error stopped the timer" << ec.message();
		/** postponed message termination to make sure io_service is ok*/
		ch->on_request_finished();
		ASSERT_TRUE(last_node_normal::request);
		ASSERT_TRUE(last_node_normal::req_eom);
		ASSERT_TRUE(std::string(last_node_normal::request->path()) == uri);
		ASSERT_TRUE(std::string(last_node_normal::request->hostname()) == hostname);
		ASSERT_TRUE(std::string(first_node::response->header("x-cache-status")) == "MISS");
		first_node::reset();
		last_node_normal::reset();
		put_waiter.expires_from_now(boost::posix_time::seconds(20)); //await so that it expires.
		put_waiter.async_wait([&](const boost::system::error_code &ec)
		{
			ASSERT_FALSE(ec);
			ch2->on_request_preamble(std::move(second));
			ch2->on_request_finished();
			ASSERT_TRUE(last_node_normal::request);
			ASSERT_TRUE(last_node_normal::req_eom);
			content_waiter.expires_from_now(boost::posix_time::seconds(1)); //second, identical request was sent; now should do something about it
			content_waiter.async_wait([&](const boost::system::error_code &ec){

				ASSERT_FALSE(ec);
				/** By now get should have been terminate; hence*/
				ASSERT_TRUE(first_node::response);
				ASSERT_TRUE(first_node::response->status_code() == 200);
				ASSERT_TRUE(std::string(first_node::response->header("x-cache-status")) == "MISS");
			});

		});
	});
	service::locator::service_pool().get_thread_io_service().run();
	ASSERT_TRUE(std::string(first_node::response->header("x-cache-status")) == "MISS");
	last_node_normal::headers.clear();
	last_node_normal::reset();
	first_node::reset();
}

TEST_F(cache_manager_test, invalid_cache_domain)
{
	std::vector<std::pair<bool, std::string>> desired_config;
	desired_config.emplace_back(false, "www.domain_not_allowed.org");
	preset::mock_conf::set_cache_domains_config(std::move(desired_config));

	auto ch = make_unique_chain<node_interface, first_node, nodes::cache_manager, last_node_normal>();
	auto ch2 = make_unique_chain<node_interface, first_node, nodes::cache_manager, last_node_normal>();
	http::http_request first, second;
	first.method(http_method::HTTP_GET);
	std::string hostname = "www.domain_not_allowed.org";
	std::string uri = "/prova_domain_not_allowed.txt";
	first.hostname(hostname.c_str());
	first.path(uri.c_str());
	first.protocol(http::proto_version::HTTP11);
	second = first;
//	dstring first_serialized;
//	first.serialize(first_serialized);
	ch->on_request_preamble(std::move(first));
	auto &io =service::locator::service_pool().get_thread_io_service();
	boost::asio::deadline_timer t{io};
	boost::asio::deadline_timer put_waiter{io};
	boost::asio::deadline_timer content_waiter{io};

	t.expires_from_now(boost::posix_time::milliseconds(1));
	t.async_wait([&](const boost::system::error_code &ec)
	{
		ASSERT_FALSE(ec) << "an unknown error stopped the timer" << ec.message();
		/** postponed message termination to make sure io_service is ok*/
		ch->on_request_finished();
		ASSERT_TRUE(last_node_normal::request);
		ASSERT_TRUE(last_node_normal::req_eom);
		ASSERT_TRUE(std::string(last_node_normal::request->path()) == uri);
		ASSERT_TRUE(std::string(last_node_normal::request->hostname()) == hostname);
		ASSERT_TRUE(std::string(first_node::response->header("x-cache-status")) == "MISS");
		first_node::reset();
		last_node_normal::reset();
		put_waiter.expires_from_now(boost::posix_time::seconds(1)); //await, just in case it wants to write on disk.
		put_waiter.async_wait([&](const boost::system::error_code &ec)
		{
			ASSERT_FALSE(ec);
			ch2->on_request_preamble(std::move(second));
			ch2->on_request_finished();
			ASSERT_TRUE(last_node_normal::request);
			ASSERT_TRUE(last_node_normal::req_eom);
			content_waiter.expires_from_now(boost::posix_time::seconds(1)); //second, identical request was sent; now should do something about it
			content_waiter.async_wait([&](const boost::system::error_code &ec){

				ASSERT_FALSE(ec);
				/** By now get should have been terminate; hence*/
				ASSERT_TRUE(first_node::response);
				ASSERT_TRUE(first_node::response->status_code() == 200);
				ASSERT_TRUE(std::string(first_node::response->header("x-cache-status")) == "MISS");
			});

		});
	});
	service::locator::service_pool().get_thread_io_service().run();
	ASSERT_TRUE(std::string(first_node::response->header("x-cache-status")) == "MISS");
	last_node_normal::headers.clear();
	last_node_normal::reset();
	first_node::reset();
}


TEST_F(cache_manager_test, valid_cache_domain)
{
	std::vector<std::pair<bool, std::string>> desired_config;
	desired_config.emplace_back(true, "ww\\d+.domain_not_allowed.org");
	desired_config.emplace_back(false, "[.*].domain_not_allowed.org");

	preset::mock_conf::set_cache_domains_config(std::move(desired_config));

	auto ch = make_unique_chain<node_interface, first_node, nodes::cache_manager, last_node_normal>();
	auto ch2 = make_unique_chain<node_interface, first_node, nodes::cache_manager, last_node_normal>();
	http::http_request first, second;
	first.method(http_method::HTTP_GET);
	std::string hostname = "ww1.domain_not_allowed.org";
	std::string uri = "/prova_domain_allowed.txt";
	first.hostname(hostname.c_str());
	first.path(uri.c_str());
	first.protocol(http::proto_version::HTTP11);
	second = first;
//	dstring first_serialized;
//	first.serialize(first_serialized);
	ch->on_request_preamble(std::move(first));
	auto &io =service::locator::service_pool().get_thread_io_service();
	boost::asio::deadline_timer t{io};
	boost::asio::deadline_timer put_waiter{io};
	boost::asio::deadline_timer content_waiter{io};

	t.expires_from_now(boost::posix_time::milliseconds(1));
	t.async_wait([&](const boost::system::error_code &ec)
	{
		ASSERT_FALSE(ec) << "an unknown error stopped the timer" << ec.message();
		/** postponed message termination to make sure io_service is ok*/
		ch->on_request_finished();
		ASSERT_TRUE(last_node_normal::request);
		ASSERT_TRUE(last_node_normal::req_eom);
		ASSERT_TRUE(std::string(last_node_normal::request->path()) == uri);
		ASSERT_TRUE(std::string(last_node_normal::request->hostname()) == hostname);
		ASSERT_TRUE(std::string(first_node::response->header("x-cache-status")) == "MISS");
		first_node::reset();
		last_node_normal::reset();
		put_waiter.expires_from_now(boost::posix_time::seconds(1)); //await, just in case it wants to write on disk.
		put_waiter.async_wait([&](const boost::system::error_code &ec)
		{
			ASSERT_FALSE(ec);
			ch2->on_request_preamble(std::move(second));
			ch2->on_request_finished();
			ASSERT_FALSE(last_node_normal::request);
			ASSERT_FALSE(last_node_normal::req_eom);
			content_waiter.expires_from_now(boost::posix_time::seconds(1)); //second, identical request was sent; now should do something about it
			content_waiter.async_wait([&](const boost::system::error_code &ec){

				ASSERT_FALSE(ec);
				/** By now get should have been terminate; hence*/
				ASSERT_TRUE(first_node::response);
				ASSERT_TRUE(first_node::response->status_code() == 200);
				ASSERT_TRUE(std::string(first_node::response->header("x-cache-status")) == "HIT");
			});

		});
	});

	service::locator::service_pool().get_thread_io_service().run();
	ASSERT_TRUE(std::string(first_node::response->header("x-cache-status")) == "HIT");
	last_node_normal::headers.clear();
	last_node_normal::reset();
	first_node::reset();

}

TEST_F(cache_manager_test, gzip_in_cache)
{
	test_utils::preset::mock_conf::set_compression(true);
	last_node_normal::headers.push_back(std::make_pair("content-type", "application/json"));

	auto plain_req = make_unique_chain<node_interface, first_node, nodes::cache_manager, nodes::gzip_filter, last_node_normal>();
	auto gzip_req = make_unique_chain<node_interface, first_node, nodes::cache_manager, nodes::gzip_filter, last_node_normal>();
	auto plain_req_check = make_unique_chain<node_interface, first_node, nodes::cache_manager, nodes::gzip_filter, last_node_normal>();
	auto gzip_req_check = make_unique_chain<node_interface, first_node, nodes::cache_manager, nodes::gzip_filter, last_node_normal>();
	std::string hostname = "ww1.hsajisakugigoregiurguyrewguyghgsaajhasg.org";
	std::string uri = "/plain_request_vs_gzip.txt";
	http::http_request plain;
	plain.method(http_method::HTTP_GET);
	plain.hostname(hostname.c_str());
	plain.path(uri.c_str());
	last_node_normal::_body = "the quick brown fox jumped over the lazy dog";
	/** plain is just plain: no encoding accepted*/
	http::http_request plain_verifier{plain};
	http::http_request gzip;
	gzip.method(http_method::HTTP_GET);
	gzip.urihost(hostname.c_str());
	gzip.hostname(hostname.c_str());
	gzip.path(uri.c_str());
	gzip.header("accept-encoding", "gzip;q=1.0, identity; q=0.5, *;q=0");
	http::http_request gzip_verifier{gzip};
	plain_req->on_request_preamble(std::move(plain));
	auto &io =service::locator::service_pool().get_thread_io_service();
	boost::asio::deadline_timer t0{io};
	boost::asio::deadline_timer t1{io};
	boost::asio::deadline_timer t2{io};
	boost::asio::deadline_timer t3{io};
	boost::asio::deadline_timer t4{io};
	std::string gzipped_data;
	t0.expires_from_now(boost::posix_time::milliseconds(1));
	t0.async_wait([&](const boost::system::error_code &ec){
		ASSERT_FALSE(ec) << "someone stopped the timer, come on!" << ec.message();
		plain_req->on_request_finished();
		ASSERT_TRUE(last_node_normal::request);
		ASSERT_TRUE(last_node_normal::req_eom);
		ASSERT_TRUE(std::string(last_node_normal::request->path()) == uri);
		ASSERT_TRUE(std::string(last_node_normal::request->hostname()) == hostname);
		ASSERT_TRUE(std::string(first_node::response->header("x-cache-status")) == "MISS");
		first_node::reset();
		last_node_normal::reset();
		/** Run the verifier: the stuff must be in cache*/
		t1.expires_from_now(boost::posix_time::seconds(1));
		t1.async_wait([&](const boost::system::error_code &ec)
		{
			ASSERT_FALSE(ec) << "failure";
			gzip_req->on_request_preamble(std::move(gzip));
			gzip_req->on_request_finished();
			ASSERT_TRUE(last_node_normal::request);
			ASSERT_TRUE(last_node_normal::req_eom);
			ASSERT_TRUE(std::string(last_node_normal::request->path()) == uri);
			ASSERT_TRUE(std::string(last_node_normal::request->hostname()) == hostname);
			ASSERT_TRUE(std::string(first_node::response->header("x-cache-status")) == "MISS");
			ASSERT_TRUE(std::string(first_node::res_body_str) != "the quick brown fox jumped over the lazy fox");
			gzipped_data = std::string(first_node::res_body_str);
			first_node::reset();
			last_node_normal::reset();
			plain_req_check->on_request_preamble(std::move(plain_verifier));
			plain_req_check->on_request_finished();
			ASSERT_FALSE(last_node_normal::request);
			ASSERT_FALSE(last_node_normal::req_eom);
			t3.expires_from_now(boost::posix_time::seconds(1));
			t3.async_wait([&](const boost::system::error_code &ec){
				/** retrieve results from cache. */
				ASSERT_TRUE(first_node::response);
				ASSERT_TRUE(std::string(first_node::response->header("x-cache-status")) == "HIT");
				ASSERT_TRUE(std::string(first_node::res_body_str) == last_node_normal::_body);
				first_node::reset();
				last_node_normal::reset();
				gzip_req_check->on_request_preamble(std::move(gzip_verifier));
				gzip_req_check->on_request_finished();
				t4.expires_from_now(boost::posix_time::seconds(1));
				t4.async_wait([&](const boost::system::error_code &ec){
					ASSERT_FALSE(last_node_normal::request);
					ASSERT_FALSE(last_node_normal::req_eom);
					ASSERT_TRUE(first_node::response);
					ASSERT_TRUE(std::string(first_node::response->header("x-cache-status")) == "HIT");
				});
			});


		});
		/** Resetting the stuff, now send over the first gzip request after a while, to be sure that it is in cache*/
		/*
		t3.async_wait([&](const boost::system::error_code &ec){
			ASSERT_FALSE(ec) << "someone stopped the timer, come on!";
			gzip_req->on_request_preamble(std::move(gzip));
			gzip_req->on_request_finished();
		});*/
	});


	io.run();
	ASSERT_TRUE(std::string(first_node::res_body_str) == gzipped_data);
	ASSERT_TRUE(std::string(first_node::response->header("x-cache-status")) == "HIT");
	test_utils::preset::mock_conf::set_compression(false);
}


#ifdef DOOORMAT_PREFETCH_ENABLED
TEST_F(cache_manager_test, head_before_get)
{
	std::vector<std::pair<bool, std::string>> desired_config;
	desired_config.emplace_back(true, "ww\\d+.domain_not_allowed.org");
	desired_config.emplace_back(false, "[.*].domain_not_allowed.org");

	preset::mock_conf::set_cache_domains_config(std::move(desired_config));

	auto ch = make_unique_chain<node_interface, first_node, nodes::cache_manager, last_node_normal>();
	auto ch2 = make_unique_chain<node_interface, first_node, nodes::cache_manager, last_node_normal>();
	http::http_request first, second;
	first.method(http_method::HTTP_HEAD);
	std::string hostname = "ww1.domain_not_allowed.org";
	std::string uri = "/prova_head_prefetch.txt";
	first.hostname(hostname.c_str());
	first.path(uri.c_str());
	first.protocol(http::proto_version::HTTP11);
	second = first;
	second.method(http_method::HTTP_GET);
	dstring first_serialized;
	first.serialize(first_serialized);
	ch->on_request_preamble(std::move(first));
	auto &io =service::locator::service_pool().get_thread_io_service();
	boost::asio::deadline_timer t{io};
	boost::asio::deadline_timer put_waiter{io};
	boost::asio::deadline_timer content_waiter{io};

	t.expires_from_now(boost::posix_time::milliseconds(1));
	t.async_wait([&](const boost::system::error_code &ec)
	{
		ASSERT_FALSE(ec) << "an unknown error stopped the timer" << ec.message();
		/** postponed message termination to make sure io_service is ok*/
		ch->on_request_finished();
		ASSERT_TRUE(last_node_normal::request);
		ASSERT_TRUE(last_node_normal::req_eom);
		ASSERT_TRUE(std::string(last_node_normal::request->path()) == uri);
		ASSERT_TRUE(std::string(last_node_normal::request->hostname()) == hostname);
		ASSERT_TRUE(std::string(first_node::response->header("x-cache-status")) == "MISS");
		ASSERT_FALSE(first_node::res_body);
		ASSERT_TRUE(first_node::response);
		ASSERT_TRUE(first_node::res_eom);
		first_node::reset();
		last_node_normal::reset();
		put_waiter.expires_from_now(boost::posix_time::seconds(1)); //await, just in case it wants to write on disk.
		put_waiter.async_wait([&](const boost::system::error_code &ec)
		{
			ASSERT_FALSE(ec);
			ch2->on_request_preamble(std::move(second));
			ch2->on_request_finished();
			ASSERT_FALSE(last_node_normal::request);
			ASSERT_FALSE(last_node_normal::req_eom);
			content_waiter.expires_from_now(boost::posix_time::seconds(1)); //second, identical request was sent; now should do something about it
			content_waiter.async_wait([&](const boost::system::error_code &ec){

				ASSERT_FALSE(ec);
				/** By now get should have been terminate; hence*/
				ASSERT_TRUE(first_node::response);
				ASSERT_TRUE(first_node::response->status_code() == 200);
				ASSERT_TRUE(std::string(first_node::response->header("x-cache-status")) == "HIT");
			});

		});
	});

	service::locator::service_pool().get_thread_io_service().run();
	ASSERT_TRUE(std::string(first_node::response->header("x-cache-status")) == "HIT");
	last_node_normal::headers.clear();
	last_node_normal::reset();
	first_node::reset();

}


TEST_F(cache_manager_test, get_before_head)
{
	std::vector<std::pair<bool, std::string>> desired_config;
	desired_config.emplace_back(true, "ww\\d+.domain_not_allowed.org");
	desired_config.emplace_back(false, "[.*].domain_not_allowed.org");

	preset::mock_conf::set_cache_domains_config(std::move(desired_config));

	auto ch = make_unique_chain<node_interface, first_node, nodes::cache_manager, last_node_normal>();
	auto ch2 = make_unique_chain<node_interface, first_node, nodes::cache_manager, last_node_normal>();
	http::http_request first, second;
	first.method(http_method::HTTP_GET);
	std::string hostname = "ww1.domain_not_allowed.org";
	std::string uri = "/prova_get_before_head.txt";
	first.hostname(hostname.c_str());
	first.path(uri.c_str());
	first.protocol(http::proto_version::HTTP11);
	second = first;
	second.method(http_method::HTTP_HEAD);
	dstring first_serialized;
	first.serialize(first_serialized);
	ch->on_request_preamble(std::move(first));
	auto &io =service::locator::service_pool().get_thread_io_service();
	boost::asio::deadline_timer t{io};
	boost::asio::deadline_timer put_waiter{io};
	boost::asio::deadline_timer content_waiter{io};

	t.expires_from_now(boost::posix_time::milliseconds(1));
	t.async_wait([&](const boost::system::error_code &ec)
	{
		ASSERT_FALSE(ec) << "an unknown error stopped the timer" << ec.message();
		/** postponed message termination to make sure io_service is ok*/
		ch->on_request_finished();
		ASSERT_TRUE(last_node_normal::request);
		ASSERT_TRUE(last_node_normal::req_eom);
		ASSERT_TRUE(std::string(last_node_normal::request->path()) == uri);
		ASSERT_TRUE(std::string(last_node_normal::request->hostname()) == hostname);
		ASSERT_TRUE(std::string(first_node::response->header("x-cache-status")) == "MISS");
		ASSERT_TRUE(first_node::res_body);
		ASSERT_TRUE(first_node::response);
		ASSERT_TRUE(first_node::res_eom);
		first_node::reset();
		last_node_normal::reset();
		put_waiter.expires_from_now(boost::posix_time::seconds(1)); //await, just in case it wants to write on disk.
		put_waiter.async_wait([&](const boost::system::error_code &ec)
		{
			ASSERT_FALSE(ec);
			ch2->on_request_preamble(std::move(second));
			ch2->on_request_finished();
			ASSERT_FALSE(last_node_normal::request);
			ASSERT_FALSE(last_node_normal::req_eom);
			content_waiter.expires_from_now(boost::posix_time::seconds(1)); //second, identical request was sent; now should do something about it
			content_waiter.async_wait([&](const boost::system::error_code &ec){

				ASSERT_FALSE(ec);
				/** By now get should have been terminate; hence*/
				ASSERT_TRUE(first_node::response);
				ASSERT_TRUE(first_node::response->status_code() == 200);
				ASSERT_FALSE(first_node::res_body);
				ASSERT_TRUE(first_node::res_eom);
				ASSERT_TRUE(std::string(first_node::response->header("x-cache-status")) == "HIT");
			});

		});
	});

	service::locator::service_pool().get_thread_io_service().run();
	ASSERT_TRUE(std::string(first_node::response->header("x-cache-status")) == "HIT");
	last_node_normal::headers.clear();
	last_node_normal::reset();
	first_node::reset();

}
#endif
