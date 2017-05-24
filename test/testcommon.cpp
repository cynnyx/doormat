#include "testcommon.h"


http::http_request make_request(http::proto_version protocol, http_method method, const std::string& host, const std::string& path)
{
	static constexpr auto accept = "accept";
	static constexpr auto accept_val = "text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8";
	static constexpr auto accept_encoding = "accept-encoding";
	static constexpr auto accept_encoding_val = "gzip, deflate, sdch, br";
	static constexpr auto accept_language = "accept-language";
	static constexpr auto accept_language_val = "en-US,en;q=0.8";
	static constexpr auto cache_control= "cache-control";
	static constexpr auto cache_control_val= "no-cache";
	static constexpr auto pragma = "pragma";
	static constexpr auto pragma_val = "no-cache";
	static constexpr auto upgrade_insecure_requests = "upgrade-insecure-requests";
	static constexpr auto upgrade_insecure_requests_val = "1";
	static constexpr auto user_agent = "user-agent";
	static constexpr auto user_agent_val = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/51.0.2704.84 Safari/537.36";

	http::http_request preamble{true};
	preamble.protocol(protocol);
	preamble.method(method);
	preamble.header(accept, accept_val);
	preamble.header(accept_encoding, accept_encoding_val);
	preamble.header(accept_language, accept_language_val);
	preamble.header(cache_control, cache_control_val);
	preamble.header(pragma, pragma_val);
	preamble.header(upgrade_insecure_requests, upgrade_insecure_requests_val);
	preamble.header(user_agent, user_agent_val);
	preamble.hostname(host);
	preamble.path(path);

	return preamble;
}

const std::string forbidden_html = "<!DOCTYPE HTML>\n<html>\n<head>\n<title>403</title>\n<meta http-equiv=\"Content-Type\" "
		"content=\"text/html; charset=utf-8\" />\n<meta name=\"viewport\" content=\"width=device-width, initial-scale=1, "
		"maximum-scale=1\">\n<link href=\"https://errors.tcynny.com:1443/css/style.css\" rel=\"stylesheet\" type=\"text/css\" "
		"media=\"all\" />\n</head>\n<body><div class=\"cynny\">\n\t\t<img src=\"https://errors.tcynny.com:1443/images/cynny.png\"/>"
		"</div>\n\t<!-----start-wrap--------->\n\t<div class=\"wrap\">\n\t\t<!-----start-content--------->\n\t\t<div class=\"content\">\n"
		"\t\t\t<!-----start-logo--------->\n\t\t\t<div class=\"logo\">\n\t\t\t\t<h1><img src=\"https://errors.tcynny.com:1443/images/403.png\"/>"
		"</h1>\n\t\t\t<span>Forbidden</span>\t\t\t</div>\n\t\t\t<!-----end-logo--------->\n\t\t\t<!-----"
		"start-search-bar-section--------->\n\t\t\t<!-----end-sear-bar--------->\n\t\t</div>\n</div>\n</body>\n</html>\n";

const std::string method_not_allowed_html = "<!DOCTYPE HTML>\n<html>\n<head>\n<title>405</title>\n<meta http-equiv=\"Content-Type\" "
		"content=\"text/html; charset=utf-8\" />\n<meta name=\"viewport\" content=\"width=device-width, initial-scale=1, maximum-scale=1\">\n"
		"<link href=\"https://errors.tcynny.com:1443/css/style.css\" rel=\"stylesheet\" type=\"text/css\" media=\"all\" />\n</head>\n<body><div "
		"class=\"cynny\">\n\t\t<img src=\"https://errors.tcynny.com:1443/images/cynny.png\"/></div>\n\t<!-----start-wrap--------->\n\t"
		"<div class=\"wrap\">\n\t\t<!-----start-content--------->\n\t\t<div class=\"content\">\n\t\t\t<!-----start-logo--------->\n\t\t\t<div "
		"class=\"logo\">\n\t\t\t\t<h1><img src=\"https://errors.tcynny.com:1443/images/405.png\"/></h1>\n\t\t\t<span>Method not Allowed</span>\t\t\t"
		"</div>\n\t\t\t<!-----end-logo--------->\n\t\t\t<!-----start-search-bar-section--------->\n\t\t\t<!-----end-sear-bar--------->\n\t\t</div>"
		"\n\t\t<!----copy-right-------------->\t</div>\n\t\n\t<!---------end-wrap---------->\n</body>\n</html>\n";

const std::string request_timeout_html = "<!DOCTYPE HTML>\n<html>\n<head>\n<title>408</title>\n<meta http-equiv=\"Content-Type\" "
		"content=\"text/html; charset=utf-8\" />\n<meta name=\"viewport\" content=\"width=device-width, initial-scale=1, maximum-scale=1\">"
		"\n<link href=\"https://errors.tcynny.com:1443/css/style.css\" rel=\"stylesheet\" type=\"text/css\" media=\"all\" />\n</head>\n<body><div "
		"class=\"cynny\">\n\t\t<img src=\"https://errors.tcynny.com:1443/images/cynny.png\"/></div>\n\t<div class=\"wrap\">\n\t\t<div class=\"content\">"
		"\n\t\t\t<div class=\"logo\">\n\t\t\t\t<h1><img src=\"https://errors.tcynny.com:1443/images/408.png\"/></h1>\n\t\t\t<span>Client Timeout</span>"
		"\t\t\t</div>\n\n\t\t</div>\n</div>\n</body>\n</html>\n";

