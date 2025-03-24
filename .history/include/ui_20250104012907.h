#ifndef UI_H
#define UI_H

#include <stdbool.h>
#include "types.h"

// 命令类型枚举
typedef enum {
    // 系统命令
    CMD_HELP,           // 显示帮助
    CMD_EXIT,           // 退出系统
    CMD_STATUS,         // 显示系统状态
    
    // 进程命令
    CMD_PROC_CREATE,    // 创建进程
    CMD_PROC_KILL,      // 终止进程
    CMD_PROC_LIST,      // 显示进程列表
    CMD_PROC_INFO,      // 显示进程信息
    CMD_PROC_PRIO,      // 设置进程优先级
    
    // 内存命令
    CMD_MEM_ALLOC,      // 分配内存
    CMD_MEM_FREE,       // 释放内存
    CMD_MEM_READ,       // 读取内存
    CMD_MEM_WRITE,      // 写入内存
    CMD_MEM_MAP,        // 显示内存映射
    CMD_MEM_STAT,       // 内存状态
    
    // 虚拟内存命令
    CMD_VM_PAGE,        // 显示页面
    CMD_VM_SWAP,        // 交换页面
    CMD_VM_STAT,        // 虚拟内存状态
    
    // 存储命令
    CMD_DISK_ALLOC,     // 分配磁盘空间
    CMD_DISK_FREE,      // 释放磁盘空间
    CMD_DISK_READ,      // 读取磁盘
    CMD_DISK_WRITE,     // 写入磁盘
    CMD_DISK_STAT,      // 磁盘状态
    
    // 系统状态命令
    CMD_STATE_SAVE,     // 保存系统状态
    CMD_STATE_LOAD,     // 加载系统状态
    CMD_STATE_RESET,    // 重置系统状态
    CMD_DEMO_SCHEDULE,  // 演示调度
    CMD_DEMO_MEMORY,    // 内存演示
    CMD_UNKNOWN,        // 未知
    
    // 新增高级命令
    CMD_APP_CREATE,     // 创建应用程序
    CMD_APP_RUN,        // 运行应用程序
    CMD_APP_MONITOR,    // 监控应用程序
    CMD_MEM_PROFILE,    // 内存使用分析
    CMD_MEM_OPTIMIZE,   // 内存布局优化
    CMD_MEM_BALANCE,    // 内存使用平衡
    CMD_TIME_TICK,      // 时间片轮转命令
    // 演示命令
    CMD_DEMO_FULL_LOAD,    // 系统满负载运行演示
    CMD_DEMO_MULTI_PROC,   // 多进程竞争演示
    CMD_DEMO_PAGE_THRASH,  // 页面抖动演示
    CMD_PROC_ACCESS,    // 模拟进程访存
    CMD_PROC_ALLOC,     // 为进程分配堆/栈空间
} CommandType;

// 命令字符串定义
#define CMD_STR_PROC_ACCESS "proc access"  // 模拟进程访存命令
#define CMD_STR_PROC_ALLOC "proc alloc"    // 进程内存分配命令
#define CMD_STR_APP_CREATE "app create"  // 创建应用程序命令
#define CMD_STR_APP_RUN "app run"        // 运行应用程序命令

// 结构体
typedef struct {
    uint32_t pid;       // ID
    uint32_t addr;      // 地址
    uint32_t size;      // 大小
    uint32_t flags;     // 标志位
    uint32_t priority;  // 进程优先级
    uint32_t code_pages;  // 代码段页数
    uint32_t data_pages;  // 数据段页数
    char* text;         // 文本
} CommandArgs;

// 结构体
typedef struct {
    CommandType type;   // 命令类型
    CommandArgs args;   // 命令参数
} Command;

// 初始化
void ui_init(void);
void ui_run(void);
void ui_shutdown(void);

// 处理命令
void handle_command(Command* cmd);
void show_help(void);
void show_detailed_help(CommandType cmd_type);

// 演示函数声明
void demo_full_load(void);
void demo_multi_process(void);
void demo_page_thrash(void);

// UI 函数声明
void print_system_status_graph(void);
void print_segment_ascii(PCB* process, uint32_t start_page, uint32_t num_pages);
void print_memory_layout(PCB* process);

// 内存访问检查
bool check_memory_access(PCB* process, uint32_t addr);

#endif // UI_H 