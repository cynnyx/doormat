#include "loop.h"

#include <utility>
#include <cassert>


void doormat_loop::setListener()
{
    // the following lambda is non-generic due to a gdb bug
    async_->on<uvw::AsyncEvent>([this](uvw::AsyncEvent&, uvw::AsyncHandle&) {
        decltype(cbs_) currentL;
        std::swap(currentL, cbs_);
        std::for_each(currentL.begin(), currentL.end(), [](auto& f){ f(); });
        if(!alive_)
            this->closeKeepalive();
    });
}

void doormat_loop::init() noexcept
{
    if(!inited_) {
        async_->init();
        inited_ = true;
    }
}

void doormat_loop::closeKeepalive() noexcept
{
    if(!inited_ || !cbs_.empty())
        return;

    async_->close();
    inited_ = false;
}

doormat_loop::doormat_loop() noexcept
    : l_{uvw::Loop::create()}
    , async_{uvw::AsyncHandle::create(l_)}
{
    assert(bool(l_));
    setListener();
}

doormat_loop::~doormat_loop() noexcept
{
    if(inited_)
        async_->close();

    async_->clear();
}

doormat_loop::doormat_loop(std::shared_ptr<uvw::Loop>&& l) noexcept
    : l_{std::move(l)}
    , async_{uvw::AsyncHandle::create(l_)}
{
    assert(bool(l_));
    setListener();
}

void doormat_loop::post(std::function<void ()> cb)
{
    cbs_.emplace_back(std::move(cb));
    init();
    async_->send();
}

void doormat_loop::run() noexcept
{
    l_->run();
}

void doormat_loop::keepAlive(bool alive) noexcept
{
    if(alive)
        init();
    else
        closeKeepalive();
    alive_ = alive;
}
