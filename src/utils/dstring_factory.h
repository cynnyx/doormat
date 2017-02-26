#pragma once

#include "dstring.h"

class dstring_factory
{
	const size_t _size;
	const bool _caseins;
	char* _data{nullptr};
public:
	dstring_factory(const size_t, const bool caseins = false);
	~dstring_factory();

	char* data() noexcept;
	size_t max_size() const noexcept;

	dstring create_dstring(const size_t len) noexcept;
	dstring create_dstring() noexcept { return create_dstring(_size); }
};
