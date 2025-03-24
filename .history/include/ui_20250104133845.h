#ifndef UI_H
#define UI_H

#include <stdint.h>
#include "process.h"

// 命令类型
typedef enum {
    CMD_UNKNOWN,
    CMD_HELP,
    CMD_EXIT,
    CMD_CLEAR,
    
    // 进程管理命令
    CMD_PROC_CREATE,    // 创建进程
    CMD_PROC_KILL,      // 终止进程
    CMD_PROC_LIST,      // 显示进程列表
    CMD_PROC_INFO,      // 显示进程信息
    CMD_PROC_PRIO,      // 设置进程优先级
    CMD_PROC_ACCESS,    // 模拟进程内存访问
    CMD_PROC_ALLOC,     // 分配内存
    
    // 内存管理命令
    CMD_MEM_MAP,        // 显示内存映射
    CMD_MEM_ALLOC,      // 分配内存
    CMD_MEM_FREE,       // 释放内存
    
    // 虚拟内存命令
    CMD_VM_PAGE,        // 页面操作
    CMD_VM_SWAP,        // 交换区操作
    
    // 应用程序命令
    CMD_APP_CREATE,     // 创建应用程序
    CMD_APP_KILL,       // 终止应用程序
    CMD_APP_LIST,       // 显示应用程序列表
    CMD_APP_INFO,       // 显示应用程序信息
    
    // 时钟命令
    CMD_CLOCK_TICK,     // 时钟滴答
} CommandType;

// 命令参数
typedef struct {
    uint32_t pid;           // 进程ID
    uint32_t priority;      // 优先级
    uint32_t addr;          // 地址
    uint32_t size;          // 大小
    uint32_t flags;         // 标志
    uint32_t code_pages;    // 代码页数
    uint32_t data_pages;    // 数据页数
    char name[32];          // 名称
} CommandArgs;

// 命令结构
typedef struct {
    CommandType type;       // 命令类型
    CommandArgs args;       // 命令参数
} Command;

// 命令字符串定义
#define CMD_STR_HELP       "help"
#define CMD_STR_EXIT       "exit"
#define CMD_STR_CLEAR      "clear"

#define CMD_STR_PROC       "proc"
#define CMD_STR_CREATE     "create"
#define CMD_STR_KILL       "kill"
#define CMD_STR_LIST       "list"
#define CMD_STR_INFO       "info"
#define CMD_STR_PRIO       "prio"
#define CMD_STR_ACCESS     "access"
#define CMD_STR_ALLOC      "alloc"

#define CMD_STR_MEM        "mem"
#define CMD_STR_MAP        "map"
#define CMD_STR_FREE       "free"

#define CMD_STR_VM         "vm"
#define CMD_STR_PAGE       "page"
#define CMD_STR_SWAP       "swap"

#define CMD_STR_APP        "app"

#define CMD_STR_CLOCK      "clock"
#define CMD_STR_TICK       "tick"

// 函数声明
void ui_init(void);
void ui_run(void);
void ui_shutdown(void);
void print_help(void);
Command parse_command(const char* cmd_line);
void execute_command(Command cmd);

#endif // UI_H 