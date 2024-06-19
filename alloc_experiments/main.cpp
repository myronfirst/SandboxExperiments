#include <cassert>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <functional>
#include <iostream>
#include <string>
#include <thread>

#include <libpmemobj++/make_persistent_array_atomic.hpp>
#include <libpmemobj++/make_persistent_atomic.hpp>
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/pool.hpp>
#include <libpmemobj++/transaction.hpp>

#include <libpmem.h>

#include <libvmmalloc.h>
#include <malloc.h>
#include <cstdlib>

/*
1.
Perform allocations with libvmalloc, pmdk
Compare read speed of allocated addresses

2.
Compare allocations with libvmalloc, pmdk, DRAM malloc
*/
namespace {
    using AllocCB = std::function<void*(std::size_t)>;
    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using Millis = std::chrono::milliseconds;

    const std::string POOL_PATH = "/mnt/pmem0/myrontsa/alloc_experiments_pool";
    constexpr std::size_t POOL_SIZE = PMEMOBJ_MIN_POOL;
    constexpr std::size_t ALLOC_SIZE = 32;

    std::size_t LOGN_OPS = 7;
    std::size_t N_THREADS = std::thread::hardware_concurrency();
    AllocCB ALLOC_CALLBACK;
    std::size_t NOps;

    struct AllocRoot {
        int test;
    };
    struct Root {
        pmem::obj::persistent_ptr<AllocRoot> allocRoot{};
    };
    pmem::obj::pool<Root> Pool{};
    pmem::obj::persistent_ptr<AllocRoot> AllocRootPtr{};

    auto Malloc(std::size_t bytes) -> void* {
        return malloc(bytes);
    }

    auto MakePersistentAtomic(std::size_t bytes) -> void* {
        pmem::obj::persistent_ptr<char[]> ptr{};
        pmem::obj::make_persistent_atomic<char[]>(Pool, ptr, bytes);
        return ptr.get();
    }

    auto IsPmem() -> bool {
        const std::string tmpPath = POOL_PATH + "_tmp";
        std::size_t mappedLen;
        int isPmem;
        void* addr = pmem_map_file(tmpPath.data(), 1, PMEM_FILE_CREATE, 0666, &mappedLen, &isPmem);
        assert(addr);
        [[maybe_unused]] int error = pmem_unmap(addr, mappedLen);
        assert(!error);
        error = std::system(("rm " + tmpPath).data());
        assert(!error);
        return isPmem;
    }

    auto ParseArgs(int argc, char* argv[]) -> void {
        if (argc >= 2) {
            ALLOC_CALLBACK = [](const std::string& name) -> AllocCB {
                if (name == "malloc") return Malloc;
                if (name == "make_persistent_atomic") return MakePersistentAtomic;
                return {};
            }(std::string{ argv[1] });
        }
        if (argc >= 3) {
            N_THREADS = std::stoul(argv[2]);
        }
        if (argc >= 4) {
            LOGN_OPS = std::stoul(argv[3]);
            NOps = static_cast<std::size_t>(std::pow(10, LOGN_OPS));
        }
        if (!ALLOC_CALLBACK) {
            std::cout << "Invalid ALLOC_CALLBACK" << N_THREADS << "\n";
            assert(false);
            std::exit(EXIT_FAILURE);
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
        [[maybe_unused]] int error = std::system(("rm " + POOL_PATH).data());
        assert(!error);
    }

    auto PinThisThreadToCore(std::size_t core) -> void {
        int err;
        err = pthread_setconcurrency(static_cast<int>(std::thread::hardware_concurrency()));
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
        std::size_t threadOps = NOps / N_THREADS;
        std::vector<void*> allocs(NOps);
        std::vector<std::thread> threads(N_THREADS);
        const auto begin = Clock::now();
        for (auto tid = 0u; tid < threads.size(); ++tid) {
            const auto threadBegin = tid * threadOps;
            const auto threadEnd = threadBegin + threadOps;
            threads.at(tid) = std::thread([&allocs, tid, threadBegin, threadEnd]() {
                PinThisThreadToCore(tid % N_THREADS);
                for (auto i = threadBegin; i < threadEnd; ++i)
                    allocs.at(i) = ALLOC_CALLBACK(ALLOC_SIZE);
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
    std::cout << "Persistent Memory Support: " << IsPmem() << "\n";
    InitPool();

    AllocBench();
    ReadAllocBench();

    DestroyPool();
}
