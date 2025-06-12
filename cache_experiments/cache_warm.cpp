#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <numeric>
#include <print>
#include <random>
#include <string_view>
#include <thread>
#include <utility>

#include <cassert>
#include <cerrno>
#include <cstdlib>

#include <pthread.h>
#include <sys/wait.h>
#include <unistd.h>

#include <libpmemobj++/make_persistent_array.hpp>
#include <libpmemobj++/p.hpp>
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/pexceptions.hpp>
#include <libpmemobj++/pool.hpp>
#include <libpmemobj++/transaction.hpp>

#include "Instrumenter.h"

// #define WBINVD_ENABLED

/* ---Config--- */
constexpr auto TRACE_FILE_NAME = "./Traces";
constexpr std::size_t CACHE_LINE_SIZE = 64;
// constexpr std::size_t L1 = (1024) * 48;                   // 48KB titan L1 Data
// constexpr std::size_t L2 = (1024 * 1024 * 0.125) * 1;     // 1MB titan L2
// constexpr std::size_t L3 = (1024 * 1024 * 0.125) * 36;    // 36MB titan L3
// constexpr std::size_t CACHE_SIZE = (1024 * 1024) * 4.66;             // 37.28MB titan L3+L2+L1D ~37.3MB
// constexpr std::size_t CACHE_SIZE = (1024 * 1024 * 0.125) * 37.28;    // 37.28MB titan L3+L2+L1D ~37.3MB
// constexpr std::size_t PERSISTENT_ARR_LEN = 2 * CACHE_SIZE;
[[maybe_unused]] constexpr std::size_t L1 = 1UL * 1024 * 48;           // 48KB titan L1 Data
[[maybe_unused]] constexpr std::size_t L2 = 1UL * 1024 * 1;            // 1MB titan L2
[[maybe_unused]] constexpr std::size_t L3 = 1UL * 1024 * 1024 * 36;    // 36MB titan L3
constexpr std::size_t PERSISTENT_ARRAY_SIZE = L3;
// Allocate only for L3, not for L1+L2+L3, since whatever exists in L3 also exists on L1 and L2
//  constexpr std::size_t CACHE_SIZE = (1024 * 1024) * 1;       // 8MB mamalakispc L3 8MB
constexpr std::size_t REPETITIONS = 10;

constexpr std::string_view POOL_PATH = "/mnt/pmem0/myrontsa/CacheWarmPool";
constexpr std::size_t POOL_SIZE = 4 * L3;
struct Root {
    constexpr static std::size_t PADDING_SZ = 48;
    std::array<std::byte, PADDING_SZ> padding;             // Root object is offset by the 16 byte header. + 48 to align to the next cache line
    std::array<std::byte, PERSISTENT_ARRAY_SIZE> array;    // Aligned to cache line
};
static_assert(POOL_SIZE >= sizeof(Root));
namespace {
    pmem::obj::pool<Root> Pool{};

    constexpr size_t COOLING_BUFFER_SIZE = 2 * PERSISTENT_ARRAY_SIZE;
    std::array<std::byte, COOLING_BUFFER_SIZE> CoolingBuffer{};
}    // namespace

namespace {
    [[maybe_unused]] auto InitializeArray(Root* root) -> void {
        auto& array = root->array;
        for (auto i = 0UL; i < array.size(); ++i) array.at(i) = std::byte{ static_cast<std::byte>(i) };
        // for (const auto& b : array) std::print("{} ", std::to_integer<std::size_t>(b));
    }
    template<typename Func, typename... Args> constexpr void CallPosix(Func&& func, Args&&... args) {
        const int err = std::forward<Func>(func)(std::forward<Args>(args)...);
        if (err != 0) {
            std::println(stdout, "CallPosix Error");
            std::fflush(stdout);
            std::abort();
        }
    }
    auto PinThisThreadToCore(std::size_t core) -> void {
        assert(core < std::jthread::hardware_concurrency());
        cpu_set_t cpu_mask;
        CPU_ZERO(&cpu_mask);
        CPU_SET(core, &cpu_mask);
        CallPosix(pthread_setaffinity_np, pthread_self(), sizeof(cpu_mask), &cpu_mask);
    }
    auto System(std::string_view cmd) -> void {
        const std::string cmdStr = std::string{ cmd };
        const int ret = std::system(cmdStr.c_str());
        if (ret != 0) {
            std::println(stdout, "System Error");
            std::fflush(stdout);
            std::abort();
        }
    }
    template<typename T> auto IsAligned(const T* ptr, std::size_t align) -> bool { return (std::bit_cast<std::uintptr_t>(ptr) % align) == 0; }
}    // namespace

