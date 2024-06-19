#include <barrier>
#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>
#include <limits>
#include <mutex>
#include <string_view>
#include <thread>
#include <vector>

namespace {
#define CALL_POSIX(func, ...)                                         \
    {                                                                 \
        int r_ = func(__VA_ARGS__);                                   \
        if (r_ != 0) {                                                \
            std::cout << #func " error: " << strerror(errno) << '\n'; \
            assert(false);                                            \
            std::abort();                                             \
        }                                                             \
    }

    using Clock = std::chrono::steady_clock;
    using Timepoint = std::chrono::time_point<Clock>;

    auto PinToCore(int core) -> void {
        cpu_set_t cpu_mask;
        CPU_ZERO(&cpu_mask);
        CPU_SET(core, &cpu_mask);
        CALL_POSIX(pthread_setaffinity_np, pthread_self(), sizeof(cpu_mask), &cpu_mask);
    }

    constexpr size_t TotalThreads = 8;
    Timepoint begin{};
    Timepoint end{};
    std::once_flag beginFlag{};
    std::once_flag endFlag{};
    std::barrier beginBarrier{ TotalThreads };
    std::barrier endBarrier{ TotalThreads };

    auto Workload([[maybe_unused]] size_t id) -> void {
        for (uint32_t i = 0u; i < std::numeric_limits<uint32_t>::max(); ++i)
            ;
    }
    auto ExecuteOneWriter(size_t id) -> void {
        beginBarrier.arrive_and_wait();
        if (id == 0)
            begin = Clock::now();

        Workload(id);
        beginBarrier.arrive_and_wait();
        if (id == 0)
            end = Clock::now();
    }
    auto ExecuteCallOnceWriter(size_t id) -> void {
        beginBarrier.arrive_and_wait();
        std::call_once(beginFlag, []() { begin = Clock::now(); });
        Workload(id);
        beginBarrier.arrive_and_wait();
        std::call_once(endFlag, []() { end = Clock::now(); });
    }
    std::barrier beginBarrierFunc{ TotalThreads, []() { begin = Clock::now(); } };
    std::barrier endBarrierFunc{ TotalThreads, []() { end = Clock::now(); } };
    auto ExecuteBarrierWriter(size_t id) -> void {
        beginBarrierFunc.arrive_and_wait();
        Workload(id);
        endBarrierFunc.arrive_and_wait();
    }
    auto ExecuteMultiWriter(size_t id) -> void {
        beginBarrier.arrive_and_wait();
        begin = Clock::now();
        Workload(id);
        beginBarrier.arrive_and_wait();
        end = Clock::now();
    }
    auto WrapCallback(size_t id, auto cb) -> void {
        PinToCore(id);
        cb(id);
    }

    auto SpawnAndJoinThreads(auto cb, std::string_view name) -> void {
        const auto totalBegin = Clock::now();
        {
            std::vector<std::jthread> threads{ TotalThreads };
            for (size_t i = 0u; i < threads.size(); ++i) threads.at(i) = std::jthread([i, &cb]() { WrapCallback(i, cb); });
        }
        const auto totalEnd = Clock::now();

        std::cout << name << " Duration " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << " ms\n";
        std::cout << name << " Total " << std::chrono::duration_cast<std::chrono::milliseconds>(totalEnd - totalBegin).count() << " ms\n";
    }

}    // namespace
auto main() -> int {
    CALL_POSIX(pthread_setconcurrency, std::thread::hardware_concurrency());
    std::cout << "Number of Logical Threads: " << pthread_getconcurrency() << '\n';
    SpawnAndJoinThreads(ExecuteOneWriter, "ExecuteOneWriter");
    SpawnAndJoinThreads(ExecuteCallOnceWriter, "ExecuteCallOnceWriter");
    SpawnAndJoinThreads(ExecuteBarrierWriter, "ExecuteBarrierWriter");
    SpawnAndJoinThreads(ExecuteMultiWriter, "ExecuteMultiWriter");
    return 0;
}
