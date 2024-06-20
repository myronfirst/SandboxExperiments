#include <array>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <functional>
#include <iostream>
#include <string>
#include <thread>

#include <libpmemobj++/make_persistent_array_atomic.hpp>
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/pool.hpp>

#include <libpmem.h>

// #include <libvmmalloc.h>
// #include <malloc.h>
// #include <cstdlib>
// #include <new>

/*
1.
Perform allocations with libvmalloc, pmdk
Compare read speed of allocated addresses

2.
Compare allocations with libvmalloc, pmdk, DRAM malloc
*/
namespace {
    namespace Config {
        constexpr std::size_t POOL_SIZE = 1024ULL * 1024ULL * 1024ULL * 16;    //16GB
        constexpr std::size_t ALLOC_SIZE = 32;
        const std::string NVM_DIR = "/mnt/pmem0/myrontsa/";
    }    // namespace Config

    namespace Interface {
        using ContextFunc = void (*)();
        using AllocFunc = void* (*)(std::size_t);
        constexpr ContextFunc InitPool;
        constexpr ContextFunc DestroyPool;
        constexpr AllocFunc Alloc;
    }    // namespace Interface

    namespace Default {
        auto InitPool() -> void {}
        auto DestroyPool() -> void {}
        auto Alloc(std::size_t sz) -> void* { return operator new(sz); }
#ifdef DEFAULT
        Interface::InitPool = InitPool;
        Interface::DestroyPool = DestroyPool;
        Interface::Alloc = Alloc;
#endif
    }    // namespace Default

    namespace Malloc {
        auto InitPool() -> void {}
        auto DestroyPool() -> void {}
        auto Alloc(std::size_t sz) -> void* {
            if (void* ptr = std::malloc(sz)) return ptr;
            throw std::bad_alloc{};
        }
#ifdef MALLOC
        Interface::InitPool = InitPool;
        Interface::DestroyPool = DestroyPool;
        Interface::Alloc = Alloc;
#endif
    }    // namespace Malloc

    namespace JMalloc {
        auto InitPool() -> void {}
        auto DestroyPool() -> void {}
        auto Alloc(std::size_t sz) -> void* {
            (void)sz;
            return {};
            // if (void* ptr = std::malloc(sz)) return ptr;
            // throw std::bad_alloc{};
        }
#ifdef JMALLOC
        Interface::InitPool = JMalloc::InitPool;
        Interface::DestroyPool = JMalloc::DestroyPool;
        Interface::Alloc = JMalloc::Alloc;
#endif
    }    // namespace JMalloc

    namespace Vmem {
        auto InitPool() -> void {}
        auto DestroyPool() -> void {}
        auto Alloc(std::size_t sz) -> void* {
            (void)sz;
            return {};
            // if (void* ptr = std::malloc(sz)) return ptr;
            // throw std::bad_alloc{};
        }
#ifdef VMEM
        Interface::InitPool = Vmem::InitPool;
        Interface::DestroyPool = Vmem::DestroyPool;
        Interface::Alloc = Vmem::Alloc;
#endif
    }    // namespace Vmem

    namespace Vmmalloc {
        auto InitPool() -> void {}
        auto DestroyPool() -> void {}
        auto Alloc(std::size_t sz) -> void* {
            if (void* ptr = std::malloc(sz)) return ptr;
            throw std::bad_alloc{};
        }
#ifdef VMMALLOC
        Interface::InitPool = VMMALLOC::InitPool;
        Interface::DestroyPool = VMMALLOC::DestroyPool;
        Interface::Alloc = VMMALLOC::Alloc;
#endif
    }    // namespace Vmmalloc

    namespace MemKind {
        auto InitPool() -> void {}
        auto DestroyPool() -> void {}
        auto Alloc(std::size_t sz) -> void* {
            if (void* ptr = std::malloc(sz)) return ptr;
            throw std::bad_alloc{};
        }
#ifdef MEMKIND
        Interface::InitPool = MemKind::InitPool;
        Interface::DestroyPool = MemKind::DestroyPool;
        Interface::Alloc = MemKind::Alloc;
#endif
    }    // namespace MemKind
    namespace PmemobjAlloc {
        auto InitPool() -> void {}
        auto DestroyPool() -> void {}
        auto Alloc(std::size_t sz) -> void* {
            (void)sz;
            return {};
            // if (void* ptr = std::malloc(sz)) return ptr;
            // throw std::bad_alloc{};
        }
#ifdef PMEMOBJ_ALLOC
        Interface::InitPool = PmemobjAlloc::InitPool;
        Interface::DestroyPool = PmemobjAlloc::DestroyPool;
        Interface::Alloc = PmemobjAlloc::Alloc;
#endif
    }    // namespace PmemobjAlloc

