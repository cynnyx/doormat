#ifndef DOORMAT_CONNECTION_H
#define DOORMAT_CONNECTION_H

#include <functional>
#include <tuple>
#include <experimental/optional>
#include <chrono>
#include <memory>
#include <iostream>

#include "connection_error.h"

namespace http 
{


struct connection : std::enable_shared_from_this<connection> 
{
	using error_callback = std::function<void(std::shared_ptr<connection>, const http::connection_error &)>;
	using timeout_callback = std::function<void(std::shared_ptr<connection>)>;

	/** Register callbacks */
	void on_error(error_callback);
	void on_timeout(std::chrono::milliseconds ms, timeout_callback);
	template<typename T>
	void on_timeout(T ms, timeout_callback tcb)
	{
		on_timeout(std::chrono::milliseconds{ms}, std::move(tcb));
	}

	/** Utilities provided to the user to manipulate the connection directly */
	virtual void set_persistent(bool persistent = true) {this->persistent = persistent; }
    virtual void close() = 0;

	virtual ~connection() = default;
protected:
	bool persistent{true};
	virtual void set_timeout(std::chrono::milliseconds)=0;
	void error(http::connection_error);
	void timeout() { if(timeout_cb) (*timeout_cb)(this->shared_from_this()); }
	inline void init(){ myself = this->shared_from_this(); }
	inline void deinit() { myself = nullptr; }
	virtual void cleared() {}
private:
	http::connection_error current{error_code::success};
	std::experimental::optional<timeout_callback> timeout_cb;
	std::experimental::optional<error_callback> error_cb;
    std::shared_ptr<connection> myself{nullptr};
};

}


#endif //DOORMAT_CONNECTION_H
