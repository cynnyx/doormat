#ifndef DOOR_MAT_SNI_SOLVER_H
#define DOOR_MAT_SNI_SOLVER_H


#include <boost/asio/ssl.hpp>
#include <boost/asio/ssl/context.hpp>
#include <list>
#include <type_traits>


namespace configuration
{
	class certificates_iterator;
}

namespace ssl_utils
{

int sni_callback(SSL *ssl, int *ad, void *arg);

/** \class sni_solver resolves SNI in TLS (https://en.wikipedia.org/wiki/Server_Name_Indication)
 */
class sni_solver
{
	//friend int sni_solver::sni_callback(SSL *ssl, int *ad, void *arg);
public:

	struct certificate
	{
		certificate(std::string server_name, boost::asio::ssl::context::method method) : server_name{server_name},
			context{method} { }

		std::string server_name;
		boost::asio::ssl::context context;
		X509* x509;
	};

	sni_solver() = default;

	/* Avoid copy, move and whatever*/
	sni_solver(const sni_solver &) = delete;

	sni_solver &operator=(const sni_solver &) = delete;

	sni_solver(sni_solver &&) = delete;

	/** \brief loads all the certificates present in the certification file and then gets the server names later used for SNI
	*/
	bool load_certificates();

	std::list<certificate>::iterator begin() { return certificates_list.begin(); }
	std::list<certificate>::iterator end() { return certificates_list.end(); }
	~sni_solver() = default;

private:

	bool prepare_certificate(configuration::certificates_iterator &current_certificate);

	std::list<certificate> certificates_list;
	std::vector<std::string> supported_protocols;
	std::vector<unsigned char> wire_format_supported_protocols;

	friend int sni_callback(SSL *ssl, int *ad, void *arg);
};


}

#endif //DOOR_MAT_SNI_SOLVER_H
