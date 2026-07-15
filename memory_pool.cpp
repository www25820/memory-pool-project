#include <iostream>
#include <cassert>

#define MEMORY_POOL_NUM 64
#define SLOT_BASE_SIZE  8
#define MAX_SLOT_SIZE   512

class SimplePool {
private:
    // Slot：空闲槽。槽空闲时前 8 字节存 next 指针，使用时被用户数据覆盖
    struct Slot {
        Slot* next;  // next 本身是指针，存的是"下一个 Slot 的地址"
    };

    Slot* freeList_ = nullptr;  // freeList_ 也是指针，指向空闲链表的头节点
    void* block_    = nullptr;  // 整块内存的首地址
    int   slotsize_;
    int   blocksize_;

public:
    SimplePool(int slotsize = 8, int blocksize = 4096)
        : slotsize_(slotsize), blocksize_(blocksize)
    {
        assert(slotsize >= (int)sizeof(Slot));
        block_ = operator new(blocksize_);

        int numSlots = blocksize_ / slotsize_;
        char* p = static_cast<char*>(block_);
        for (int i = 0; i < numSlots; i++) {
            Slot* slot = reinterpret_cast<Slot*>(p + i * slotsize_); // 算出第 i 个槽的地址
            slot->next = freeList_;   // 新槽的 next 指向当前链表头
            freeList_ = slot;         // 链表头移到新槽
        }
        // 结果：freeList_ → 槽N → 槽N-1 → ... → 槽0 → nullptr
        //        ↑              ↑
        //    freeList_      每个槽里的 next，都是同一种指针
    }

    ~SimplePool() {
        operator delete(block_);
    }

    // 取走链表头
    void* allocate() {
        if (freeList_ == nullptr) return nullptr;
        Slot* slot = freeList_;          // 记下头节点
        freeList_ = freeList_->next;     // 头指针后移（跳过第一个）
        return slot;
    }

    // 归还到链表头
    void deallocate(void* p) {
        if (p == nullptr) return;
        Slot* slot = static_cast<Slot*>(p);
        slot->next = freeList_;   // 归还的槽指向当前链表头
        freeList_ = slot;         // 链表头移到归还的槽上
    }
    void init(int slotsize){
        slotsize_=slotsize;
        freeList_=nullptr;
        block_=operator new(blocksize_);

        int numSlots = blocksize_ / slotsize_;
        char* p = static_cast<char*>(block_);
        for (int i = 0; i < numSlots; i++) {
            Slot* slot = reinterpret_cast<Slot*>(p + i * slotsize_); // 算出第 i 个槽的地址
            slot->next = freeList_;   // 新槽的 next 指向当前链表头
            freeList_ = slot;         // 链表头移到新槽
        }
    }
};
class HashBucket{
    public:
    static void init(){
        for(int i=0;i<MEMORY_POOL_NUM;i++){
            int slotSize=(i+1)*SLOT_BASE_SIZE;
            pools_[i].init(slotSize);
        }
    }
    static void* useMemory(size_t size){
        if(size==0)return nullptr;
        if(size>MAX_SLOT_SIZE)return operator new(size);
        
        int index=(size+SLOT_BASE_SIZE-1)/SLOT_BASE_SIZE-1;
        return pools_[index].allocate();
    }
    static void freeMemory(void*ptr,size_t size){
        if(ptr==nullptr)return;
        if(size>MAX_SLOT_SIZE){
            operator delete(ptr);
            return ;
        }
        int index=(size+SLOT_BASE_SIZE-1)/SLOT_BASE_SIZE-1;
        pools_[index].deallocate(ptr);
         
        
    }
    private:
    static SimplePool pools_[MEMORY_POOL_NUM];
};

// 给静态数组真正分配内存
SimplePool HashBucket::pools_[MEMORY_POOL_NUM];

int main() {
    HashBucket::init();
    void* p1 = HashBucket::useMemory(10);
    void* p2 = HashBucket::useMemory(100);
    std::cout << "p1=" << p1 << std::endl;
    std::cout << "p2=" << p2 << std::endl;

    HashBucket::freeMemory(p1, 10);
    HashBucket::freeMemory(p2, 100);

    std::cout << "done" << std::endl;
    return 0;
}
