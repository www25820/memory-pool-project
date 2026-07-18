#pragma once
#include "Common.h"
#include <map>
#include <mutex>

// ===== 页缓存：最底层，直接向 OS 申请/释放 4KB 页 =====
class PageCache {
public:
    static const size_t PAGE_SIZE = 4096;   // 一页 4KB

    // 全局单例：整个程序一个 PageCache
    static PageCache& getInstance() {
        static PageCache instance;
        return instance;
    }

    // 分配 numPages 页，返回起始地址
    void* allocateSpan(size_t numPages);

    // 归还 numPages 页（会自动合并相邻空闲页）
    void  deallocateSpan(void* ptr, size_t numPages);

private:
    PageCache() = default;                  // 私有构造，只能通过 getInstance()

    // 直接调 VirtualAlloc 向 OS 申请
    void* systemAlloc(size_t numPages);

    // ===== Span：连续若干页的描述信息 =====
    struct Span {
        void*  pageAddr;    // 起始地址
        size_t numPages;    // 几页
        Span*  next;        // 同页数的 Span 串成链表
    };

    std::map<size_t, Span*> freeSpans_;     // 页数 → 空闲 Span 链表头（货架）
    std::map<void*, Span*>  spanMap_;       // 地址 → Span 指针（账本，用于回收查找）
    std::mutex mutex_;                      // 保护以上两个 map（低频，用 mutex）
};
