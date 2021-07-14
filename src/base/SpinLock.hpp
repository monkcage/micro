#ifndef EASY_BASE_SPINLOCK_INC
#define EASY_BASE_SPINLOCK_INC

#include <atomic>
#include <thread>


namespace easy {


class SpinLock 
{
    static const uint16_t SIZE_CACHELINE = 64;
    static const uint16_t SIZE_CACHE_PADDING = SIZE_CACHELINE - sizeof(uint64_t);
public:
    SpinLock()
        : flag_(1)
    {
    }

    void lock()
    {
        while(std::atomic_exchange_explicit(&flag_, uint64_t(0), std::memory_order_acquire)) {
            std::this_thread::yield();
        }
    }

    void unlock()
    {
        std::atomic_store_explicit(&flag_, uint64_t(1), std::memory_order_release);
    }

    bool try_lock()
    {
        return !std::atomic_exchange_explicit(&flag_, uint64_t(0), std::memory_order_acquire);
    }

private:
    char padding0[SIZE_CACHE_PADDING];
    std::atomic<uint64_t> flag_;
    char padding1[SIZE_CACHE_PADDING];
};



} // namespace easy



#endif