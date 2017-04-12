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
	dstring method() const noexcept;

	void schema(const dstring& val) noexcept { _schema = val; }
	const dstring& schema() const noexcept { return _schema; }

    template<typename DS>
    void urihost(DS&& val) noexcept
    {
        static_assert(std::is_convertible<DS, dstring>::value, "Must be dstring-convertible");
        _urihost = std::forward<DS>(val);
    }
	const dstring& urihost() const noexcept { return _urihost; }

	void port(const dstring& val) noexcept {_port=val;}
	const dstring& port() const noexcept { return _port; }

	void path(const dstring& val) noexcept {_path=val;}
	const dstring& path() const noexcept { return _path; }

	void query(const dstring& val) noexcept {_query=val;}
	const dstring& query() const noexcept { return _query; }

	void fragment(const dstring& val) noexcept {_fragment=val;}
	const dstring& fragment() const noexcept { return _fragment; }

	void userinfo(const dstring& val) noexcept {_userinfo=val;}
	const dstring& userinfo() const noexcept { return _userinfo; }

	void ssl(bool v) noexcept { _ssl = v;}
	bool ssl() const noexcept { return _ssl;}

	dstring serialize() const noexcept;

	bool operator==(const http_request&req) const;
	bool operator!=(const http_request&req) const;

    void setParameter(const std::string& param_name, const std::string& param_value);
    void removeParameter(const std::string& name);
    bool hasParameter(const std::string &param_name);
    const std::string& getParameter(const std::string&param_name);
private:
	bool _ssl;
	http_method _method;
	dstring _schema;
	dstring _urihost;
	dstring _port;
	dstring _path;
	dstring _query;
	dstring _fragment;
	dstring _userinfo;
	std::unordered_map<std::string, std::string> _params;
};

}

#endif //DOOR_MAT_HTTP_REQUEST_H
