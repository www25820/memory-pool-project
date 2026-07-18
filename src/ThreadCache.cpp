#include"../include/ThreadCache.h"
#include"../include/CentralCache.h"

void* ThreadCache::allocate(size_t size){
    if(size==0){
        size=ALIGNMENT;//默认最小为8字节
    }
    if(size>MAX_BYTES){
        return malloc(size);
    }
    size_t index=SizeClass::getIndex(size);

    if(freeList_[index]!=nullptr ){
        void*ptr=freeList_[index];
        freeList_[index]=*reinterpret_cast<void**>(ptr);
        freeListSize_[index]--;      // 计数减 1
        return ptr;
    }

    // 本地空 → 从中心缓存批量取
    return fetchFromCentralCache(index);
}
void ThreadCache::deallocate(void*ptr,size_t size){
    if(size>MAX_BYTES){
        free(ptr);
        return;
    }

    size_t index=SizeClass::getIndex(size);

    *reinterpret_cast<void**>(ptr)=freeList_[index];
    freeList_[index]=ptr;
    freeListSize_[index]++;

    if(shouldReturnToCentralCache(index)){
        returnToCentralCache(freeList_[index],size);
    }
}

bool ThreadCache::shouldReturnToCentralCache(size_t index) {
    return freeListSize_[index] > 64;
}

size_t ThreadCache::getBatchNum(size_t size){
    constexpr size_t MAX_BATCH=4*1024;//一次最多批4KB

    size_t base;//一次性拿多少块
    //小块多拿，大块少拿
     if (size <= 32)      base = 64;
    else if (size <= 64)  base = 32;
    else if (size <= 128) base = 16;
    else if (size <= 256) base = 8;
    else if (size <= 512) base = 4;
    else if (size <= 1024) base = 2;
    else                   base = 1;

    size_t maxNum = std::max(size_t(1), MAX_BATCH / size);//确保最少有一块
    return std::max(size_t(1), std::min(maxNum, base));//取min确保不超过上限
}
void* ThreadCache::fetchFromCentralCache(size_t index){
    size_t size=(index+1)*ALIGNMENT;
    size_t batchNum=getBatchNum(size);

    void*start=CentralCache::getInstance().fetchRange(index,batchNum);
    if(!start)return nullptr;

    freeListSize_[index]+=batchNum-1;

    void*result=start;
    if(batchNum>1){
        freeList_[index]=*reinterpret_cast<void**>(start);
    }
    return result;
}
void ThreadCache::returnToCentralCache(void* start,size_t size){
    size_t index=SizeClass::getIndex(size);
    size_t batchNum=freeListSize_[index];

    if(batchNum<=1){
        return ;
    }
    //保留四分之一
    size_t keepNum=std::max(batchNum/4,size_t(1));
    size_t returnNum=batchNum-keepNum;
    
    char*node=static_cast<char*>(start);

    for(size_t i=0;i<keepNum-1;i++){
        node=reinterpret_cast<char*>(*reinterpret_cast<void**>(node));
        if(!node){
            returnNum=batchNum-(i+1);break;
        }
    }

    if(node){
        void* toRuturn=*reinterpret_cast<void**>(node);
        *reinterpret_cast<void**>(node)=nullptr;
        freeList_[index]=start;
        freeListSize_[index]=keepNum;

        if(returnNum>0&&toRuturn){
            CentralCache::getInstance().returnRange(toRuturn,returnNum*size,index);
        }
    }
    
    
}


