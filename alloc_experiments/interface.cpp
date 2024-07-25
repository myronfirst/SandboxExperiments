#include "interface.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#include <jemalloc/jemalloc.h>
#include <cstdlib>

#include <libvmem.h>

#include <memkind.h>

#include <libpmemobj.h>

// #include <libpmemobj++/make_persistent_array_atomic.hpp>
#include <libpmemobj++/make_persistent_atomic.hpp>
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/pool.hpp>

#include <libpmem.h>

namespace Default {
    [[maybe_unused]] auto InitPool() -> void {}
    [[maybe_unused]] auto DestroyPool() -> void {}
    [[maybe_unused]] auto Alloc(std::size_t sz) -> void* { return operator new(sz); }
}    // namespace Default

namespace Malloc {
    [[maybe_unused]] auto InitPool() -> void {}
    [[maybe_unused]] auto DestroyPool() -> void {}
    [[maybe_unused]] auto Alloc(std::size_t sz) -> void* {
        if (void* ptr = std::malloc(sz)) return ptr;
        throw std::bad_alloc{};
    }
}    // namespace Malloc

namespace JeMalloc {
    [[maybe_unused]] auto InitPool() -> void {}
    [[maybe_unused]] auto DestroyPool() -> void {}
    [[maybe_unused]] auto Alloc(std::size_t sz) -> void* {
        if (void* ptr = std::malloc(sz)) return ptr;
        throw std::bad_alloc{};
    }
}    // namespace JeMalloc

namespace Vmem {
    VMEM* Pool{};
    [[maybe_unused]] auto InitPool() -> void {
        Pool = vmem_create(Config::NVM_DIR.data(), Config::POOL_SIZE);
        if (Pool == nullptr) throw std::runtime_error{ "vmem_create" };
    }
    [[maybe_unused]] auto DestroyPool() -> void {
        vmem_delete(Pool);
    }
    [[maybe_unused]] auto Alloc(std::size_t sz) -> void* {
        if (void* ptr = vmem_malloc(Pool, sz)) return ptr;
        throw std::bad_alloc{};
    }
}    // namespace Vmem

namespace Vmmalloc {
    [[maybe_unused]] auto InitPool() -> void {}
    [[maybe_unused]] auto DestroyPool() -> void {}
    [[maybe_unused]] auto Alloc(std::size_t sz) -> void* {
        if (void* ptr = std::malloc(sz)) return ptr;
        throw std::bad_alloc{};
    }
}    // namespace Vmmalloc

namespace Memkind {
    memkind_t Pool{};
    [[maybe_unused]] auto InitPool() -> void {
        int ok = memkind_create_pmem(Config::NVM_DIR.data(), Config::POOL_SIZE, &Pool);
        if (ok != MEMKIND_SUCCESS) throw std::runtime_error{ "memkind_create_pmem" };
    }
    [[maybe_unused]] auto DestroyPool() -> void {
        int ok = memkind_destroy_kind(Pool);
        if (ok != MEMKIND_SUCCESS) throw std::runtime_error{ "memkind_destroy_kind" };
    }
    [[maybe_unused]] auto Alloc(std::size_t sz) -> void* {
        if (void* ptr = memkind_malloc(Pool, sz)) return ptr;
        throw std::bad_alloc{};
    }
}    // namespace Memkind

namespace PmemobjAlloc {
    const std::string PoolPath = std::string{ Config::NVM_DIR } + "/pmemobj_alloc";
    PMEMobjpool* Pool{};
    [[maybe_unused]] auto InitPool() -> void {
        const std::string poolLayout = std::filesystem::path{ PoolPath }.filename().string();
        if (!std::filesystem::exists(PoolPath.data())) {
            Pool = pmemobj_create(PoolPath.data(), poolLayout.data(), Config::POOL_SIZE, 0666);
            pmemobj_close(Pool);
        }
        int ok = pmemobj_check(PoolPath.data(), poolLayout.data());
        if (!ok) throw std::runtime_error{ "pmemobj_check" };
        Pool = pmemobj_open(PoolPath.data(), poolLayout.data());
        if (!Pool) throw std::runtime_error{ "pmemobj_open" };
    }
    [[maybe_unused]] auto DestroyPool() -> void {
        pmemobj_close(Pool);
        [[maybe_unused]] int err = std::system(("rm " + std::string{ PoolPath }).data());
        if (err) throw std::system_error{ std::error_code(err, std::system_category()) };
    }

    [[maybe_unused]] auto Alloc(std::size_t sz) -> void* {
        //Allocating below 64B is inefficient
        PMEMoid ptr;
        int err = pmemobj_alloc(Pool, &ptr, sizeof(char) * sz, 0, nullptr, nullptr);
        if (err) throw std::bad_alloc{};
        return pmemobj_direct(ptr);
    }
}    // namespace PmemobjAlloc

