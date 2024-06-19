
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
namespace {

    constexpr const char* POOL_PATH = "/mnt/pmem0/myrontsa/alloc_experiments_pool";
    constexpr std::size_t POOL_SIZE = PMEMOBJ_MIN_POOL;
    constexpr std::size_t ALLOC_SIZE = 32;

    std::size_t LOGN_OPS = 7;
    std::size_t N_THREADS = std::thread::hardware_concurrency();

    const std::size_t NOps = std::pow(10, LOGN_OPS);
    // constexpr uint64_t WORKLOAD = 1LU * 1'0;

    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using Millis = std::chrono::milliseconds;

    struct AllocRoot {
        int test;
    };
    struct Root {
        pmem::obj::persistent_ptr<AllocRoot> allocRoot{};
    };
    pmem::obj::pool<Root> Pool{};
    pmem::obj::persistent_ptr<AllocRoot> AllocRootPtr{};

    auto ParseArgs(int argc, char* argv[]) -> void {
        if (argc == 2) {
            N_THREADS = std::stoi(argv[1]);
        }
        if (argc == 3) {
            LOGN_OPS = std::stoi(argv[2]);
        }
        if (N_THREADS <= 0) {
            std::cout << "Invalid N_THREADS" << N_THREADS << "\n";
            assert(false);
            std::exit(EXIT_FAILURE);
        }
        if (LOGN_OPS <= 0) {
            std::cout << "Invalid LOGN_OPS: " << LOGN_OPS << "\n";
            assert(false);
            std::exit(EXIT_FAILURE);
        }
    }

    auto InitPool() -> void {
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
        Pool = pmem::obj::pool<Root>::open(POOL_PATH, poolLayout);
        if (Pool.root()->allocRoot == nullptr) {
            pmem::obj::make_persistent_atomic<AllocRoot>(Pool, Pool.root()->allocRoot);
        }
        AllocRootPtr = Pool.root()->allocRoot;
    }

    auto DestroyPool() -> void {
        pmem::obj::delete_persistent_atomic<AllocRoot>(Pool.root()->allocRoot);
        Pool.root()->allocRoot = nullptr;
        Pool.persist(Pool.root()->allocRoot);
        Pool.close();
    }

    auto PinThisThreadToCore(int core) -> void {
        int err;
        err = pthread_setconcurrency(std::thread::hardware_concurrency());
        if (err) {
            std::cout << "pthread_setconcurrency error: " << strerror(errno) << '\n';
            assert(false);
            std::exit(EXIT_FAILURE);
        }
        cpu_set_t cpu_mask;
        CPU_ZERO(&cpu_mask);
        CPU_SET(core, &cpu_mask);
        err = pthread_setaffinity_np(pthread_self(), sizeof(cpu_mask), &cpu_mask);
        if (err) {
            std::cout << "pthread_setaffinity_np error: " << strerror(errno) << '\n';
            assert(false);
            std::exit(EXIT_FAILURE);
        }
    }

    auto AllocBench() -> void {
        int threadOps = NOps / N_THREADS;
        std::vector<void*> allocs(NOps);
        std::vector<std::thread> threads(N_THREADS);
        const auto begin = Clock::now();
        for (auto tid = 0u; tid < threads.size(); ++tid) {
            const auto threadBegin = tid * threadOps;
            const auto threadEnd = threadBegin + threadOps;
            threads.at(tid) = std::thread([&allocs, tid, threadBegin, threadEnd]() {
                PinThisThreadToCore(tid % N_THREADS);
                for (auto i = threadBegin; i < threadEnd; ++i)
                    allocs.at(i) = malloc(ALLOC_SIZE);
            });
        }
        for (auto& t : threads) t.join();
        const auto end = Clock::now();
        std::cout << std::chrono::duration_cast<Millis>(end - begin).count() << "\n";
    }

    auto ReadAllocBench() -> void {
    }

}    // namespace

auto main(int argc, char* argv[]) -> int {
    ParseArgs(argc, argv);
    InitPool();

    AllocBench();
    ReadAllocBench();

    DestroyPool();
}
