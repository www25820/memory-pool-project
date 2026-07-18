#include "../include/MemoryPool.h"
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <random>
#include <atomic>

using namespace std::chrono;

// ==================== 计时器 ====================
class Timer {
    high_resolution_clock::time_point start_;
public:
    Timer() : start_(high_resolution_clock::now()) {}
    double ms() {
        return duration_cast<microseconds>(high_resolution_clock::now() - start_).count() / 1000.0;
    }
};

// ==================== 测试 ====================

// 1. 单线程小对象
void testSmallAlloc() {
    const size_t N = 100000;
    const size_t SIZE = 32;

    std::cout << "[1] 单线程 " << N << " 次 × " << SIZE << "B 分配/释放" << std::endl;

    // 内存池
    {
        Timer t;
        std::vector<void*> ptrs; ptrs.reserve(N);
        for (size_t i = 0; i < N; i++) {
            ptrs.push_back(MemoryPool::allocate(SIZE));
            if (i % 4 == 0 && !ptrs.empty()) {
                MemoryPool::deallocate(ptrs.back(), SIZE);
                ptrs.pop_back();
            }
        }
        for (void* p : ptrs) MemoryPool::deallocate(p, SIZE);
        std::cout << "  MemoryPool: " << t.ms() << " ms" << std::endl;
    }

    // new/delete
    {
        Timer t;
        std::vector<char*> ptrs; ptrs.reserve(N);
        for (size_t i = 0; i < N; i++) {
            ptrs.push_back(new char[SIZE]);
            if (i % 4 == 0 && !ptrs.empty()) {
                delete[] ptrs.back();
                ptrs.pop_back();
            }
        }
        for (char* p : ptrs) delete[] p;
        std::cout << "  new/delete: " << t.ms() << " ms" << std::endl;
    }
}

// 2. 多线程
void testMultiThread() {
    const size_t TH = 4, N = 25000;
    std::cout << "[2] " << TH << " 线程 × " << N << " 次随机大小分配/释放" << std::endl;

    auto fn = [](bool usePool) {
        std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<size_t> dist(8, 256);
        std::vector<std::pair<void*, size_t>> allocs; allocs.reserve(N);
        for (size_t i = 0; i < N; i++) {
            size_t sz = dist(rng);
            void* p = usePool ? MemoryPool::allocate(sz) : new char[sz];
            allocs.push_back({p, sz});
            if (rand() % 4 == 0 && !allocs.empty()) {
                size_t idx = rand() % allocs.size();
                if (usePool) MemoryPool::deallocate(allocs[idx].first, allocs[idx].second);
                else         delete[] static_cast<char*>(allocs[idx].first);
                allocs[idx] = allocs.back();
                allocs.pop_back();
            }
        }
        for (auto& [p, sz] : allocs)
            if (usePool) MemoryPool::deallocate(p, sz);
            else         delete[] static_cast<char*>(p);
    };

    {
        Timer t;
        std::vector<std::thread> threads;
        for (size_t i = 0; i < TH; i++) threads.emplace_back(fn, true);
        for (auto& th : threads) th.join();
        std::cout << "  MemoryPool: " << t.ms() << " ms" << std::endl;
    }
    {
        Timer t;
        std::vector<std::thread> threads;
        for (size_t i = 0; i < TH; i++) threads.emplace_back(fn, false);
        for (auto& th : threads) th.join();
        std::cout << "  new/delete: " << t.ms() << " ms" << std::endl;
    }
}

// 3. 混合大小
void testMixedSize() {
    const size_t N = 50000;
    const size_t SIZES[] = {16, 32, 64, 128, 256, 512, 1024, 2048};
    std::cout << "[3] 混合大小 " << N << " 次分配/释放" << std::endl;

    auto fn = [&](bool usePool) {
        std::vector<std::pair<void*, size_t>> allocs; allocs.reserve(N);
        for (size_t i = 0; i < N; i++) {
            size_t sz = SIZES[rand() % 8];
            void* p = usePool ? MemoryPool::allocate(sz) : new char[sz];
            allocs.push_back({p, sz});
            if (i % 100 == 0 && allocs.size() > 20) {
                for (size_t j = 0; j < 20; j++) {
                    if (usePool) MemoryPool::deallocate(allocs.back().first, allocs.back().second);
                    else         delete[] static_cast<char*>(allocs.back().first);
                    allocs.pop_back();
                }
            }
        }
        for (auto& [p, sz] : allocs)
            if (usePool) MemoryPool::deallocate(p, sz);
            else         delete[] static_cast<char*>(p);
    };

    {
        Timer t;
        fn(true);
        std::cout << "  MemoryPool: " << t.ms() << " ms" << std::endl;
    }
    {
        Timer t;
        fn(false);
        std::cout << "  new/delete: " << t.ms() << " ms" << std::endl;
    }
}

int main() {
    std::cout << "=== MemoryPool v2 Performance Test ===" << std::endl;
    testSmallAlloc();
    testMultiThread();
    testMixedSize();
    std::cout << "按回车退出..." << std::endl;
    std::cin.get();
    return 0;
}
