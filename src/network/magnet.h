#ifndef DOORMAT_MAGNET_H
#define DOORMAT_MAGNET_H

#include <openssl/aes.h>
#include <cstdint>
#include <vector>
#include "../http/http_request.h"
#include "socket_pool.h"
#include "../utils/log_wrapper.h"
#include "../utils/json.hpp"
#include "../service_locator/service_locator.h"
#include <boost/algorithm/hex.hpp>
#include <boost/regex.hpp>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/md5.h>
#include <openssl/sha.h>
#include <fstream>
#include "../configuration/configuration_wrapper.h"


namespace network
{
// @todo Refactoring
// proxy to socket pool. ← No it is not. It has methods with different signature and a dirty interface 
// Lots of stuff that belongs to the private part (or to another module) and not exposed for sake of testing!
class magnet: public socket_factory
{
public:
	/** \brief creates a magnet enabled socket pool
	 * \param pool_size initial number of sockets opened in advance.
	 * */
	magnet(unsigned int pool_size = 2);

	/** Static methods used in the board extraction algorithm */

	/** \brief extracts the download token id from the hex representation
	 *  \param download_token hex representation of the download token
	 *  \return vector of bytes representing the encryption of the download token.
	 * */
	static std::vector<uint8_t> hex_string_to_dec_vector(const std::string &download_token);


	/** \brief deciphers the download token returning a json representation
	 * \param ctext binary representation of the download token to be decrypted
	 * \return decrypted download token (as json string).
	 *
	 * */
	static std::string decipher_download_token(const std::vector<uint8_t> &ctext);


	/** \brief extracts from the string representation of the download token the parent id used to calculate the chunk location
	 * \param str json representation of the download content
	 * \return the parent id contained in the download token.
	 * */
	static std::string decode_descriptor(std::string str);

	/** \brief compute seed from given inputs
	 * \param descriptor
	 * \param offset
	 * \return a 20 byte vector containing seed in decimal format
	 * */
	static std::string compute_seed(const std::string& descriptor, const std::string& offset);

	/** \brief returns required chunk_id
	 *	\param seed the seed where to start from
	 *	\return a 20 byte vector containing required infos.
	 *
	 * */
	static std::vector<uint8_t> compute_chunk_id(const std::string& seed);

	/** \brief returns the index of the board in map containing the required copy.
	 * \param h resource representation
	 * \param copy the requested copy
	 * \return board index.
	 * */
	unsigned int extract_board_index(const std::vector<uint8_t> h, uint8_t copy);

	/** \brief returns the address where to find required the data.
	 * \param req the request representation
	 * \return the address of the board where the data should be if any or an invalid address.
	 * */
	routing::abstract_destination_provider::address get_board(const http::http_request &req, uint copy_id);

	/** \brief analyzes the request and returns a socket accordingly.
	 * \param req the request representation
	 * \param socket_callback callback to be called when the socket is ready or there's an error
	 * */
	void get_socket(const http::http_request &req, socket_pool::socket_callback socket_callback) override;


	/** \brief returns a socket without caring of optimizations
	 * \param socket_callback the callback to return the socket to the caller
	 *
	 * */
	void get_socket(socket_pool::socket_callback socket_callback) override;

	/** \brief returns a socket with a given endpoint.
	 * \param address the endpoint with which the connection is requested
	 * \param socket_callback the callback to give back the socket to the user.
	 * */
	void get_socket(const routing::abstract_destination_provider::address &address, socket_factory::socket_callback socket_callback) override
	{
		return sp->get_socket(address, std::move(socket_callback));
	}

	/** \brief stops the magnet socket pool.
	 * */
	void stop() override;

	/** \brief returns the number of board registered to magnet
	 * */
	size_t map_size() const noexcept { return data_map.size(); }

private:
	static const boost::regex magnetdata_applies;
	static const boost::regex magnetdata_downloadtoken;
	std::unique_ptr<network::socket_pool> sp{nullptr};
	std::vector<routing::abstract_destination_provider::address> data_map;
	bool enabled{false};
};

class magnet_factory : public abstract_factory_of_socket_factory
{
public:
	std::unique_ptr<socket_factory> get_socket_factory( std::size_t size ) const override;
};
}

#endif //DOORMAT_MAGNET_H