    namespace MakePersistentAtomic {
        const std::string PoolPath = Config::NVM_DIR + "alloc_experiments_pool";
        pmem::obj::pool_base Pool{};
        auto InitPool() -> void {
            const auto poolLayout = std::filesystem::path{ PoolPath }.filename().string();
            if (!std::filesystem::exists(PoolPath)) {
                pmem::obj::pool_base::create(PoolPath, poolLayout, Config::POOL_SIZE).close();
            }
            int ret = pmem::obj::pool_base::check(PoolPath, poolLayout);
            if (ret == 0) {
                std::cout << "Pool Inconsistent, exiting, ret: " << ret << "\n";
                assert(false);
                std::exit(EXIT_FAILURE);
            }
            Pool = pmem::obj::pool_base::open(PoolPath, poolLayout);
        }
        auto DestroyPool() -> void {
            Pool.close();
            [[maybe_unused]] int error = std::system(("rm " + PoolPath).data());
            assert(!error);
        }
        auto Alloc(std::size_t bytes) -> void* {
            pmem::obj::persistent_ptr<std::int8_t[]> ptr{};
            pmem::obj::make_persistent_atomic<std::int8_t[]>(Pool, ptr, bytes);
            return ptr.get();
        }
#ifdef MAKE_PERSISTENT_ATOMIC
        InitPool = MakePersistentAtomic::InitPool;
        DestroyPool = MakePersistentAtomic::DestroyPool;
        Alloc = MakePersistentAtomic::Alloc;
#endif
    }    // namespace MakePersistentAtomic

    std::size_t LOGN_OPS = 7;
    std::size_t N_THREADS = std::thread::hardware_concurrency();
    std::size_t Nops;

    auto IsPmem() -> bool {
        const std::string tmpPath = Config::NVM_DIR + "tmp_pool_tmp";
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

    auto constexpr Pow10(std::size_t exp) -> std::size_t { return exp == 0 ? 1 : 10 * Pow10(exp - 1); }
    auto ParseArgs(int argc, char* argv[]) -> void {
        if (argc >= 2) {
            N_THREADS = std::stoul(argv[1]);
        }
        if (argc >= 3) {
            LOGN_OPS = std::stoul(argv[2]);
            Nops = Pow10(LOGN_OPS);
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

    auto PinThisThreadToCore(std::size_t core) -> void {
        int err;
        err = pthread_setconcurrency(static_cast<int>(std::thread::hardware_concurrency()));
        if (err) {
            std::cout << "pthread_setconcurrency error: " << strerror(errno) << "\n";
            assert(false);
            std::exit(EXIT_FAILURE);
        }
        cpu_set_t cpu_mask;
        CPU_ZERO(&cpu_mask);
        CPU_SET(core, &cpu_mask);
        err = pthread_setaffinity_np(pthread_self(), sizeof(cpu_mask), &cpu_mask);
        if (err) {
            std::cout << "pthread_setaffinity_np error: " << strerror(errno) << "\n";
            assert(false);
            std::exit(EXIT_FAILURE);
        }
    }

    auto AllocBenchmark() -> void {
        using Clock = std::chrono::steady_clock;
        using Millis = std::chrono::milliseconds;
        std::size_t threadOps = Nops / N_THREADS;
        std::vector<void*> allocs(Nops);
        std::vector<std::thread> threads(N_THREADS);
        // std::cout << "allocs capacity byte size: " << allocs.capacity() * sizeof(void*) << "\n";
        const auto begin = Clock::now();
        for (auto tid = 0u; tid < threads.size(); ++tid) {
            const auto threadBegin = tid * threadOps;
            const auto threadEnd = threadBegin + threadOps;
            threads.at(tid) = std::thread([&allocs, tid, threadBegin, threadEnd]() {
                PinThisThreadToCore(tid % N_THREADS);
                for (auto i = threadBegin; i < threadEnd; ++i)
                    allocs.at(i) = Interface::Alloc(Config::ALLOC_SIZE);
            });
        }
        for (auto& t : threads) t.join();
        const auto end = Clock::now();
        std::cout << std::chrono::duration_cast<Millis>(end - begin).count() << "\n";
    }

}    // namespace

auto main(int argc, char* argv[]) -> int {
    ParseArgs(argc, argv);
    std::cout << "Persistent Memory Support: " << IsPmem() << "\n";

    Interface::InitPool();
    AllocBenchmark();
    Interface::DestroyPool();
}
