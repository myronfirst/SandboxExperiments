#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <memory>
#include <numeric>
#include <random>
#include <thread>

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

/* ---Config--- */
constexpr uint64_t POOL_SIZE = PMEMOBJ_MIN_POOL * 64;
constexpr auto TraceFileName = "./Traces";
// constexpr uint64_t CACHE_SIZE = (1024 * 1024) * 4;    // 32MB pc-myron L3 35MB
constexpr uint64_t CACHE_SIZE = (1024 * 1024) * 4.66;    // 37.28MB titan L3+L2+L1D ~37.3MB
// Allocate only for L3, not for L1+L2+L3, since whatever exists in L3 also exists on L1 and L2
//  constexpr uint64_t CACHE_SIZE = (1024 * 1024) * 1;       // 8MB mamalakispc L3 8MB
constexpr uint64_t REPETITIONS = 30;
// constexpr uint64_t CACHE_SIZE = 10;
// constexpr uint64_t REPETITIONS = 10;
// constexpr uint64_t CACHE_SIZE = (1024 * 1024) * 1;    // ~8MB

/* --- */

/* ---Persistent--- */
#define MOUNT_DIR "/mnt/pmem0/myrontsa/"
#define POOL_PATH_REL "pool_dir/pool"
constexpr auto PoolPath = MOUNT_DIR POOL_PATH_REL;
constexpr const char* PoolLayout = "pool";
struct RootType {
    pmem::obj::persistent_ptr<uint64_t[CACHE_SIZE]> arrayPersistent;
};
pmem::obj::pool<RootType> Pool{};
pmem::obj::persistent_ptr<RootType> RootPtr{};
/* --- */

/* ---Volatile--- */
constexpr size_t DOUBLE_CACHE_SIZE = 2 * CACHE_SIZE;
using CoolingBufferType = std::array<uint64_t, DOUBLE_CACHE_SIZE>;
auto CoolingBuffer = std::make_unique<CoolingBufferType>();
volatile uint64_t ReadDestination{};
/* --- */

namespace {
    auto PrintPersistence(size_t limit = CACHE_SIZE) -> void {
        std::cout << "Persistence:";
        if (RootPtr->arrayPersistent)
            for (auto i = 0ul; i < limit; ++i)
                std::cout << ' ' << RootPtr->arrayPersistent[i];
        std::cout << '\n';
    }

    [[maybe_unused]] auto PersistArrayAndExit() -> void {
        auto buffer = std::make_unique<std::array<uint64_t, CACHE_SIZE>>();
        std::iota(buffer->begin(), buffer->end(), 0);
        pmem::obj::transaction::run(Pool, [&buffer] {
            if (RootPtr->arrayPersistent)
                pmem::obj::delete_persistent<uint64_t[]>(RootPtr->arrayPersistent, CACHE_SIZE);
            RootPtr->arrayPersistent = pmem::obj::make_persistent<uint64_t[CACHE_SIZE]>();
            for (auto i = 0ul; i < CACHE_SIZE; ++i)
                RootPtr->arrayPersistent[i] = buffer->at(i);
        });
        PrintPersistence(30);
        exit(EXIT_SUCCESS);
    }

    auto PinThisThreadToCore(int core) -> void {
        {
            int ret = pthread_setconcurrency(std::thread::hardware_concurrency());
            if (ret != 0) {
                std::cout << "pthread_setconcurrency error: " << strerror(errno) << '\n';
                assert(false);
            }
        }
        cpu_set_t cpu_mask;
        CPU_ZERO(&cpu_mask);
        CPU_SET(core, &cpu_mask);
        {
            int ret = pthread_setaffinity_np(pthread_self(), sizeof(cpu_mask), &cpu_mask);
            if (ret != 0) {
                std::cout << "pthread_setaffinity_np error: " << strerror(errno) << '\n';
                assert(false);
            }
        }
    }
}    // namespace

class ExperimentDataType {
private:
    std::string name;
    size_t repetitions;
    std::function<void()> wrapperCallback;
    std::function<void()> prepareCallback;
    std::function<void()> benchmarkCallback;

