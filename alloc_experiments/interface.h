#ifndef INTERFACE_H
#define INTERFACE_H

#include <chrono>
#include <cstdint>
#include <string_view>

namespace Interface {
    auto InitPool() -> void;
    auto DestroyPool() -> void;
    auto Alloc(std::size_t sz) -> void*;
}    // namespace Interface

namespace Config {
    constexpr inline std::size_t POOL_SIZE = 1024ULL * 1024ULL * 1024ULL * 32;    //32GB
    constexpr inline std::size_t ALLOC_SIZE = 128;
    constexpr inline std::string_view NVM_DIR = "/mnt/pmem0/myrontsa/alloc_experiments_pools";
}    // namespace Config

using Clock = std::chrono::steady_clock;
using Millis = std::chrono::milliseconds;
using Micros = std::chrono::microseconds;
struct Type {
    char data[Config::ALLOC_SIZE]{};
};
inline constexpr std::size_t TypeSize = sizeof(Type);

#endif
