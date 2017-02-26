#include "magnet.h"

namespace network
{
	/** Regexes used to identify requests on which magnet should be applied (data) */
	const boost::regex magnet::magnetdata_applies{"^/generic/api/v1/chunk/[[:xdigit:]]{46}/(\\d+)"};
	const boost::regex magnet::magnetdata_downloadtoken{"[?|&]?downloadtoken=([[:xdigit:]]{256})"};

	magnet::magnet(unsigned int pool_size): sp{new network::socket_pool(2)}
	{
		if(service::locator::configuration().magnet_enabled())
		{
			enabled = true;
			std::string datamap = service::locator::configuration().get_magnet_data_map();
			if(datamap.empty())
			{
				enabled = false;
				LOGERROR("Could not enable magnet: data map not provided in configuration.");
				return;
			}
			std::ifstream infile(datamap);
			std::string ip;
			while (infile >> ip)
			{
				ip = ip.substr(0, ip.rfind('/'));
				auto addr = routing::abstract_destination_provider::address(ip, 8080);
				if(addr.is_valid())
					data_map.emplace_back(std::move(addr));
			}

			if(data_map.empty())
			{
				LOGERROR("Could not enable magnet: data map provided is empty.");
				enabled = false;
				return;
			}
			return;
		}
		enabled = false;
	}

	std::vector<uint8_t> magnet::hex_string_to_dec_vector(const std::string &in)
	{
		std::vector<uint8_t> out;
		boost::algorithm::unhex(in.cbegin(), in.cend(), std::back_inserter(out));
		return out;
	}

	std::string magnet::decipher_download_token(const std::vector<uint8_t> &ctext)
	{
		auto cipher = EVP_get_cipherbyname("aes192");
		if(!cipher)
		{
			throw std::runtime_error("Could not get a cypher");
		}
		unsigned char actual_key[EVP_MAX_KEY_LENGTH];
		unsigned char key[] = {'_','0', 'k', '_', 'T', 'h', '1', 's', 'I', 's', '0', 'u','r', 'P', '4', 's', 's', 'w', 'o', 'r', 'd', '_', 'W', 'h', 'a', 't', 'E', 'l', 's', '3', '_'};
		unsigned char iv[EVP_MAX_IV_LENGTH];
		int keylen = EVP_BytesToKey(cipher, EVP_md5(), nullptr, key, sizeof(key), 1, actual_key, iv);

		EVP_CIPHER_CTX ctx;
		EVP_CIPHER_CTX_init(&ctx);

		EVP_CipherInit_ex(&ctx, cipher, nullptr,  reinterpret_cast<unsigned char*>(actual_key),
						  reinterpret_cast<unsigned char*>(iv), 0);
		if (!EVP_CIPHER_CTX_set_key_length(&ctx, keylen)) {
			EVP_CIPHER_CTX_cleanup(&ctx);
			throw std::runtime_error("Could not execute set_key_len correctly");
		}

		std::string rtext;
		rtext.resize(ctext.size(), 0);
		int outlen = 0;
		EVP_CIPHER_CTX_set_padding(&ctx, 0);
		int rc = EVP_DecryptUpdate(&ctx, (unsigned char *) rtext.data(), &outlen, (const unsigned char *) ctext.data(), (int) ctext.size());
		if(rc != 1) throw std::runtime_error("could not decript update");
		int outlen_final = 16;
		rc = EVP_DecryptFinal(&ctx, (unsigned char *) rtext.data() + outlen, &outlen_final);
		if(rc != 1) throw std::runtime_error("could not decrypt final");
		rtext.resize(rtext.size() - (rtext[rtext.size()-1] + 16));

		/* ========== WARNING ======================================================================================== *
		 *  MD5 sum has been disabled as this is not a critical component but just a mere (potential) optimization.
		 *  Hence it seemed silly to waste further CPU time to check for the correctness of the received data in this
		 *  place.
		 *
		 *  If you need for any reason to reintroduce it, you must remove "+16" from the line before the content, and
		 *  de-comment the rows below. Then everything should work.
		 * =========================================================================================================== */

		//std::vector<uint8_t> checksum{rtext.end()-16, rtext.end()};
		//unsigned char calculated_checksum[MD5_DIGEST_LENGTH];
		//rtext.resize(rtext.size()-16);
		//MD5(rtext.data(), rtext.size(), calculated_checksum);
		//if(std::memcmp(calculated_checksum, checksum.data(), 16) != 0)
		//{
		//	throw std::runtime_error("Invalid checksum decrypted.");
		//}
		// rtext.resize(rtext.size()-16);

		return rtext;
	}