    std::function<void()> PickCallback(const std::string& val) const {
        if (val == "NoWrapper") return [this]() { NoWrapper(); };
        if (val == "ThreadWrapper") return [this]() { ThreadWrapper(); };
        if (val == "SystemCallWrapper") return [this]() { SystemCallWrapper(); };
        if (val == "ForkWrapper") return [this]() { ForkWrapper(); };
        if (val == "WarmCachePrepare") return [this]() { WarmCachePrepare(); };
        if (val == "CoolCachePrepare") return [this]() { CoolCachePrepare(); };
        if (val == "ComplexCoolCachePrepare") return [this]() { ComplexCoolCachePrepare(); };
        if (val == "InvalidateCachePrepare") return [this]() { InvalidateCachePrepare(); };
        if (val == "InvalidateWarmCachePrepare") return [this]() { InvalidateWarmCachePrepare(); };
        if (val == "ReadBench") return [this]() { ReadBench(); };
        if (val == "WriteBench") return [this]() { WriteBench(); };
        if (val == "") return [](auto&&...) {};
        assert(false);
        return [](auto&&...) {};
    }

public:
    ExperimentDataType(const std::string& _name,
                       size_t _repetitions,
                       const std::string& wrapperCallbackStr,
                       const std::string& prepareCallbackStr,
                       const std::string& benchmarkCallbackStr)
        : name{ _name },
          repetitions{ _repetitions },
          wrapperCallback{ PickCallback(wrapperCallbackStr) },
          prepareCallback{ PickCallback(prepareCallbackStr) },
          benchmarkCallback{ PickCallback(benchmarkCallbackStr) } {}
    auto Execute() const -> void {
        assert(wrapperCallback);
        assert(prepareCallback);
        assert(benchmarkCallback);
        assert(RootPtr->arrayPersistent);
        for (auto i = 0ul; i < repetitions; ++i) {
            wrapperCallback();
            std::cout << i << " - " << name << " - done\n";
        }
    }
    auto NoWrapper() const -> void {
        PrepareAndBenchmark();
    }
    auto ThreadWrapper() const -> void {
        std::jthread t([this]() {
            PinThisThreadToCore(1);
            PrepareAndBenchmark();
        });
    }
    auto ForkWrapper() const -> void {
        pid_t pid = fork();
        assert(pid >= 0);
        if (pid > 0) {    // parent
            assert(pid > 0);
            [[maybe_unused]] pid_t cid = wait(nullptr);
            assert(cid >= 0);
            return;
        } else {    // child
            assert(pid == 0);
            PrepareAndBenchmark();
            exit(EXIT_SUCCESS);
        }
        assert(false);
    }
    auto SystemCallWrapper() const -> void {
        prepareCallback();
        Pool.close();
        [[maybe_unused]] int ret = std::system("./system_call_main");
        assert(ret >= 0);
        Pool = pmem::obj::pool<RootType>::open(PoolPath, PoolLayout);
    }
    auto PrepareAndBenchmark() const -> void {
        prepareCallback();
        benchmarkCallback();
    }

    auto WarmCachePrepare() const -> void {
        for (auto i = 0ul; i < CACHE_SIZE; ++i)
            ReadDestination = RootPtr->arrayPersistent[i];
    }
    auto CoolCachePrepare() const -> void {
        for (auto i = 0ul; i < CoolingBuffer->size(); ++i)
            CoolingBuffer->at(i) = i;
    }
    auto ComplexCoolCachePrepare() const -> void {
        CoolingBuffer->fill(1);
        std::random_device r;
        std::default_random_engine e(r());
        std::uniform_int_distribution<size_t> uniform_dist(0, DOUBLE_CACHE_SIZE - 1);
        for (auto i = 0ul; i < CoolingBuffer->size(); ++i) CoolingBuffer->at(i) = uniform_dist(e);
        auto coolingBufferIndexes = std::make_unique<std::array<size_t, DOUBLE_CACHE_SIZE>>();
        for (auto begin = std::begin(*coolingBufferIndexes); begin < std::end(*coolingBufferIndexes); begin += DOUBLE_CACHE_SIZE)
            std::iota(begin, begin + DOUBLE_CACHE_SIZE, 0);
        std::ranges::shuffle(*coolingBufferIndexes, e);
        for (auto i = 0ul; i < coolingBufferIndexes->size(); ++i) {
            const auto idx = coolingBufferIndexes->at(i);
            ReadDestination = CoolingBuffer->at(idx);
        }
    }
    auto InvalidateCachePrepare() const -> void {
        [[maybe_unused]] int ret = std::system("sudo cat /proc/wbinvd");
        assert(ret >= 0);
        {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(100ms);
        }
    }
    auto InvalidateWarmCachePrepare() const -> void {
        InvalidateCachePrepare();
        WarmCachePrepare();
    }