namespace {
    auto WarmCachePrepare() -> void {
        const auto& array = Pool.root()->array;
        for (const auto& b : array) [[maybe_unused]]
            volatile auto temp = b;
    }
    auto CoolCachePrepare() -> void {
        for (auto i = 0UL; i < CoolingBuffer.size(); ++i) CoolingBuffer[i] = std::byte{ static_cast<std::byte>(i) };
    }
    auto ComplexCoolCachePrepare() -> void {
        CoolingBuffer.fill(std::byte{ 1 });
        std::random_device r;
        std::default_random_engine e(r());
        std::uniform_int_distribution<std::uint8_t> uniform_dist(std::numeric_limits<std::uint8_t>::min(), std::numeric_limits<std::uint8_t>::max());
        for (auto& b : CoolingBuffer) b = std::byte{ uniform_dist(e) };
        /*
            auto coolingBufferIndexes = std::make_unique<std::array<size_t, COOLING_BUFFER_SIZE>>();
            for (auto begin = std::begin(*coolingBufferIndexes); begin < std::end(*coolingBufferIndexes); begin += COOLING_BUFFER_SIZE)
                std::iota(begin, begin + COOLING_BUFFER_SIZE, 0);
            std::ranges::shuffle(*coolingBufferIndexes, e);
            for (auto i = 0UL; i < coolingBufferIndexes->size(); ++i) {
                const auto idx = coolingBufferIndexes->at(i);
                [[maybe_unused]] volatile auto temp = CoolingBuffer->at(idx);
            }
        */
    }
    auto InvalidateCachePrepare() -> void {
// [[maybe_unused]] int ret = std::system("sudo cat /proc/wbinvd");
// assert(ret >= 0);
#ifdef WBINVD_ENABLED
        System("sudo cat /proc/wbinvd");
        constexpr static auto SLEEP_DUR = std::chrono::milliseconds{ 100 };
        std::this_thread::sleep_for(SLEEP_DUR);
#endif
    }
    auto InvalidateWarmCachePrepare() -> void {
        InvalidateCachePrepare();
        WarmCachePrepare();
    }
}    // namespace

class ExperimentDataType {
private:
    std::string name;
    // size_t repetitions;
    // size_t size;
    std::function<void()> wrapperCallback;
    std::function<void()> prepareCallback;
    std::function<void()> benchmarkCallback;

    [[nodiscard]] auto PickCallback(const std::string& val) const -> std::function<void()> {
        if (val == "NoWrapper") return [this]() { NoWrapper(); };
        if (val == "ThreadWrapper") return [this]() { ThreadWrapper(); };
        if (val == "SystemCallWrapper") return [this]() { SystemCallWrapper(); };
        if (val == "ForkWrapper") return [this]() { ForkWrapper(); };
        if (val == "WarmCachePrepare") return []() { WarmCachePrepare(); };
        if (val == "CoolCachePrepare") return []() { CoolCachePrepare(); };
        if (val == "ComplexCoolCachePrepare") return []() { ComplexCoolCachePrepare(); };
        if (val == "InvalidateCachePrepare") return []() { InvalidateCachePrepare(); };
        if (val == "InvalidateWarmCachePrepare") return []() { InvalidateWarmCachePrepare(); };
        if (val == "ReadBench") return [this]() { ReadBench(); };
        if (val == "WriteBench") return [this]() { WriteBench(); };
        if (val == "") return [](auto&&...) {};
        std::unreachable();
        // assert(false);
        // return [](auto&&...) {};
    }

public:
    ExperimentDataType(std::string_view _name, /* std::size_t _size,  */ const std::string& wrapperCallbackStr, const std::string& prepareCallbackStr,
                       const std::string& benchmarkCallbackStr)
        : name{ _name },
          //   repetitions{ _repetitions },
          //   size{ _size },
          wrapperCallback{ PickCallback(wrapperCallbackStr) },
          prepareCallback{ PickCallback(prepareCallbackStr) },
          benchmarkCallback{ PickCallback(benchmarkCallbackStr) } {}
    auto Execute() const -> void {
        assert(wrapperCallback);
        assert(prepareCallback);
        assert(benchmarkCallback);
        for (auto i = 0UL; i < REPETITIONS; ++i) {
            wrapperCallback();
            std::println("{} - {} - done", i, name);
        }
    }
    auto NoWrapper() const -> void { PrepareAndBenchmark(); }
    auto ThreadWrapper() const -> void {
        std::jthread t([this]() {
            PinThisThreadToCore(1);
            PrepareAndBenchmark();
        });
    }
    auto ForkWrapper() const -> void {
        const pid_t pid = fork();
        assert(pid >= 0);
        if (pid > 0) {
            // parent
            assert(pid > 0);
            [[maybe_unused]] const pid_t cid = wait(nullptr);
            assert(cid >= 0);
            return;
        }
        // child
        assert(pid == 0);
        PrepareAndBenchmark();
        exit(EXIT_SUCCESS);
    }
    auto SystemCallWrapper() const -> void {
        prepareCallback();
        Pool.close();
        // [[maybe_unused]] int ret = std::system("./system_call_main");
        // assert(ret >= 0);
        System("./system_call_main");
        const std::string layout = std::filesystem::path{ POOL_PATH }.filename().string();
        const std::string poolPath{ POOL_PATH };
        Pool = pmem::obj::pool<Root>::open(poolPath, layout);
    }
    auto PrepareAndBenchmark() const -> void {
        prepareCallback();
        benchmarkCallback();
    }

