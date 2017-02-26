#ifndef DOOR_MAT_INTERNAL_ERROR_H
#define DOOR_MAT_INTERNAL_ERROR_H

#include <cstdint>
#include <string>

namespace errors
{

// Antipattern: log AND return an error
// Don't do that!
class internal_error final
{
	int32_t code_;
	const char* source_;
	const char* file_;
	int32_t line_;
public:
	internal_error(): code_(0), source_( nullptr), file_(nullptr), line_(0) {}
	internal_error( int32_t code, const char* source ) noexcept:
		code_(code), source_(source), file_(nullptr), line_(0) {}
	internal_error( int32_t code, const char* source, const char* file, int32_t line ) noexcept:
		code_(code), source_(source), file_(file), line_(line) {}

	internal_error( const internal_error& e ) noexcept: code_(e.code_), source_(e.source_), file_(e.file_), line_(e.line_) { }
	internal_error& operator=( const internal_error& e ) noexcept;

	internal_error( internal_error&& e ) noexcept: code_(e.code_), source_(e.source_), file_(e.file_), line_(e.line_) { }
	internal_error& operator=( internal_error&& e ) noexcept;

	bool operator==( const internal_error& e ) noexcept { return code_ == e.code_; }
	bool operator!=( const internal_error& e ) noexcept { return code_ != e.code_; }

	int32_t code() const noexcept { return code_; }
	const char* source() const noexcept { return source_; }

	operator std::string() const;
	operator bool() const noexcept;
};

}

#define INTERNAL_ERROR(code) errors::internal_error((int32_t)code, __PRETTY_FUNCTION__)
#define INTERNAL_ERROR_LONG(code) errors::internal_error ((int32_t)code, __PRETTY_FUNCTION__, __FILE__, __LINE__ )

#endif //DOOR_MAT_INTERNAL_ERROR_H
