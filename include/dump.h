#ifndef DUMP_H
#define DUMP_H

#include <stdio.h>
#include <stdbool.h>
#include "types.h"

// 转储结构定义
typedef struct {
    // 系统状态
    uint32_t total_pages;          // 总页面数
    uint32_t physical_pages;       // 物理页面数
    uint32_t swap_size;           // 交换区大小
    
    // 统计信息
    MemoryStats memory_stats;     // 内存统计
    
    // 进程信息
    uint32_t process_count;       // 进程数量
    uint32_t current_pid;         // 当前运行进程ID
} SystemStateHeader;

// 数据转储与恢复接口
bool dump_system_state(const char* filename);
bool restore_system_state(const char* filename);

#endif // DUMP_H 