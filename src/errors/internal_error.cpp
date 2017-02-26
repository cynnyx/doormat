#include "internal_error.h"
#include "../utils/utils.h"

namespace errors
{

internal_error::operator std::string() const
{
	std::string r{ source_ };
	r += " - ";
	r += utils::from( code_ );
	if ( file_ != nullptr )
	{
		r += " ";
		r += file_;
	}
	if ( line_ != 0 )
	{
		r += " ";
		r += utils::from( line_ );
	}
	return r;
}

internal_error& internal_error::operator=( const internal_error& e ) noexcept
{
	code_ = e.code_;
	source_ = e.source_;
	file_ = e.file_;
	line_ = e.line_;
	return *this;
}


internal_error& internal_error::operator=( internal_error&& e ) noexcept
{
	code_ = e.code_;
	source_ = e.source_;
	file_ = e.file_;
	line_ = e.line_;
	return *this;
}

internal_error::operator bool() const noexcept
{
	return code_ > 0;
}

}
