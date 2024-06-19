/*
 * Copyright (c) 2020, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * pmemkv.cpp -- demonstrate a high-level key-value API for pmem
 */


#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <new>
#include <thread>
// Aligned allocator class
template <typename T, std::size_t Alignment>
class AlignedAllocator {
 public:
  AlignedAllocator() = default;

  template <typename U>
  AlignedAllocator(const AlignedAllocator<U, Alignment>&) {}

  T* allocate(std::size_t n) {
    void* ptr;
    if (posix_memalign(&ptr, Alignment, n * sizeof(T))) {
      throw std::bad_alloc();
    }
    return static_cast<T*>(ptr);
  }

  void deallocate(T* ptr, std::size_t) { free(ptr); }
};

constexpr size_t WORK = 100000000;
void f1() {
  std::cout << "f1\n";
  uint64_t c = 0;
  for (size_t i = 0; i < WORK; ++i) c += 1;
}
struct line_data {
  uint64_t data[8];
};
void f2() {
  std::cout << "f2\n";
  //   std::vector<uint64_t> v(WORK);
  //   std::vector<line_data, AlignedAllocator<line_data, 64>> v(1000);
  //   std::vector<uint64_t, alignas(64)> v(1000);

  line_data* arr = (line_data*)aligned_alloc(64, WORK * sizeof(line_data));
  if (((std::size_t)arr) % 64 == 0)
    std::cout << "arr is aligned to 64 bytes." << std::endl;
  for (size_t i = 0; i < WORK; ++i) {
    for (size_t j = 0; j < 7; ++j) arr[i].data[j] = i;
  }
  free(arr);
  //   std::iota(std::begin(v), std::end(v), 0);
}
void f3() {
  std::cout << "f3\n";
  double mul = 0.0;
  for (size_t i = 0; i < WORK; ++i) mul *= (1.0 * i);
}

int main() {
  std::cout << sizeof(uint64_t) << "\n";
  //   std::cout << std::hardware_destructive_interference_size << "\n";

  f1();
  f2();
  f3();
  return 0;
}
