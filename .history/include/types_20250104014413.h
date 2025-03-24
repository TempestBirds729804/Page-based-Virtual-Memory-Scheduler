#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stdbool.h>

// ǰ������
typedef struct PCB PCB;

// ϵͳ���ó���
#define PAGE_SIZE           4096    // ҳ��С��4KB
#define VIRTUAL_PAGES       1024    // ����ҳ����
#define PHYSICAL_PAGES      256     // ����ҳ����
#define MAX_PROCESSES       64      // 
#define MAX_PROCESS_NAME    32      // 进程名称最大长度
#define SWAP_SIZE           (4 * PHYSICAL_PAGES)  // Сڴ4
#define SWAP_BLOCK_SIZE     PAGE_SIZE            // С
#define MAX_MEMORY_ACCESSES 1000  // ڴʼ¼
#define PAGE_TABLE_ENTRIES  1024  // ҳ

// 内存相关常量
#define VIRTUAL_MEMORY_SIZE (1024 * PAGE_SIZE)  // 虚拟内存大小为1024页

// ״̬
typedef enum {
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_BLOCKED,
    PROCESS_TERMINATED,
    PROCESS_WAITING
} ProcessState;

// �ڴ���������չ
typedef enum {
    FIRST_FIT,       // 
    BEST_FIT,        // 
    WORST_FIT,       // 
    NEXT_FIT,        // ѭ
    BUDDY_SYSTEM     // 
} AllocationStrategy;

// 
#define SWAP_BLOCKS 1024            // 
#define SWAP_BLOCK_SIZE PAGE_SIZE   // 

// ??Ϣ
typedef struct {
    bool is_used;           // 
    uint32_t process_id;    // 
    uint32_t virtual_page;  // 
} SwapBlockInfo;

// 页表项标志位
typedef struct {
    unsigned int present : 1;        // 页面是否在内存中
    unsigned int swapped : 1;        // 页面是否在交换区
    unsigned int dirty : 1;       // 页面是否被修改
    unsigned int referenced : 1;  // 页面是否被访问
    unsigned int swap_index : 28; // 交换区索引
} PageFlags;

// 内存统计信息结构
typedef struct {
    uint32_t total_accesses;      // 总访问次数
    uint32_t page_faults;         // 缺页次数
    uint32_t page_replacements;   // 页面替换次数
    uint32_t disk_writes;         // 写入磁盘次数
    uint32_t pages_swapped_out;   // 换出页面数
    uint32_t pages_swapped_in;    // 换入页面数
    uint32_t writes_to_disk;      // 写入磁盘次数
    uint32_t last_replaced_page;  // 最后被替换的页面
} MemoryStats;

// ȼ
typedef enum {
    PRIORITY_HIGH = 0,
    PRIORITY_NORMAL = 1,
    PRIORITY_LOW = 2
} ProcessPriority;

// ̵صĳ
#define MAX_PROCESS_QUEUE   3       // 进程队列数（优先级队列）
#define TIME_SLICE 10        // ʱƬС룩

// 内存布局结构
typedef struct {
    uint32_t code_start;    // 代码段起始地址
    uint32_t code_size;     // 代码段大小
    uint32_t data_start;    // 数据段起始地址
    uint32_t data_size;     // 数据段大小
    uint32_t heap_start;    // 堆区起始地址
    uint32_t heap_size;     // 堆区大小
    uint32_t stack_start;   // 栈区起始地址
    uint32_t stack_size;    // 栈区大小
} MemoryLayout;

// ״̬չ
typedef struct {
    MemoryLayout layout;         // ڴ沼
    MemoryStats mem_stats;       // ڴʹͳ
    uint32_t priority;           // ȼ
    uint32_t time_slice;         // ʱƬ
    uint32_t wait_time;          // ȴʱ
    uint32_t total_runtime;      // ʱ
    uint32_t memory_usage;       // ڴʹ
    uint32_t page_faults;        // ȱҳ
} ProcessStats;

// ڴ
typedef enum {
    ACCESS_CODE,     // 
    ACCESS_DATA,     // ݷ
    ACCESS_HEAP,     // ѷ
    ACCESS_STACK     // ջ
} AccessType;

// ڴ
typedef struct {
    bool track_access_pattern;   // Ƿٷģʽ
    bool auto_balance;           // ǷԶƽ
    bool collect_stats;          // ǷռͳϢ
    uint32_t monitor_interval;   // ؼ룩
    uint32_t balance_threshold;  // ƽֵ
} MonitorConfig;

// Ӧó
typedef struct {
    char name[32];              // Ӧó
    uint32_t min_memory;        // Сڴ
    uint32_t max_memory;        // ڴ
    uint32_t priority;          // ȼ
    MemoryLayout layout;        // ڴ沼
    MonitorConfig monitor;      // 
    uint32_t page_table_size;   // 页表大小
} AppConfig;

// ڴ
typedef struct {
    uint32_t size;              // С
    AccessType type;            // 
    bool is_continuous;         // ǷҪռ
    uint32_t alignment;         // Ҫ
} MemoryRequest;

// 页表项结构
typedef struct {
    uint32_t frame_number;   // 物理页框号
    PageFlags flags;         // 页状态标志
    uint64_t last_access_time;  // 最后访问时间
} PageTableEntry;

#endif // TYPES_H 