    auto ReadBench() const -> void {
        TIME_SCOPE(name);
        for (auto i = 0ul; i < CACHE_SIZE; ++i)
            ReadDestination = RootPtr->arrayPersistent[i];
    }
    auto WriteBench() const -> void {
        TIME_SCOPE(name);
        for (auto i = 0ul; i < CACHE_SIZE; ++i)
            RootPtr->arrayPersistent[i] = i;
        RootPtr->arrayPersistent.persist();
    }
};

auto ReadExperiments() -> void {
    {
        ExperimentDataType{ "Main_Read", REPETITIONS, "NoWrapper", "", "ReadBench" }.Execute();
        ExperimentDataType{ "Main_Invalidate_Read", REPETITIONS, "NoWrapper", "InvalidateCachePrepare", "ReadBench" }.Execute();
        // ExperimentDataType{ "Main_Invalidate_Warm_Read", REPETITIONS, "NoWrapper", "InvalidateWarmCachePrepare", "ReadBench" }.Execute();
    }
    {    // 1 Rep, restart executable with repeat_execution.sh
         // ExperimentDataType{ "Main_Terminate_Read", 1, "NoWrapper", "", "ReadBench" }.Execute();
         // ExperimentDataType{ "Main_Terminate_Invalidate_Read", 1, "NoWrapper", "InvalidateCachePrepare", "ReadBench" }.Execute();
    }
    {
        ExperimentDataType{ "Thread_Read", REPETITIONS, "ThreadWrapper", "", "ReadBench" }.Execute();
        ExperimentDataType{ "Thread_Invalidate_Read", REPETITIONS, "ThreadWrapper", "InvalidateCachePrepare", "ReadBench" }.Execute();
        // ExperimentDataType{ "Thread_Invalidate_Warm_Read", REPETITIONS, "ThreadWrapper", "InvalidateWarmCachePrepare", "ReadBench" }.Execute();

        // ExperimentDataType{ "Thread_Cool_Read", REPETITIONS, "ThreadWrapper", "CoolCachePrepare", "ReadBench" }.Execute();
        // ExperimentDataType{ "Thread_ComplexCool_Read", REPETITIONS, "ThreadWrapper", "ComplexCoolCachePrepare", "ReadBench" }.Execute();
    }
    {    // 1 Rep, restart executable with repeat_execution.sh
         // ExperimentDataType{ "Thread_Terminate_Read", 1, "ThreadWrapper", "", "ReadBench" }.Execute();
         // ExperimentDataType{ "Thread_Terminate_Invalidate_Read", 1, "ThreadWrapper", "InvalidateCachePrepare", "ReadBench" }.Execute();
    }
    {    // System Call experiment, system_call_main.cpp manages parameters other than REPETITIONS
         // ExperimentDataType{ "System_Call_Read", REPETITIONS, "SystemCallWrapper", "", "" }.Execute();
         // ExperimentDataType{ "System_Call_Invalidate_Read", REPETITIONS, "SystemCallWrapper", "InvalidateCachePrepare", "" }.Execute();
    }
    {
        // ExperimentDataType{ "Fork_Read", REPETITIONS, "ForkWrapper", "", "ReadBench" }.Execute();
        // ExperimentDataType{ "Fork_Invalidate_Read", REPETITIONS, "ForkWrapper", "InvalidateCachePrepare", "ReadBench" }.Execute();

        // ExperimentDataType{ "Fork_Warm_Read", REPETITIONS, "ForkWrapper", "WarmCachePrepare", "ReadBench" }.Execute();
        // ExperimentDataType{ "Fork_Cool_Read", REPETITIONS, "ForkWrapper", "CoolCachePrepare", "ReadBench" }.Execute();
        // ExperimentDataType{ "Fork_ComplexCool_Read", REPETITIONS, "ForkWrapper", "ComplexCoolCachePrepare", "ReadBench" }.Execute();
    }
}
auto WriteExperiments() -> void {
    {
        ExperimentDataType{ "Main_Write", REPETITIONS, "NoWrapper", "", "WriteBench" }.Execute();
        // ExperimentDataType{ "Main_Invalidate_Write", REPETITIONS, "NoWrapper", "InvalidateCachePrepare", "WriteBench" }.Execute();
    }
    {    // 1 Rep, restart executable with repeat_execution.sh
         // ExperimentDataType{ "Main_Terminate_Write", 1, "NoWrapper", "", "WriteBench" }.Execute();
         // ExperimentDataType{ "Main_Terminate_Invalidate_Write", 1, "NoWrapper", "InvalidateCachePrepare", "WriteBench" }.Execute();
    }
    {
        // ExperimentDataType{ "Thread_Write", REPETITIONS, "ThreadWrapper", "", "WriteBench" }.Execute();
        // ExperimentDataType{ "Thread_Invalidate_Write", REPETITIONS, "ThreadWrapper", "InvalidateCachePrepare", "WriteBench" }.Execute();

        // ExperimentDataType{ "Thread_Warm_Write", REPETITIONS, "ThreadWrapper", "WarmCachePrepare", "WriteBench" }.Execute();
        // ExperimentDataType{ "Thread_Cool_Write", REPETITIONS, "ThreadWrapper", "CoolCachePrepare", "WriteBench" }.Execute();
        // ExperimentDataType{ "Thread_ComplexCool_Write", REPETITIONS, "ThreadWrapper", "ComplexCoolCachePrepare", "WriteBench" }.Execute();
    }
    {    // System Call experiment, system_call_main.cpp manages parameters other than REPETITIONS
         // ExperimentDataType{ "System_Call_Write", REPETITIONS, "SystemCallWrapper", "", "" }.Execute();
         // ExperimentDataType{ "System_Call_Invalidate_Write", REPETITIONS, "SystemCallWrapper", "InvalidateCachePrepare", "" }.Execute();
    }
    {
        // ExperimentDataType{ "Fork_Write", REPETITIONS, "ForkWrapper", "", "WriteBench" }.Execute();
        // ExperimentDataType{ "Fork_Invalidate_Write", REPETITIONS, "ForkWrapper", "InvalidateCachePrepare", "WriteBench" }.Execute();

        // ExperimentDataType{ "Fork_Warm_Write", REPETITIONS, "ForkWrapper", "WarmCachePrepare", "WriteBench" }.Execute();
        // ExperimentDataType{ "Fork_Cool_Write", REPETITIONS, "ForkWrapper", "CoolCachePrepare", "WriteBench" }.Execute();
        // ExperimentDataType{ "Fork_ComplexCool_Write", REPETITIONS, "ForkWrapper", "ComplexCoolCachePrepare", "WriteBench" }.Execute();
    }
}

