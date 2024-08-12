#include <array>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <memory>
#include <memory_resource>
#include <thread>
#include <vector>

namespace {
    constexpr std::size_t N_THREADS = 8;
    constexpr std::size_t LOG2N_OPS = 25;
    constexpr std::size_t ALLOC_SIZE = 16;
    constexpr std::size_t ALLOC_BUFFER_SIZE = 1024 * 1024 * 1024;

    auto constexpr Pow(std::size_t base, std::size_t exp) -> std::size_t { return exp == 0 ? 1 : base * Pow(base, exp - 1); }
    constexpr std::size_t Nops = Pow(2, LOG2N_OPS);

    // [[maybe_unused]] auto IsPmem() -> bool {
    //     const std::string tmpPath = std::string{ Config::NVM_DIR } + "tmp_pool_tmp";
    //     std::size_t mappedLen;
    //     int isPmem;
    //     void* addr = pmem_map_file(tmpPath.data(), 1, PMEM_FILE_CREATE, 0666, &mappedLen, &isPmem);
    //     assert(addr);
    //     [[maybe_unused]] int error = pmem_unmap(addr, mappedLen);
    //     assert(!error);
    //     error = std::system(("rm " + tmpPath).data());
    //     assert(!error);
    //     return isPmem;
    // }

    // auto constexpr Pow10(std::size_t exp) -> std::size_t { return exp == 0 ? 1 : 10 * Pow10(exp - 1); }
    // auto ParseArgs(int argc, char* argv[]) -> void {
    //     if (argc >= 2) {
    //         N_THREADS = std::stoul(argv[1]);
    //     }
    //     if (argc >= 3) {
    //         LOGN_OPS = std::stoul(argv[2]);
    //         Nops = Pow10(LOGN_OPS);
    //     }
    //     if (N_THREADS <= 0) {
    //         std::cout << "Invalid N_THREADS" << N_THREADS << "\n";
    //         assert(false);
    //         std::exit(EXIT_FAILURE);
    //     }
    //     if (LOGN_OPS <= 0) {
    //         std::cout << "Invalid LOGN_OPS: " << LOGN_OPS << "\n";
    //         assert(false);
    //         std::exit(EXIT_FAILURE);
    //     }
    // }

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

    auto Benchmark() -> void {
        using Clock = std::chrono::steady_clock;
        using Millis = std::chrono::milliseconds;
        using TimePoint = Clock::time_point;
        std::size_t threadOps = Nops / N_THREADS;
        std::vector<std::thread> threads(N_THREADS);
        TimePoint begin;
        TimePoint end{};
        Init();
        for (auto tid = 0u; tid < threads.size(); ++tid) {
            threads.at(tid) = std::thread([tid, threadOps, &begin, &end]() {
                PinThisThreadToCore(tid % N_THREADS);
                ThreadInit();
                //BarrierWait
                if (tid == 0) begin = Clock::now();
                for (auto i = 0u; i < threadOps; ++i) Allocate(ALLOC_SIZE);
                //BarrierWait
                if (tid == 0) end = Clock::now();
            });
        }
        for (auto& t : threads) t.join();
        std::cout << std::chrono::duration_cast<Millis>(end - begin).count() << "\n";
    }

    class Allocator {
    public:
        virtual auto Init() -> void = 0;
        virtual auto ThreadInit() -> void = 0;
        virtual auto AllocateBytes(std::size_t) -> void* = 0;
    };
    class SyncPoolHeapAllocator {
    private:
    public:
        virtual auto Init() -> void = 0;
        virtual auto ThreadInit() -> void = 0;
        virtual auto AllocateBytes(std::size_t) -> void* = 0;
    };

    namespace SyncPoolHeap {
        std::unique_ptr<std::pmr::polymorphic_allocator> Allocator{};
        auto Init() -> void {
            std::pmr::set_default_resource(std::pmr::null_memory_resource());
            std::pmr::synchronized_pool_resource r{ std::pmr::new_delete_resource() };
            std::pmr::polymorphic_allocator a{ &r };
            Allocator = std::move(a);
        }
        auto Allocate(std::size_t bytes) -> void* {
            return Allocator->allocate_bytes(bytes);
        }
    }    // namespace SyncPoolHeap
    namespace SyncPoolMBR {
        auto Init() -> void*;
        auto Allocate(std::size_t bytes) -> void*;
    }    // namespace SyncPoolMBR
    auto FunStuff() -> void {
        using Clock = std::chrono::steady_clock;
        using Millis = std::chrono::milliseconds;
        std::pmr::set_default_resource(std::pmr::null_memory_resource());
        std::pmr::memory_resource* ndr = std::pmr::new_delete_resource();
        std::byte* buffer = new std::byte[ALLOC_BUFFER_SIZE];
        std::pmr::monotonic_buffer_resource mbr{ buffer, sizeof(std::byte) * ALLOC_BUFFER_SIZE };
        std::pmr::memory_resource* res{};
        // std::pmr::monotonic_buffer_resource mbr{ ndr };
        // std::pmr::unsynchronized_pool_resource upr{ ndr };
        std::pmr::unsynchronized_pool_resource upr{ &mbr };
        std::pmr::synchronized_pool_resource spr{ &mbr };
        // res = &mbr;
        res = &upr;
        // res = &spr;
        // res = ndr;
        std::pmr::polymorphic_allocator allocator{ res };
        constexpr double totalMB = (16 * Nops) / (1.0 * Pow(2, 20));
        std::cout << "Total: " << totalMB << " MB\n";
        constexpr double bufferSizeMB = (sizeof(std::byte) * ALLOC_BUFFER_SIZE) / (1.0 * Pow(2, 20));
        std::cout << "BufferSize: " << bufferSizeMB << " MB\n";

        const auto begin = Clock::now();
        for (auto i = 0u; i < Nops; ++i) {
            [[maybe_unused]] void* p = allocator.allocate_bytes(ALLOC_SIZE);
        }
        const auto end = Clock::now();
        std::cout << std::chrono::duration_cast<Millis>(end - begin).count() << " ms\n";
    }

}    // namespace

auto main() -> int {
    FunStuff();
    return 0;
    // ParseArgs(argc, argv);
    // // std::cout << "Persistent Memory Support: " << IsPmem() << "\n";
    // [[maybe_unused]] int err = std::system(("mkdir -p " + std::string{ Config::NVM_DIR }).data());
    // if (err) throw std::system_error{ std::error_code(err, std::system_category()) };

    // Interface::InitPool();
    // AllocBenchmark();
    // Interface::DestroyPool();
}
