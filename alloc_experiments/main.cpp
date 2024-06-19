
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <thread>

#include <libpmemobj++/make_persistent_atomic.hpp>
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/pool.hpp>
#include <libpmemobj++/transaction.hpp>

/*
1.
Perform allocations with libvmalloc, pmdk
Compare read speed of allocated addresses

2.
Compare allocations with libvmalloc, pmdk, DRAM malloc
*/

constexpr auto POOL_PATH = "/mnt/pmem0/myrontsa/alloc_experiments_pool";
constexpr auto POOL_SIZE = PMEMOBJ_MIN_POOL;
// constexpr uint64_t WORKLOAD = 1LU * 1'0;

using Clock = std::chrono::steady_clock;
using Seconds = std::chrono::seconds;
using TimePoint = std::chrono::time_point<Clock>;

struct AllocRoot {
    int test;
};
struct Root {
    pmem::obj::persistent_ptr<AllocRoot> allocRoot{};
};
pmem::obj::persistent_ptr<AllocRoot> AllocRootPtr{};

auto AllocBench() -> void {
}

auto ReadAllocBench() -> void {
}

auto main() -> int {
    const auto poolLayout = std::filesystem::path{ POOL_PATH }.extension().c_str();
    if (!std::filesystem::exists(POOL_PATH)) {
        pmem::obj::pool<Root>::create(POOL_PATH, poolLayout, POOL_SIZE).close();
    }
    int ret = pmem::obj::pool<Root>::check(POOL_PATH, poolLayout);
    if (ret == 0) {
        std::cout << "Pool Inconsistent, exiting, ret: " << ret << "\n";
        assert(false);
        std::exit(EXIT_FAILURE);
    }
    auto pool = pmem::obj::pool<Root>::open(POOL_PATH, poolLayout);
    if (pool.root()->allocRoot == nullptr) {
        pmem::obj::make_persistent_atomic<AllocRoot>(pool, pool.root()->allocRoot);
    }
    AllocRootPtr = pool.root()->allocRoot;

    AllocBench();
    ReadAllocBench();

    pmem::obj::delete_persistent_atomic<AllocRoot>(pool.root()->allocRoot);
    pool.root()->allocRoot = nullptr;
    pool.persist(pool.root()->allocRoot);
    pool.close();
}
