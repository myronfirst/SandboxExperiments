#include <jemalloc/jemalloc.h>
#include <array>
#include <barrier>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <memory>
#include <memory_resource>
#include <new>
#include <thread>
#include <vector>
extern "C" {
#include <pool.h>
}

#undef CACHE_LINE_SIZE
namespace {
    std::size_t threadsNum;
    constexpr std::size_t N_THREADS = 96;
    constexpr std::size_t LOG2N_OPS = 18;
    constexpr std::size_t ALLOC_SIZE = 16;
    constexpr std::size_t MBR_SIZE = 1u * 1024 * 1024 * 256;
    std::pmr::pool_options opts{ .max_blocks_per_chunk = 1024 * 1024 * 4 };

    auto consteval Pow(std::size_t base, std::size_t exp) -> std::size_t { return exp == 0 ? 1 : base * Pow(base, exp - 1); }
    constexpr std::size_t Nops = Pow(2, LOG2N_OPS);
    std::size_t ThreadOps;
    constexpr std::size_t MaxThreadOps = Pow(2, 14);
    constexpr std::size_t MaxAllocSize = Pow(2, 12);
    // static_assert(ThreadOps <= MaxThreadOps);
    static_assert(ALLOC_SIZE <= MaxAllocSize);

#ifdef __cpp_lib_hardware_interference_size
    constexpr std::size_t CACHE_LINE_SIZE = std::hardware_constructive_interference_size;
#else
    constexpr std::size_t CACHE_LINE_SIZE = 64;
#endif

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
        auto AllocateBytes(std::size_t bytes, [[maybe_unused]] std::size_t tid) -> void* override { return allocator.allocate_bytes(bytes); }
    };

    class SharedPoolHeapAllocator : public Allocator {
    private:
        std::pmr::synchronized_pool_resource pool{ opts, std::pmr::new_delete_resource() };
        std::pmr::polymorphic_allocator<> allocator{ &pool };

    public:
        SharedPoolHeapAllocator() { name = "SharedPoolHeapAllocator"; }
        auto AllocateBytes(std::size_t bytes, [[maybe_unused]] std::size_t tid) -> void* override {
            // const auto opts = pool.options();
            // std::cout << "max_blocks_per_chunk: " << opts.max_blocks_per_chunk << "\n";
            // std::cout << "largest_required_pool_block: " << opts.largest_required_pool_block << "\n";
            return allocator.allocate_bytes(bytes);
        }
    };

    class SharedPoolBufferAllocator : public Allocator {
    private:
        std::array<std::byte, MBR_SIZE> buffer{};
        std::pmr::monotonic_buffer_resource mbr{ buffer.data(), buffer.size() * sizeof(std::byte) };
        std::pmr::synchronized_pool_resource pool{ opts, &mbr };
        std::pmr::polymorphic_allocator<> allocator{ &pool };

    public:
        SharedPoolBufferAllocator() { name = "SharedPoolBufferAllocator"; }
        auto AllocateBytes(std::size_t bytes, [[maybe_unused]] std::size_t tid) -> void* override { return allocator.allocate_bytes(bytes); }
    };

    class ThreadPoolHeapAllocator : public Allocator {
    private:
        struct alignas(CACHE_LINE_SIZE) Element {
            std::pmr::unsynchronized_pool_resource pool{ opts, std::pmr::new_delete_resource() };
            std::pmr::polymorphic_allocator<> allocator{ &pool };
        };
        // std::array<Element, N_THREADS> allocators;
        std::unique_ptr<Element[]> allocators;

    public:
        ThreadPoolHeapAllocator() {
            allocators = std::make_unique<Element[]>(threadsNum);
            name = "ThreadPoolHeapAllocator";
        }
        auto AllocateBytes(std::size_t bytes, std::size_t tid) -> void* override { return allocators[tid].allocator.allocate_bytes(bytes); }
    };

    class ThreadPoolBufferAllocator : public Allocator {
    private:
        struct alignas(CACHE_LINE_SIZE) Element {
            std::unique_ptr<std::byte[]> buffer;
            std::pmr::monotonic_buffer_resource mbr;
            std::pmr::unsynchronized_pool_resource pool;
            std::pmr::polymorphic_allocator<> allocator;
            Element() : buffer{ std::make_unique<std::byte[]>(MBR_SIZE / threadsNum) }, mbr{ buffer.get(), MBR_SIZE / threadsNum }, pool{ opts, &mbr }, allocator{ &pool } {}
        };
        std::unique_ptr<Element[]> allocators;

    public:
        ThreadPoolBufferAllocator() {
            allocators = std::make_unique<Element[]>(threadsNum);
            name = "ThreadPoolBufferAllocator";
        }
        auto AllocateBytes(std::size_t bytes, std::size_t tid) -> void* override { return allocators[tid].allocator.allocate_bytes(bytes); }
    };

    class ThreadBufferAllocator : public Allocator {
    private:
        struct alignas(CACHE_LINE_SIZE) Element {
            // std::array<std::byte, MBR_SIZE / N_THREADS> buffer{};
            std::unique_ptr<std::byte[]> buffer;
            std::pmr::monotonic_buffer_resource mbr;
            std::pmr::polymorphic_allocator<> allocator;
            Element() : buffer{ std::make_unique<std::byte[]>(MBR_SIZE / threadsNum) },
                        mbr{ buffer.get(), MBR_SIZE / threadsNum },
                        allocator(&mbr) {}
        };
        // std::array<Element, N_THREADS> allocators;
        std::unique_ptr<Element[]> allocators;

    public:
        ThreadBufferAllocator() {
            allocators = std::make_unique<Element[]>(threadsNum);
            name = "ThreadBufferAllocator";
        }
        auto AllocateBytes(std::size_t bytes, std::size_t tid) -> void* override { return allocators[tid].allocator.allocate_bytes(bytes); }
    };

    class JemallocAllocator : public Allocator {
    public:
        JemallocAllocator() { name = "JemallocAllocator"; }
        auto AllocateBytes(std::size_t bytes, [[maybe_unused]] std::size_t tid) -> void* override { return je_malloc(bytes); }
    };

    class PoolAllocator : public Allocator {
    private:
        std::unique_ptr<SynchPoolStruct[]> pools CACHE_ALIGN;

    public:
        PoolAllocator() {
            pools = std::make_unique<SynchPoolStruct[]>(threadsNum);
            for (int i = 0; i < threadsNum; i++) synchInitPool(&(pools[i]), ALLOC_SIZE);
            name = "PoolAllocator";
        }
        auto AllocateBytes([[maybe_unused]] std::size_t bytes, std::size_t tid) -> void* override { return synchAllocObj(&(pools[tid])); }
    };

    auto Benchmark(Allocator* allocator, int numberOfThreads) -> void {
        using Clock = std::chrono::steady_clock;
        using Nano = std::chrono::nanoseconds;
        using TimePoint = Clock::time_point;
        std::vector<std::thread> threads(numberOfThreads);
        TimePoint begin{};
        TimePoint end{};
        std::barrier clockBarrier{ numberOfThreads };

        for (auto tid = 0u; tid < threads.size(); ++tid) {
            threads.at(tid) = std::thread([tid, allocator, numberOfThreads, &begin, &end, &clockBarrier]() {
                PinThisThreadToCore(tid % 96);
                clockBarrier.arrive_and_wait();
                if (tid == 0) begin = Clock::now();
                for (auto i = 0u; i < ThreadOps; ++i) {
                    char * volatile p = static_cast<char*volatile>(allocator->AllocateBytes(16, tid));
                    *p = 1;
                    p = static_cast<char*volatile>(allocator->AllocateBytes(64, tid));
                    *p = 1;
                    p = static_cast<char*volatile>(allocator->AllocateBytes(128, tid));
                    *p = 1;
                    p = static_cast<char*volatile>(allocator->AllocateBytes(256, tid));
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

    auto help(std::string prog) -> void {
        std::cout << prog << " <Allocator> <Threads>\n";
        std::cout << "Allocator should be a number between 0 and 8\n";
    }
}    // namespace

enum {
    ALLOCATOR = 1,
    THREADS
};

auto main(int argc, char* argv[]) -> int {
    if (argc < 3) {
        //     std::pmr::synchronized_pool_resource pool{opts};
        //     std::pmr::polymorphic_allocator alloc{&pool};
        //     std::cout << pool.options().max_blocks_per_chunk << '\n';
        //     std::size_t a = (std::size_t)alloc.allocate_bytes(1);
        //     std::size_t b = (std::size_t)alloc.allocate_bytes(1);
        //     std::size_t c = b-a;
        //     std::cout << c << '\n';
        help(argv[0]);
        return EXIT_FAILURE;
    }

    auto allocator = std::stoi(argv[ALLOCATOR]);
    threadsNum = std::stoi(argv[THREADS]);

    ThreadOps = Nops / threadsNum;

    std::pmr::set_default_resource(std::pmr::null_memory_resource());

    std::vector<std::unique_ptr<Allocator>> allocators;
    allocators.push_back(std::make_unique<NewDeleteAllocator>());
    allocators.push_back(std::make_unique<SharedPoolHeapAllocator>());
    allocators.push_back(std::make_unique<SharedPoolBufferAllocator>());
    allocators.push_back(std::make_unique<ThreadPoolHeapAllocator>());
    allocators.push_back(std::make_unique<ThreadPoolBufferAllocator>());
    allocators.push_back(std::make_unique<ThreadBufferAllocator>());
    allocators.push_back(std::make_unique<JemallocAllocator>());
    allocators.push_back(std::make_unique<PoolAllocator>());

    if (allocator > allocators.size() - 1) return EXIT_FAILURE;

    Benchmark(allocators.at(allocator).get(), threadsNum);

    // std::unique_ptr<Allocator> allocator{};
    // allocator = std::make_unique<NewDeleteAllocator>();
    // Benchmark(allocator.get());
    // allocator = std::make_unique<SharedPoolHeapAllocator>();
    // Benchmark(allocator.get());
    // allocator = std::make_unique<SharedPoolBufferAllocator>();
    // Benchmark(allocator.get());
    // allocator = std::make_unique<ThreadPoolHeapAllocator>();
    // Benchmark(allocator.get());
    // allocator = std::make_unique<ThreadPoolBufferAllocator>();
    // Benchmark(allocator.get());
    // allocator = std::make_unique<ThreadBufferAllocator>();
    // Benchmark(allocator.get());
}
