#include <ctime>

#include "configurable_header_filter.h"
#include "../configuration/configuration_wrapper.h"
#include "../service_locator/service_locator.h"
#include "../utils/utils.h"

namespace nodes
{

void configurable_header_filter::on_request_preamble(http::http_request && preamble)
{
	const configuration::header& conf_header = service::locator::configuration().header_configuration();

	const configuration::header::variable_set& var = conf_header.variables();

	for ( configuration::header::header_map::const_iterator it = conf_header.begin();
		it != conf_header.end(); ++it )
	{
		dstring head{it->first.data(),it->first.size(),true};
		if ( head != "x-forwarded-for" && preamble.has( head ) )
			continue;

		dstring val{it->second.data(),it->second.size()};
		if ( var.find( val ) != var.end() )
		{
			if ( val == "$timestamp" )
				val = dstring::to_string( std::time(nullptr) );
			else if ( val == "$msec" )
			{
				auto ms = std::chrono::duration_cast<std::chrono::milliseconds>
					(std::chrono::system_clock::now().time_since_epoch()).count();
				val = dstring::to_string( ms );
			}
			else if ( val == "$scheme" )
				val = preamble.ssl() ? "https" : "http";
			else if ( val == "$proxy_add_x_forwarded_for" || val == "$remote_addr" )
			{
				// These header will be managed later by set_destination_header
				// When destination is known!
				preamble.add_destination_header( std::move(head) );
				val = {};
			}
		}
		if ( ! val.empty() )
		{
			storage.emplace_back( std::move( val ) );
			preamble.header( dstring{it->first.data(), it->first.size(), true} , storage.back() );
		}
	}

	for ( auto it = conf_header.begin_kill(); it != conf_header.end_kill(); ++it )
		preamble.remove_header( dstring{it->data(), it->size(), true} );

	node_interface::on_request_preamble( std::move ( preamble ) );
}

}
