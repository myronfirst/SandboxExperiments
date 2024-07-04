#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <libpmem.h>

#include "interface.h"

namespace {
    std::size_t LOGN_OPS = 7;
    std::size_t N_THREADS = std::thread::hardware_concurrency();
    enum class BenchOpType { Alloc,
                             Read,
                             Write };
    BenchOpType BenchOp = BenchOpType::Alloc;
    std::size_t Nops;

    [[maybe_unused]] auto IsPmem() -> bool {
        const std::string tmpPath = std::string{ Config::NVM_DIR } + "tmp_pool_tmp";
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
        if (argc >= 4) {
            BenchOp = ([](const std::string& arg) -> BenchOpType {
                if (arg == "Alloc") return BenchOpType::Alloc;
                if (arg == "Read") return BenchOpType::Read;
                if (arg == "Write") return BenchOpType::Write;
                throw std::logic_error{ "BenchOp invalid arg" };
            })(argv[3]);
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

    using Clock = std::chrono::steady_clock;
    using Millis = std::chrono::milliseconds;
    struct Type {
        char data[Config::ALLOC_SIZE]{};
    };
    inline constexpr std::size_t TypeSize = sizeof(Type);

    auto Operation(std::vector<Type*>& vec, std::size_t idx) -> void {
        switch (BenchOp) {
            case BenchOpType::Alloc: vec.at(idx) = static_cast<Type*>(Interface::Alloc(TypeSize)); break;
            case BenchOpType::Read: (void)*(vec.at(idx)); break;
            case BenchOpType::Write: *(vec.at(idx)) = Type{}; break;
            default: throw std::logic_error{ "BenchOp default case" };
        }
    }

    auto Benchmark() -> void {
        std::size_t threadOps = Nops / N_THREADS;
        std::vector<Type*> allocs(Nops);
        std::vector<std::thread> threads(N_THREADS);
        // std::cout << "allocs capacity byte size: " << allocs.capacity() * sizeof(void*) << "\n";
        if (BenchOp != BenchOpType::Alloc) {
            for (auto& ptr : allocs)
                ptr = static_cast<Type*>(Interface::Alloc(TypeSize));
        }

        const auto begin = Clock::now();
        for (auto tid = 0u; tid < threads.size(); ++tid) {
            const auto threadBegin = tid * threadOps;
            const auto threadEnd = threadBegin + threadOps;
            threads.at(tid) = std::thread([&allocs, tid, threadBegin, threadEnd]() {
                PinThisThreadToCore(tid % N_THREADS);
                for (auto i = threadBegin; i < threadEnd; ++i)
                    Operation(allocs, i);
            });
        }
        for (auto& t : threads) t.join();
        const auto end = Clock::now();
        std::cout << std::chrono::duration_cast<Millis>(end - begin).count() << "\n";
    }

}    // namespace

auto main(int argc, char* argv[]) -> int {
    ParseArgs(argc, argv);
    // std::cout << "Persistent Memory Support: " << IsPmem() << "\n";
    [[maybe_unused]] int err = std::system(("mkdir -p " + std::string{ Config::NVM_DIR }).data());
    if (err) throw std::system_error{ std::error_code(err, std::system_category()) };

    Interface::InitPool();
    Benchmark();
    Interface::DestroyPool();
}
