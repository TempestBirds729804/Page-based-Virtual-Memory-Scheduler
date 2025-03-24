#ifndef VM_H
#define VM_H

#include "types.h"
#include "process.h"

// 虚拟内存管理器结构
typedef struct {
    SwapBlockInfo* swap_blocks;        // 交换区块信息
    uint32_t swap_free_blocks;         // 空闲交换块数量
    void* swap_area;                   // 模拟的交换区空间
    MemoryStats stats;                 // 内存访问统计
} VMManager;

// 声明全局虚拟内存管理器实例
extern VMManager vm_manager;

// 函数声明
void vm_init(void);
void vm_shutdown(void);

// 页面置换相关
bool page_out(PCB* process, uint32_t virtual_page);

// 页面调入相关
bool page_in(PCB* process, uint32_t virtual_page);

// 缺页中断处理
bool handle_page_fault(PCB* process, uint32_t virtual_address);

// 交换区管理
uint32_t allocate_swap_block(uint32_t process_id, uint32_t virtual_page);
void free_swap_block(uint32_t swap_index);

// 内存访问和统计
void access_memory(PCB* process, uint32_t virtual_address, bool is_write);
void print_vm_stats(void);
MemoryStats get_memory_stats(void);

// 调试和监控
void dump_page_table(PCB* process);
void dump_swap_area(void);

// 页面数据读写
bool write_page_data(PCB* process, uint32_t virtual_address, const void* data, size_t size);
bool read_page_data(PCB* process, uint32_t virtual_address, void* buffer, size_t size);

// 交换区数据读写
bool write_to_swap(uint32_t swap_index, const void* data);
bool read_from_swap(uint32_t swap_index, void* buffer);

// 交换区访问函数
SwapBlockInfo* get_swap_blocks(void);
void* get_swap_area(void);

// 交换区管理函数
void clean_swap_area(void);
void print_swap_status(void);

// 页面置换统计函数
uint32_t get_last_replaced_page(void);
void update_replacement_stats(uint32_t page_num);

// 页面置换相关函数
uint32_t select_victim_page(PCB* process);
uint32_t select_victim_frame(void);
bool swap_out_page(uint32_t frame);
bool page_in(PCB* process, uint32_t page_num);
bool page_out(PCB* process, uint32_t page_num);

// 虚拟内存管理的核心接口
void vm_init(void);                                                    // 初始化虚拟内存系统
void vm_shutdown(void);                                                // 关闭虚拟内存系统
void access_memory(PCB* process, uint32_t virtual_address, bool is_write);  // 访问内存
bool handle_page_fault(PCB* process, uint32_t address);               // 处理缺页中断

// 添加函数声明
uint64_t get_current_time(void);

// 页面置换统计信息
typedef struct {
    uint32_t total_page_faults;     // 总缺页次数
    uint32_t page_replacements;      // 页面置换次数
    uint32_t disk_reads;            // 从磁盘读取次数
    uint32_t disk_writes;           // 写入磁盘次数
    uint32_t lru_hits;              // LRU命中次数
    uint32_t lru_misses;            // LRU未命中次数
} PageReplacementStats;

// 声明全局统计信息
extern PageReplacementStats page_stats;

// 打印页面置换统计信息
void print_page_replacement_stats(void);

// 页面交换相关函数
bool swap_out_page(uint32_t frame);
bool swap_in_page(uint32_t pid, uint32_t virtual_page, uint32_t frame);

#endif // VM_H 