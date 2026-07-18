#include "../include/CentralCache.h"
#include "../include/PageCache.h"
#include <thread>

static const size_t SPAN_PAGES = 8;  // 每次从PageCache固定取8页

// 1. 从PageCache拿Span，切成小块
void* CentralCache::fetchFromPageCache(size_t size) {
    // 小块固定取8页批发，大块按实际需求
    if (size <= SPAN_PAGES * PageCache::PAGE_SIZE)
        return PageCache::getInstance().allocateSpan(SPAN_PAGES);
    else {
        size_t numPages = (size + PageCache::PAGE_SIZE - 1) / PageCache::PAGE_SIZE;
        return PageCache::getInstance().allocateSpan(numPages);
    }
}

// 2. 批量取：ThreadCache来要货
void* CentralCache::fetchRange(size_t index, size_t& batchNum) {
    // 越界检查
    if (index >= FREE_LIST_SIZE || batchNum == 0)
        return nullptr;

    // 自旋锁：抢到锁才继续，没抢到就yield让出CPU
    while (lock_[index].test_and_set(std::memory_order_acquire))// 试着上锁
        std::this_thread::yield();// 当前线程暂时让出CPU，防止死循环

    // 从中心缓存链表头读取
    void* result = CentralFreeList_[index].load(std::memory_order_relaxed);

    if (!result) {
        // 链表是空的 → 从PageCache拿Span，切成小块
        size_t size = (index + 1) * ALIGNMENT;//申请的块大小
        result = fetchFromPageCache(size);// 去对应尺寸的页缓存取

        if (!result) {// 还是没要到，解锁返回空
            lock_[index].clear(std::memory_order_release);
            return nullptr;
        }

        //要到了
        // 把Span切成小块，串成链表（每块前8字节存next指针）
        char* start = static_cast<char*>(result);
        size_t totalBlocks = (SPAN_PAGES * PageCache::PAGE_SIZE) / size;// 总共能切多少块
        size_t allocBlocks = std::min(batchNum, totalBlocks);// 这次给ThreadCache的数量

        // 串链表：块0→块1→块2→...→块N→nullptr
        if (allocBlocks > 1) {
            for (size_t i = 1; i < allocBlocks; i++) {
                void* cur = start + (i - 1) * size;
                void* nxt = start + i * size;
                *reinterpret_cast<void**>(cur) = nxt;// 当前块的前8字节指向下一块
            }
        }
        // 最后一块的next = nullptr
        *reinterpret_cast<void**>(start + (allocBlocks - 1) * size) = nullptr;

        // 剩余的块挂在中心缓存，下次别人直接拿不用再切
        if (totalBlocks > allocBlocks) {
            void* remain = start + allocBlocks * size;
            for (size_t i = allocBlocks + 1; i < totalBlocks; i++) {
                void* cur = start + (i - 1) * size;
                void* nxt = start + i * size;
                *reinterpret_cast<void**>(cur) = nxt;
            }
            *reinterpret_cast<void**>(start + (totalBlocks - 1) * size) = nullptr;
            CentralFreeList_[index].store(remain, std::memory_order_relaxed);
        }
    } else {
        // 中心缓存本来就有货 → 从现有链表取 batchNum 个
        void* cur = result;
        void* prev = nullptr;
        size_t count = 0;
        while (cur && count < batchNum) {
            prev = cur;
            cur = *reinterpret_cast<void**>(cur);// 沿着next走到下一块
            count++;
        }
        batchNum = count;// 实际拿到多少（可能不够）
        if (prev)
            *reinterpret_cast<void**>(prev) = nullptr;// 断开链表
        CentralFreeList_[index].store(cur, std::memory_order_relaxed);// 剩余挂回去
    }

    lock_[index].clear(std::memory_order_release);// 解锁
    return result;// 返回链表头给ThreadCache
}

// 3. 批量还：ThreadCache来退货
void CentralCache::returnRange(void* start, size_t /*size*/, size_t index) {
    if(!start||index>=FREE_LIST_SIZE){
        return;
    }

    while (lock_[index].test_and_set(std::memory_order_acquire))// 试着上锁
        std::this_thread::yield();
    
    void*end=start;
    while (*reinterpret_cast<void**>(end) != nullptr)//end所在地址作为指针指向下一个链表
        end = *reinterpret_cast<void**>(end);//end则递推变成下一个链表的前8位地址所代表的指针

    void*oldHead=CentralFreeList_[index].load(std::memory_order_relaxed);
    *reinterpret_cast<void**>(end) = oldHead;//链表末尾的next接上CentralFreeList_[index]，尾连下一个表的开头

    CentralFreeList_[index].store(start,std::memory_order_relaxed);//把新的表头头放进CentralFreeList_[index]

    lock_[index].clear(std::memory_order_release);//解锁
}
