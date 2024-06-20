#include "interface.h"

#include <exception>
#include <filesystem>
#include <iostream>
#include <string>

#include <libpmemobj++/make_persistent_array_atomic.hpp>
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/pool.hpp>

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
        (void)sz;
        return {};
        // if (void* ptr = std::malloc(sz)) return ptr;
        // throw std::bad_alloc{};
    }
}    // namespace JeMalloc

namespace Vmem {
    [[maybe_unused]] auto InitPool() -> void {}
    [[maybe_unused]] auto DestroyPool() -> void {}
    [[maybe_unused]] auto Alloc(std::size_t sz) -> void* {
        (void)sz;
        return {};
        // if (void* ptr = std::malloc(sz)) return ptr;
        // throw std::bad_alloc{};
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
    [[maybe_unused]] auto InitPool() -> void {}
    [[maybe_unused]] auto DestroyPool() -> void {}
    [[maybe_unused]] auto Alloc(std::size_t sz) -> void* {
        if (void* ptr = std::malloc(sz)) return ptr;
        throw std::bad_alloc{};
    }
}    // namespace Memkind
namespace PmemobjAlloc {
    [[maybe_unused]] auto InitPool() -> void {}
    [[maybe_unused]] auto DestroyPool() -> void {}
    [[maybe_unused]] auto Alloc(std::size_t sz) -> void* {
        (void)sz;
        return {};
        // if (void* ptr = std::malloc(sz)) return ptr;
        // throw std::bad_alloc{};
    }
}    // namespace PmemobjAlloc

namespace MakePersistentAtomic {
    const std::string PoolPath = std::string{ Config::NVM_DIR } + "alloc_experiments_pool";
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
        [[maybe_unused]] int error = std::system(("rm " + PoolPath).data());
        assert(!error);
    }
    [[maybe_unused]] auto Alloc(std::size_t bytes) -> void* {
        pmem::obj::persistent_ptr<std::int8_t[]> ptr{};
        pmem::obj::make_persistent_atomic<std::int8_t[]>(Pool, ptr, bytes);
        return ptr.get();
    }
}    // namespace MakePersistentAtomic

namespace Interface {
#ifdef DEFAULT
    auto InitPool() -> void { Default::InitPool(); }
    auto DestroyPool() -> void { Default::DestroyPool(); }
    auto Alloc(std::size_t sz) -> void* { return Default::Alloc(sz); }
#endif
#ifdef MALLOC
    auto InitPool() -> void { Malloc::InitPool(); }
    auto DestroyPool() -> void { Malloc::DestroyPool(); }
    auto Alloc(std::size_t sz) -> void* { return Malloc::Alloc(sz); }
#endif
#ifdef JEMALLOC
    auto InitPool() -> void { JeMalloc::InitPool(); }
    auto DestroyPool() -> void { JeMalloc::DestroyPool(); }
    auto Alloc(std::size_t sz) -> void* { return JeMalloc::Alloc(sz); }
#endif
#ifdef VMEM
    auto InitPool() -> void { Vmem::InitPool(); }
    auto DestroyPool() -> void { Vmem::DestroyPool(); }
    auto Alloc(std::size_t sz) -> void* { return Vmem::Alloc(sz); }
#endif
#ifdef VMMALLOC
    auto InitPool() -> void { Vmmalloc::InitPool(); }
    auto DestroyPool() -> void { Vmmalloc::DestroyPool(); }
    auto Alloc(std::size_t sz) -> void* { return Vmmalloc::Alloc(sz); }
#endif
#ifdef MEMKIND
    auto InitPool() -> void { Memkind::InitPool(); }
    auto DestroyPool() -> void { Memkind::DestroyPool(); }
    auto Alloc(std::size_t sz) -> void* { return Memkind::Alloc(sz); }
#endif
#ifdef PMEMOBJ_ALLOC
    auto InitPool() -> void { PmemobjAlloc::InitPool(); }
    auto DestroyPool() -> void { PmemobjAlloc::DestroyPool(); }
    auto Alloc(std::size_t sz) -> void* { return PmemobjAlloc::Alloc(sz); }
#endif
#ifdef MAKE_PERSISTENT_ATOMIC
    auto InitPool() -> void { MakePersistentAtomic::InitPool(); }
    auto DestroyPool() -> void { MakePersistentAtomic::DestroyPool(); }
    auto Alloc(std::size_t sz) -> void* { return MakePersistentAtomic::Alloc(sz); }
#endif
}    // namespace Interface