const std::string internal_server_error_html = "<!DOCTYPE HTML>\n<html>\n<head>\n<title>500</title>\n<meta http-equiv=\"Content-Type\" "
		"content=\"text/html; charset=utf-8\" />\n<meta name=\"viewport\" content=\"width=device-width, initial-scale=1, maximum-scale=1\">\n"
		"<link href=\"https://errors.tcynny.com:1443/css/style.css\" rel=\"stylesheet\" type=\"text/css\" media=\"all\" />\n</head>\n<body><div "
		"class=\"cynny\">\n\t\t<img src=\"https://errors.tcynny.com:1443/images/cynny.png\"/></div>\n\t<!-----start-wrap--------->\n\t<div class=\"wrap\">"
		"\n\t\t<!-----start-content--------->\n\t\t<div class=\"content\">\n\t\t\t<!-----start-logo--------->\n\t\t\t<div class=\"logo\">"
		"\n\t\t\t\t<h1><img src=\"https://errors.tcynny.com:1443/images/500.png\"/></h1>\n\t\t\t<span>Server Error</span>\t\t\t</div>\n\t\t\t<!-----"
		"end-logo--------->\n\t\t\t<!-----start-search-bar-section--------->\n\t\t\t<!-----end-sear-bar--------->\n\t\t</div>\n\t\t<!----"
		"copy-right-------------->\t</div>\n\t\n\t<!---------end-wrap---------->\n</body>\n</html>\n";

const std::string bad_gateway_html = "<!DOCTYPE HTML>\n<html>\n<head>\n<title>502</title>\n<meta http-equiv=\"Content-Type\" "
		"content=\"text/html; charset=utf-8\" />\n<meta name=\"viewport\" content=\"width=device-width, initial-scale=1, maximum-scale=1\">"
		"\n<link href=\"https://errors.tcynny.com:1443/css/style.css\" rel=\"stylesheet\" type=\"text/css\" media=\"all\" />\n</head>\n<body><div "
		"class=\"cynny\">\n\t\t<img src=\"https://errors.tcynny.com:1443/images/cynny.png\"/></div>\n\t<!-----start-wrap--------->\n\t<div class=\"wrap\">"
		"\n\t\t<!-----start-content--------->\n\t\t<div class=\"content\">\n\t\t\t<!-----start-logo--------->\n\t\t\t<div class=\"logo\">"
		"\n\t\t\t\t<h1><img src=\"https://errors.tcynny.com:1443/images/502.png\"/></h1>\n\t\t\t<span>Board Error</span>\t\t\t</div>\n\t\t</div>"
		"\n\t\t<!----copy-right-------------->\t</div>\n\t\n\t<!---------end-wrap---------->\n</body>\n</html>\n";

const std::string service_unavailable_html = "<!DOCTYPE HTML>\n<html>\n<head>\n<title>503</title>\n<meta http-equiv=\"Content-Type\" "
		"content=\"text/html; charset=utf-8\" />\n<meta name=\"viewport\" content=\"width=device-width, initial-scale=1, maximum-scale=1\">"
		"\n<link href=\"https://errors.tcynny.com:1443/css/style.css\" rel=\"stylesheet\" type=\"text/css\" media=\"all\" />\n</head>\n<body><div "
		"class=\"cynny\">\n    <img src=\"https://errors.tcynny.com:1443/images/cynny.png\"/></div>\n\t<div class=\"wrap\">\n\t\t<div class=\"content\">"
		"\n\t\t\t<div class=\"logo\">\n\t\t\t\t<h1><img src=\"https://errors.tcynny.com:1443/images/503.png\"/></h1>\n\t\t\t<span>Not Available</span>"
		"\t\t\t</div>\n\t\t</div>\n</div>\n\n</body>\n</html>\n";

const std::string gateway_timeout_html = "<!DOCTYPE HTML>\n<html>\n<head>\n<title>504</title>\n<meta http-equiv=\"Content-Type\" "
		"content=\"text/html; charset=utf-8\" />\n<meta name=\"viewport\" content=\"width=device-width, initial-scale=1, maximum-scale=1\">"
		"\n<link href=\"https://errors.tcynny.com:1443/css/style.css\" rel=\"stylesheet\" type=\"text/css\" media=\"all\" />\n</head>\n<body><div "
		"class=\"cynny\">\n        <img src=\"https://errors.tcynny.com:1443/images/cynny.png\"/></div>\n\t<div class=\"wrap\">\n\t\t<!-----"
		"start-content--------->\n\t\t<div class=\"content\">\n\t\t\t<!-----start-logo--------->\n\t\t\t<div class=\"logo\">\n\t\t\t\t<h1>"
		"<img src=\"https://errors.tcynny.com:1443/images/504.png\"/></h1>\n\t\t\t<span>Board Timeout</span>\t\t\t</div>\n\t\t</div>\n\t</div>\n</body>\n</html>\n";


std::string request_generator(const http_method method, const std::string& content, bool chunked /* = false */ )
{
	//HEADERs
	std::string req(http_method_str(method));
	req.append(" /path HTTP/1.1")
		.append(http::crlf)
		.append("connection: close")
		.append(http::crlf);

	if(!chunked)
		req.append(http::hf_content_len)
			.append(": ")
			.append(std::to_string(content.size()))
			.append(http::crlf);

	req.append("date: Wed,  6 Jul 2016 11:07:20 CEST")
		.append(http::crlf)
		.append("host: localhost:2000")
		.append(http::crlf)
		.append("test: test_1, test_2")
		.append(http::crlf);

	if(chunked)
		req.append(http::hf_transfer_encoding)
		.append(": ")
		.append(http::hv_chunked)
		.append(http::crlf);

	req.append("x-cyn-date: 1470328824")
		.append(http::crlf);

	req.append(http::crlf);
	if(content.size()!= 0)
		req.append(content);

	return req;
}
