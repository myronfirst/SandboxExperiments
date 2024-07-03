#include <cassert>
#include <chrono>
#include <iostream>

namespace {
    constexpr uint64_t Workload = 100'000'000;
    using SteadyClock = std::chrono::steady_clock;
    using Millis = std::chrono::milliseconds;

    auto SteadyClockNow() -> void { SteadyClock::now(); }
    auto GetTimeMillisNow() -> void {
        struct timespec tm;
        const auto r = clock_gettime(CLOCK_MONOTONIC, &tm);
        assert(r != -1);
    }

    using NowFunc = void (*)();
    NowFunc Now = SteadyClockNow;
    auto BenchmarkClock() -> void {
        const auto begin = SteadyClock::now();
        for (auto i = 0u; i < Workload; ++i) {
            Now();
            // ;
        }
        const auto end = SteadyClock::now();
        const auto dur = std::chrono::duration_cast<Millis>(end - begin);
        std::cout << dur.count() << " ms\n";
    }
}    // namespace

auto main() -> int {
    Now = SteadyClockNow;
    BenchmarkClock();
    Now = GetTimeMillisNow;
    BenchmarkClock();
}
