#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 跨平台的延时函数支持
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#define Sleep(x) usleep((x)*1000)
#endif

#include "../include/ui.h"
#include "../include/process.h"
#include "../include/memory.h"
#include "../include/vm.h"
#include "../include/storage.h"
#include "../include/dump.h"

#define MAX_CMD_LEN 256
#define MAX_ARGS 4

// 声明外部变量
extern MemoryManager memory_manager;

// 命令解析
static Command parse_command(char* cmd_str) {
    Command cmd = {CMD_UNKNOWN, {0}};  // 初始化所有字段为0
    char* token = strtok(cmd_str, " \n");
    
    if (!token) return cmd;
    
    // 基本命令
    if (strcmp(token, "help") == 0) {
        cmd.type = CMD_HELP;
    } else if (strcmp(token, "status") == 0) {
        cmd.type = CMD_STATUS;
    } else if (strcmp(token, "exit") == 0) {
        cmd.type = CMD_EXIT;
    } else if (strcmp(token, "proc") == 0) {
        // 进程管理命令
        token = strtok(NULL, " \n");
        if (token) {
            if (strcmp(token, "create") == 0) {
                cmd.type = CMD_PROC_CREATE;
                token = strtok(NULL, " \n");
                if (token) cmd.args.pid = (uint32_t)strtoul(token, NULL, 0);
            } else if (strcmp(token, "kill") == 0) {
                cmd.type = CMD_PROC_KILL;
                token = strtok(NULL, " \n");
                if (token) cmd.args.pid = (uint32_t)strtoul(token, NULL, 0);
            } else if (strcmp(token, "list") == 0) {
                cmd.type = CMD_PROC_LIST;
            } else if (strcmp(token, "info") == 0) {
                cmd.type = CMD_PROC_INFO;
                token = strtok(NULL, " \n");
                if (token) cmd.args.pid = (uint32_t)strtoul(token, NULL, 0);
            } else if (strcmp(token, "prio") == 0) {
                cmd.type = CMD_PROC_PRIO;
                token = strtok(NULL, " \n");  // pid
                if (token) cmd.args.pid = (uint32_t)strtoul(token, NULL, 0);
                token = strtok(NULL, " \n");  // priority
                if (token) cmd.args.flags = (uint32_t)strtoul(token, NULL, 0);
            } else if (strcmp(token, "access") == 0) {
                cmd.type = CMD_PROC_ACCESS;
                token = strtok(NULL, " \n");  // pid
                if (token) cmd.args.pid = (uint32_t)strtoul(token, NULL, 0);
                token = strtok(NULL, " \n");  // access_count
                if (token) cmd.args.size = (uint32_t)strtoul(token, NULL, 0);
            } else if (strcmp(token, "alloc") == 0) {
                cmd.type = CMD_PROC_ALLOC;
                token = strtok(NULL, " \n");  // pid
                if (token) cmd.args.pid = (uint32_t)strtoul(token, NULL, 0);
                token = strtok(NULL, " \n");  // size
                if (token) cmd.args.size = (uint32_t)strtoul(token, NULL, 0);
                token = strtok(NULL, " \n");  // flags
                if (token) cmd.args.flags = (uint32_t)strtoul(token, NULL, 0);
            } else if (strcmp(token, "time") == 0) {
                cmd.type = CMD_PROC_TIME;
                token = strtok(NULL, " \n");  // pid
                if (token) cmd.args.pid = (uint32_t)strtoul(token, NULL, 0);
                token = strtok(NULL, " \n");  // time_slice
                if (token) cmd.args.time_slice = (uint32_t)strtoul(token, NULL, 0);
            }
        }
    } else if (strcmp(token, "mem") == 0) {
        // 内存管理命令
        token = strtok(NULL, " \n");
        if (token) {
            if (strcmp(token, "alloc") == 0) {
                cmd.type = CMD_MEM_ALLOC;
                token = strtok(NULL, " \n");  // pid
                if (token) cmd.args.pid = (uint32_t)strtoul(token, NULL, 0);
                token = strtok(NULL, " \n");  // addr
                if (token) cmd.args.addr = (uint32_t)strtoul(token, NULL, 0);
                token = strtok(NULL, " \n");  // size
                if (token) cmd.args.size = (uint32_t)strtoul(token, NULL, 0);
            } else if (strcmp(token, "free") == 0) {
                cmd.type = CMD_MEM_FREE;
                token = strtok(NULL, " \n");  // pid
                if (token) cmd.args.pid = (uint32_t)strtoul(token, NULL, 0);
                token = strtok(NULL, " \n");  // addr
                if (token) cmd.args.addr = (uint32_t)strtoul(token, NULL, 0);
                token = strtok(NULL, " \n");  // size
                if (token) cmd.args.size = (uint32_t)strtoul(token, NULL, 0);
            } else if (strcmp(token, "write") == 0) {
                cmd.type = CMD_MEM_WRITE;
                token = strtok(NULL, " \n");  // pid
                if (token) cmd.args.pid = (uint32_t)strtoul(token, NULL, 0);
                token = strtok(NULL, " \n");  // addr
                if (token) cmd.args.addr = (uint32_t)strtoul(token, NULL, 0);
                token = strtok(NULL, "\"");   // text (in quotes)
                if (token) cmd.args.text = strdup(token);
            } else if (strcmp(token, "read") == 0) {
                cmd.type = CMD_MEM_READ;
                token = strtok(NULL, " \n");  // pid
                if (token) cmd.args.pid = (uint32_t)strtoul(token, NULL, 0);
                token = strtok(NULL, " \n");  // addr
                if (token) cmd.args.addr = (uint32_t)strtoul(token, NULL, 0);
            } else if (strcmp(token, "map") == 0) {
                cmd.type = CMD_MEM_MAP;
                token = strtok(NULL, " \n");  // pid
                if (token) cmd.args.pid = (uint32_t)strtoul(token, NULL, 0);
            } else if (strcmp(token, "stat") == 0) {
                cmd.type = CMD_MEM_STAT;
            } else if (strcmp(token, "profile") == 0) {
                cmd.type = CMD_MEM_PROFILE;
            } else if (strcmp(token, "optimize") == 0) {
                cmd.type = CMD_MEM_OPTIMIZE;
                token = strtok(NULL, " \n");  // pid
                if (token) cmd.args.pid = (uint32_t)strtoul(token, NULL, 0);
            } else if (strcmp(token, "balance") == 0) {
                cmd.type = CMD_MEM_BALANCE;
                token = strtok(NULL, " \n");  // pid
                if (token) cmd.args.pid = (uint32_t)strtoul(token, NULL, 0);
            } else if (strcmp(token, "strategy") == 0) {
                cmd.type = CMD_MEM_STRATEGY;
                token = strtok(NULL, " \n");  // strategy
                if (token) {
                    if (strcmp(token, "first") == 0) cmd.args.flags = FIRST_FIT;
                    else if (strcmp(token, "best") == 0) cmd.args.flags = BEST_FIT;
                    else if (strcmp(token, "worst") == 0) cmd.args.flags = WORST_FIT;
                }
            }
        }
    } else if (strcmp(token, "vm") == 0) {
        // 虚拟内存管理命令
        token = strtok(NULL, " \n");
        if (token) {
            if (strcmp(token, "page") == 0) {
                cmd.type = CMD_VM_PAGE;
                token = strtok(NULL, " \n");  // in/out
                if (token) {
                    cmd.args.flags = (strcmp(token, "in") == 0) ? 1 : 0;
                    token = strtok(NULL, " \n");  // pid
                    if (token) cmd.args.pid = (uint32_t)strtoul(token, NULL, 0);
                    token = strtok(NULL, " \n");  // page
                    if (token) cmd.args.addr = (uint32_t)strtoul(token, NULL, 0);
                }
            } else if (strcmp(token, "swap") == 0) {
                cmd.type = CMD_VM_SWAP;
                token = strtok(NULL, " \n");  // list/clean
                if (token) cmd.args.flags = (strcmp(token, "clean") == 0) ? 1 : 0;
            } else if (strcmp(token, "stat") == 0) {
                cmd.type = CMD_VM_STAT;
            }
        }
    } else if (strcmp(token, "disk") == 0) {
        // 磁盘管理命令
        token = strtok(NULL, " \n");
        if (token) {
            if (strcmp(token, "alloc") == 0) {
                cmd.type = CMD_DISK_ALLOC;
                token = strtok(NULL, " \n");  // size
                if (token) cmd.args.size = (uint32_t)strtoul(token, NULL, 0);
            } else if (strcmp(token, "free") == 0) {
                cmd.type = CMD_DISK_FREE;
                token = strtok(NULL, " \n");  // block
                if (token) cmd.args.addr = (uint32_t)strtoul(token, NULL, 0);
            } else if (strcmp(token, "write") == 0) {
                cmd.type = CMD_DISK_WRITE;
                token = strtok(NULL, " \n");  // block
                if (token) cmd.args.addr = (uint32_t)strtoul(token, NULL, 0);
                token = strtok(NULL, "\"");   // text (in quotes)
                if (token) cmd.args.text = strdup(token);
            } else if (strcmp(token, "read") == 0) {
                cmd.type = CMD_DISK_READ;
                token = strtok(NULL, " \n");  // block
                if (token) cmd.args.addr = (uint32_t)strtoul(token, NULL, 0);
            } else if (strcmp(token, "stat") == 0) {
                cmd.type = CMD_DISK_STAT;
            }
        }
    } else if (strcmp(token, "state") == 0) {
        // 系统状态管理命令
        token = strtok(NULL, " \n");
        if (token) {
            if (strcmp(token, "save") == 0) {
                cmd.type = CMD_STATE_SAVE;
                token = strtok(NULL, " \n");  // optional filename
                if (token) cmd.args.text = strdup(token);
            } else if (strcmp(token, "load") == 0) {
                cmd.type = CMD_STATE_LOAD;
                token = strtok(NULL, " \n");  // optional filename
                if (token) cmd.args.text = strdup(token);
            } else if (strcmp(token, "reset") == 0) {
                cmd.type = CMD_STATE_RESET;
            }
        }
    } else if (strcmp(token, "app") == 0) {
        token = strtok(NULL, " \n");
        if (token) {
            if (strcmp(token, "create") == 0) {
                cmd.type = CMD_APP_CREATE;
                // 解析PID
                token = strtok(NULL, " \n");
                if (token) cmd.args.pid = (uint32_t)strtoul(token, NULL, 0);
                // 解析优先级
                token = strtok(NULL, " \n");
                if (token) cmd.args.priority = (uint32_t)strtoul(token, NULL, 0);
                // 解析代码段页数
                token = strtok(NULL, " \n");
                if (token) cmd.args.code_pages = (uint32_t)strtoul(token, NULL, 0);
                // 解析数据段页数
                token = strtok(NULL, " \n");
                if (token) cmd.args.data_pages = (uint32_t)strtoul(token, NULL, 0);
            } else if (strcmp(token, "run") == 0) {
                cmd.type = CMD_APP_RUN;
                token = strtok(NULL, " \n");
                if (token) cmd.args.pid = (uint32_t)strtoul(token, NULL, 0);
            } else if (strcmp(token, "monitor") == 0) {
                cmd.type = CMD_APP_MONITOR;
                token = strtok(NULL, " \n");
                if (token) cmd.args.pid = (uint32_t)strtoul(token, NULL, 0);
            }
        }
    } else if (strcmp(token, "time") == 0) {
        token = strtok(NULL, " \n");
        if (token) {
            if (strcmp(token, "tick") == 0) {
                cmd.type = CMD_TIME_TICK;
                token = strtok(NULL, " \n");  // 可选的tick数量
                if (token) {
                    cmd.args.size = (uint32_t)strtoul(token, NULL, 0);
                } else {
                    cmd.args.size = 1;  // 默认一个时间片
                }
            }
        }
    } else if (strncmp(cmd_str, CMD_STR_PROC_ACCESS, strlen(CMD_STR_PROC_ACCESS)) == 0) {
        cmd.type = CMD_PROC_ACCESS;
        sscanf(cmd_str, "proc access %u %u", &cmd.args.pid, &cmd.args.size);
    } else if (strncmp(cmd_str, CMD_STR_PROC_ALLOC, strlen(CMD_STR_PROC_ALLOC)) == 0) {
        cmd.type = CMD_PROC_ALLOC;
        sscanf(cmd_str, "proc alloc %u %u %u", &cmd.args.pid, &cmd.args.size, &cmd.args.flags);
    }
    
    return cmd;
}

