#ifndef HTTP_CODEC_H
#define HTTP_CODEC_H

#include <memory>
#include <cassert>
#include <functional>

struct http_parser;

namespace http
{

class http_structured_data;

class http_codec final
{
public:
	enum class direction_t : uint8_t
	{
		BOTH = 0,
		REQUEST = 1,
		RESPONSE = 2
	};

	enum class encoder_state : uint8_t
	{
		ZERO,
		HEADER,
		BODY,
		TRAILER,
	};

	using structured_cb = std::function<void(http::http_structured_data**)>;
	using stream_cb = std::function<void(std::string)>;
	using trailer_cb = std::function<void(std::string, std::string)>;
	using void_cb = std::function<void(void)>;
	using error_cb = std::function<void(int, bool&)>;

	http_codec();
	~http_codec();

	template<typename T>
	std::string encode_header(const T& msg)
	{
		assert(_encoder_state == encoder_state::ZERO);
		_encoder_state = encoder_state::HEADER;
		_chunked = msg.chunked();
		return msg.serialize();
	}

	std::string encode_body(const std::string& data);
	std::string encode_trailer(const std::string& key, const std::string& data);
	std::string encode_eom();

	//Caller need to register its own cb to get notified
	void register_callback(const structured_cb& begin, const void_cb& header, const stream_cb& body,
		const trailer_cb& trailer, const void_cb& completion, const error_cb& error ) noexcept;

	//Callback to handle new data from lower level
	bool decode(const char* data, size_t len) noexcept;
	void ingnore_content_len() noexcept { _ignore_content_len = true; }

private:
	static int on_url(http_parser*, const char *at, size_t length);
	static int on_status(http_parser*, const char *at, size_t length);
	static int on_header_field(http_parser*, const char *at, size_t length);
	static int on_header_value(http_parser*, const char *at, size_t length);
	static int on_body(http_parser*, const char *at, size_t length);
	static int on_message_begin(http_parser*);
	static int on_headers_complete(http_parser*);
	static int on_chunk_header(http_parser*);
	static int on_chunk_complete( http_parser* );
	static int on_message_complete( http_parser* );

	bool _chunked{false};

	//Add some fault tollerance
	bool _skip_next_header{false};
	bool _ignore_content_len{false};

	encoder_state _encoder_state{encoder_state::ZERO};

	/**
	 * @note Static callbacks lands here in the end.
	 */
	class impl;
	std::unique_ptr<impl> codec_impl;
};

}
#endif // HTTP_CODEC_H
