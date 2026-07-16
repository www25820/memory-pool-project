# MemoryPool

基于 C++17 的轻量级内存池实现，支持多尺寸分配、无锁回收和线程安全。

## 功能

- **多尺寸管理**：64 个哈希桶，8B~512B 共 64 种尺寸，`(size+7)/8-1` 自动路由
- **CAS 无锁回收**：`std::atomic<Slot*>` + `compare_exchange_weak`，多线程安全
- **Block 链表扩容**：curSlot_ 游标 + allocateNewBlock，Block 用完自动申请新块
- **对齐填充**：padPointer 保证每个槽地址对齐到槽大小倍数
- **placement new 接口**：`newElement<T>(args...)` / `deleteElement<T>(ptr)` 替换 new/delete
- **大对象兜底**：>512B 直接走 `operator new`/`operator delete`

## 架构

```text
HashBucket (64 个桶)
  ├── [0]  MemoryPool(8B)
  ├── [1]  MemoryPool(16B)
  ├── ...
  └── [63] MemoryPool(512B)
  └── >512B → operator new

每个 MemoryPool:
  Block1 → Block2 → Block3 (头插链表)
  ├── freeList_: CAS 无锁回收链表
  ├── curSlot_:  未分配区域游标
  └── lastSlot_: 当前块末尾标记
```

## 环境要求

- 操作系统：Windows
- 编译器：MinGW-w64 (g++ 8.0+) 或 MSVC 2017+
- 标准：C++17 或更高

## 编译

VS Code 下打开 `memory_pool.cpp`，按 `Ctrl+Shift+B` 编译（已配置 `-O2` 优化）。

或命令行：

```bash
g++ -O2 -std=c++17 -pthread -finput-charset=UTF-8 -fexec-charset=GBK memory_pool.cpp -o memory_pool.exe
```

## 模块

| 模块 | 内容 | 状态 |
| ---- | ---- | ---- |
| SimplePool | 批发内存切片、链表管理、allocate/deallocate | ✅ |
| HashBucket | 64 桶多尺寸路由、useMemory/freeMemory | ✅ |
| CAS freeList | pushFreeList / popFreeList，compare_exchange_weak | ✅ |
| Block 扩容 | curSlot_ 游标 + allocateNewBlock + padPointer | ✅ |
| 对外接口 | newElement/deleteElement（placement new） | ✅ |
