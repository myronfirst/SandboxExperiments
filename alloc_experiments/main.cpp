#include <cassert>
#include <cstring>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include <libpmem.h>

#include "interface.h"

namespace {
    enum class BenchOpType { Alloc,
                             Read,
                             Write };
    enum class AllocOpType { None,
                             Block,
                             Sparse };
    struct RNGType {
        using Generator = std::mt19937_64;
        using Distribution = std::uniform_int_distribution<std::size_t>;
        Generator gen;
        Distribution dis;
        RNGType(std::size_t max) : gen{ std::random_device{}() }, dis{ 0, max } {}
        auto Get() -> std::size_t { return dis(gen); }
    };
    std::size_t LognOps;
    std::size_t NThreads;
    AllocOpType AllocOp;
    BenchOpType BenchOp;
    constexpr std::size_t MaxWork = 16;

    RNGType RNG{ MaxWork };
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
        if (argc < 5) {
            std::cout << "Arguments missing\n";
            assert(false);
            std::exit(EXIT_FAILURE);
        }
        NThreads = std::stoul(argv[1]);
        LognOps = std::stoul(argv[2]);
        Nops = Pow10(LognOps);
        AllocOp = ([](const std::string& arg) -> AllocOpType {
            if (arg == "None") return AllocOpType::None;
            if (arg == "Block") return AllocOpType::Block;
            if (arg == "Sparse") return AllocOpType::Sparse;
            throw std::logic_error{ "AllocOp invalid arg" };
        })(argv[3]);
        BenchOp = ([](const std::string& arg) -> BenchOpType {
            if (arg == "Alloc") return BenchOpType::Alloc;
            if (arg == "Read") return BenchOpType::Read;
            if (arg == "Write") return BenchOpType::Write;
            throw std::logic_error{ "BenchOp invalid arg" };
        })(argv[4]);
        if (NThreads <= 0) {
            std::cout << "Invalid NThreads" << NThreads << "\n";
            assert(false);
            std::exit(EXIT_FAILURE);
        }
        if (LognOps <= 0) {
            std::cout << "Invalid LognOps: " << LognOps << "\n";
            assert(false);
            std::exit(EXIT_FAILURE);
        }
        if (BenchOp == BenchOpType::Alloc) assert(AllocOp == AllocOpType::None);
        if (BenchOp == BenchOpType::Read || BenchOp == BenchOpType::Write) assert(AllocOp != AllocOpType::None);
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

    [[maybe_unused]] auto RandomWork() -> void {
        const std::size_t limit = RNG.Get();
        volatile bool dummy{};    // Volatile dependency prevents loop from being optimized-away
        for (std::size_t i = 0; i < limit; ++i)
            dummy = dummy;
    }

    auto Operation(std::vector<Type*>& vec, std::size_t idx) -> void {
        switch (BenchOp) {
            case BenchOpType::Alloc: vec.at(idx) = static_cast<Type*>(Interface::Alloc(TypeSize)); break;
            case BenchOpType::Read: (void)*(vec.at(idx)); break;
            case BenchOpType::Write: *(vec.at(idx)) = Type{}; break;
            default: throw std::logic_error{ "BenchOp default case" };
        }
    }

    auto Benchmark() -> void {
        std::size_t threadOps = Nops / NThreads;
        std::vector<Type*> allocs(Nops);
        std::vector<std::thread> threads(NThreads);
        // std::cout << "allocs capacity byte size: " << allocs.capacity() * sizeof(void*) << "\n";
        if (AllocOp == AllocOpType::Block) {
            Type* block = static_cast<Type*>(Interface::Alloc(Nops * TypeSize));
            for (auto i = 0u; i < allocs.size(); ++i) allocs.at(i) = block + i;
        }
        if (AllocOp == AllocOpType::Sparse) {
            for (auto& ptr : allocs) ptr = static_cast<Type*>(Interface::Alloc(TypeSize));
        }
        const auto begin = Clock::now();
        for (auto tid = 0u; tid < threads.size(); ++tid) {
            const auto threadBegin = tid * threadOps;
            const auto threadEnd = threadBegin + threadOps;
            threads.at(tid) = std::thread([&allocs, tid, threadBegin, threadEnd]() {
                PinThisThreadToCore(tid % NThreads);
                for (auto i = threadBegin; i < threadEnd; ++i) {
                    Operation(allocs, i);
                    // RandomWork();
                }
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
