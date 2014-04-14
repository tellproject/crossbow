#pragma once

#include <functional>

namespace crossbow
{

namespace impl{

struct thread_impl;

} // namespace impl

namespace this_thread
{

void yield();

} // namespace this_thread

class thread
{
    impl::thread_impl* impl_ = nullptr;
    static impl::thread_impl* create_impl(std::function<void()>);
public:
    thread() {}
    thread(const thread&) = delete;
    thread(thread&& other);
    template<typename Fun>
    explicit thread(Fun&& f)
    {
        impl_ = create_impl(std::forward<Fun>(f));
    }

    void join();
    bool joinable() const;

    ~thread();

    void detach();
};

} // namespace crossbow
