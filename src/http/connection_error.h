#ifndef DOORMAT_CONNECTION_ERROR_H
#define DOORMAT_CONNECTION_ERROR_H

namespace http {

enum class error_code : int
{   success = 0,
	decoding = 1,
    closed_by_client = 2
};


class connection_error {
public:
    connection_error(error_code ec) : ec{ec} {}
    error_code errc() const noexcept { return ec; }
    std::string message() const noexcept { return std::string{"there was an error"};}
    virtual ~connection_error() = default;
protected:
    error_code ec;
};


}

#endif //DOORMAT_CONNECTION_ERROR_H
