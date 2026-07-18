# MemoryPool

基于 C++17 的轻量级内存池实现。

## 版本

### v1 — 单层哈希桶（`memory_pool.cpp`）

64 桶多尺寸管理 + CAS 无锁 freeList + Block 链表扩容，单文件 ~270 行。

**编译：**
```bash
g++ -O2 -std=c++17 -pthread memory_pool.cpp -o memory_pool.exe
```

### v2 — 三级缓存（`include/` + `src/`，已完成）

```
ThreadCache (thread_local，无锁)
  → CentralCache (自旋锁，批量调拨)
    → PageCache (VirtualAlloc，按4KB页管理)
```

**进度：**

| 模块 | 内容 | 状态 |
| ---- | ---- | ---- |
| Common.h | ALIGNMENT、MAX_BYTES、SizeClass | ✅ |
| PageCache | systemAlloc、allocateSpan、deallocateSpan | ✅ |
| CentralCache | fetchFromPageCache、fetchRange、returnRange | ✅ |
| ThreadCache | allocate、deallocate、批量收发 | ✅ |
| MemoryPool.h | 对外统一接口 | ✅ |
| UnitTest | 基础分配 + 多线程测试 | ✅ |

**编译：**
```bash
g++ -O2 -std=c++17 -pthread src/PageCache.cpp src/CentralCache.cpp src/ThreadCache.cpp tests/UnitTest.cpp -o test_v2.exe
```

## 环境要求

- 操作系统：Windows
- 编译器：MinGW-w64 (g++ 8.0+)
- 标准：C++17
- VS Code `Ctrl+Shift+B`
