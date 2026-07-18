#include "../include/MemoryPool.h"
#include <iostream>
#include <vector>
#include <thread>
#include <cassert>

void testBasic() {
    std::cout << "[1] basic..." << std::endl;
    void* p1 = MemoryPool::allocate(8);     assert(p1);
    void* p2 = MemoryPool::allocate(1024);  assert(p2);
    void* p3 = MemoryPool::allocate(1024*1024); assert(p3);
    MemoryPool::deallocate(p1, 8);
    MemoryPool::deallocate(p2, 1024);
    MemoryPool::deallocate(p3, 1024*1024);
    std::cout << "  PASS" << std::endl;
}

void testMultiThread() {
    std::cout << "[2] multi-thread..." << std::endl;
    const int TH = 4, N = 500;
    auto fn = [](int) {
        std::vector<std::pair<void*, size_t>> allocs;
        for (int i = 0; i < N; i++) {
            size_t sz = (rand() % 128 + 1) * 8;
            void* p = MemoryPool::allocate(sz);
            assert(p);
            allocs.push_back({p, sz});
            if (rand() % 3 == 0 && !allocs.empty()) {
                MemoryPool::deallocate(allocs.back().first, allocs.back().second);
                allocs.pop_back();
            }
        }
        for (auto& [p, sz] : allocs)
            MemoryPool::deallocate(p, sz);
    };
    std::vector<std::thread> threads;
    for (int i = 0; i < TH; i++) threads.emplace_back(fn, i);
    for (auto& t : threads) t.join();
    std::cout << "  PASS" << std::endl;
}

int main() {
    std::cout << "MemoryPool v2 Tests" << std::endl;
    testBasic();
    testMultiThread();
    std::cout << "ALL TESTS PASSED" << std::endl;
    return 0;
}
