#ifndef DOOR_MAT_HTTP_REQUEST_H
#define DOOR_MAT_HTTP_REQUEST_H

#include "http_commons.h"
#include "http_structured_data.h"

#include <string>
#include <unordered_map>

namespace http
{

/**
 * @note All the special uri setter require an if in the protocol's code that has to make the conversion
 * Be generic means sometimes be faster.
 *
 * Should we make an http2_request ( the const interface should remain the same )?
 */
class http_request : public http_structured_data
{
public:
	http_request( bool ssl_ = false, http_method method = HTTP_GET )
		: http_structured_data(typeid(http_request))
		, _ssl(ssl_)
		, _method(method)
	{
	}

	http_request(const http_request&) = default;
	http_request(http_request&&) = default;
	http_request& operator=(const http_request&) = default;
	http_request& operator=(http_request&&) = default;
	~http_request() = default;

	void method(http_method val) noexcept {_method = val;}
	void method(const std::string&) noexcept;
	http_method method_code() const noexcept {return _method;}
	std::string method() const noexcept;

	void schema(const std::string& val) noexcept { _schema = val; }
	const std::string& schema() const noexcept { return _schema; }

	// NOTE: urihost and hostname are independent in our code;
	// we may want to establish some relation between them

    template<typename DS>
    void urihost(DS&& val) noexcept
    {
		static_assert(std::is_convertible<DS, std::string>::value, "Must be std::string-convertible");
        _urihost = std::forward<DS>(val);
    }
	const std::string& urihost() const noexcept { return _urihost; }

	void port(const std::string& val) noexcept {_port=val;}
	const std::string& port() const noexcept { return _port; }

	void path(const std::string& val) noexcept {_path=val;}
	const std::string& path() const noexcept { return _path; }

	void query(const std::string& val) noexcept {_query=val;}
	const std::string& query() const noexcept { return _query; }

	void fragment(const std::string& val) noexcept {_fragment=val;}
	const std::string& fragment() const noexcept { return _fragment; }

	void userinfo(const std::string& val) noexcept {_userinfo=val;}
	const std::string& userinfo() const noexcept { return _userinfo; }

	void ssl(bool v) noexcept { _ssl = v;}
	bool ssl() const noexcept { return _ssl;}

	std::string serialize() const noexcept;

	bool operator==(const http_request&req) const;
	bool operator!=(const http_request&req) const;

    void setParameter(const std::string& param_name, const std::string& param_value);
    void removeParameter(const std::string& name);
    bool hasParameter(const std::string &param_name) const;
    const std::string& getParameter(const std::string&param_name) const;
private:
	bool _ssl;
	http_method _method;
	std::string _schema;
	std::string _urihost;
	std::string _port;
	std::string _path;
	std::string _query;
	std::string _fragment;
	std::string _userinfo;
	std::unordered_map<std::string, std::string> _params;
};

}

#endif //DOOR_MAT_HTTP_REQUEST_H
