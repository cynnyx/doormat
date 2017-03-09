#ifndef DOORMAT_LOOP_H_
#define DOORMAT_LOOP_H_

#include <functional>
#include <memory>
#include <list>

#include "uvw/loop.hpp"
#include "uvw/async.hpp"


class doormat_loop {
    std::shared_ptr<uvw::Loop> l_;
    std::shared_ptr<uvw::AsyncHandle> async_;
    bool alive_{false};
    bool inited_{false};
    std::list<std::function<void()>> cbs_;

    void setListener();
    void init() noexcept; // idempotent
    void closeKeepalive() noexcept; // idempotent

public:
	doormat_loop() noexcept;
	~doormat_loop() noexcept;
	doormat_loop(std::shared_ptr<uvw::Loop>&& l) noexcept;

    template<typename R, typename... Args>
    std::shared_ptr<R> resource(Args&&... args)
    {
        return l_->resource<R>(std::forward<Args>(args)...);
    }

    void post(std::function<void()> cb);
    void run() noexcept;
    void keepAlive(bool alive) noexcept;
    operator uvw::Loop&() noexcept { return *l_; }
};


#endif // DOORMAT_LOOP_H_