    auto ReadBench() const -> void {
        // for (auto i = 0UL; i < size; ++i) [[maybe_unused]]
        //     volatile auto temp = RootPtr->array[i];
        const auto& array = Pool.root()->array;
        TIME_SCOPE(name);
        for (const auto& b : array) [[maybe_unused]]
            volatile auto temp = b;
    }
    auto WriteBench() const -> void {
        // for (auto i = 0UL; i < size; ++i) RootPtr->array[i] = i;
        auto& array = Pool.root()->array;
        TIME_SCOPE(name);
        for (auto i = 0UL; i < array.size(); ++i) array[i] = std::byte{ static_cast<std::byte>(i) };
        Pool.persist(array.data(), sizeof(array));
    }
};

namespace {
    auto ReadExperiments() -> void {
        {
            // for (const auto& [size, suffix] : std::vector<std::tuple<size_t, std::string>>{ { L1, "L1" }, { L2, "L2" }, { L3, "L3" } })
            ExperimentDataType{ "Main_Read", "NoWrapper", "", "ReadBench" }.Execute();
            // for (const auto& size : { L1, L2, L3 })
            ExperimentDataType{ "Main_Invalidate_Read", "NoWrapper", "InvalidateCachePrepare", "ReadBench" }.Execute();
            // ExperimentDataType{ "Main_Invalidate_Warm_Read", "NoWrapper", "InvalidateWarmCachePrepare", "ReadBench" }.Execute();
        }
        {
            // 1 Rep, restart executable with repeat_execution.sh
            // ExperimentDataType{ "Main_Terminate_Read", 1, "NoWrapper", "", "ReadBench" }.Execute();
            // ExperimentDataType{ "Main_Terminate_Invalidate_Read", 1, "NoWrapper", "InvalidateCachePrepare", "ReadBench" }.Execute();
        }
        {
            // for (const auto& size : { L1, L2, L3 })
            ExperimentDataType{ "Thread_Read", "ThreadWrapper", "", "ReadBench" }.Execute();
            // for (const auto& size : { L1, L2, L3 })
            ExperimentDataType{ "Thread_Invalidate_Read", "ThreadWrapper", "InvalidateCachePrepare", "ReadBench" }.Execute();
            // ExperimentDataType{ "Thread_Invalidate_Warm_Read", "ThreadWrapper", "InvalidateWarmCachePrepare", "ReadBench" }.Execute();

            // ExperimentDataType{ "Thread_Cool_Read", "ThreadWrapper", "CoolCachePrepare", "ReadBench" }.Execute();
            // ExperimentDataType{ "Thread_ComplexCool_Read", "ThreadWrapper", "ComplexCoolCachePrepare", "ReadBench" }.Execute();
        }
        {
            // 1 Rep, restart executable with repeat_execution.sh
            // ExperimentDataType{ "Thread_Terminate_Read", 1, "ThreadWrapper", "", "ReadBench" }.Execute();
            // ExperimentDataType{ "Thread_Terminate_Invalidate_Read", 1, "ThreadWrapper", "InvalidateCachePrepare", "ReadBench" }.Execute();
        }
        {
            // System Call experiment, system_call_main.cpp manages parameters other than        // ExperimentDataType{ "System_Call_Read", "SystemCallWrapper", "", "" }.Execute();
            // ExperimentDataType{ "System_Call_Invalidate_Read", "SystemCallWrapper", "InvalidateCachePrepare", "" }.Execute();
        }
        {
            // ExperimentDataType{ "Fork_Read", "ForkWrapper", "", "ReadBench" }.Execute();
            // ExperimentDataType{ "Fork_Invalidate_Read", "ForkWrapper", "InvalidateCachePrepare", "ReadBench" }.Execute();

            // ExperimentDataType{ "Fork_Warm_Read", "ForkWrapper", "WarmCachePrepare", "ReadBench" }.Execute();
            // ExperimentDataType{ "Fork_Cool_Read", "ForkWrapper", "CoolCachePrepare", "ReadBench" }.Execute();
            // ExperimentDataType{ "Fork_ComplexCool_Read", "ForkWrapper", "ComplexCoolCachePrepare", "ReadBench" }.Execute();
        }
    }
    auto WriteExperiments() -> void {
        {
            // for (const auto& size : { L1, L2, L3 })
            ExperimentDataType{ "Main_Write", "NoWrapper", "", "WriteBench" }.Execute();
            // ExperimentDataType{ "Main_Invalidate_Write", "NoWrapper", "InvalidateCachePrepare", "WriteBench" }.Execute();
        }
        {
            // 1 Rep, restart executable with repeat_execution.sh
            // ExperimentDataType{ "Main_Terminate_Write", 1, "NoWrapper", "", "WriteBench" }.Execute();
            // ExperimentDataType{ "Main_Terminate_Invalidate_Write", 1, "NoWrapper", "InvalidateCachePrepare", "WriteBench" }.Execute();
        }
        {
            // ExperimentDataType{ "Thread_Write", "ThreadWrapper", "", "WriteBench" }.Execute();
            // ExperimentDataType{ "Thread_Invalidate_Write", "ThreadWrapper", "InvalidateCachePrepare", "WriteBench" }.Execute();

            // ExperimentDataType{ "Thread_Warm_Write", "ThreadWrapper", "WarmCachePrepare", "WriteBench" }.Execute();
            // ExperimentDataType{ "Thread_Cool_Write", "ThreadWrapper", "CoolCachePrepare", "WriteBench" }.Execute();
            // ExperimentDataType{ "Thread_ComplexCool_Write", "ThreadWrapper", "ComplexCoolCachePrepare", "WriteBench" }.Execute();
        }
        {
            // System Call experiment, system_call_main.cpp manages parameters other than        // ExperimentDataType{ "System_Call_Write", "SystemCallWrapper", "", ""
            // }.Execute(); ExperimentDataType{ "System_Call_Invalidate_Write", "SystemCallWrapper", "InvalidateCachePrepare", "" }.Execute();
        }
        {
            // ExperimentDataType{ "Fork_Write", "ForkWrapper", "", "WriteBench" }.Execute();
            // ExperimentDataType{ "Fork_Invalidate_Write", "ForkWrapper", "InvalidateCachePrepare", "WriteBench" }.Execute();

            // ExperimentDataType{ "Fork_Warm_Write", "ForkWrapper", "WarmCachePrepare", "WriteBench" }.Execute();
            // ExperimentDataType{ "Fork_Cool_Write", "ForkWrapper", "CoolCachePrepare", "WriteBench" }.Execute();
            // ExperimentDataType{ "Fork_ComplexCool_Write", "ForkWrapper", "ComplexCoolCachePrepare", "WriteBench" }.Execute();
        }
    }
}    // namespace