void show_help(void) {
    printf("\n=== 系统命令 ===\n");
    printf("\n基本命令\n");
    printf("help [command]          - 显示帮助信息\n");
    printf("status                  - 显示系统状态\n");
    printf("exit                    - 退出系统\n");
    
    printf("\n进程管理\n");
    printf("app create <pid> <prio> <code_pages> <data_pages> - 创建进程\n");
    printf("proc kill <pid>         - 终止进程\n");
    printf("proc list               - 显示进程列表\n");
    printf("proc info <pid>         - 显示进程信息\n");
    printf("proc prio <pid> <prio>  - 设置优先级\n");
    printf("proc access <pid> <count> - 模拟进程内存访问\n");
    printf("proc alloc <pid> <size> <type> - 分配内存(type:0堆/1栈)\n");
    
    printf("\n内存管理\n");
    printf("mem map                 - 显示内存映射\n");
    printf("mem stat                - 显示内存统计\n");
    
    printf("\n虚拟内存管理\n");
    printf("vm page <in/out> <pid> <page> - 页面操作\n");
    printf("vm swap <list/clean>    - 交换区管理\n");
    printf("vm stat                 - 显示虚拟内存统计\n");
    
    printf("\n提示：\n");
    printf("1. 内存大小单位为字节\n");
    printf("2. 优先级范围：0(高) - 2(低)\n");
    printf("3. 页大小：4096字节\n");
    
    printf("proc time <pid> <time>  - 设置进程时间片\n");
    printf("mem strategy <type>      - 设置内存分配策略(first/best/worst)\n");
}

