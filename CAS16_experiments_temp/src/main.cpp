#include <array>
#include <atomic>
#include <barrier>
#include <cassert>
#include <chrono>
#include <iostream>
#include <numeric>
#include <thread>

namespace {
    auto consteval Pow(std::size_t base, std::size_t exp) -> std::size_t { return exp == 0 ? 1 : base * Pow(base, exp - 1); }
    auto consteval Sum(auto&& container) { return std::accumulate(std::begin(container), std::end(container), 0U); }

    // #define CAS2_BUILTIN
    // #define CAS2_KALLIM
#define CAS2_LIBATOMIC
    constexpr std::memory_order Order = std::memory_order_relaxed;
    constexpr std::size_t N_THREADS = 24;
    constexpr std::size_t LOG2N_OPS = 26;
    constexpr std::size_t NOps = Pow(2, LOG2N_OPS);
    constexpr std::size_t ThreadOps = NOps / N_THREADS;

    // constexpr std::size_t LOG2N_OPS = 16;

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

    inline auto CAS128(uint64_t* A, uint64_t B0, uint64_t B1, uint64_t C0, uint64_t C1) -> bool {
        bool res;
#if defined(__OLD_GCC_X86__) || defined(__amd64__) || defined(__x86_64__)
        std::uint64_t dummy;
        asm volatile(
            "lock;"
            "cmpxchg16b %2; setz %1"
            : "=d"(dummy), "=a"(res), "+m"(*A)
            : "b"(C0), "c"(C1), "a"(B0), "d"(B1));
#else
        assert(false);
        __uint128_t old_value = (__uint128_t)(B0) | (((__uint128_t)(B1)) << 64ULL);
        __uint128_t new_value = (__uint128_t)(C0) | (((__uint128_t)(C1)) << 64ULL);
        res = __atomic_compare_exchange_16(A, &old_value, new_value, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#endif
        return res;
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
    auto AtomicCompareExchangeStrongExplicit(std::atomic<T>* obj,
                                             typename std::atomic<T>::value_type* expected,
                                             typename std::atomic<T>::value_type desired,
                                             [[maybe_unused]] std::memory_order success,
                                             [[maybe_unused]] std::memory_order failure) noexcept -> bool {
#ifdef CAS2_BUILTIN
        const __uint128_t ret = __sync_val_compare_and_swap(
            reinterpret_cast<__uint128_t*>(obj),
            *reinterpret_cast<__uint128_t*>(expected),
            *reinterpret_cast<__uint128_t*>(std::addressof(desired)));
        auto prev = *reinterpret_cast<const std::atomic<T>::value_type*>(std::addressof(ret));
        if (prev == *expected) return true;
        *expected = prev;
        return false;
#endif
#ifdef CAS2_KALLIM
        bool ok = CAS128(reinterpret_cast<std::uint64_t*>(obj), expected->val[0], expected->val[1], desired.val[0], desired.val[1]);
        if (!ok) *expected = obj->load(failure);
        return ok;
#endif
#ifdef CAS2_LIBATOMIC
        return obj->compare_exchange_strong(*expected, desired, success, failure);
#endif
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
    std::cout << "is_always_lock_free: " << SharedCounter.is_always_lock_free << "\n";
    std::cout << "is_lock_free: " << SharedCounter.is_lock_free() << "\n";

    Benchmark();
    std::cout << "Expected: " << ThreadOps * N_THREADS << "\n";
    std::cout << "Desired: " << SharedCounter.load().Get() << "\n";
    assert(SharedCounter.load().Get() == ThreadOps * N_THREADS);
}
