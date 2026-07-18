#include"../include/PageCache.h"
#include<windows.h>
void* PageCache::systemAlloc(size_t numPages){
    //调用windows底层的malloc 让OS自己找内存   申请页数*4K的内存  地址空间申请位置再分配真实物理地址  能读写
    return VirtualAlloc(nullptr,numPages*PAGE_SIZE,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
}

void* PageCache::allocateSpan(size_t numPages){
    std::lock_guard<std::mutex>lock(mutex_);

    //找大于等于numPages的
    auto it=freeSpans_.lower_bound(numPages);
    
    //std::map<void*, Span*>  spanMap_; sencond指的是value
    if(it!=freeSpans_.end()){
        Span* span=it->second;//拿到页数对应span链表头

        if(span->next){ 
            freeSpans_[it->first]=span->next;//如果后续还有，链表头后移
        }

        else{
            freeSpans_.erase(it);//链表后续没东西了就删掉整个页数
        }

        // 如果比需要的大，切分
        if (span->numPages > numPages) {
        Span* rest = new Span;
        rest->pageAddr = (char*)span->pageAddr + numPages * PAGE_SIZE;
        rest->numPages = span->numPages - numPages;
        rest->next = nullptr;
        // 多余部分放回对应页数货架
        auto& list = freeSpans_[rest->numPages];
        rest->next = list;
        list = rest;

        span->numPages = numPages;  // 给出去的就是要的页数
        }

        spanMap_[span->pageAddr] = span;  // 记录
        return span->pageAddr;
    }


    //没找到就走os申请
    void*mem =systemAlloc(numPages);
    if(!mem)return nullptr;//如果申请失败返回空

    Span* span=new Span;
    span->pageAddr=mem;
    span->numPages=numPages;
    span->next=nullptr;

    spanMap_[mem]=span;//记录
    return mem;

}
void PageCache::deallocateSpan(void* ptr, size_t) {  // numPages 暂未使用
    std::lock_guard<std::mutex>lock(mutex_);

    auto it=spanMap_.find(ptr);

    if(it==spanMap_.end())return;//说明不是分配出去的

    Span*span=it->second;

    //放回
    auto& list=freeSpans_[span->numPages];
    span->next=list;
    list=span;

}