void show_detailed_help(CommandType cmd_type) {
    switch (cmd_type) {
        case CMD_MEM_ALLOC:
            printf("\n内存分配命令详细说明\n");
            printf("mem alloc <pid> <addr> <size> [flags]\n");
            printf("  pid  - 进程ID\n");
            printf("  addr - 起始地址，格式：0x1000\n");
            printf("  size - 大小（字节数）\n");
            printf("  flags - 可选项\n");
            printf("    -r  只读\n");
            printf("    -w  写入\n");
            printf("    -x  执行\n");
            printf("示例：mem alloc 1 0x1000 4096 -w\n");
            break;
            
        default:
            printf("无效命令\n");
            break;
    }
}

void ui_init(void) {
    printf("欢迎使用内存管理系统\n");
    printf("输入 'help' 获取帮助\n");
}

void ui_run(void) {
    char cmd_str[MAX_CMD_LEN];
    Command cmd;
    
    while (1) {
        printf("\n> ");
        if (!fgets(cmd_str, MAX_CMD_LEN, stdin)) break;
        
        cmd = parse_command(cmd_str);
        if (cmd.type == CMD_EXIT) break;
        
        handle_command(&cmd);
        
        // 释放参数
        if (cmd.args.text) {
            free(cmd.args.text);
            cmd.args.text = NULL;
        }
    }
}

