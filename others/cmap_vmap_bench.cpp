#include <cassert>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include <memkind.h>
#include <libpmemkv.hpp>

#define MOUNT_DIR "/mnt/pmem0/myrontsa/"
#define PERSIST_POOL_PATH_REL "pool_dir/persist_kvfile"
#define VOLATILE_DIR_PATH_REL "pool_dir/volatile_dir/"
constexpr uint64_t POOL_SIZE = MEMKIND_PMEM_MIN_SIZE;
constexpr uint64_t WORKLOAD = 1LLU * powl(10, 6);
constexpr auto PersistPoolPath = MOUNT_DIR PERSIST_POOL_PATH_REL;

constexpr auto VolatileDirPath = VOLATILE_DIR_PATH_REL;
auto KVPrint(std::string_view k, std::string_view v) -> int {
    std::cout << "key: " << k.data() << " value: " << v.data() << std::endl;
    return 0;
}

class KVOperations {
public:
    struct KVPair {
        std::string key;
        std::string value;
    };
    using DB = pmem::kv::db;

    KVOperations(const std::string& path, const std::string& engine)
        : kv{ std::make_unique<DB>() } {
        assert(engine == "cmap" || engine == "vcmap" || engine == "vsmap");
        pmem::kv::config cfg;
        if (cfg.put_string("path", path) != pmem::kv::status::OK) {
            std::cerr << pmemkv_errormsg() << std::endl;
            assert(false);
            exit(1);
        }
        if (cfg.put_uint64("size", POOL_SIZE) != pmem::kv::status::OK) {
            std::cerr << pmemkv_errormsg() << std::endl;
            assert(false);
            exit(1);
        }
        if (engine == "cmap") {
            const bool fileMissing = !std::filesystem::exists(path);
            if (cfg.put_uint64("create_if_missing", fileMissing) != pmem::kv::status::OK) {
                std::cerr << pmemkv_errormsg() << std::endl;
                assert(false);
                exit(1);
            }
        }
        if (kv->open(engine, std::move(cfg)) != pmem::kv::status::OK) {
            std::cerr << pmemkv_errormsg() << std::endl;
            assert(false);
            exit(1);
        }
    }
    auto PrepareKeys() -> void {
        std::generate(std::begin(pairs), std::end(pairs), [c = 0LLU]() mutable {
            c += 1;
            return KVPair{ "key" + std::to_string(c), "value" + std::to_string(c) };
        });
    }
    auto Run() -> void {
        pmem::kv::status ok{};
        for (const auto& [key, value] : pairs) {
            ok = kv->put(key, value);
            assert(ok == pmem::kv::status::OK);
            ok = kv->exists(key);
            assert(ok == pmem::kv::status::OK);
        }
    }
    auto CheckRecover() -> void {
        std::cout << "Not Recovered Keys:\n";
        for (const auto& [key, value] : pairs) {
            const auto ok = kv->exists(key);
            if (ok != pmem::kv::status::OK) std::cout << key << " ";
        }
        std::cout << "\n";
    }
    auto Print() const -> void {
        kv->get_all(KVPrint);
    }

private:
    std::unique_ptr<DB> kv{};
    std::vector<KVPair> pairs{ WORKLOAD };
};
KVOperations PersistKv{ PersistPoolPath, "cmap" };
KVOperations VolatileKv{ VolatileDirPath, "vcmap" };

class TimeIt {
public:
    TimeIt(std::string _id) : id{ std::move(_id) }, begin{ std::chrono::high_resolution_clock::now() } {}
    ~TimeIt() {
        const auto end = std::chrono::high_resolution_clock::now();
        std::cout << id << ": " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << " ms\n";
    }

private:
    std::string id{};
    std::chrono::time_point<std::chrono::high_resolution_clock> begin{};

public:
    TimeIt(const TimeIt&) = delete;
    TimeIt(const TimeIt&&) noexcept = delete;
    auto operator=(const TimeIt&) -> TimeIt& = delete;
    auto operator=(const TimeIt&&) noexcept -> TimeIt& = delete;
};

auto RunBenchAll() -> void {
    PersistKv.PrepareKeys();
    VolatileKv.PrepareKeys();

    {
        TimeIt _{ "Persistent KV Run" };
        PersistKv.Run();
    }
    // PersistKv.Print();

    {
        TimeIt _{ "Volatile KV Run" };
        VolatileKv.Run();
    }
    // VolatileKv.Print();
}

auto RecoverAll() -> void {
    PersistKv.PrepareKeys();
    VolatileKv.PrepareKeys();
    {
        TimeIt _{ "Persistent KV Recovery" };
        PersistKv.CheckRecover();
    }
    // {
    //     TimeIt _{ "Volatile KV Recovery" };
    //     VolatileKv.CheckRecover();
    // }
}

auto main() -> int {
    RunBenchAll();
    RecoverAll();
    return 0;
}
