#ifndef MEMORY_H
#define MEMORY_H

#include <stdbool.h>
#include "types.h"

// 页框信息结构
typedef struct {
    bool is_allocated;          // 是否已分配
    bool is_swapping;          // 是否正在进行交换
    uint32_t process_id;        // 占用进程ID
    uint32_t virtual_page_num;  // 对应的虚拟页号
} FrameInfo;

// 内存管理器结构
typedef struct {
    FrameInfo frames[PHYSICAL_PAGES];  // 页框信息数组
    uint32_t free_frames_count;        // 空闲页框计数
    AllocationStrategy strategy;       // 分配策略
} MemoryManager;

// 物理内存结构
typedef struct {
    uint8_t* memory;           // 物理内存空间
    uint32_t total_frames;     // 总页框数
    uint32_t free_frames;      // 空闲页框数
    bool* frame_map;           // 页框使用位图
} PhysicalMemory;

// 声明物理内存结构（但保持为静态）
extern PhysicalMemory phys_mem;

// 声明全局内存管理器
extern MemoryManager memory_manager;

// 内存管理函数声明
void memory_init(void);
void memory_shutdown(void);

// 获取页框
uint32_t allocate_frame(uint32_t process_id, uint32_t virtual_page_num);
void free_frame(uint32_t frame_number);

// 读取页框
bool read_frame(uint32_t frame_number, void* buffer);
bool write_frame(uint32_t frame_number, const void* buffer);

// 获取空闲页框数量
uint32_t get_free_frames_count(void);
bool is_frame_allocated(uint32_t frame_number);

// 读取物理内存
bool read_physical_memory(uint32_t frame, uint32_t offset, void* buffer, size_t size);
bool write_physical_memory(uint32_t frame, uint32_t offset, const void* data, size_t size);

// 设置分配策略
void set_allocation_strategy(AllocationStrategy strategy);
AllocationStrategy get_allocation_strategy(void);

// 获取物理内存
uint8_t* get_physical_memory(void);
bool* get_frame_map(void);

// 更新空闲页框数量
void update_free_frames_count(uint32_t free_count);

// 打印内存映射
void print_memory_map(void);

// 计算内存碎片数量
uint32_t count_memory_fragments(void);

// 内存管理相关函数声明
void memory_init(void);
uint32_t allocate_frame(uint32_t process_id, uint32_t page_num);
void free_frame(uint32_t frame);
void* get_physical_address(uint32_t frame);
uint32_t count_memory_fragments(void);
// 检查内存管理器状态一致性
void check_memory_state(void);

// 物理内存管理的核心接口
void memory_init(void);                                              // 初始化物理内存
void memory_shutdown(void);                                          // 关闭物理内存系统
uint32_t allocate_frame(uint32_t pid, uint32_t page_num);           // 分配页框
void free_frame(uint32_t frame_number);                             // 释放页框

// 在现有定义之后添加
typedef struct {
    uint32_t frame_number;     // 页框号
    bool is_valid;            // 页面是否有效
    bool is_locked;           // 页面是否被锁定
    bool is_dirty;           // 页面是否被修改
    unsigned long last_access; // 最后访问时间
} page_t;

// 声明全局变量
extern page_t *pages;         // 页面数组
extern uint32_t total_pages;  // 总页面数

// 在其他声明后添加
uint64_t get_current_time(void);  // 获取当前时间戳
bool write_to_disk(page_t *page); // 将页面写入磁盘

// 选择要置换的页框
uint32_t select_victim_frame(void);

#endif // MEMORY_H
