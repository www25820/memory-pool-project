#pragma once
#include "Common.h"
#include <atomic>
#include <array>
#include <mutex>

// ===== 中心缓存：线程间共享，自旋锁保护，批量调拨 =====
class CentralCache {
public:
    // 全局单例
    static CentralCache& getInstance() {
        static CentralCache instance;
        return instance;
    }

    // 批量取：ThreadCache 来要货，给 batchNum 个块
    // batchNum 是引用 → 实际拿到多少会写回
    void* fetchRange(size_t index, size_t& batchNum);

    // 批量还：ThreadCache 太多，归还给中心
    void  returnRange(void* start, size_t size, size_t index);

private:
    // 构造函数：atomic类型没法保证清零 array必须手动初始化为空
    CentralCache() {
        for (auto& ptr : CentralFreeList_)
            ptr.store(nullptr, std::memory_order_relaxed);  // 所有链表 = 空
        for (auto& lock : lock_)
            lock.clear();                                    // 所有锁 = 开
    }

    // 从 PageCache 拿 Span 切成小块
    void* fetchFromPageCache(size_t size);

    std::array<std::atomic<void*>, FREE_LIST_SIZE> CentralFreeList_; // 32768条空闲链表
    std::array<std::atomic_flag, FREE_LIST_SIZE>   lock_;            // 32768把自旋锁
};