	std::string magnet::decode_descriptor(std::string str)
	{
		/** We just use the JSON parser to return the proper parent id. It should be of size 46*/
		std::string retval;
		retval.reserve(50);
		nlohmann::json::parse(str, [&retval](int depth, nlohmann::json::parse_event_t event, nlohmann::json& parsed) mutable
		{
			if (parsed.is_object() && depth == 0)
			{
				retval = parsed.value("descriptor", "");
				if(retval.empty())throw std::runtime_error("Could not find the description field in the passed json.");
			}
			return true;
		});

		return retval;
	}

	std::string magnet::compute_seed(const std::string& descriptor, const std::string& offset)
	{
		assert(descriptor.size() == 46);
		std::string seed = descriptor.substr(6);
		seed += offset;

		std::vector<uint8_t> hash(20, 0);
		SHA1((unsigned char*)seed.data(), seed.size(), hash.data());
		return utils::to_lower_hex(hash);
	}

	std::vector<uint8_t> magnet::compute_chunk_id(const std::string& seed)
	{
		//assert(seed.size() == 40);
		std::vector<uint8_t> out(20,0);
		SHA1((unsigned char*)seed.data(), seed.size(), out.data());
		return out;
	}

	unsigned int magnet::extract_board_index(const std::vector<uint8_t> h, uint8_t copy)
	{
		static const auto step = floor(static_cast<float>(1<<20)/data_map.size());

		//marks whether the copy representation begins at the byte boundary or at half_byte.
		bool odd = copy%2;
		char mask[3];
		uint8_t right_shift;

		if(!odd)
		{
			mask[0] = 0xFF; mask[1] = 0xFF; mask[2] = 0xF0;
			right_shift = 4;
		}
		else
		{
			mask[0] = 0x0F; mask[1] = 0xFF; mask[2] = 0xFF;
			right_shift = 0;
		}
		int representation_begin = (copy * 5)/2; //position of the starting byte.

		uint hash = (((h[representation_begin] & mask[0]) << 16) +
				((h[representation_begin+1] & mask[1]) << 8) +
				(h[representation_begin+2] & mask[2])) >> right_shift;

		return hash/step;
	}

	routing::abstract_destination_provider::address magnet::get_board(const http::http_request &req, uint copy_id)
	{
		if(enabled)
		{
			auto &path = req.path();
			auto &query = req.query();

			if(path.size() && query.size())
			{
				boost::cmatch uri_matcher;
				if(boost::regex_match(path.cbegin(), path.cend(), uri_matcher, magnetdata_applies))
				{
					std::string c_offset(uri_matcher[1]);
					boost::cmatch matcher;
					if(boost::regex_search(query.cbegin(), query.cend(), matcher, magnetdata_downloadtoken))
					{
						try
						{
							auto dt = hex_string_to_dec_vector(matcher[1]);
							auto des = decode_descriptor(decipher_download_token(dt));
							auto seed = compute_seed(des, c_offset);
							auto chunk_id = compute_chunk_id(seed);
							auto b_offset = extract_board_index(chunk_id, copy_id);
							if( b_offset < data_map.size() )
								return data_map[b_offset];
						}
						catch(const boost::algorithm::hex_decode_error& hex_error)
						{
							LOGERROR("Error while parsing hex representation of download token: ", hex_error.what());
						}
						catch (const std::runtime_error& decrypt_error)
						{
							LOGERROR("Error while decrypting the download token ", decrypt_error.what());
						}
						catch (...)
						{
							LOGERROR("Generic erorr while executing magnet.");
						}
					}
				}
			}
		}
		return routing::abstract_destination_provider::address{};
	}

	void magnet::get_socket(const http::http_request &req, socket_factory::socket_callback socket_callback)
	{
		static constexpr unsigned int redundancy{4};

		auto board = get_board(req, rand()%redundancy);
		if(board.is_valid())
			sp->get_socket(board, std::move(socket_callback));
		else
			get_socket(std::move(socket_callback));
	}

	void magnet::get_socket(socket_factory::socket_callback socket_callback)
	{
		sp->get_socket(std::move(socket_callback));
	}

	void magnet::stop()
	{
		sp->stop();
	}
	
		
	std::unique_ptr<socket_factory> magnet_factory::get_socket_factory( std::size_t size ) const
	{
		return std::unique_ptr<socket_factory>{ new socket_pool{ size } };
	}


}