void ui_shutdown(void) {
    printf("系统退出...\n");
}

// 内存访问检查函数
bool check_memory_access(PCB* process, uint32_t addr) {
    if (!process) return false;
    
    // 计算页号
    uint32_t page_num = addr / PAGE_SIZE;
    
    // 检查各个段的范围
    // 代码段
    if (page_num >= process->memory_layout.code.start_page && 
        page_num < process->memory_layout.code.start_page + process->memory_layout.code.num_pages) {
        return true;
    }
    
    // 数据段
    if (page_num >= process->memory_layout.data.start_page && 
        page_num < process->memory_layout.data.start_page + process->memory_layout.data.num_pages) {
        return true;
    }
    
    // 堆区
    if (page_num >= process->memory_layout.heap.start_page && 
        page_num < process->memory_layout.heap.start_page + process->memory_layout.heap.num_pages) {
        return true;
    }
    
    // 栈区
    if (page_num >= process->memory_layout.stack.start_page && 
        page_num < process->memory_layout.stack.start_page + process->memory_layout.stack.num_pages) {
        return true;
    }
    
    return false;
}

// 打印内存段状态的 ASCII 图形
void print_segment_ascii(PCB* process, uint32_t start_page, uint32_t num_pages) {
    for (uint32_t i = 0; i < num_pages; i++) {
        uint32_t page = start_page + i;
        if (page < process->page_table_size) {
            if (process->page_table[page].flags.present) {
                printf("■ ");
            } else if (process->page_table[page].flags.swapped) {
                printf("S ");
            } else {
                printf("□ ");
            }
        }
    }
}

