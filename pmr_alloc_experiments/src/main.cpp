#include <array>
#include <barrier>
#include <chrono>
#include <fstream>
#include <iostream>
#include <memory_resource>
#include <numeric>
#include <thread>

#include <jemalloc/jemalloc.h>
#include "synch_pool.h"

#ifndef PARAM_N_THREADS
#define PARAM_N_THREADS 8
#endif
#ifndef PARAM_ALLOC_SIZES
#define PARAM_ALLOC_SIZES \
    { 16 }
// { 64, 128, 256 }
// { 16, 32, 64, 128, 256 }
#endif
#ifndef PARAM_ALLOCATOR
#define PARAM_ALLOCATOR ArenaPoolBuffer
#endif
#ifndef PARAM_ENABLE_DEALLOCATE
#define PARAM_ENABLE_DEALLOCATE false
#endif

namespace {
    auto consteval Pow(std::size_t base, std::size_t exp) -> std::size_t { return exp == 0 ? 1 : base * Pow(base, exp - 1); }
    auto consteval Sum(auto&& container) { return std::accumulate(std::begin(container), std::end(container), 0UL); }

    constexpr std::size_t N_THREADS = PARAM_N_THREADS;
    constexpr auto ALLOC_SIZES = std::to_array<std::size_t>(PARAM_ALLOC_SIZES);
    constexpr bool ENABLE_DEALLOCATE = PARAM_ENABLE_DEALLOCATE;
    constexpr std::size_t LOG2N_OPS = 24;

    constexpr std::size_t MAX_BLOCKS_PER_CHUNK = Pow(2, LOG2N_OPS + 1);
    constexpr std::size_t MBR_SIZE = Sum(ALLOC_SIZES) * MAX_BLOCKS_PER_CHUNK;

#ifdef __cpp_lib_hardware_interference_size
    constexpr std::size_t CACHE_LINE_SIZE = std::hardware_constructive_interference_size;
#else
    constexpr std::size_t CACHE_LINE_SIZE = 64;
#endif

    // auto& DStream = std::cout;
    auto DStream = std::ofstream{ "/dev/null", std::ios_base::app };

    auto PinThisThreadToCore(std::size_t core) -> void {
        int err{};
        err = pthread_setconcurrency(static_cast<int>(std::thread::hardware_concurrency()));
        if (err != 0) throw std::runtime_error{ "pthread_setconcurrency" };
        cpu_set_t cpu_mask;
        CPU_ZERO(&cpu_mask);
        CPU_SET(core, &cpu_mask);
        err = pthread_setaffinity_np(pthread_self(), sizeof(cpu_mask), &cpu_mask);
        if (err != 0) throw std::runtime_error{ "pthread_setaffinity_np" };
    }

    template<typename T>
    struct alignas(CACHE_LINE_SIZE) Aligned { T data; };

    class NewDelete {
    private:
        std::pmr::polymorphic_allocator<> allocator{ std::pmr::new_delete_resource() };

    public:
        auto AllocateBytes(std::size_t bytes, [[maybe_unused]] std::size_t tid) -> void* {
            return allocator.allocate_bytes(bytes);
        }
        auto DeallocateBytes(void* p, std::size_t bytes, [[maybe_unused]] std::size_t tid) -> void {
            allocator.deallocate_bytes(p, bytes);
        }
    };

    class SyncPoolHeap {
    private:
        std::pmr::synchronized_pool_resource pool{ { .max_blocks_per_chunk = MAX_BLOCKS_PER_CHUNK }, std::pmr::new_delete_resource() };
        std::pmr::polymorphic_allocator<> allocator{ &pool };

    public:
        SyncPoolHeap() {
            DStream << "SyncPoolHeapAllocator\n"
                    << "max_blocks_per_chunk: " << pool.options().max_blocks_per_chunk << "\n";
        }
        auto AllocateBytes(std::size_t bytes, [[maybe_unused]] std::size_t tid) -> void* {
            return allocator.allocate_bytes(bytes);
        }
        auto DeallocateBytes(void* p, std::size_t bytes, [[maybe_unused]] std::size_t tid) -> void {
            allocator.deallocate_bytes(p, bytes);
        }
    };

    class SyncPoolBuffer {
    private:
        std::array<std::byte, MBR_SIZE> buffer{};
        std::pmr::monotonic_buffer_resource mbr{ buffer.data(), buffer.size() * sizeof(std::byte), std::pmr::null_memory_resource() };
        std::pmr::synchronized_pool_resource pool{ { .max_blocks_per_chunk = MAX_BLOCKS_PER_CHUNK }, &mbr };
        std::pmr::polymorphic_allocator<> allocator{ &pool };

