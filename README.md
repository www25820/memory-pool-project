# MemoryPool

基于 C++17 的轻量级内存池实现，支持多尺寸分配、无锁回收和线程安全。

## 功能

- **预分配 + 复用**：一次批发大块内存，切成固定大小槽，链表管理空闲槽，避免频繁系统调用
- **多尺寸支持**：64 个哈希桶，管理 8B~512B 共 64 种尺寸，自动路由到对应桶
- **无锁回收**：`std::atomic` + CAS（`compare_exchange_weak`）实现空闲链表无锁入队出队
- **对齐填充**：每个槽起始地址对齐到槽大小倍数，提升内存访问效率
- **大对象兜底**：超过 512B 直接走 `operator new`/`operator delete`

## 架构

```
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

VS Code 下打开 `memory_pool.cpp`，按 `Ctrl+Shift+B` 编译。

或命令行：

```bash
g++ -std=c++17 -pthread -finput-charset=UTF-8 -fexec-charset=GBK memory_pool.cpp -o memory_pool.exe
```

## 版本

| 版本 | 内容 |
|---|---|
| v1 | SimplePool + HashBucket，单线程安全 |
| v2 | CAS 无锁 freeList，多线程安全（开发中） |