// 打印内存布局
void print_memory_layout(PCB* process) {
    if (!process) return;
    
    printf("\n=== 进程 %u 内存布局 ===\n", process->pid);
    printf("代码段：页号 %u-%u (%u 页)%s\n",
           process->memory_layout.code.start_page,
           process->memory_layout.code.start_page + process->memory_layout.code.num_pages - 1,
           process->memory_layout.code.num_pages,
           process->memory_layout.code.is_allocated ? " [已分配]" : "");
    
    printf("数据段：页号 %u-%u (%u 页)%s\n",
           process->memory_layout.data.start_page,
           process->memory_layout.data.start_page + process->memory_layout.data.num_pages - 1,
           process->memory_layout.data.num_pages,
           process->memory_layout.data.is_allocated ? " [已分配]" : "");
    
    printf("堆区：  页号 %u-%u (%u 页)%s\n",
           process->memory_layout.heap.start_page,
           process->memory_layout.heap.start_page + process->memory_layout.heap.num_pages - 1,
           process->memory_layout.heap.num_pages,
           process->memory_layout.heap.is_allocated ? " [已分配]" : "");
    
    printf("栈区：  页号 %u-%u (%u 页)%s\n",
           process->memory_layout.stack.start_page,
           process->memory_layout.stack.start_page + process->memory_layout.stack.num_pages - 1,
           process->memory_layout.stack.num_pages,
           process->memory_layout.stack.is_allocated ? " [已分配]" : "");
}

// 系统状态图形显示
void print_system_status_graph(void) {
    printf("\n=== 系统内存使用情况 ===\n\n");
    
    // 显示图例
    printf("图例: □空闲 ■已用 ?交换 ?未分配\n\n");
    
    // 显示物理内存使用情况
    printf("物理内存使用:\n");
    for (int i = 0; i < PHYSICAL_PAGES; i++) {
        if (i % 32 == 0) printf("%3d: ", i);
        if (memory_manager.frames[i].is_allocated)
            printf("■");
        else
            printf("□");
        if ((i + 1) % 32 == 0) printf("\n");
    }
    
    printf("\n=== 进程内存映射 ===\n");
    for (uint32_t i = 0; i < scheduler.total_processes; i++) {
        PCB* process = get_process_by_pid(i + 1);
        if (process) {
            printf("\n进程 %d (%s):\n", process->pid, process->name);
            
            // 显示内存段使用情况
            printf("     0  1  2  3  4  5  6  7  8  9\n");
            printf("代码 ");
            print_segment_ascii(process, process->memory_layout.code.start_page,
                              process->memory_layout.code.num_pages);
            printf("\n数据 ");
            print_segment_ascii(process, process->memory_layout.data.start_page,
                              process->memory_layout.data.num_pages);
            printf("\n堆   ");
            print_segment_ascii(process, process->memory_layout.heap.start_page,
                              process->memory_layout.heap.num_pages);
            printf("\n栈   ");
            print_segment_ascii(process, process->memory_layout.stack.start_page,
                              process->memory_layout.stack.num_pages);
            printf("\n");
        }
    }
}