    public:
        SyncPoolBuffer() {
            DStream << "SyncPoolBufferAllocator\n"
                    << "mbr size: " << buffer.size() << "\n"
                    << "max_blocks_per_chunk: " << pool.options().max_blocks_per_chunk << "\n";
        }
        auto AllocateBytes(std::size_t bytes, [[maybe_unused]] std::size_t tid) -> void* {
            return allocator.allocate_bytes(bytes);
        }
        auto DeallocateBytes(void* p, std::size_t bytes, [[maybe_unused]] std::size_t tid) -> void {
            allocator.deallocate_bytes(p, bytes);
        }
    };

    class DebugResource : public std::pmr::memory_resource {
    public:
        DebugResource(std::pmr::memory_resource* r) : resource{ r } {}

    private:
        std::pmr::memory_resource* resource{};
        auto do_allocate(std::size_t bytes, std::size_t alignment = alignof(std::max_align_t)) -> void* override {
            DStream << "DebugResource::Allocate bytes: " << bytes << "\n";
            return resource->allocate(bytes, alignment);
        }
        auto do_deallocate(void* p, std::size_t bytes, std::size_t alignment = alignof(std::max_align_t)) -> void override {
            DStream << "DebugResource::Allocate p: " << p << " bytes: " << bytes << "\n";
            return resource->deallocate(p, bytes, alignment);
        }
        auto do_is_equal(const std::pmr::memory_resource& other) const noexcept -> bool override { return this == &other; }
    };

    class ArenaBuffer {
    private:
        struct Arena {
            std::array<std::byte, MBR_SIZE / N_THREADS> buffer{};
            std::pmr::monotonic_buffer_resource mbr{ buffer.data(), buffer.size() * sizeof(std::byte), std::pmr::null_memory_resource() };
            std::pmr::polymorphic_allocator<> allocator{ &mbr };
        };
        std::array<Aligned<Arena>, N_THREADS> arenas;

    public:
        ArenaBuffer() {
            DStream << "ArenaBufferAllocator\n"
                    << "mbr size: " << arenas.at(0).data.buffer.size() << "\n";
        }
        auto AllocateBytes(std::size_t bytes, std::size_t tid) -> void* {
            return arenas.at(tid).data.allocator.allocate_bytes(bytes);
        }
        auto DeallocateBytes(void* p, std::size_t bytes, std::size_t tid) -> void {
            arenas.at(tid).data.allocator.deallocate_bytes(p, bytes);
        }
    };

    class ArenaPoolHeap {
    private:
        struct Arena {
            std::pmr::unsynchronized_pool_resource pool{ { .max_blocks_per_chunk = MAX_BLOCKS_PER_CHUNK / N_THREADS }, std::pmr::new_delete_resource() };
            std::pmr::polymorphic_allocator<> allocator{ &pool };
        };
        std::array<Aligned<Arena>, N_THREADS> arenas;

    public:
        ArenaPoolHeap() {
            DStream << "ArenaPoolHeapAllocator\n"
                    << "max_blocks_per_chunk: " << arenas.at(0).data.pool.options().max_blocks_per_chunk << "\n";
        }
        auto AllocateBytes(std::size_t bytes, std::size_t tid) -> void* {
            return arenas.at(tid).data.allocator.allocate_bytes(bytes);
        }
        auto DeallocateBytes(void* p, std::size_t bytes, std::size_t tid) -> void {
            arenas.at(tid).data.allocator.deallocate_bytes(p, bytes);
        }
    };

    class ArenaPoolBuffer {
    private:
        struct Arena {
            std::array<std::byte, MBR_SIZE / N_THREADS> buffer{};
            std::pmr::monotonic_buffer_resource mbr{ buffer.data(), buffer.size() * sizeof(std::byte), std::pmr::null_memory_resource() };
            std::pmr::unsynchronized_pool_resource pool{ { .max_blocks_per_chunk = MAX_BLOCKS_PER_CHUNK / N_THREADS }, &mbr };
            std::pmr::polymorphic_allocator<> allocator{ &pool };
        };
        std::array<Aligned<Arena>, N_THREADS> arenas;