auto main() -> int {
    try {
        int isConsistent = pmem::obj::pool<RootType>::check(PoolPath, PoolLayout);
        if (isConsistent > 0)
            Pool = pmem::obj::pool<RootType>::open(PoolPath, PoolLayout);
        else if (isConsistent < 0)
            Pool = pmem::obj::pool<RootType>::create(PoolPath, PoolLayout, POOL_SIZE);
        else {
            std::cout << "IsConsistent: " << isConsistent << std::endl;
            assert(false);
        }
        RootPtr = Pool.root();
        // PersistArrayAndExit();
        PinThisThreadToCore(0);
        std::cout << "Number of Logical Threads: " << pthread_getconcurrency() << '\n';
        [[maybe_unused]] int ret = std::system("sudo insmod wbinvd.ko");
        assert(ret >= 0);
        INSTRUMENT_BEGIN_SESSION(TraceFileName);

        ReadExperiments();
        // WriteExperiments();

        INSTRUMENT_END_SESSION();
        ret = std::system("sudo rmmod wbinvd.ko");
        assert(ret >= 0);

    } catch (const pmem::pool_error& e) {
        std::cout << e.what() << '\n';
        assert(false);
    } catch (const pmem::transaction_error& e) {
        std::cout << e.what() << '\n';
        assert(false);
    } catch (const std::bad_alloc& e) {
        std::cout << e.what() << '\n';
        assert(false);
    }
    return 0;
}
