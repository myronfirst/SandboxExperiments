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
constexpr uint64_t CACHE_SIZE = (1024 * 1024) * 4;    // 32MB pc-myron L3 35MB
// constexpr uint64_t CACHE_SIZE = (1024 * 1024) * 4.66;    // 37.28MB titan L3+L2+L1D ~37.3MB
// Allocate only for L3, not for L1+L2+L3, since whatever exists in Le also exists on L1 and L2
//  constexpr uint64_t CACHE_SIZE = (1024 * 1024) * 1;       // 8MB mamalakispc L3 8MB
// constexpr uint64_t REPETITIONS = 30;
// constexpr uint64_t CACHE_SIZE = 10;
constexpr uint64_t REPETITIONS = 5;
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

    auto PersistArrayAndExit() -> void {
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

    auto PinToCore(int core) -> void {
        {
            int ret = pthread_setconcurrency(std::thread::hardware_concurrency());
            if (ret != 0) {
                std::cout << "pthread_setconcurrency error: " << strerror(errno) << '\n';
                assert(false);
            }
        }
        std::cout << "Number of Logical Threads: " << pthread_getconcurrency() << '\n';
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
        if (val == "ForkWrapper") return [this]() { ForkWrapper(); };
        if (val == "WarmCachePrepare") return [this]() { WarmCachePrepare(); };
        if (val == "CoolCachePrepare") return [this]() { CoolCachePrepare(); };
        if (val == "ComplexCoolCachePrepare") return [this]() { ComplexCoolCachePrepare(); };
        if (val == "InvalidateCachePrepare") return [this]() { InvalidateCachePrepare(); };
        if (val == "ReadFromPersistenceBenchmark") return [this]() { ReadFromPersistenceBenchmark(); };
        if (val == "WriteToPersistenceBenchmark") return [this]() { WriteToPersistenceBenchmark(); };
        if (val == "") return [](auto&&...) {};
        assert(false);
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
            PinToCore(1);
            PrepareAndBenchmark();
        });
    }
    auto ForkWrapper() const -> void {
        pid_t pid = fork();
        assert(pid >= 0);
        if (pid > 0) {    // parent
            assert(pid > 0);
            pid_t cid = wait(nullptr);
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
        // PrepareAndBenchmark
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
        // std::system("cat /proc/wbinvd");
        std::system("echo std::system Test");
    }
    auto ReadFromPersistenceBenchmark() const -> void {
        TIME_SCOPE(name);
        for (auto i = 0ul; i < CACHE_SIZE; ++i)
            ReadDestination = RootPtr->arrayPersistent[i];
    }
    auto WriteToPersistenceBenchmark() const -> void {
        TIME_SCOPE(name);
        for (auto i = 0ul; i < CACHE_SIZE; ++i)
            RootPtr->arrayPersistent[i] = i;
        RootPtr->arrayPersistent.persist();
    }
};

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
        PinToCore(0);
        return 1;

        // PersistArrayAndExit();
        INSTRUMENT_BEGIN_SESSION(TraceFileName);
        {
            // ExperimentDataType{ "Thread_Read", REPETITIONS, "ThreadWrapper", "", "ReadFromPersistenceBenchmark" }.Execute();
            // ExperimentDataType{ "Thread_Warm_Read", REPETITIONS, "ThreadWrapper", "WarmCachePrepare", "ReadFromPersistenceBenchmark" }.Execute();
            // ExperimentDataType{ "Thread_Cool_Read", REPETITIONS, "ThreadWrapper", "CoolCachePrepare", "ReadFromPersistenceBenchmark" }.Execute();
            // ExperimentDataType{ "Thread_ComplexCool_Read", REPETITIONS, "ThreadWrapper", "ComplexCoolCachePrepare", "ReadFromPersistenceBenchmark" }.Execute();

            // ExperimentDataType{ "Thread_Write", REPETITIONS, "ThreadWrapper", "", "WriteToPersistenceBenchmark" }.Execute();
            // ExperimentDataType{ "Thread_Warm_Write", REPETITIONS, "ThreadWrapper", "WarmCachePrepare", "WriteToPersistenceBenchmark" }.Execute();
            // ExperimentDataType{ "Thread_Cool_Write", REPETITIONS, "ThreadWrapper", "CoolCachePrepare", "WriteToPersistenceBenchmark" }.Execute();
            // ExperimentDataType{ "Thread_ComplexCool_Write", REPETITIONS, "ThreadWrapper", "ComplexCoolCachePrepare", "WriteToPersistenceBenchmark" }.Execute();
        }
        {
            // ExperimentDataType{ "Fork_Read", REPETITIONS, "ForkWrapper", "", "ReadFromPersistenceBenchmark" }.Execute();
            // ExperimentDataType{ "Fork_Warm_Read", REPETITIONS, "ForkWrapper", "WarmCachePrepare", "ReadFromPersistenceBenchmark" }.Execute();
            // ExperimentDataType{ "Fork_Cool_Read", REPETITIONS, "ForkWrapper", "CoolCachePrepare", "ReadFromPersistenceBenchmark" }.Execute();
            // ExperimentDataType{ "Fork_ComplexCool_Read", REPETITIONS, "ForkWrapper", "ComplexCoolCachePrepare", "ReadFromPersistenceBenchmark" }.Execute();

            // ExperimentDataType{ "Fork_Write", REPETITIONS, "ForkWrapper", "", "WriteToPersistenceBenchmark" }.Execute();
            // ExperimentDataType{ "Fork_Warm_Write", REPETITIONS, "ForkWrapper", "WarmCachePrepare", "WriteToPersistenceBenchmark" }.Execute();
            // ExperimentDataType{ "Fork_Cool_Write", REPETITIONS, "ForkWrapper", "CoolCachePrepare", "WriteToPersistenceBenchmark" }.Execute();
            // ExperimentDataType{ "Fork_ComplexCool_Write", REPETITIONS, "ForkWrapper", "ComplexCoolCachePrepare", "WriteToPersistenceBenchmark" }.Execute();
        }
        {
            // ExperimentDataType{ "SystemCall_Read", REPETITIONS, "SystemCallWrapper", "", "ReadFromPersistenceBenchmark" }.Execute();
            // ExperimentDataType{ "SystemCall_Warm_Read", REPETITIONS, "SystemCallWrapper", "WarmCachePrepare", "ReadFromPersistenceBenchmark" }.Execute();
            // ExperimentDataType{ "SystemCall_Cool_Read", REPETITIONS, "SystemCallWrapper", "CoolCachePrepare", "ReadFromPersistenceBenchmark" }.Execute();
            // ExperimentDataType{ "SystemCall_ComplexCool_Read", REPETITIONS, "SystemCallWrapper", "ComplexCoolCachePrepare", "ReadFromPersistenceBenchmark" }.Execute();

            // ExperimentDataType{ "SystemCall_Write", REPETITIONS, "SystemCallWrapper", "", "WriteToPersistenceBenchmark" }.Execute();
            // ExperimentDataType{ "SystemCall_Warm_Write", REPETITIONS, "SystemCallWrapper", "WarmCachePrepare", "WriteToPersistenceBenchmark" }.Execute();
            // ExperimentDataType{ "SystemCall_Cool_Write", REPETITIONS, "SystemCallWrapper", "CoolCachePrepare", "WriteToPersistenceBenchmark" }.Execute();
            // ExperimentDataType{ "SystemCall_ComplexCool_Write", REPETITIONS, "SystemCallWrapper", "ComplexCoolCachePrepare", "WriteToPersistenceBenchmark" }.Execute();
        }
        {    // Mallis
             // ExperimentDataType{ "Main_Read", 1, "NoWrapper", "", "ReadFromPersistenceBenchmark" }.Execute();
             // ExperimentDataType{ "Main_Write", 1, "NoWrapper", "", "WriteToPersistenceBenchmark" }.Execute();
        }
        {    // WBINVD
            ExperimentDataType{ "Thread_Invalidate_Read", REPETITIONS, "ThreadWrapper", "InvalidateCachePrepare", "ReadFromPersistenceBenchmark" }.Execute();
            ExperimentDataType{ "Thread_Invalidate_Write", REPETITIONS, "ThreadWrapper", "InvalidateCachePrepare", "WriteToPersistenceBenchmark" }.Execute();
        }

        INSTRUMENT_END_SESSION();

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