    public:
        ArenaPoolBuffer() {
            DStream << "ArenaPoolBufferAllocator\n"
                    << "mbr size: " << arenas.at(0).data.buffer.size() << "\n"
                    << "max_blocks_per_chunk: " << arenas.at(0).data.pool.options().max_blocks_per_chunk << "\n";
        }
        auto AllocateBytes(std::size_t bytes, std::size_t tid) -> void* {
            return arenas.at(tid).data.allocator.allocate_bytes(bytes);
        }
        auto DeallocateBytes(void* p, std::size_t bytes, std::size_t tid) -> void {
            arenas.at(tid).data.allocator.deallocate_bytes(p, bytes);
        }
    };

    class JeMalloc {
    public:
        static auto AllocateBytes(std::size_t bytes, [[maybe_unused]] std::size_t tid) -> void* {
            return je_malloc(bytes);
        }
        static auto DeallocateBytes(void* p, [[maybe_unused]] std::size_t bytes, [[maybe_unused]] std::size_t tid) -> void {
            je_free(p);
        }
    };

    class SynchPool {
    private:
        struct Arena {
            std::array<SynchPoolStruct, ALLOC_SIZES.size()> synchPools{};
        };
        std::array<Aligned<Arena>, N_THREADS> arenas;

        static auto Idx(std::size_t val) -> std::size_t {
            for (auto i = 0U; i < ALLOC_SIZES.size(); ++i)
                if (val == ALLOC_SIZES.at(i)) return i;
            throw(std::runtime_error{ "Idx" });
        }

    public:
        SynchPool(const SynchPool&) = default;
        SynchPool(SynchPool&&) = default;
        auto operator=(const SynchPool&) -> SynchPool& = default;
        auto operator=(SynchPool&&) -> SynchPool& = default;

        SynchPool() {
            for (auto& arena : arenas)
                for (auto i = 0U; i < arena.data.synchPools.size(); ++i)
                    synchInitPool(&(arena.data.synchPools.at(i)), static_cast<std::uint32_t>(ALLOC_SIZES.at(i)));
            const auto entries = arenas.at(0).data.synchPools.at(0).entries_per_block;
            const auto obj_size = arenas.at(0).data.synchPools.at(0).obj_size;
            DStream << "SynchPoolAllocator\n"
                    << "block size: " << entries * obj_size << "\n";
        }
        ~SynchPool() {
            for (auto& arena : arenas)
                for (auto& synchPool : arena.data.synchPools)
                    synchDestroyPool(&synchPool);
        }
        auto AllocateBytes(std::size_t bytes, std::size_t tid) -> void* {
            return synchAllocObj(&(arenas.at(tid).data.synchPools.at(Idx(bytes))));
        }
        auto DeallocateBytes(void* p, std::size_t bytes, std::size_t tid) -> void {
            synchRecycleObj(&(arenas.at(tid).data.synchPools.at(Idx(bytes))), p);
        };
    };
    auto Benchmark(auto* allocator) -> void {
        using Clock = std::chrono::steady_clock;
        using Unit = std::chrono::milliseconds;
        using TimePoint = Clock::time_point;
        constexpr std::size_t NOps = Pow(2, LOG2N_OPS);
        constexpr std::size_t threadOps = NOps / N_THREADS;
        std::array<std::thread, N_THREADS> threads{};
        TimePoint begin{};
        TimePoint end{};
        std::barrier clockBarrier{ N_THREADS };

        for (auto tid = 0U; tid < threads.size(); ++tid) {
            threads.at(tid) = std::thread([tid, allocator, &begin, &end, &clockBarrier]() {
                PinThisThreadToCore(tid % N_THREADS);
                clockBarrier.arrive_and_wait();
                if (tid == 0) begin = Clock::now();
                for (auto i = 0U; i < threadOps; ++i) {
                    const auto allocSize = ALLOC_SIZES.at(i % ALLOC_SIZES.size());
                    void* p = allocator->AllocateBytes(allocSize, tid);
                    *static_cast<char*>(p) = 1;
                    if constexpr (ENABLE_DEALLOCATE)
                        allocator->DeallocateBytes(p, allocSize, tid);
                }
                clockBarrier.arrive_and_wait();
                if (tid == 0) end = Clock::now();
            });
        }
        for (auto& t : threads) t.join();
        std::cout << std::chrono::duration_cast<Unit>(end - begin).count() << "\n"
                  << NOps << "\n";
    }
}    // namespace

auto main() -> int {
#ifdef PARAM_ALLOCATOR
    std::unique_ptr allocator = std::make_unique<PARAM_ALLOCATOR>();
#else
#error "PARAM_ALLOCATOR not defined"
#endif
    std::pmr::set_default_resource(std::pmr::null_memory_resource());
    Benchmark(allocator.get());
}
