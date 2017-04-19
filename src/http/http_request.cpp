#include "http_request.h"
#include "http_structured_data.h"
namespace http
{

#define YY(num, name, foo) #name,
	const char* method_mapper[] =
	{
		HTTP_METHOD_MAP(YY)
		"NONE"
	};
#undef YY


dstring http_request::serialize() const noexcept
{
	dstring msg;
	msg.append(method()).append(http::space);

	bool supports_full_paths = protocol_version() == http::proto_version::HTTP20 || protocol_version() == http::proto_version::HTTP11;
	if(_schema && supports_full_paths)
		msg.append(_schema).append("://");

	//TODO: DRM-207 userinfo not yet supported
	//if(_userinfo.valid())
	//	chunks.push_back(_userinfo);

	if(_urihost && supports_full_paths)
		msg.append(_urihost);

	if(_port && supports_full_paths)
		msg.append(http::colon).append(_port);

	if(_path)
		msg.append(_path);

	if(_query)
		msg.append(http::questionmark).append(_query);

	if(_fragment)
		msg.append(http::hash).append(_fragment);

	msg.append(http::space)
		.append(protocol())
		.append(http::crlf)
		.append(http_structured_data::serialize());

	return msg;
}

void http_request::method(const std::string& val) noexcept
{

	for(int first = http_method::HTTP_DELETE; first != http_method::END; ++first)
	{
		if (method_mapper[first] == val)
		{
			_method = static_cast<http_method>(first);
			return;
		}
		_method = http_method::END;
	}
}

dstring http_request::method() const noexcept
{
	return dstring{method_mapper[_method]};
}

bool http_request::operator==(const http_request&req) const
{
	return http_structured_data::operator==(req) && _method == req._method && _urihost == req._urihost
			&& _port == req._port && _path == req._path && _query == req._query
			&& _fragment == req._fragment && _userinfo == req._userinfo;
}

bool http_request::operator!=(const http_request&req) const
{
	return ! ( *this == req );
}

void http_request::setParameter(const std::string& param_name, const std::string& param_value)
{
    _params[param_name] = param_value;
}

void http::http_request::removeParameter(const std::string& name)
{
    _params.erase(name);
}

bool http_request::hasParameter(const std::string &param_name) const
{
    auto p = _params.find(param_name);
    return p != _params.end();
}

const std::string& http_request::getParameter(const std::string &param_name) const
{
    auto p = _params.find(param_name);
    assert(p != _params.end());
    return p->second;
}

}
