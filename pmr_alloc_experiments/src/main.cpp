#include <array>
#include <barrier>
#include <chrono>
#include <iostream>
#include <memory>
#include <memory_resource>
#include <thread>

#include <jemalloc/jemalloc.h>
extern "C" {
#include "synch_pool.h"
}

#define PARAM_N_THREADS 96
#define PARAM_ALLOCATOR ArenaBufferAllocator

namespace {
    auto consteval Pow(std::size_t base, std::size_t exp) -> std::size_t { return exp == 0 ? 1 : base * Pow(base, exp - 1); }

    constexpr std::size_t N_THREADS = PARAM_N_THREADS;
    constexpr std::size_t ALLOC_SIZE = 16;
    constexpr std::size_t LOG2N_OPS = 18;
    constexpr std::size_t MAX_BLOCKS_PER_CHUNK = Pow(2, LOG2N_OPS + 1);
    constexpr std::size_t MBR_SIZE = ALLOC_SIZE * MAX_BLOCKS_PER_CHUNK;

#ifdef __cpp_lib_hardware_interference_size
    constexpr std::size_t CACHE_LINE_SIZE = std::hardware_constructive_interference_size;
#else
    constexpr std::size_t CACHE_LINE_SIZE = 64;
#endif

    auto PinThisThreadToCore(std::size_t core) -> void {
        int err;
        err = pthread_setconcurrency(static_cast<int>(std::thread::hardware_concurrency()));
        if (err) throw std::runtime_error{ "pthread_setconcurrency" };
        cpu_set_t cpu_mask;
        CPU_ZERO(&cpu_mask);
        CPU_SET(core, &cpu_mask);
        err = pthread_setaffinity_np(pthread_self(), sizeof(cpu_mask), &cpu_mask);
        if (err) throw std::runtime_error{ "pthread_setaffinity_np" };
    }

    template<typename T>
    struct alignas(CACHE_LINE_SIZE) Aligned { T data; };

    class Allocator {
    public:
        std::string name;
        virtual auto AllocateBytes(std::size_t bytes, std::size_t tid) -> void* = 0;
        virtual ~Allocator() = default;
    };

    class NewDeleteAllocator : public Allocator {
    private:
        std::pmr::polymorphic_allocator<> allocator{ std::pmr::new_delete_resource() };

    public:
        NewDeleteAllocator() { name = "NewDeleteAllocator"; }
        auto AllocateBytes(std::size_t bytes, [[maybe_unused]] std::size_t tid) -> void* override {
            return allocator.allocate_bytes(bytes);
        }
    };

    class SyncPoolHeapAllocator : public Allocator {
    private:
        std::pmr::synchronized_pool_resource pool{ { .max_blocks_per_chunk = MAX_BLOCKS_PER_CHUNK }, std::pmr::new_delete_resource() };
        std::pmr::polymorphic_allocator<> allocator{ &pool };

    public:
        SyncPoolHeapAllocator() {
            name = "SyncPoolHeapAllocator";
            std::cout << "SyncPoolHeapAllocator\n"
                      << "max_blocks_per_chunk: " << pool.options().max_blocks_per_chunk << "\n"
                      << "largest_required_pool_block: " << pool.options().largest_required_pool_block << "\n";
        }
        auto AllocateBytes(std::size_t bytes, [[maybe_unused]] std::size_t tid) -> void* override {
            return allocator.allocate_bytes(bytes);
        }
    };

    class SyncPoolBufferAllocator : public Allocator {
    private:
        std::array<std::byte, MBR_SIZE> buffer{};
        std::pmr::monotonic_buffer_resource mbr{ buffer.data(), buffer.size() * sizeof(std::byte), std::pmr::null_memory_resource() };
        std::pmr::synchronized_pool_resource pool{ { .max_blocks_per_chunk = MAX_BLOCKS_PER_CHUNK }, &mbr };
        std::pmr::polymorphic_allocator<> allocator{ &pool };

    public:
        SyncPoolBufferAllocator() { name = "SyncPoolBufferAllocator"; }
        auto AllocateBytes(std::size_t bytes, [[maybe_unused]] std::size_t tid) -> void* override {
            return allocator.allocate_bytes(bytes);
        }
    };

    class ArenaBufferAllocator : public Allocator {
    private:
        struct Arena {
            std::array<std::byte, MBR_SIZE / N_THREADS> buffer{};
            std::pmr::monotonic_buffer_resource mbr{ buffer.data(), buffer.size() * sizeof(std::byte), std::pmr::null_memory_resource() };
            std::pmr::polymorphic_allocator<> allocator{ &mbr };
        };
        std::array<Aligned<Arena>, N_THREADS> arenas;