namespace MakePersistentAtomic {
    const std::string PoolPath = std::string{ Config::NVM_DIR } + "/make_persistent_atomic";
    pmem::obj::pool_base Pool{};
    [[maybe_unused]] auto InitPool() -> void {
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
    [[maybe_unused]] auto DestroyPool() -> void {
        Pool.close();
        [[maybe_unused]] int err = std::system(("rm " + PoolPath).data());
        if (err) throw std::system_error{ std::error_code(err, std::system_category()) };
    }
    [[maybe_unused]] auto Alloc(std::size_t sz) -> void* {
        //Allocating below 64B is inefficient
        (void)sz;
        pmem::obj::persistent_ptr<Type> ptr{};
        pmem::obj::make_persistent_atomic<Type>(Pool, ptr);
        return ptr.get();
    }
}    // namespace MakePersistentAtomic

namespace Pmem {
    const std::string PoolPath = std::string{ Config::NVM_DIR } + "/pmem";
    char* PmemAddr{};
    std::size_t MappedLen{};
    char* AllocIdx{};
    [[maybe_unused]] auto InitPool() -> void {
        int isPmem;
        PmemAddr = static_cast<char*>(pmem_map_file(PoolPath.data(), Config::POOL_SIZE, PMEM_FILE_CREATE, 0666, &MappedLen, &isPmem));
        if (!PmemAddr) throw std::runtime_error{ "pmem_map_file" };
        if (!isPmem) throw std::runtime_error{ "pmem_map_file" };
        AllocIdx = PmemAddr;
    }
    [[maybe_unused]] auto DestroyPool() -> void {
        [[maybe_unused]] int err = pmem_unmap(PmemAddr, MappedLen);
        if (err) throw std::runtime_error{ "pmem_unmap" };
        err = std::system(("rm " + PoolPath).data());
        if (err) throw std::system_error{ std::error_code(err, std::system_category()) };
        AllocIdx = nullptr;
    }
    [[maybe_unused]] auto Alloc(std::size_t sz) -> void* {
        if (PmemAddr + MappedLen <= AllocIdx + sz) throw std::bad_alloc{};
        void* ptr = AllocIdx;
        AllocIdx += sz;
        return ptr;
    }
}    // namespace Pmem

namespace Interface {
#ifdef BENCH_DEFAULT
    auto InitPool() -> void { Default::InitPool(); }
    auto DestroyPool() -> void { Default::DestroyPool(); }
    auto Alloc(std::size_t sz) -> void* { return Default::Alloc(sz); }
#endif
#ifdef BENCH_MALLOC
    auto InitPool() -> void { Malloc::InitPool(); }
    auto DestroyPool() -> void { Malloc::DestroyPool(); }
    auto Alloc(std::size_t sz) -> void* { return Malloc::Alloc(sz); }
#endif
#ifdef BENCH_JEMALLOC
    auto InitPool() -> void { JeMalloc::InitPool(); }
    auto DestroyPool() -> void { JeMalloc::DestroyPool(); }
    auto Alloc(std::size_t sz) -> void* { return JeMalloc::Alloc(sz); }
#endif
#ifdef BENCH_VMEM
    auto InitPool() -> void { Vmem::InitPool(); }
    auto DestroyPool() -> void { Vmem::DestroyPool(); }
    auto Alloc(std::size_t sz) -> void* { return Vmem::Alloc(sz); }
#endif
#ifdef BENCH_VMMALLOC
    auto InitPool() -> void { Vmmalloc::InitPool(); }
    auto DestroyPool() -> void { Vmmalloc::DestroyPool(); }
    auto Alloc(std::size_t sz) -> void* { return Vmmalloc::Alloc(sz); }
#endif
#ifdef BENCH_MEMKIND
    auto InitPool() -> void { Memkind::InitPool(); }
    auto DestroyPool() -> void { Memkind::DestroyPool(); }
    auto Alloc(std::size_t sz) -> void* { return Memkind::Alloc(sz); }
#endif
#ifdef BENCH_PMEMOBJ_ALLOC
    auto InitPool() -> void { PmemobjAlloc::InitPool(); }
    auto DestroyPool() -> void { PmemobjAlloc::DestroyPool(); }
    auto Alloc(std::size_t sz) -> void* { return PmemobjAlloc::Alloc(sz); }
#endif
#ifdef BENCH_MAKE_PERSISTENT_ATOMIC
    auto InitPool() -> void { MakePersistentAtomic::InitPool(); }
    auto DestroyPool() -> void { MakePersistentAtomic::DestroyPool(); }
    auto Alloc(std::size_t sz) -> void* { return MakePersistentAtomic::Alloc(sz); }
#endif
#ifdef BENCH_PMEM
    auto InitPool() -> void { Pmem::InitPool(); }
    auto DestroyPool() -> void { Pmem::DestroyPool(); }
    auto Alloc(std::size_t sz) -> void* { return Pmem::Alloc(sz); }
#endif
}    // namespace Interface