// 处理用户命令
void handle_command(Command* cmd) {
    PCB* process;
    uint32_t block;
    
    switch (cmd->type) {
        case CMD_HELP:
            show_help();
            break;
            
        case CMD_STATUS:
            printf("\n=== 系统状态 ===\n");
            print_scheduler_status();
            print_memory_map();
            print_vm_stats();
            print_storage_status();
            break;
            
        case CMD_PROC_CREATE:
            // 为新进程设置默认内存布局
            MemoryLayout default_layout = {
                .code_start = 0x1000,
                .code_size = PAGE_SIZE,
                .data_start = 0x2000,
                .data_size = PAGE_SIZE,
                .heap_start = 0x3000,
                .heap_size = PAGE_SIZE * 5,  // 5页堆空间
                .stack_start = 0x8000,       // 给堆区留出足够空间
                .stack_size = PAGE_SIZE * 5   // 5页栈空间
            };
            
            // 计算需要的页表大小
            uint32_t page_table_size = (default_layout.stack_start + default_layout.stack_size) / PAGE_SIZE;
            
            process = process_create(cmd->args.pid, page_table_size);
            if (process) {
                printf("进程 %u 创建成功\n", cmd->args.pid);
                // 创建默认内存布局
                ProcessMemoryLayout default_memory_layout = {
                    .code = {
                        .start_page = 0,
                        .num_pages = 1,
                        .is_allocated = true
                    },
                    .data = {
                        .start_page = 1,
                        .num_pages = 1,
                        .is_allocated = true
                    },
                    .heap = {
                        .start_page = 2,
                        .num_pages = 5,
                        .is_allocated = false
                    },
                    .stack = {
                        .start_page = 7,
                        .num_pages = 5,
                        .is_allocated = false
                    }
                };
                process->memory_layout = default_memory_layout;
            } else {
                printf("进程创建失败\n");
            }
            break;
            
        case CMD_PROC_KILL:
            process = get_process_by_pid(cmd->args.pid);
            if (process) {
                process_destroy(process);
                printf("进程 %u 已终止\n", cmd->args.pid);
            } else {
                printf("找不到进程 %u\n", cmd->args.pid);
            }
            break;
            
        case CMD_PROC_LIST:
            print_scheduler_status();
            break;
            
        case CMD_PROC_INFO:
            process = get_process_by_pid(cmd->args.pid);
            if (process) {
                printf("\n进程 %u 信息：\n", process->pid);
                print_process_status(process->state);
                printf("优先级：%d\n", process->priority);
                printf("时间片：%u\n", process->time_slice);
                print_process_stats(process);
            } else {
                printf("找不到进程 %u\n", cmd->args.pid);
            }
            break;
            
        case CMD_PROC_PRIO:
            process = get_process_by_pid(cmd->args.pid);
            if (process) {
                ProcessPriority new_priority = (ProcessPriority)cmd->args.flags;
                if (new_priority >= PRIORITY_HIGH && new_priority <= PRIORITY_LOW) {
                    set_process_priority(process, new_priority);
                    printf("进程 %u 的优先级已更新为 %d\n", cmd->args.pid, new_priority);
                } else {
                    printf("无效的优先级值（应为 0-2）\n");
                }
            } else {
                printf("找不到进程 %u\n", cmd->args.pid);
            }
            break;
            
        case CMD_MEM_ALLOC:
            process = get_process_by_pid(cmd->args.pid);
            if (process) {
                if (process_allocate_memory(process, cmd->args.addr, cmd->args.size)) {
                    printf("内存分配成功\n");
                }
            } else {
                printf("找不到进程 %u\n", cmd->args.pid);
            }
            break;
            
        case CMD_MEM_FREE:
            process = get_process_by_pid(cmd->args.pid);
            if (process) {
                process_free_memory(process, cmd->args.addr, cmd->args.size);
                printf("内存已释放\n");
            } else {
                printf("找不到进程 %u\n", cmd->args.pid);
            }
            break;
            
        case CMD_MEM_MAP:
            print_memory_map();
            break;
            
        case CMD_VM_PAGE:
            process = get_process_by_pid(cmd->args.pid);
            if (process) {
                if (cmd->args.flags) {  // page in
                    if (page_in(process, cmd->args.addr / PAGE_SIZE)) {
                        printf("页面调入成功\n");
                    }
                } else {  // page out
                    if (page_out(process, cmd->args.addr / PAGE_SIZE)) {
                        printf("页面调出成功\n");
                    }
                }
            } else {
                printf("找不到进程 %u\n", cmd->args.pid);
            }
            break;
            
        case CMD_VM_SWAP:
            if (cmd->args.flags) {  // clean
                clean_swap_area();
                printf("交换区已清理\n");
            } else {  // list
                print_swap_status();
            }
            break;
            
        case CMD_VM_STAT:
            print_vm_stats();
            break;
            
        case CMD_DISK_ALLOC:
            block = storage_allocate(cmd->args.size);
            if (block != (uint32_t)-1) {
                printf("分配磁盘块成功：%u\n", block);
            } else {
                printf("磁盘空间分配失败\n");
            }
            break;
            
        case CMD_DISK_FREE:
            storage_free(cmd->args.addr);
            printf("磁盘块 %u 已释放\n", cmd->args.addr);
            break;
            
        case CMD_DISK_STAT:
            print_storage_status();
            break;
            
        case CMD_STATE_SAVE:
            if (dump_system_state(cmd->args.text ? cmd->args.text : "system.dump")) {
                printf("系统状态保存成功\n");
            } else {
                printf("系统状态保存失败\n");
            }
            break;
            
        case CMD_STATE_LOAD:
            if (restore_system_state(cmd->args.text ? cmd->args.text : "system.dump")) {
                printf("系统状态恢复成功\n");
            } else {
                printf("系统状态恢复失败\n");
            }
            break;
            
        case CMD_STATE_RESET:
            scheduler_shutdown();
            vm_shutdown();
            memory_init();
            vm_init();
            scheduler_init();
            printf("系统已重置\n");
            break;
            
        case CMD_APP_CREATE: {
            PCB* process = create_process_with_pid(
                cmd->args.pid,
                cmd->args.priority,
                cmd->args.code_pages ? cmd->args.code_pages : 1,
                cmd->args.data_pages ? cmd->args.data_pages : 1
            );
            
            if (!process) {
                printf("创建进程失败\n");
                return;
            }
            
            printf("创建进程成功，PID=%u\n", process->pid);
            printf("内存布局：\n");
            printf("代码段：%u页\n", process->memory_layout.code.num_pages);
            printf("数据段：%u页\n", process->memory_layout.data.num_pages);
            printf("堆：%u页（未分配）\n", process->memory_layout.heap.num_pages);
            printf("栈：%u页（未分配）\n", process->memory_layout.stack.num_pages);
            break;
        }
            
        case CMD_APP_MONITOR:
            process = get_process_by_pid(cmd->args.pid);
            if (process) {
                monitor_process(process);
            } else {
                printf("找不到进程 %u\n", cmd->args.pid);
            }
            break;
            
        case CMD_MEM_PROFILE:
            printf("\n=== 系统内存使用分析 ===\n");
            PCB* analyzed[MAX_PROCESSES] = {NULL};  // 记录已分析的进程
            int analyzed_count = 0;
            
            for (int i = 0; i < 3; i++) {
                PCB* current = scheduler.ready_queue[i];
                while (current) {
                    bool already_analyzed = false;
                    for (int j = 0; j < analyzed_count; j++) {
                        if (analyzed[j] == current) {
                            already_analyzed = true;
                            break;
                        }
                    }
                    if (!already_analyzed) {
                        analyze_memory_usage(current);
                        analyzed[analyzed_count++] = current;
                    }
                    current = current->next;
                }
            }
            if (scheduler.running_process && 
                !is_process_analyzed(scheduler.running_process, analyzed, analyzed_count)) {
                analyze_memory_usage(scheduler.running_process);
            }
            break;
            
        case CMD_MEM_OPTIMIZE:
            process = get_process_by_pid(cmd->args.pid);
            if (process) {
                optimize_memory_layout(process);
            } else {
                printf("找不到进程 %u\n", cmd->args.pid);
            }
            break;
            
        case CMD_MEM_BALANCE:
            balance_memory_usage();
            break;
            
        case CMD_APP_RUN:
            process = get_process_by_pid(cmd->args.pid);
            if (process) {
                if (process->state == PROCESS_READY) {
                    process->state = PROCESS_RUNNING;
                    set_running_process(process);
                    printf("应用程序 PID %u 已启动\n", cmd->args.pid);
                    printf("使用 'time tick [n]' 命令来模拟时间片轮转\n");
                } else {
                    printf("应用程序 PID %u 无法启动，当前状态：%d\n", cmd->args.pid, process->state);
                }
            } else {
                printf("找不到进程 %u\n", cmd->args.pid);
            }
            break;
            
        case CMD_TIME_TICK:
            for (uint32_t i = 0; i < cmd->args.size; i++) {
                time_tick();
                printf("\n=== 时间片 %u ===\n", i + 1);
                print_scheduler_status();
                Sleep(1000);  // 延迟1秒以便观察
            }
            break;
            
        case CMD_MEM_WRITE:
            process = get_process_by_pid(cmd->args.pid);
            if (!process) {
                printf("找不到进程 %u\n", cmd->args.pid);
                break;
            }
            
            // 检查地址是否有效
            bool address_valid = false;
            if (check_memory_access(process, cmd->args.addr)) {
                address_valid = true;
            }
            
            if (!address_valid) {
                printf("无效的内存地址: 0x%x\n", cmd->args.addr);
                break;
            }
            
            // 写入内存
            if (cmd->args.text) {
                size_t len = strlen(cmd->args.text);
                for (size_t i = 0; i < len; i++) {
                    access_memory(process, cmd->args.addr + i, true);  // 写入操作
                    // 实际写入数据
                    uint32_t frame = process->page_table[cmd->args.addr / PAGE_SIZE].frame_number;
                    uint32_t offset = cmd->args.addr % PAGE_SIZE;
                    uint8_t* phys_addr = (uint8_t*)get_physical_address(frame);
                    if (phys_addr) {
                        phys_addr[offset + i] = cmd->args.text[i];
                    }
                }
                printf("成功写入 %zu 字节到地址 0x%x\n", len, cmd->args.addr);
            }
            break;
            
        case CMD_MEM_READ:
            process = get_process_by_pid(cmd->args.pid);
            if (!process) {
                printf("找不到进程 %u\n", cmd->args.pid);
                break;
            }
            
            // 检查地址是否有效
            if (!check_memory_access(process, cmd->args.addr)) {
                printf("无效的内存地址: 0x%x\n", cmd->args.addr);
                break;
            }
            
            // 读取内存
            access_memory(process, cmd->args.addr, false);  // 读取操作
            uint32_t frame = process->page_table[cmd->args.addr / PAGE_SIZE].frame_number;
            uint32_t offset = cmd->args.addr % PAGE_SIZE;
            uint8_t* phys_addr = (uint8_t*)get_physical_address(frame);
            if (phys_addr) {
                printf("地址 0x%x 的内容:\n", cmd->args.addr);
                // 显示十六进制
                printf("HEX:  ");
                for (int i = 0; i < 16 && (offset + i) < PAGE_SIZE; i++) {
                    printf("%02x ", phys_addr[offset + i]);
                }
                printf("\n");
                
                // 显示ASCII
                printf("ASCII: ");
                for (int i = 0; i < 16 && (offset + i) < PAGE_SIZE; i++) {
                    char c = phys_addr[offset + i];
                    // 只显示可打印字符，其他显示为点
                    printf("%c  ", (c >= 32 && c <= 126) ? c : '.');
                }
                printf("\n");
            }
            break;
            
        case CMD_UNKNOWN:
            printf("未知命令\n");
            break;
            
        case CMD_PROC_ACCESS:
            simulate_process_memory_access(cmd->args.pid, cmd->args.size);
            break;
            
        case CMD_PROC_ALLOC:
            allocate_process_memory(cmd->args.pid, cmd->args.size, cmd->args.flags);
            break;
            
        case CMD_PROC_TIME:
            process = get_process_by_pid(cmd->args.pid);
            if (process) {
                if (cmd->args.time_slice > 0) {
                    process->time_slice = cmd->args.time_slice;
                    printf("进程 %u 的时间片已更新为 %u\n", 
                           cmd->args.pid, cmd->args.time_slice);
                } else {
                    printf("错误：时间片必须大于0\n");
                }
            } else {
                printf("找不到进程 %u\n", cmd->args.pid);
            }
            break;
            
        case CMD_MEM_STRATEGY:
            switch (cmd->args.flags) {
                case FIRST_FIT:
                    set_allocation_strategy(FIRST_FIT);
                    printf("内存分配策略已设置为首次适应算法\n");
                    break;
                case BEST_FIT:
                    set_allocation_strategy(BEST_FIT);
                    printf("内存分配策略已设置为最佳适应算法\n");
                    break;
                case WORST_FIT:
                    set_allocation_strategy(WORST_FIT);
                    printf("内存分配策略已设置为最差适应算法\n");
                    break;
                default:
                    printf("无效的内存分配策略\n");
                    printf("可用策略：\n");
                    printf("  first - 首次适应算法\n");
                    printf("  best  - 最佳适应算法\n");
                    printf("  worst - 最差适应算法\n");
                    break;
            }
            break;
            
        default:
            printf("命令未实现\n");
            break;
    }
} 