    public:
        ArenaBufferAllocator() {
            name = "ArenaBufferAllocator";
        }
        auto AllocateBytes(std::size_t bytes, std::size_t tid) -> void* override {
            return arenas.at(tid).data.allocator.allocate_bytes(bytes);
        }
    };

    class ArenaPoolHeapAllocator : public Allocator {
    private:
        struct Arena {
            std::pmr::unsynchronized_pool_resource pool{ { .max_blocks_per_chunk = MAX_BLOCKS_PER_CHUNK / N_THREADS }, std::pmr::new_delete_resource() };
            std::pmr::polymorphic_allocator<> allocator{ &pool };
        };
        std::array<Aligned<Arena>, N_THREADS> arenas;

    public:
        ArenaPoolHeapAllocator() {
            name = "ThreadPoolHeapAllocator";
        }
        auto AllocateBytes(std::size_t bytes, std::size_t tid) -> void* override {
            return arenas.at(tid).data.allocator.allocate_bytes(bytes);
        }
    };

    class ArenaPoolBufferAllocator : public Allocator {
    private:
        struct Arena {
            std::array<std::byte, MBR_SIZE / N_THREADS> buffer{};
            std::pmr::monotonic_buffer_resource mbr{ buffer.data(), buffer.size() * sizeof(std::byte), std::pmr::null_memory_resource() };
            std::pmr::unsynchronized_pool_resource pool{ { .max_blocks_per_chunk = MAX_BLOCKS_PER_CHUNK / N_THREADS }, &mbr };
            std::pmr::polymorphic_allocator<> allocator{ &pool };
        };
        std::array<Aligned<Arena>, N_THREADS> arenas;

    public:
        ArenaPoolBufferAllocator() {
            name = "ArenaPoolBufferAllocator";
        }
        auto AllocateBytes(std::size_t bytes, std::size_t tid) -> void* override {
            return arenas.at(tid).data.allocator.allocate_bytes(bytes);
        }
    };

    class JemallocAllocator : public Allocator {
    public:
        JemallocAllocator() { name = "JemallocAllocator"; }
        auto AllocateBytes(std::size_t bytes, [[maybe_unused]] std::size_t tid) -> void* override {
            return je_malloc(bytes);
        }
    };

    class SynchPoolAllocator : public Allocator {
    private:
        struct Arena {
            SynchPoolStruct synchPool{};
        };
        std::array<Aligned<Arena>, N_THREADS> arenas;

    public:
        SynchPoolAllocator() {
            name = "SynchPoolAllocator";
            for (auto& arena : arenas) synchInitPool(&(arena.data.synchPool), ALLOC_SIZE);
        }
        ~SynchPoolAllocator() {
            for (auto& arena : arenas) synchDestroyPool(&(arena.data.synchPool));
        }
        auto AllocateBytes([[maybe_unused]] std::size_t bytes, std::size_t tid) -> void* override {
            return synchAllocObj(&(arenas.at(tid).data.synchPool));
        }
    };

    auto Benchmark(Allocator* allocator) -> void {
        using Clock = std::chrono::steady_clock;
        using Nano = std::chrono::nanoseconds;
        using TimePoint = Clock::time_point;
        std::array<std::thread, N_THREADS> threads{};
        TimePoint begin{};
        TimePoint end{};
        std::barrier clockBarrier{ N_THREADS };

        for (auto tid = 0u; tid < threads.size(); ++tid) {
            threads.at(tid) = std::thread([tid, allocator, &begin, &end, &clockBarrier]() {
                PinThisThreadToCore(tid % N_THREADS);
                constexpr std::size_t threadOps = Pow(2, LOG2N_OPS) / N_THREADS;
                clockBarrier.arrive_and_wait();
                if (tid == 0) begin = Clock::now();
                for (auto i = 0u; i < threadOps; ++i) {
                    char* volatile p = static_cast<char* volatile>(allocator->AllocateBytes(16, tid));
                    *p = 1;
                    p = static_cast<char* volatile>(allocator->AllocateBytes(64, tid));
                    *p = 1;
                    p = static_cast<char* volatile>(allocator->AllocateBytes(128, tid));
                    *p = 1;
                    p = static_cast<char* volatile>(allocator->AllocateBytes(256, tid));
                    *p = 1;
                }
                clockBarrier.arrive_and_wait();
                if (tid == 0) end = Clock::now();
            });
        }
        for (auto& t : threads) t.join();
        std::cout << allocator->name << std::endl
                  << std::chrono::duration_cast<Nano>(end - begin).count() << " ns\n";
    }
}    // namespace

auto main() -> int {
    std::unique_ptr<Allocator> allocator{};
#ifdef PARAM_ALLOCATOR
    allocator = std::make_unique<PARAM_ALLOCATOR>();
#else
#error "PARAM_ALLOCATOR not defined"
#endif
    std::pmr::set_default_resource(std::pmr::null_memory_resource());
    Benchmark(allocator.get());
}
