#ifndef DOOR_MAT_SNI_SOLVER_H
#define DOOR_MAT_SNI_SOLVER_H


#include <boost/asio/ssl.hpp>
#include <boost/asio/ssl/context.hpp>
#include <list>

namespace ssl_utils
{

int sni_callback(SSL *ssl, int *ad, void *arg);

/** \class sni_solver resolves SNI in TLS (https://en.wikipedia.org/wiki/Server_Name_Indication)
 */
class sni_solver
{
public:

	struct certificate
	{
		std::string server_name;
		boost::asio::ssl::context context;
		X509* x509;
	};

	sni_solver() = default;

	/* Avoid copy, move and whatever*/
	sni_solver(const sni_solver &) = delete;
	sni_solver &operator=(const sni_solver &) = delete;
	sni_solver(sni_solver &&) = delete;
	sni_solver &operator=(sni_solver &&) = delete;
	
	bool load_certificate( const std::string& cert, const std::string& key, const std::string &pass ) noexcept;

	std::list<certificate>::iterator begin() { return certificates_list.begin(); }
	std::list<certificate>::iterator end() { return certificates_list.end(); }
	std::size_t size() const noexcept { return certificates_list.size(); }
	~sni_solver() = default;
private:
	std::list<certificate> certificates_list;
	friend int sni_callback(SSL *ssl, int *ad, void *arg);
};


}

#endif //DOOR_MAT_SNI_SOLVER_H
