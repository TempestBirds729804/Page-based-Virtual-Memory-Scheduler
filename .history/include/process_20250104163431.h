#ifndef PROCESS_H
#define PROCESS_H

#include "types.h"
#include "memory.h"

// 内存段类型
typedef enum {
    SEGMENT_CODE,
    SEGMENT_DATA,
    SEGMENT_HEAP,
    SEGMENT_STACK
} SegmentType;

// 内存段信息
typedef struct {
    uint32_t start_page;    // 起始页号
    uint32_t num_pages;     // 页数
    bool is_allocated;      // 是否已分配物理内存
} SegmentInfo;

// 进程内存布局
typedef struct {
    struct {
        uint32_t start_page;
        uint32_t num_pages;
        bool is_allocated;
    } code, data, heap, stack;
} ProcessMemoryLayout;

// 进程统计信息
typedef struct {
    uint32_t page_faults;         // 缺页次数
    uint32_t page_replacements;   // 页面置换次数
    uint32_t pages_swapped_in;    // 换入页面数
    uint32_t pages_swapped_out;   // 换出页面数
    struct {
        uint32_t total_accesses;  // 总访问次数
        uint32_t code_accesses;   // 代码段访问次数
        uint32_t data_accesses;   // 数据段访问次数
        uint32_t heap_accesses;   // 堆访问次数
        uint32_t stack_accesses;  // 栈访问次数
        uint32_t reads_from_disk; // 从磁盘读取次数
    } mem_stats;
} ProcessStats;

uint32_t select_victim_page(PCB* process);

// PCB结构体定义
struct PCB {
    uint32_t pid;                           // 进程ID
    char name[MAX_PROCESS_NAME];            // 进程名称
    ProcessState state;                     // 进程状态
    uint8_t priority;                       // 进程优先级
    uint32_t time_slice;                    // 剩余时间片
    uint32_t total_time_slice;              // 总时间片
    uint32_t wait_time;                     // 等待时间
    uint64_t last_schedule_time;            // 上次调度时间
    bool was_preempted;                     // 是否被抢占
    PageTableEntry *page_table;             // 页表指针
    uint32_t page_table_size;               // 页表大小
    struct PCB* next;                       // 链表下一节点
    
    // 新增字段
    ProcessStats stats;                     // 进程统计信息
    ProcessMemoryLayout memory_layout;      // 内存布局
    AppConfig app_config;                   // 应用程序配置
    MonitorConfig monitor_config;           // 监控配置
};

// 进程调度器结构
typedef struct {
    PCB* ready_queue[3];     // 3个优先级的就绪队列
    PCB* blocked_queue;      // 阻塞队列
    PCB* running_process;    // 当前运行进程
    uint32_t total_processes;// 总进程数
    
    // 新增字段
    uint32_t next_pid;      // 下一个可用的进程ID
    uint32_t total_runtime; // 系统总运行时间
    bool auto_balance;      // 是否启用自动平衡
} ProcessScheduler;

// 声明全局调度器
extern ProcessScheduler scheduler;

// 新增的进程管理函数
PCB* create_application(const char* name, AppConfig* config);
bool setup_process_memory(PCB* process, ProcessMemoryLayout* layout);
void update_process_stats(PCB* process, AccessType type);
void balance_process_memory(PCB* process);
void print_process_stats(PCB* process);
void monitor_process(PCB* process);

// 新增的内存管理函数
bool allocate_segment(PCB* process, uint32_t start, uint32_t size);
bool free_segment(PCB* process, uint32_t start, uint32_t size);
void collect_memory_stats(PCB* process);
void optimize_memory_layout(PCB* process);

// 进程管理函数
PCB* process_create(uint32_t pid, uint32_t page_table_size);         // 创建进程
void process_destroy(PCB* process);                                   // 销毁进程
bool process_allocate_memory(PCB* pcb, uint32_t start_page, uint32_t num_pages);
void process_free_memory(PCB* pcb, uint32_t start_page, uint32_t num_pages);

// 进程调度函数
void scheduler_init(void);
void scheduler_shutdown(void);
void schedule(void);                     // 进程调度
void add_to_ready_queue(PCB* process);   // 添加进程到就绪队列
void block_process(PCB* process);        // 进程阻塞
void wake_up_process(PCB* process);      // 进程唤醒
void set_process_priority(PCB* process, ProcessPriority priority);
void update_process_state(PCB* process, ProcessState new_state);
PCB* get_current_process(void);
void time_tick(void);                    // 时间片轮转
void set_running_process(PCB* process);

// 进程状态查询函数
PCB* get_ready_queue(ProcessPriority priority);
PCB* get_blocked_queue(void);
PCB* get_running_process(void);
uint32_t get_total_processes(void);

// 进程状态打印函数
void print_scheduler_status(void);
void print_process_stats(PCB* process);
void print_process_status(ProcessState state);

// 进程分析辅助函数
bool is_process_analyzed(PCB* process, PCB** analyzed_list, int analyzed_count);

// 进程查询函数
PCB* get_process_by_pid(uint32_t pid);

// 内存优化相关函数
void analyze_memory_usage(PCB* process);
void optimize_memory_layout(PCB* process);
void balance_memory_usage(void);

// 进程管理相关函数声明
PCB* create_process_with_pid(uint32_t pid, uint32_t priority, uint32_t code_pages, uint32_t data_pages);
PCB* create_process(uint32_t priority, uint32_t code_pages, uint32_t data_pages);
void process_destroy(PCB* process);
PCB* get_process_by_pid(uint32_t pid);
void balance_process_memory(PCB* process);

// 初始化进程内存
void initialize_process_memory(PCB* process);

// 进程相关常量
#define DEFAULT_TIME_SLICE 10
#define DEFAULT_PRIORITY 1
#define MAX_PAGES_PER_PROCESS 256  // 每个进程最大页表项数

// 模拟进程访存
void simulate_process_memory_access(uint32_t pid, uint32_t access_count);

// 为进程分配堆/栈空间
// flags: 0表示堆空间，1表示栈空间
void allocate_process_memory(uint32_t pid, uint32_t size, uint32_t flags);

// 新增函数声明
void check_preemption(PCB* new_process);           // 检查是否需要进程抢占
void preempt_process(PCB* current, PCB* new_proc); // 执行进程抢占
void restore_preempted_process(PCB* process);      // 恢复被抢占的进程
bool should_preempt(PCB* current, PCB* new_proc);  // 判断是否应该抢占

#endif // PROCESS_H 