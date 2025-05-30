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
                token = strtok(NULL, " \n");  // 应用名称
                if (token) {
                    cmd.args.text = strdup(token);
                    // 读取代码段大小（页数）
                    token = strtok(NULL, " \n");
                    if (token) cmd.args.code_pages = (uint32_t)strtoul(token, NULL, 0);
                    // 读取数据段大小（页数）
                    token = strtok(NULL, " \n");
                    if (token) cmd.args.data_pages = (uint32_t)strtoul(token, NULL, 0);
                }
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
    printf("proc create <pid>       - 创建进程\n");
    printf("proc kill <pid>         - 终止进程\n");
    printf("proc list              - 显示进程列表\n");
    printf("proc info <pid>        - 显示进程信息\n");
    printf("proc prio <pid> <prio> - 设置优先级\n");
    
    printf("\n内存管理\n");
    printf("mem alloc <pid> <addr> <size> [flags] - 分配内存\n");
    printf("mem free <pid> <addr> <size>         - 释放内存\n");
    printf("mem read <pid> <addr> <size>         - 读取内存\n");
    printf("mem write <pid> <addr> \"text\"        - 写入内存\n");
    printf("mem map <pid>                        - 显示内存映射\n");
    printf("mem stat                             - 显示内存统计\n");
    
    printf("\n虚拟内存管理\n");
    printf("vm page <in/out> <pid> <page>       - 页面操作\n");
    printf("vm swap <list/clean>                - 交换区管理\n");
    printf("vm stat                             - 显示虚拟内存统计\n");
    
    printf("\n磁盘管理\n");
    printf("disk alloc <size>                   - 分配磁盘空间\n");
    printf("disk free <block>                   - 释放磁盘空间\n");
    printf("disk read <block>                   - 读取磁盘\n");
    printf("disk write <block> \"text\"           - 写入磁盘\n");
    printf("disk stat                           - 显示磁盘状态\n");
    
    printf("\n系统状态管理\n");
    printf("state save [file]                   - 保存系统状态\n");
    printf("state load [file]                   - 加载系统状态\n");
    printf("state reset                         - 重置系统状态\n");

    printf("\n提示：\n");
    printf("地址格式：0x1000，大小格式：字节数\n");
    printf("优先级格式：0-3，数字越小优先级越高\n");
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
                // 设置默认内存布局
                process->memory_layout = default_layout;
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
            
        case CMD_APP_CREATE:
            AppConfig config = {0};
            
            // 设置内存布局
            config.layout.code_start = 0x1000;
            config.layout.code_size = PAGE_SIZE * (cmd->args.code_pages > 0 ? cmd->args.code_pages : 1);
            config.layout.data_start = config.layout.code_start + config.layout.code_size;
            config.layout.data_size = PAGE_SIZE * (cmd->args.data_pages > 0 ? cmd->args.data_pages : 1);
            config.layout.heap_start = config.layout.data_start + config.layout.data_size;
            config.layout.heap_size = PAGE_SIZE * 5;  // 5页堆空间
            config.layout.stack_start = config.layout.heap_start + config.layout.heap_size;
            config.layout.stack_size = PAGE_SIZE * 5;  // 5页栈空间
            
            // 计算需要的总页表大小
            config.page_table_size = (config.layout.stack_start + config.layout.stack_size) / PAGE_SIZE;
            
            PCB* app = create_application(cmd->args.text, &config);
            if (app) {
                printf("应用程序 '%s' 创建成功，PID=%u\n", cmd->args.text, app->pid);
                printf("已分配内存：%u 字节\n", config.min_memory);
                printf("内存布局：\n");
                printf("  代码段：0x%08x - 0x%08x\n", 
                       config.layout.code_start,
                       config.layout.code_start + config.layout.code_size);
                printf("  数据段：0x%08x - 0x%08x\n",
                       config.layout.data_start,
                       config.layout.data_start + config.layout.data_size);
                printf("  堆区：  0x%08x - 0x%08x\n",
                       config.layout.heap_start,
                       config.layout.heap_start + config.layout.heap_size);
                printf("  栈区：  0x%08x - 0x%08x\n",
                       config.layout.stack_start,
                       config.layout.stack_start + config.layout.stack_size);
            }
            break;
            
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
            process = get_process_by_pid(cmd->args.pid);
            if (process) {
                balance_memory_usage(process);
            } else {
                printf("找不到进程 %u\n", cmd->args.pid);
            }
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
            // 检查代码段
            if (cmd->args.addr >= process->memory_layout.code_start && 
                cmd->args.addr < process->memory_layout.code_start + process->memory_layout.code_size) {
                address_valid = true;
            }
            // 检查数据段
            else if (cmd->args.addr >= process->memory_layout.data_start && 
                cmd->args.addr < process->memory_layout.data_start + process->memory_layout.data_size) {
                address_valid = true;
            }
            // 检查堆区
            else if (cmd->args.addr >= process->memory_layout.heap_start && 
                cmd->args.addr < process->memory_layout.heap_start + process->memory_layout.heap_size) {
                address_valid = true;
            }
            // 检查栈区
            else if (cmd->args.addr >= process->memory_layout.stack_start && 
                cmd->args.addr < process->memory_layout.stack_start + process->memory_layout.stack_size) {
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
            if (cmd->args.addr >= process->memory_layout.code_start + 
                process->memory_layout.code_size + 
                process->memory_layout.data_size + 
                process->memory_layout.heap_size + 
                process->memory_layout.stack_size) {
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
            
        default:
            printf("命令未实现\n");
            break;
    }
}

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
            print_segment_ascii(process, process->memory_layout.code_start,
                              process->memory_layout.code_size);
            printf("\n数据 ");
            print_segment_ascii(process, process->memory_layout.data_start,
                              process->memory_layout.data_size);
            printf("\n堆   ");
            print_segment_ascii(process, process->memory_layout.heap_start,
                              process->memory_layout.heap_size);
            printf("\n栈   ");
            print_segment_ascii(process, process->memory_layout.stack_start,
                              process->memory_layout.stack_size);
            printf("\n");
        }
    }
}

// 新增：打印内存段状态的 ASCII 图形
void print_segment_ascii(PCB* process, uint32_t start_addr, uint32_t size) {
    uint32_t start_page = start_addr / PAGE_SIZE;
    uint32_t page_count = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    
    for (uint32_t i = 0; i < page_count; i++) {
        uint32_t page = start_page + i;
        if (process->page_table[page].flags.present)
            printf("■ ");
        else if (process->page_table[page].flags.swapped)
            printf("? ");
        else
            printf("? ");
    }
} 