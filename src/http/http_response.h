#ifndef DOOR_MAT_HTTP_RESPONSE_H
#define DOOR_MAT_HTTP_RESPONSE_H

#include "http_structured_data.h"

namespace http
{

class http_response : public http_structured_data
{
	uint16_t _status_code;
	dstring _status_message;
public:
	http_response(): http_structured_data(typeid(http_response)){}
	http_response(const http_response&) = default;
	http_response(http_response&&) = default;
	http_response& operator=(const http_response&) = default;
	http_response& operator=(http_response&&) = default;
	~http_response() = default;

	bool operator==(const http_response&res);
	bool operator!=(const http_response&res);

	void status(uint16_t val) noexcept;
	void status(uint16_t code, const std::string& msg) noexcept;
	uint16_t status_code() const noexcept{return _status_code;}
	std::string status_message() const noexcept{return _status_message;}
	std::string serialize() const noexcept;
};

}


#endif //DOOR_MAT_HTTP_RESPONSE_H
