#pragma once
#include "Common.h"
#include <cstdlib>
#include <array>

class ThreadCache {
public:
    // ===== 线程本地单例：每个线程一份，无锁 =====
    static ThreadCache* getInstance() {
        static thread_local ThreadCache instance;  // thread_local = 每线程独立
        return &instance;
    }

    // ===== 对外接口 =====
    void* allocate(size_t size);                   // 分配
    void  deallocate(void* ptr, size_t size);       // 回收

private:
    // ===== 构造函数：初始化数组 =====
    ThreadCache() {
        freeList_.fill(nullptr);                   // 所有链表头 = 空
        freeListSize_.fill(0);                     // 所有计数 = 0
    }

    // ===== 内部方法 =====
    void* fetchFromCentralCache(size_t index);      // 本地没货 → 找 CentralCache 批量拿
    void  returnToCentralCache(void* start, size_t size); // 本地太多 → 批量还给 CentralCache
    size_t getBatchNum(size_t size);                // 根据块大小算每次批量拿几个
    bool   shouldReturnToCentralCache(size_t index);// 判断是否该归还了（>64个）

    // ===== 成员变量 =====
    std::array<void*, FREE_LIST_SIZE>  freeList_;       // 空闲链表: 32768个尺寸各一条链表
    std::array<size_t, FREE_LIST_SIZE> freeListSize_;    // 空闲计数: 每个尺寸还剩多少块
};
