#ifndef DOORMAT_DATE_SETTER_H_
#define DOORMAT_DATE_SETTER_H_

#include "../chain_of_responsibility/node_interface.h"
#include "../http/http_structured_data.h"
#include "../log/format.h"

#include <array>
#include <chrono>

namespace nodes
{

/**
 * @brief The date_setter class is a node that intercept
 * the response _header_ up to the chain, check whether
 * the date header has been set and sets it if it wasn't.
 */
class date_setter : public node_interface
{
public:
	using node_interface::node_interface;

	void on_header(http::http_response&& preamble)
	{
		if(preamble.date().empty())
		{
			const auto& date = logging::format::http_header_date(std::chrono::system_clock::now());
			preamble.date(dstring{date.data(), date.size()});
		}
		base::on_header(std::move(preamble));
	}
private:
	std::array<uint8_t,29> buf;
};

}


#endif // DOORMAT_DATE_SETTER_H_