auto main() -> int {
    CallPosix(pthread_setconcurrency, static_cast<int>(std::jthread::hardware_concurrency()));

    const std::string layout = std::filesystem::path{ POOL_PATH }.filename().string();
    const bool recovered = std::filesystem::exists(POOL_PATH);
    const std::string poolPath{ POOL_PATH };
    if (!recovered) {
        auto pool = pmem::obj::pool<Root>::create(poolPath, layout, POOL_SIZE);
        InitializeArray(pool.root().get());
        pool.close();
    }
    if (pmem::obj::pool<Root>::check(poolPath, layout) != 1) {
        std::println(stdout, "pool inconsistent {}", poolPath);
        std::fflush(stdout);
        std::abort();
    }
    Pool = pmem::obj::pool<Root>::open(poolPath, layout);
    // std::println("{:p}", static_cast<void*>(Pool.root()->array.data()));

    constexpr auto PrintAligned = [](const auto* ptr, const std::size_t align) { std::println("{}", std::bit_cast<std::uintptr_t>(ptr) % align); };
    PrintAligned(Pool.root().get(), CACHE_LINE_SIZE);
    PrintAligned(Pool.root()->padding.data(), CACHE_LINE_SIZE);
    PrintAligned(Pool.root()->array.data(), CACHE_LINE_SIZE);
    assert(IsAligned(Pool.root()->array.data(), CACHE_LINE_SIZE));

    PinThisThreadToCore(0);
    // [[maybe_unused]] int ret = std::system("sudo insmod wbinvd.ko");
    // assert(ret >= 0);
#ifdef WBINVD_ENABLED
    System("sudo insmod wbinvd.ko");
#endif
    INSTRUMENT_BEGIN_SESSION(TRACE_FILE_NAME);

    ReadExperiments();
    // WriteExperiments();

    INSTRUMENT_END_SESSION();
    // ret = std::system("sudo rmmod wbinvd.ko");
    // assert(ret >= 0);
#ifdef WBINVD_ENABLED
    System("sudo rmmod wbinvd.ko");
#endif
    Pool.close();
}
