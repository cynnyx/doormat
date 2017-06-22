#include <iostream>
#include <vector>
#include <iostream>
#include <future>
#include <string>

#include "boost/asio.hpp"

#include "doormat/doormat.hpp"

static std::unique_ptr<char[]> make_data_ptr(const std::string& s)
{
	auto ptr = std::make_unique<char[]>(s.size());
	std::copy(s.begin(), s.end(), ptr.get());
	return std::move(ptr);
}


int main ( int argc, const char**argv )
{
	std::string cert = "../app/cert/newcert.pem";
	std::string key = "../app/cert/newkey.pem";
	std::string password = "../app/cert/keypass";
	if(argc > 1)
		cert = argv[1];
	if(argc > 2)
		key = argv[2];
	if(argc > 3)
		password = argv[3];

	::log_wrapper::init ( false, "trace", "" );
	std::unique_ptr<doormat::http_server> doormat_srv = std::make_unique<doormat::http_server>(5000, 1443, 8081);
	doormat_srv->on_client_connect([](auto&& connection) {
		std::cout << "ON CONNECT" << std::endl;
		connection->on_request([](auto&&, auto&& req, auto&& res) {
			std::cout << "ON REQUEST" << std::endl;
			req->on_headers([res](auto&& req) {
				std::cout << "ON HEADERS" << std::endl;
				req->on_body([](auto&&, auto data, auto len) {
					std::cout << "ON BODY" << std::endl;
				});
				req->on_trailer([](auto&&, auto&& k, auto&& v) {
					std::cout << "ON TRAILER" << std::endl;
				});
				req->on_finished([res, proto=req->preamble().protocol_version()](auto&&) {
					std::cout << "ON FINISHED" << std::endl;
					std::string body = "Ave http2 client, server te salutat";
					http::http_response r;
					r.protocol(proto);
					r.status(200);
					r.header("content-type", "text/plain");
					r.header("date", "Tue, 17 May 2016 14:53:09 GMT");
					r.hostname("doormat_app.org");
					r.content_len(body.size());
					res->headers(std::move(r));
					res->body(make_data_ptr(body), body.size());
					res->end();
				});
				req->on_error([](auto&&, auto&& e) {
					std::cout << "ON ERROR" << std::endl;
				});
			});
		});
	});

	boost::asio::io_service io;
	doormat_srv->add_certificate(cert, key, password);
	doormat_srv->start (io);
	io.run();
}
