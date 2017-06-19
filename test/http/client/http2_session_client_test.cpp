#include <gtest/gtest.h>

#include "src/http2/session_client.h"
#include "src/http2/session.h"
#include "src/http/server/request.h"
#include "src/http/server/response.h"
#include "src/http/client/request.h"
#include "src/http/client/response.h"
#include "mocks/mock_connector/mock_connector.h"

#include "boost/asio.hpp"

static std::unique_ptr<char[]> make_data_ptr(const std::string& s)
{
	auto ptr = std::make_unique<char[]>(s.size());
	std::copy(s.begin(), s.end(), ptr.get());
	return std::move(ptr);
}



TEST(http2_session_client_test, ok)
{
	std::function<void(std::string)> wcb_c;
	std::function<void(std::string)> wcb_s;
	boost::asio::io_service io;
	auto keep_alive = std::make_unique<boost::asio::io_service::work>(io);
	auto conn_c = std::make_shared<MockConnector>(io, wcb_c);
	auto conn_s = std::make_shared<MockConnector>(io, wcb_s);
	wcb_c = [&conn_s, &io](auto&& s) {
		io.post([&conn_s, s=std::move(s)] { conn_s->read(std::move(s)); });
	};
	wcb_s = [&conn_c, &io](auto&& s) {
		io.post([&conn_c, s=std::move(s)] { conn_c->read(std::move(s)); });
	};

	auto server = std::make_shared<http2::session>();
	server->connector(conn_s);
	conn_s->handler(server);
	server->on_request([](auto conn, auto req, auto res) {
		req->on_headers([](auto) {
			std::cout <<" youbeeeeeeeeee\n" << std::endl;
		});
		req->on_finished([res=std::move(res)](auto&&) {
			static const std::string body{"Allright"};
			http::http_response r;
			r.protocol(http::proto_version::HTTP20);
			r.status(200);
			r.header("content-type", "text/plain");
			r.header("date", "Tue, 17 May 2016 14:53:09 GMT");
			r.hostname("doormat_app.org");
			r.content_len(body.size());
			res->headers(std::move(r));
			res->body(make_data_ptr(body), body.size());
			res->end();
		});
	});

	auto client = std::make_shared<http2::session_client>();
	client->connector(conn_c);
	conn_c->handler(client);

	std::shared_ptr<http::client_request> req;
	std::shared_ptr<http::client_response> res;
	std::tie(req, res) = client->create_transaction();

	auto host = "gstatic.com";
	http::http_request preamble{};
	preamble.schema("schema");
	preamble.hostname(host);
	req->headers(std::move(preamble));
	req->end();
	res->on_finished([&keep_alive, &server, &client](auto res) {
		keep_alive.reset();
		server->close();
		client->close();
		std::cout << "tripleteeee:\n" << res->preamble().serialize() << std::endl;
	});

	io.run();
}






#include "src/http_client.h"
#include "src/network/communicator/dns_communicator_factory.h"

TEST(http2_client_test, client)
{
	assert(0 && "This test is somway blackbox...");
	auto ssl = true;
	auto port_num = ssl ? 443U : 80U;
	auto port = std::to_string(port_num);
	auto schema = ssl ? "https" : "http";
	auto path = "/images";
	auto host = "encrypted-tbn0.gstatic.com";
	auto query = "q=tbn:ANd9GcSeqVHzGaJD-KwfxaJ_eLBe696xkAzeW0CvvqZFiUIABhoozBM93t92QwL7";

	boost::asio::io_service io;
	auto keep_alive{std::make_unique<boost::asio::io_service::work>(io)};
	client::http_client<network::dns_connector_factory> client{io, std::chrono::milliseconds{1000}};
	client.connect([&](auto connection){
		std::cout << "youveee" << std::endl;
		std::shared_ptr<http::client_request> req;
		std::shared_ptr<http::client_response> res;
		std::tie(req, res) = connection->create_transaction();

		http::http_request preamble{};
		preamble.ssl(ssl);
		preamble.schema(schema);
		preamble.protocol(http::proto_version::HTTP11);
		preamble.urihost(host);
		preamble.hostname(host);
		preamble.query(query);
		preamble.setParameter("hostname", host);
		preamble.port(port);
		preamble.setParameter("port", port);
		preamble.path(path);
		req->headers(std::move(preamble));
		req->end();
		res->on_finished([&keep_alive, connection, &io](auto res) {
			keep_alive.reset();
			connection->close();
			std::cout << "tripleteeee:\n" << res->preamble().serialize() << std::endl;
		});
	}, [](auto&&){}, host, port_num, ssl);
	io.run();
}
