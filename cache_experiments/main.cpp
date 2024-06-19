
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <thread>

#include <libpmemobj++/make_persistent_atomic.hpp>
#include <libpmemobj++/p.hpp>
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/pexceptions.hpp>
#include <libpmemobj++/pext.hpp>
#include <libpmemobj++/pool.hpp>
#include <libpmemobj++/transaction.hpp>

#define MOUNT_DIR "/mnt/pmem0/myrontsa/"
#define POOL_PATH_REL "pool_dir/pool"
constexpr uint64_t SIZE = PMEMOBJ_MIN_POOL;
constexpr uint64_t WORKLOAD = 1LU * 1'0;
constexpr auto PoolPath = MOUNT_DIR POOL_PATH_REL;

struct RootType {
    pmem::obj::p<uint64_t> counter1;
    pmem::obj::p<uint64_t> counter2;
    pmem::obj::p<uint64_t> counter3;
    pmem::obj::persistent_ptr<uint64_t> ptr;
};

constexpr const char* PoolLayout = "pool";
pmem::obj::pool<RootType> Pool{};
pmem::obj::persistent_ptr<RootType> RootPtr{};

using Clock = std::chrono::steady_clock;
using Seconds = std::chrono::seconds;
using TimePoint = std::chrono::time_point<Clock>;

void KillTask() {
    bool done = false;
    const auto killTimePoint = TimePoint(Clock::now() + Seconds(4));
    while (!done) {
        const auto now = Clock::now();
        std::this_thread::sleep_until(killTimePoint);
        if (now >= killTimePoint) {
            std::abort();
            done = true;
        }
    }
}

void RecoverPrintReset() {
    std::cout << "VolatileWriteMaybeFlush: " << RootPtr->counter1 << "\n";
    std::cout << "VolatileWriteForceFlush: " << RootPtr->counter2 << "\n";
    std::cout << "AtomicWriteTransaction: " << RootPtr->counter3 << "\n";
    if (RootPtr->ptr) std::cout << "AtomicWriteAllocation: " << *(RootPtr->ptr) << "\n";
    RootPtr->counter1 = 0LU;
    RootPtr->counter2 = 0LU;
    RootPtr->counter3 = 0LU;
    pmem::obj::make_persistent_atomic<uint64_t>(Pool, RootPtr->ptr, 0U);
}

void VolatileWriteMaybeFlush() {
    for (auto i = 0LU; i < WORKLOAD; ++i) {
        RootPtr->counter1 += 1;
    }
}

void VolatileWriteForceFlush() {
    for (auto i = 0LU; i < WORKLOAD; ++i) {
        RootPtr->counter2 += 1LU;
        Pool.persist(RootPtr->counter2);
    }
}

void AtomicWriteTransaction() {
    for (auto i = 0LU; i < WORKLOAD; ++i) {
        pmem::obj::transaction::run(Pool, []() {
            RootPtr->counter3 += 1LU;
        });
    }
}

void AtomicWriteAllocation() {
    for (auto i = 0LU; i < WORKLOAD; ++i) {
        pmem::obj::make_persistent_atomic<uint64_t>(Pool, RootPtr->ptr, i);
        pmem::obj::delete_persistent_atomic<uint64_t>(RootPtr->ptr);
    }
}

int main() {
    try {
        int isConsistent = pmem::obj::pool<RootType>::check(PoolPath, PoolLayout);
        if (isConsistent > 0)
            Pool = pmem::obj::pool<RootType>::open(PoolPath, PoolLayout);
        else if (isConsistent < 0)
            Pool = pmem::obj::pool<RootType>::create(PoolPath, PoolLayout, PMEMOBJ_MIN_POOL);
        else {
            std::cout << "IsConsistent: " << isConsistent << "\n";
            assert(false);
        }
        RootPtr = Pool.root();

        RecoverPrintReset();

        std::jthread killThread(KillTask);

        VolatileWriteMaybeFlush();
        VolatileWriteForceFlush();
        AtomicWriteTransaction();
        AtomicWriteAllocation();

    } catch (const pmem::pool_error& e) {
        std::cout << e.what() << "\n";
        assert(false);
    } catch (const pmem::transaction_error& e) {
        std::cout << e.what() << "\n";
        assert(false);
    } catch (const std::bad_alloc& e) {
        std::cout << e.what() << "\n";
        assert(false);
    }
    return 0;
}
