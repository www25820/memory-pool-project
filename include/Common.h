#pragma once
#include <cstddef>

// ===== 全局常量 =====
constexpr size_t ALIGNMENT      = 8;                        // 对齐单位 = 最小块 8 字节
constexpr size_t MAX_BYTES      = 256 * 1024;               // 最大 256KB，超过走 malloc
constexpr size_t FREE_LIST_SIZE = MAX_BYTES / ALIGNMENT;    // 32768 种尺寸，各占一条链表

// ===== 大小类工具：字节数 ↔ 数组下标 互转 =====
class SizeClass {
public:
    // 向上取整到 8 的倍数：1→8, 9→16
    static size_t roundUp(size_t bytes) {
        return (bytes + 7) & ~7;
    }

    // 字节数 → freelist 下标：8→0, 16→1, 24→2 ...
    static size_t getIndex(size_t bytes) {
        if (bytes < ALIGNMENT) bytes = ALIGNMENT;
        return (bytes + ALIGNMENT - 1) / ALIGNMENT - 1;
    }
};
