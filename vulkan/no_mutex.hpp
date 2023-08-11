#pragma once


#include <concepts>
#include <mutex>


template <typename T>
concept Mutex_c = requires (T t) {
    { t.lock() };
    { t.try_lock() } -> std::convertible_to<bool>;
    { t.unlock() };
};


struct NoMutex {
    void lock() noexcept {}
    bool try_lock() noexcept { return true; }
    void unlock() noexcept {}
};
