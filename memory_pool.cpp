#include <iostream>
#include <cassert>
#include<atomic>
#include<mutex>
#define MEMORY_POOL_NUM 64
#define SLOT_BASE_SIZE  8
#define MAX_SLOT_SIZE   512

class SimplePool {
private:
    // Slot：空闲槽。槽空闲时前 8 字节存 next 指针，使用时被用户数据覆盖
    struct Slot {
       std::atomic< Slot*> next;  // next 本身是指针，存的是"下一个 Slot 的地址"
    };

    std::atomic<Slot*> freeList_{nullptr};  // freeList_ 也是指针，指向空闲链表的头节点
    int   slotsize_;//申请槽的大小
    int   blocksize_;//申请内存块大小
    Slot*   curSlot_     = nullptr;   // 指向 Block 里还没用过的槽
    Slot*   lastSlot_    = nullptr;   // 当前 Block 的末尾（最后可用位置）
    Slot*   firstBlock_  = nullptr;   // Block 链表头（析构时遍历释放用）
    std::mutex mutexForBlock_;        // 保护 Block 扩容（低频操作）

    //CAS入队
    bool pushFreeList(Slot* slot){
        while (true)
        {
           Slot* OldHead=freeList_.load();//读取当前链表头

           slot->next.store(OldHead);//新节点指向当前列表头

           //如果当前链表头不变就尝试把当前链表头改成新的节点slot
           if(freeList_.compare_exchange_weak(OldHead,slot)){
                return true;
           }
           //如果已经被改了--OldHead已经改了--继续while循环
        }
        
    }

    //CAS出队
    Slot* popFreelist(){
        while(true){
            Slot*OldHead=freeList_.load();//读取当前链表头

            if(OldHead==nullptr)return nullptr;

            Slot*NewHead=OldHead->next.load();//如果不为空，当前表头指向第二个节点
            //如果当前链表头不变就尝试把当前链表头改成新的节点NewHead
            if (freeList_.compare_exchange_weak(OldHead, NewHead)) {
            return OldHead;  // 成功，返回刚取出来的链表头，拿出去用
            }
            //失败继续while循环尝试
        }
    }

    size_t padPointer(char*p,size_t align){
        //让指针对齐槽大小倍数 rem=余数，返回需要填充的字节数
        size_t rem=(reinterpret_cast<size_t>(p)%align);
        
        return rem==0?0:(align-rem);
    }

    void allocateNewBlock(){
        //   申请新内存块
        void* newBlock=operator new(blocksize_);

        //   头插法串到Block链表（因为析构需要遍历释放）
        reinterpret_cast<Slot*>(newBlock)->next.store(firstBlock_);//新内存块next指向当前Block表头
        firstBlock_=reinterpret_cast<Slot*>(newBlock);//更新表头

         //  计算槽区域的起始位置（跳过 Block 头的 8 字节指针） Block的头永远保留用来存指针
        char*body=reinterpret_cast<char*>(newBlock)+sizeof(Slot*);
        
        //  对齐填充 算出第一个槽的初始位置
        size_t padding =padPointer(body,slotsize_);
        curSlot_=reinterpret_cast<Slot*>(body+padding);

        //  标记末尾 最后一个槽的初始位置、

        lastSlot_ = reinterpret_cast<Slot*>(
        reinterpret_cast<size_t>(newBlock) + blocksize_ - slotsize_ + 1
        );
    }

public:
    SimplePool(int slotsize = 8, int blocksize = 4096)
        : slotsize_(slotsize), blocksize_(blocksize)
    {
        assert(slotsize >= (int)sizeof(Slot));//槽装不下next指针报错
       allocateNewBlock();//申请第一块内存
    }

    ~SimplePool() {
       Slot*cur=firstBlock_;//当前释放进度初始化为第一个内存块
       while(cur){
        Slot*nextBlock=cur->next.load();
        operator delete(cur);
        cur = nextBlock;
       }
    }

    // 取走链表头拿去用
    void* allocate() {
        Slot* slot =popFreelist();          // 先尝试 CAS 从回收链表拿
        if(slot!=nullptr)return slot;
        //如果空了,Block里面取新槽
        {
            std::lock_guard<std::mutex>lock(mutexForBlock_);
            if(curSlot_>=lastSlot_){
                allocateNewBlock();//内存块用完了再申请扩容
            }
        
        slot=curSlot_;//拿内存块的第一个新槽

        curSlot_+=slotsize_/sizeof(Slot);//第一个槽被取走，游标后移，后移量为申请槽大小/Slot*  步长，也就是8字节
        //因为curSlot_也是指针类型，跳24字节等于跳3步
        }
        return slot;
    }

