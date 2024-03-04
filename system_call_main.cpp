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
    [[maybe_unused]] auto ReadBench() -> void {
        TIME_SCOPE("System_Call_Read");
        for (auto i = 0ul; i < CACHE_SIZE; ++i)
            ReadDestination = RootPtr->arrayPersistent[i];
    }
    [[maybe_unused]] auto WriteBench() -> void {
        TIME_SCOPE("System_Call_Write");
        for (auto i = 0ul; i < CACHE_SIZE; ++i)
            RootPtr->arrayPersistent[i] = i;
        RootPtr->arrayPersistent.persist();
    }
}    // namespace

auto main() -> int {
    try {
        int isConsistent = pmem::obj::pool<RootType>::check(PoolPath, PoolLayout);
        // std::cout << isConsistent << " errno: " << strerror(errno) << '\n';
        if (isConsistent > 0)
            Pool = pmem::obj::pool<RootType>::open(PoolPath, PoolLayout);
        else if (isConsistent < 0)
            Pool = pmem::obj::pool<RootType>::create(PoolPath, PoolLayout, POOL_SIZE);
        else {
            std::cout << "IsConsistent: " << isConsistent << std::endl;
            assert(false);
        }
        RootPtr = Pool.root();
        PinThisThreadToCore(0);
        INSTRUMENT_BEGIN_SESSION(TraceFileName);
        {
            ReadBench();
            // WriteBench();
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
