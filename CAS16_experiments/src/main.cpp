#include <array>
#include <atomic>
#include <barrier>
#include <cassert>
#include <chrono>
#include <iostream>
#include <numeric>
#include <thread>

#ifndef PARAM_N_THREADS
#define PARAM_N_THREADS 24
#endif
#ifndef PARAM_CAS2_IMPL
#define PARAM_CAS2_IMPL LibAtomic
#endif

namespace {
    auto consteval Pow(std::size_t base, std::size_t exp) -> std::size_t { return exp == 0 ? 1 : base * Pow(base, exp - 1); }
    auto consteval Sum(auto&& container) { return std::accumulate(std::begin(container), std::end(container), 0U); }

    enum class CAS2Impl {
        SynchBuiltin,
        AssemblySynch,
        LibAtomic,
    };
    constexpr CAS2Impl CAS2_IMPL = CAS2Impl::PARAM_CAS2_IMPL;
    constexpr std::memory_order Order = std::memory_order_relaxed;
    constexpr std::size_t N_THREADS = PARAM_N_THREADS;
    constexpr std::size_t LOG2N_OPS = 25;
    constexpr std::size_t NOps = Pow(2, LOG2N_OPS);
    constexpr std::size_t ThreadOps = NOps / N_THREADS;

#ifdef __cpp_lib_hardware_interference_size
    constexpr std::size_t CACHE_LINE_SIZE = std::hardware_constructive_interference_size;
#else
    constexpr std::size_t CACHE_LINE_SIZE = 64;
#endif

    auto PinThisThreadToCore(std::size_t core) -> void {
        int err{};
        err = pthread_setconcurrency(static_cast<int>(std::thread::hardware_concurrency()));
        if (err != 0) throw std::runtime_error{ "pthread_setconcurrency" };
        cpu_set_t cpu_mask;
        CPU_ZERO(&cpu_mask);
        CPU_SET(core, &cpu_mask);
        err = pthread_setaffinity_np(pthread_self(), sizeof(cpu_mask), &cpu_mask);
        if (err != 0) throw std::runtime_error{ "pthread_setaffinity_np" };
    }

    template<class T>
    inline auto CAS2SyncBuiltin(std::atomic<T>* obj,
                                typename std::atomic<T>::value_type* expected,
                                typename std::atomic<T>::value_type desired,
                                [[maybe_unused]] std::memory_order success,
                                [[maybe_unused]] std::memory_order failure) noexcept -> bool {
        const bool ok = __sync_bool_compare_and_swap(
            reinterpret_cast<__uint128_t*>(obj),
            *reinterpret_cast<__uint128_t*>(expected),
            *reinterpret_cast<__uint128_t*>(std::addressof(desired)));
        if (!ok) *expected = obj->load(failure);
        return ok;
    }
    inline auto CAS128(uint64_t* A, uint64_t B0, uint64_t B1, uint64_t C0, uint64_t C1) -> bool {
#if defined(__OLD_GCC_X86__) || defined(__amd64__) || defined(__x86_64__)
        bool res;
        std::uint64_t dummy;
        asm volatile(
            "lock;"
            "cmpxchg16b %2; setz %1"
            : "=d"(dummy), "=a"(res), "+m"(*A)
            : "b"(C0), "c"(C1), "a"(B0), "d"(B1));
        return res;
#else
        __builtin_trap();
#endif
    }
    template<class T>
    inline auto CAS2AssemblySynch(std::atomic<T>* obj,
                                  typename std::atomic<T>::value_type* expected,
                                  typename std::atomic<T>::value_type desired,
                                  [[maybe_unused]] std::memory_order success,
                                  [[maybe_unused]] std::memory_order failure) noexcept -> bool {
        const bool ok = CAS128(reinterpret_cast<std::uint64_t*>(obj), expected->val[0], expected->val[1], desired.val[0], desired.val[1]);
        if (!ok) *expected = obj->load(failure);
        return ok;
    }
    template<class T>
    inline auto CAS2LibAtomic(std::atomic<T>* obj,
                              typename std::atomic<T>::value_type* expected,
                              typename std::atomic<T>::value_type desired,
                              [[maybe_unused]] std::memory_order success,
                              [[maybe_unused]] std::memory_order failure) noexcept -> bool {
        return obj->compare_exchange_strong(*expected, desired, success, failure);
    }

    struct Type {
        Type(std::uint64_t v) : val{ v } {}    // second element default-initialized to zero
        std::array<uint64_t, 2> val;
        auto Get() const { return val[0]; }
        auto operator+=(const Type& rhs) -> Type& {
            val[0] += rhs.val[0];
            return *this;
        }
        auto friend operator+(Type lhs, const Type& rhs) -> Type {
            lhs += rhs;
            return lhs;
        }
        auto operator<=>(const Type&) const = default;
    };
    std::atomic<Type> SharedCounter{ 0 };

    template<class T>
    inline auto AtomicCompareExchangeStrongExplicit(std::atomic<T>* obj,
                                                    typename std::atomic<T>::value_type* expected,
                                                    typename std::atomic<T>::value_type desired,
                                                    [[maybe_unused]] std::memory_order success,
                                                    [[maybe_unused]] std::memory_order failure) noexcept -> bool {
        if constexpr (CAS2_IMPL == CAS2Impl::SynchBuiltin)
            return CAS2SyncBuiltin(obj, expected, desired, success, failure);
        else if constexpr (CAS2_IMPL == CAS2Impl::AssemblySynch)
            return CAS2AssemblySynch(obj, expected, desired, success, failure);
        else if constexpr (CAS2_IMPL == CAS2Impl::LibAtomic)
            return CAS2LibAtomic(obj, expected, desired, success, failure);
        else
            __builtin_trap();
    }

    auto Benchmark() -> void {
        using Clock = std::chrono::steady_clock;
        using Unit = std::chrono::milliseconds;
        using TimePoint = Clock::time_point;
        std::array<std::thread, N_THREADS> threads{};
        TimePoint begin{};
        TimePoint end{};
        std::barrier clockBarrier{ N_THREADS };

        for (auto tid = 0UL; tid < threads.size(); ++tid) {
            threads.at(tid) = std::thread([tid, &begin, &end, &clockBarrier]() {
                PinThisThreadToCore(tid % N_THREADS);
                clockBarrier.arrive_and_wait();
                Type expected{ 0 };
                if (tid == 0) begin = Clock::now();
                auto i = 0ULL;
                while (i < ThreadOps) {
                    Type desired = expected + Type{ 1 };
                    if (AtomicCompareExchangeStrongExplicit(&SharedCounter, &expected, desired, Order, Order)) {
                        expected += Type{ 1 };
                        i += 1;
                    }
                }
                clockBarrier.arrive_and_wait();
                if (tid == 0) end = Clock::now();
            });
        }
        for (auto& t : threads) t.join();
        std::cout << std::chrono::duration_cast<Unit>(end - begin).count() << "\n"
                  << NOps << "\n";
    }
}    // namespace

auto main() -> int {
    Benchmark();
    assert(SharedCounter.load().Get() == ThreadOps * N_THREADS);
}