    // 归还到链表头（注意：这里还是非 CAS 的，多线程不安全，Step 3 后面会改成 CAS）
    void deallocate(void* p) {
        if (p == nullptr) return;
        Slot* slot = static_cast<Slot*>(p);
        pushFreeList(slot);
    }
    void init(int slotsize){
        Slot*cur=firstBlock_;
        //删除现有槽
        while(cur){
            Slot*nextBlock=cur->next.load();
            operator delete(cur);
            cur=nextBlock;
        }
        //重新初始化
        firstBlock_ = nullptr;    // 旧链已清空，重置链表头
        slotsize_=slotsize;
        freeList_.store(nullptr);
        curSlot_=nullptr;
        lastSlot_=nullptr;
        allocateNewBlock();//按新大小重新分配
        
    }
};
class HashBucket{
     private:
    static SimplePool pools_[MEMORY_POOL_NUM];
    public:
    static void init(){//划分桶，根据桶内槽的大小
        for(int i=0;i<MEMORY_POOL_NUM;i++){
            int slotSize=(i+1)*SLOT_BASE_SIZE;
            pools_[i].init(slotSize);
        }
    }
    static void* useMemory(size_t size){
        if(size==0)return nullptr;

        if(size>MAX_SLOT_SIZE)return operator new(size);//需求太大直接跳过 不走内存池
        
        int index=(size+SLOT_BASE_SIZE-1)/SLOT_BASE_SIZE-1;

        return pools_[index].allocate();
    }
    static void freeMemory(void*ptr,size_t size){
        if(ptr==nullptr)return;

        if(size>MAX_SLOT_SIZE){//需求太大被跳过了，不在桶里直接删
            operator delete(ptr);
            return ;
        }

        int index=(size+SLOT_BASE_SIZE-1)/SLOT_BASE_SIZE-1;
        pools_[index].deallocate(ptr);
         
    }

    template<typename T, typename... Args>
    static T* newElement(Args&&... args) {
        void*p=HashBucket::useMemory(sizeof(T));

        if(p==nullptr)return nullptr;

        //new(p) T(...);   // ← 不申请内存，直接在 p 上造对象
        //forward避免不必要的拷贝
        //直接在申请下来的地址上把参数传给构造函数创建对象
        return new(p) T(std::forward<Args>(args)...);
        
    }

    template<typename T>
    static void deleteElement(T* p) {
        if (!p) return;                    // 空指针不处理
        p->~T();                           // ① 先调用析构：对象死了，但是不释放内存，直接delete释放的内存不在内存池了
        HashBucket::freeMemory(p, sizeof(T)); // ② 归还内存给内存池
    }
};

// 给静态数组真正分配内存
SimplePool HashBucket::pools_[MEMORY_POOL_NUM];

// ==================== Benchmark ====================
#include <chrono>
#include <thread>
#include <vector>

class P1 { int id_; };
class P2 { int id_[5]; };
class P3 { int id_[10]; };
class P4 { int id_[20]; };

size_t benchPool(size_t ntimes) {
    size_t sum = 0;
    for (size_t i = 0; i < ntimes; i++) {
        P1* p1 = HashBucket::newElement<P1>();
        sum += (size_t)p1;
        HashBucket::deleteElement(p1);
        P2* p2 = HashBucket::newElement<P2>();
        sum += (size_t)p2;
        HashBucket::deleteElement(p2);
        P3* p3 = HashBucket::newElement<P3>();
        sum += (size_t)p3;
        HashBucket::deleteElement(p3);
        P4* p4 = HashBucket::newElement<P4>();
        sum += (size_t)p4;
        HashBucket::deleteElement(p4);
    }
    return sum;
}

size_t benchNew(size_t ntimes) {
    size_t sum = 0;
    for (size_t i = 0; i < ntimes; i++) {
        P1* p1 = new P1; sum += (size_t)p1; delete p1;
        P2* p2 = new P2; sum += (size_t)p2; delete p2;
        P3* p3 = new P3; sum += (size_t)p3; delete p3;
        P4* p4 = new P4; sum += (size_t)p4; delete p4;
    }
    return sum;
}

int main() {
    HashBucket::init();

    const size_t N = 50000;         // 每线程循环次数
    const size_t nThreads = 4;      // 线程数
    const size_t rounds = 5;        // 轮次

    size_t poolCost = 0, newCost = 0;
    size_t poolSum = 0, newSum = 0;

    for (size_t r = 0; r < rounds; r++) {
        // ---- 内存池多线程 ----
        {
            std::vector<std::thread> threads;
            std::vector<size_t> results(nThreads, 0);
            auto t1 = std::chrono::high_resolution_clock::now();
            for (size_t t = 0; t < nThreads; t++)
                threads.emplace_back([&results, t, N]() { results[t] = benchPool(N); });
            for (auto& th : threads) th.join();
            auto t2 = std::chrono::high_resolution_clock::now();
            for (auto v : results) poolSum += v;
            poolCost += std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
        }

        // ---- new/delete 多线程 ----
        {
            std::vector<std::thread> threads;
            std::vector<size_t> results(nThreads, 0);
            auto t3 = std::chrono::high_resolution_clock::now();
            for (size_t t = 0; t < nThreads; t++)
                threads.emplace_back([&results, t, N]() { results[t] = benchNew(N); });
            for (auto& th : threads) th.join();
            auto t4 = std::chrono::high_resolution_clock::now();
            for (auto v : results) newSum += v;
            newCost += std::chrono::duration_cast<std::chrono::milliseconds>(t4 - t3).count();
        }
    }

    std::cout << "(校验和: pool=" << poolSum << " new=" << newSum << ")" << std::endl;

    std::cout << "======= Benchmark (" << nThreads << " threads x "
              << N << " ops x " << rounds << " rounds) =======" << std::endl;
    std::cout << "MemoryPool: " << poolCost << " ms" << std::endl;
    std::cout << "new/delete: " << newCost << " ms" << std::endl;
    if (newCost > 0)
        std::cout << "加速比:     " << (double)newCost / poolCost << "x" << std::endl;

    std::cout << "按回车退出..." << std::endl;
    std::cin.get();
    return 0;
}
