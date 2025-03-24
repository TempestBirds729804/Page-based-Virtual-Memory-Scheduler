#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../include/process.h"
#include "../include/memory.h"
#include "../include/vm.h"

// 进程表
static PCB processes[MAX_PROCESSES];  // 进程控制块表

// 全进程调度器
ProcessScheduler scheduler;

// 初始化进程调度器
void scheduler_init(void) {
    // 初始化调度器结构
    memset(&scheduler, 0, sizeof(ProcessScheduler));
    scheduler.next_pid = 1;
    scheduler.total_runtime = 0;
    scheduler.auto_balance = true;

    // 初始化进程表
    for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
        processes[i].pid = 0;
        processes[i].state = PROCESS_TERMINATED;
        processes[i].page_table = NULL;
        processes[i].page_table_size = 0;
    }

    // 初始化就绪队列
    for (int i = 0; i < 3; i++) {
        scheduler.ready_queue[i] = NULL;
    }
    scheduler.blocked_queue = NULL;
    scheduler.running_process = NULL;
    scheduler.total_processes = 0;
}

// 关闭进程调度器
void scheduler_shutdown(void) {
    // 遍历就绪队列
    for (int i = 0; i < 3; i++) {
        PCB* current = scheduler.ready_queue[i];
        while (current) {
            PCB* next = current->next;
            process_destroy(current);
            current = next;
        }
        scheduler.ready_queue[i] = NULL;
    }

    // 遍历阻塞队列
    PCB* current = scheduler.blocked_queue;
    while (current) {
        PCB* next = current->next;
        process_destroy(current);
        current = next;
    }
    scheduler.blocked_queue = NULL;

    // 当前运行进程
    if (scheduler.running_process) {
        process_destroy(scheduler.running_process);
        scheduler.running_process = NULL;
    }
}

// 调度
void schedule(void) {
    // 当前没有运行进程，从优先级最高的进程开始调度
    if (!scheduler.running_process) {
        printf("\n开始进程调度...\n");
        for (int i = 0; i < 3; i++) {
            if (scheduler.ready_queue[i]) {
                scheduler.running_process = scheduler.ready_queue[i];
                scheduler.ready_queue[i] = scheduler.ready_queue[i]->next;
                scheduler.running_process->next = NULL;
                scheduler.running_process->state = PROCESS_RUNNING;
                printf("调度进程 PID %u (优先级 %d) 开始运行，时间片 %u\n", 
                       scheduler.running_process->pid,
                       scheduler.running_process->priority,
                       scheduler.running_process->time_slice);
                return;  // 找到进程后立即返回
            } else {
                printf("优先级 %d 队列为空\n", i);
            }
        }
        printf("没有可运行的进程\n");
    } else {
        printf("当前有进程正在运行（PID %u，优先级 %u）\n",
               scheduler.running_process->pid,
               scheduler.running_process->priority);
    }
}

// 时钟滴答
void time_tick(void) {
    if (scheduler.running_process) {
        printf("\n当前运行进程 PID %u，剩余时间片 %u\n", 
               scheduler.running_process->pid,
               scheduler.running_process->time_slice);
        
        scheduler.running_process->time_slice--;
        if (scheduler.running_process->time_slice == 0) {
            printf("进程 PID %u 时间片用完，进程结束\n", scheduler.running_process->pid);
            
            // 时间片用完，终止进程
            PCB* proc = scheduler.running_process;
            process_destroy(proc);
            scheduler.running_process = NULL;
            
            // 调度下一个进程
            schedule();
            
            if (scheduler.running_process) {
                printf("进程调度成功，新进程 PID %u 开始运行\n", scheduler.running_process->pid);
            } else {
                printf("没有可运行的进程\n");
            }
        }
    }
}

// 获取就绪队列
PCB* get_ready_queue(ProcessPriority priority) {
    if (priority >= PRIORITY_HIGH && priority <= PRIORITY_LOW) {
        return scheduler.ready_queue[priority];
    }
    return NULL;
}

// 获取当前运行进程
PCB* get_running_process(void) {
    return scheduler.running_process;
}

// 获取进程总数
uint32_t get_total_processes(void) {
    return scheduler.total_processes;
}

// 设置进程优先级
void set_process_priority(PCB* process, ProcessPriority priority) {
    if (!process || priority < PRIORITY_HIGH || priority > PRIORITY_LOW) {
        return;
    }
    
    // 进程优先级不能改变，直接返回
    if (process->priority == priority) {
        return;
    }
    
    // 删除进程从旧优先级队列
    PCB** old_queue = &scheduler.ready_queue[process->priority];
    if (*old_queue == process) {
        *old_queue = process->next;
    } else {
        PCB* current = *old_queue;
        while (current && current->next != process) {
            current = current->next;
        }
        if (current) {
            current->next = process->next;
        }
    }
    
    // 设置进程优先级
    process->priority = priority;
    
    // 将进程添加到新优先级队列的队首
    process->next = scheduler.ready_queue[priority];
    scheduler.ready_queue[priority] = process;
}

// 打印调度器状态
void print_scheduler_status(void) {
    printf("\n=== 调度器状态 ===\n");
    printf("进程总数%u\n", scheduler.total_processes);
    if (scheduler.running_process) {
        printf("正在运行的进程，PID %u\n", scheduler.running_process->pid);
    } else {
        printf("没有正在运行的进程\n");
    }
    
    // 打印就绪队列
    for (int i = 0; i < 3; i++) {
        printf("\n优先级 %d 进程队列：", i);
        PCB* current = scheduler.ready_queue[i];
        if (!current) {
            printf("空");
        }
        while (current) {
            printf("PID %u -> ", current->pid);
            current = current->next;
        }
        printf("\n");
    }
}

// 创建进程
PCB* process_create(uint32_t pid, uint32_t page_table_size) {
    PCB* process = (PCB*)malloc(sizeof(PCB));
    if (!process) return NULL;
    
    // 初始化进程信息
    process->pid = pid;
    process->state = PROCESS_READY;
    process->priority = PRIORITY_NORMAL;  // 使用枚举值PRIORITY_NORMAL作为默认优先级
    process->time_slice = TIME_SLICE;
    
    // 分配页表
    process->page_table = (PageTableEntry*)calloc(page_table_size, sizeof(PageTableEntry));
    if (!process->page_table) {
        printf("进程内存分配失败，无法分配页表\n");
        free(process);
        return NULL;
    }
    process->page_table_size = page_table_size;
    
    // 初始化进程统计信息
    memset(&process->stats, 0, sizeof(ProcessStats));
    
    return process;
}

// 销毁进程
void process_destroy(PCB* pcb) {
    if (!pcb) return;
    
    printf("销毁进程 %u\n", pcb->pid);
    
    // 1. 先将进程从调度器中移除
    if (scheduler.running_process == pcb) {
        scheduler.running_process = NULL;
    }
    
    // 2. 从就绪队列中移除
    PCB** queue = &scheduler.ready_queue[pcb->priority];
    if (*queue == pcb) {
        *queue = pcb->next;
    } else {
        PCB* current = *queue;
        while (current && current->next != pcb) {
            current = current->next;
        }
        if (current) {
            current->next = pcb->next;
        }
    }
    
    // 3. 设置进程状态为终止
    pcb->state = PROCESS_TERMINATED;
    
    // 4. 释放进程占用的页框
    if (pcb->page_table) {
        for (uint32_t i = 0; i < pcb->page_table_size; i++) {
            if (pcb->page_table[i].flags.present) {
                uint32_t frame = pcb->page_table[i].frame_number;
                pcb->page_table[i].flags.present = false;
                pcb->page_table[i].frame_number = (uint32_t)-1;
                free_frame(frame);
            }
        }
        // 5. 释放页表
        free(pcb->page_table);
        pcb->page_table = NULL;
    }
    
    scheduler.total_processes--;
}

// 为进程分配内存
bool process_allocate_memory(PCB* pcb, uint32_t start_page, uint32_t num_pages) {
    for (uint32_t page = start_page; page < start_page + num_pages; page++) {
        pcb->page_table[page].frame_number = (uint32_t)-1;
        pcb->page_table[page].flags.present = false;
        pcb->page_table[page].flags.swapped = false;
        pcb->page_table[page].last_access_time = get_current_time();
    }
    return true;
}

// 释放进程占用的内存
void process_free_memory(PCB* pcb, uint32_t start_page, uint32_t num_pages) {
    for (uint32_t page = start_page; page < start_page + num_pages; page++) {
        if (page < pcb->page_table_size) {
            pcb->page_table[page].frame_number = (uint32_t)-1;
            pcb->page_table[page].flags.present = false;
            pcb->page_table[page].flags.swapped = false;
        }
    }
}

// 根据PID获取进程
PCB* get_process_by_pid(uint32_t pid) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].pid == pid && processes[i].state != PROCESS_TERMINATED) {
            return &processes[i];
        }
    }
    return NULL;
}

// 打印进程统计信息
void print_process_stats(PCB* process) {
    if (!process) return;
    
    printf("\n=== 进程 %u 内存使用统计 ===\n", process->pid);
    printf("\n内存布局：\n");
    printf("代码段：起始页=%u, 页数=%u, 状态=%s\n",
           process->memory_layout.code.start_page,
           process->memory_layout.code.num_pages,
           process->memory_layout.code.is_allocated ? "已分配" : "未分配");
    printf("数据段：起始页=%u, 页数=%u, 状态=%s\n",
           process->memory_layout.data.start_page,
           process->memory_layout.data.num_pages,
           process->memory_layout.data.is_allocated ? "已分配" : "未分配");
    printf("堆：起始页=%u, 页数=%u, 状态=%s\n",
           process->memory_layout.heap.start_page,
           process->memory_layout.heap.num_pages,
           process->memory_layout.heap.is_allocated ? "已分配" : "未分配");
    printf("栈：起始页=%u, 页数=%u, 状态=%s\n",
           process->memory_layout.stack.start_page,
           process->memory_layout.stack.num_pages,
           process->memory_layout.stack.is_allocated ? "已分配" : "未分配");
    
    printf("\n内存访问统计：\n");
    uint32_t total_accesses = process->stats.mem_stats.total_accesses;
    if (total_accesses > 0) {
        printf("代码段访问次数：%u次 (%.1f%%)\n",
               process->stats.mem_stats.code_accesses,
               (float)process->stats.mem_stats.code_accesses * 100.0 / total_accesses);
        printf("数据段访问次数：%u次 (%.1f%%)\n",
               process->stats.mem_stats.data_accesses,
               (float)process->stats.mem_stats.data_accesses * 100.0 / total_accesses);
        printf("堆访问次数：%u次 (%.1f%%)\n",
               process->stats.mem_stats.heap_accesses,
               (float)process->stats.mem_stats.heap_accesses * 100.0 / total_accesses);
        printf("栈访问次数：%u次 (%.1f%%)\n",
               process->stats.mem_stats.stack_accesses,
               (float)process->stats.mem_stats.stack_accesses * 100.0 / total_accesses);
    }
    
    printf("\n页面统计：\n");
    printf("缺页次数：%u\n", process->stats.page_faults);
    printf("页面替换次数：%u\n", process->stats.page_replacements);
    printf("页面换入次数：%u\n", process->stats.pages_swapped_in);
    printf("页面换出次数：%u\n", process->stats.pages_swapped_out);
    printf("从磁盘读取的页数：%u\n", process->stats.mem_stats.reads_from_disk);
}

// 创建应用程序
PCB* create_application(const char* name, AppConfig* config) {
    if (!config) return NULL;
    
    PCB* process = (PCB*)malloc(sizeof(PCB));
    if (!process) return NULL;
    
    // 初始化进程信息
    process->pid = scheduler.next_pid++;
    strncpy(process->name, name, MAX_PROCESS_NAME - 1);
    process->name[MAX_PROCESS_NAME - 1] = '\0';
    process->state = PROCESS_READY;
    process->priority = config->priority;
    process->time_slice = TIME_SLICE;
    process->next = NULL;
    
    // 设置进程内存布局
    ProcessMemoryLayout proc_layout = {
        .code = {
            .start_page = 0,
            .num_pages = config->layout.code_size / PAGE_SIZE,
            .is_allocated = true
        },
        .data = {
            .start_page = config->layout.code_size / PAGE_SIZE,
            .num_pages = config->layout.data_size / PAGE_SIZE,
            .is_allocated = true
        },
        .heap = {
            .start_page = (config->layout.code_size + config->layout.data_size) / PAGE_SIZE,
            .num_pages = config->layout.heap_size / PAGE_SIZE,
            .is_allocated = false
        },
        .stack = {
            .start_page = (config->layout.code_size + config->layout.data_size + config->layout.heap_size) / PAGE_SIZE,
            .num_pages = config->layout.stack_size / PAGE_SIZE,
            .is_allocated = false
        }
    };
    
    if (!setup_process_memory(process, &proc_layout)) {
        free(process);
        return NULL;
    }
    
    // 设置应用程序信息
    process->app_config = *config;
    
    return process;
}

// 设置进程内存布局
bool setup_process_memory(PCB* process, ProcessMemoryLayout* layout) {
    if (!process || !layout) return false;
    
    // 分配页表
    uint32_t total_pages = layout->code.num_pages + layout->data.num_pages +
                           layout->heap.num_pages + layout->stack.num_pages;
    
    // 分配页表
    process->page_table = (PageTableEntry*)calloc(total_pages, sizeof(PageTableEntry));
    if (!process->page_table) {
        return false;
    }
    process->page_table_size = total_pages;
    
    // 设置内存布局
    process->memory_layout = *layout;
    
    return true;
}

// 监控进程
void monitor_process(PCB* process) {
    if (!process) return;
    
    printf("\n=== 监控进程 %u 信息 ===\n", process->pid);
    
    // 计算内存使用情况
    uint32_t present_pages = 0;
    uint32_t swapped_pages = 0;
    
    for (uint32_t i = 0; i < process->page_table_size; i++) {
        if (process->page_table[i].flags.present) {
            present_pages++;
        }
        if (process->page_table[i].flags.swapped) {
            swapped_pages++;
        }
    }
    
    printf("\n内存使用情况：\n");
    printf("总页数：%u\n", process->page_table_size);
    printf("在内存中的页数：%u\n", present_pages);
    printf("在磁盘中的页数：%u\n", swapped_pages);
    
    printf("\n内存访问统计：\n");
    printf("总访问次数：%u\n", process->stats.mem_stats.total_accesses);
    printf("代码段访问次数：%u\n", process->stats.mem_stats.code_accesses);
    printf("数据段访问次数：%u\n", process->stats.mem_stats.data_accesses);
    printf("堆访问次数：%u\n", process->stats.mem_stats.heap_accesses);
    printf("栈访问次数：%u\n", process->stats.mem_stats.stack_accesses);
    
    printf("\n页面统计：\n");
    printf("缺页次数：%u\n", process->stats.page_faults);
    printf("页面替换次数：%u\n", process->stats.page_replacements);
    printf("页面换入次数：%u\n", process->stats.pages_swapped_in);
    printf("页面换出次数：%u\n", process->stats.pages_swapped_out);
    printf("从磁盘读取的页数：%u\n", process->stats.mem_stats.reads_from_disk);
    
    if (process->stats.mem_stats.total_accesses > 0) {
        float fault_rate = (float)process->stats.page_faults * 100.0 / 
                          process->stats.mem_stats.total_accesses;
        printf("\n页面访问率：\n");
        printf("缺页率：%.2f%%\n", fault_rate);
    }
}

// 分析内存使用情况
void analyze_memory_usage(PCB* process) {
    if (!process) return;
    
    uint32_t used_pages = 0;
    uint64_t current_time = get_current_time();
    uint32_t active_pages = 0;  // 当前活跃页数
    
    // 统计内存使用情况
    for (uint32_t i = 0; i < process->page_table_size; i++) {
        if (process->page_table[i].flags.present) {
            used_pages++;
            // 判断是否为活跃页，1000ms内被访问过
            if (current_time - process->page_table[i].last_access_time < 1000) {
                active_pages++;
            }
        }
    }
    
    printf("\n=== 进程 %u 内存使用情况 ===\n", process->pid);
    printf("总页数：%u\n", process->page_table_size);
    printf("活跃页数：%u (%.1f%%)\n", 
           used_pages, 
           (float)used_pages * 100 / process->page_table_size);
    printf("不活跃页数：%u (%.1f%%)\n", 
           active_pages,
           (float)active_pages * 100 / used_pages);
    
    // 内存访问统计
    uint32_t total_accesses = process->stats.mem_stats.total_accesses;
    if (total_accesses > 0) {
        printf("\n内存访问分布：\n");
        printf("代码段：%.1f%%\n", 
               (float)process->stats.mem_stats.code_accesses * 100 / total_accesses);
        printf("数据段：%.1f%%\n", 
               (float)process->stats.mem_stats.data_accesses * 100 / total_accesses);
        printf("堆：%.1f%%\n", 
               (float)process->stats.mem_stats.heap_accesses * 100 / total_accesses);
        printf("栈：%.1f%%\n", 
               (float)process->stats.mem_stats.stack_accesses * 100 / total_accesses);
    }
    
    // 页面访问统计
    printf("\n页面访问统计：\n");
    printf("缺页次数：%u\n", process->stats.page_faults);
    printf("页面替换次数：%u\n", process->stats.page_replacements);
    if (total_accesses > 0) {
        printf("缺页率：%.2f%%\n", 
               (float)process->stats.page_faults * 100 / total_accesses);
    }
}

// 优化内存布局
void optimize_memory_layout(PCB* process) {
    if (!process) return;
    
    printf("\n=== 内存布局优化 ===\n");
    printf("进程 %u (%s)\n", process->pid, process->app_config.name);
    
    // 1. 合并空闲页
    uint32_t free_start = (uint32_t)-1;
    uint32_t free_count = 0;
    
    for (uint32_t i = 0; i < process->page_table_size; i++) {
        if (!process->page_table[i].flags.present) {
            if (free_start == (uint32_t)-1) {
                free_start = i;
            }
            free_count++;
        } else if (free_count > 0) {
            printf("合并空闲页，起始页=%u，页数=%u\n", free_start, free_count);
            free_start = (uint32_t)-1;
            free_count = 0;
        }
    }
    
    // 2. 移动空闲页
    printf("\n移动空闲页...\n");
    // 实际实现需要根据具体需求实现
}

// 平衡内存使用
void balance_memory_usage(void) {
    for (uint32_t pid = 1; pid <= scheduler.next_pid; pid++) {
        PCB* process = get_process_by_pid(pid);
        if (!process) continue;
        
        for (uint32_t i = 0; i < process->page_table_size; i++) {
            if (process->page_table[i].flags.present) {
                // 判断是否为未分配页
                uint64_t current_time = get_current_time();
                if (current_time - process->page_table[i].last_access_time > 1000) {
                    // 设置为未分配页
                    // ...
                }
            }
        }
    }
}

// 设置当前运行进程
void set_running_process(PCB* process) {
    if (!process) return;
    
    // 将当前运行进程放回就绪队列
    if (scheduler.running_process) {
        scheduler.running_process->state = PROCESS_READY;
        scheduler.running_process->next = scheduler.ready_queue[scheduler.running_process->priority];
        scheduler.ready_queue[scheduler.running_process->priority] = scheduler.running_process;
    }
    
    // 设置运行进程
    scheduler.running_process = process;
    scheduler.running_process->state = PROCESS_RUNNING;
    scheduler.running_process->next = NULL;
}

// 打印进程状态
void print_process_status(ProcessState state) {
    printf("进程状态：");
    switch (state) {
        case PROCESS_NEW:
            printf("新建\n");
            break;
        case PROCESS_READY:
            printf("就绪\n");
            break;
        case PROCESS_RUNNING:
            printf("运行\n");
            break;
        case PROCESS_BLOCKED:
            printf("阻塞\n");
            break;
        case PROCESS_TERMINATED:
            printf("终止\n");
            break;
        default:
            printf("未知状态\n");
            break;
    }
}

// 判断进程是否已分析
bool is_process_analyzed(PCB* process, PCB** analyzed_list, int analyzed_count) {
    if (!process || !analyzed_list) {
        return false;
    }
    
    // 遍历已分析进程列表
    for (int i = 0; i < analyzed_count; i++) {
        if (analyzed_list[i] == process) {
            return true;  // 找到进程，表示已分配
        }
    }
    
    return false;  // 未找到进程，表示未分配
}

// 初始化进程内存
void initialize_process_memory(PCB* process) {
    if (!process) return;
    
    // 只初始化前24页
    for (uint32_t i = 0; i < 24; i++) {
        uint32_t page_num = i + 1;  // 从1开始，0页为空
        uint32_t frame = allocate_frame(process->pid, page_num);
        if (frame == (uint32_t)-1) {
            printf("分配页失败，进程创建失败\n");
            return;
        }
        process->page_table[page_num].frame_number = frame;
        process->page_table[page_num].flags.present = true;
    }
    printf("为进程 %d 分配了 24 页内存\n", process->pid);
}

// 初始化页表
void init_page_table(PCB* process, uint32_t size) {
    process->page_table = (PageTableEntry*)calloc(size, sizeof(PageTableEntry));
    process->page_table_size = size;
    
    // 初始化每页
    for (uint32_t i = 0; i < size; i++) {
        process->page_table[i].frame_number = (uint32_t)-1;
        process->page_table[i].flags.present = false;
        process->page_table[i].flags.swapped = false;
        process->page_table[i].last_access_time = get_current_time();
    }
}

// 添加进程到就绪队列
void add_to_ready_queue(PCB* process) {
    if (!process) return;
    
    printf("[DEBUG] add_to_ready_queue - 进程 %u 的优先级: %u (%s)\n", 
           process->pid, process->priority,
           process->priority == PRIORITY_HIGH ? "高优先级" :
           process->priority == PRIORITY_NORMAL ? "普通优先级" :
           process->priority == PRIORITY_LOW ? "低优先级" : "未知优先级");
    
    // 确保优先级有效（0-2）
    if (process->priority >= 3) {
        printf("警告：进程 %u 优先级 %u 无效，设置为最低优先级\n", 
               process->pid, process->priority);
        process->priority = PRIORITY_LOW;
    }
    
    printf("将进程 %u (优先级 %u) 添加到就绪队列\n", 
           process->pid, process->priority);
    
    // 如果就绪队列对应优先级为空，直接插入
    if (!scheduler.ready_queue[process->priority]) {
        scheduler.ready_queue[process->priority] = process;
        process->next = NULL;
        printf("进程 %u 添加到优先级 %u 就绪队列头部\n", 
               process->pid, process->priority);
        
        // 如果当前没有运行进程，立即进行调度
        if (!scheduler.running_process) {
            schedule();
        }
        return;
    }
    
    // 添加进程到就绪队列尾部
    PCB* current = scheduler.ready_queue[process->priority];
    while (current->next) {
        current = current->next;
    }
    current->next = process;
    process->next = NULL;
    
    printf("进程 %u 添加到优先级 %u 就绪队列尾部\n", 
           process->pid, process->priority);
    
    // 如果当前没有运行进程，立即进行调度
    if (!scheduler.running_process) {
        schedule();
    }
}

// 创建进程
PCB* create_process(uint32_t priority, uint32_t code_pages, uint32_t data_pages) {
    printf("\n=== 创建进程 ===\n");
    printf("代码页数 %u 页，数据页数 %u 页\n", code_pages, data_pages);
    
    // 转换优先级为枚举值
    ProcessPriority proc_priority;
    switch(priority) {
        case 0: proc_priority = PRIORITY_HIGH; break;
        case 1: proc_priority = PRIORITY_NORMAL; break;
        case 2: proc_priority = PRIORITY_LOW; break;
        default: 
            printf("警告：优先级 %u 无效，设置为普通优先级\n", priority);
            proc_priority = PRIORITY_NORMAL;
            break;
    }
    
    // 计算总页数
    uint32_t total_pages = code_pages + data_pages + 10;  // 预分配页数
    if (total_pages > PHYSICAL_PAGES) {
        printf("总页数 %u 超过系统页数 %u\n", total_pages, PHYSICAL_PAGES);
        return NULL;
    }
    
    PCB* new_process = (PCB*)malloc(sizeof(PCB));
    if (!new_process) return NULL;
    
    // 找到空闲进程
    uint32_t process_index = MAX_PROCESSES;
    for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state == PROCESS_TERMINATED || processes[i].pid == 0) {
            process_index = i;
            break;
        }
    }
    
    if (process_index == MAX_PROCESSES) {
        free(new_process);
        return NULL;
    }
    
    // 初始化进程
    new_process->pid = scheduler.next_pid++;
    new_process->priority = proc_priority;  // 使用转换后的优先级枚举值
    new_process->state = PROCESS_READY;
    new_process->time_slice = TIME_SLICE;
    new_process->next = NULL;
    
    // 设置进程内存布局
    new_process->memory_layout.code.start_page = 0;
    new_process->memory_layout.code.num_pages = code_pages;
    new_process->memory_layout.code.is_allocated = true;
    
    new_process->memory_layout.data.start_page = code_pages;
    new_process->memory_layout.data.num_pages = data_pages;
    new_process->memory_layout.data.is_allocated = true;
    
    new_process->memory_layout.heap.start_page = code_pages + data_pages;
    new_process->memory_layout.heap.num_pages = 5;
    new_process->memory_layout.heap.is_allocated = false;
    
    new_process->memory_layout.stack.start_page = code_pages + data_pages + 5;
    new_process->memory_layout.stack.num_pages = 5;
    new_process->memory_layout.stack.is_allocated = false;
    
    // 分配页表
    new_process->page_table = (PageTableEntry*)calloc(total_pages, sizeof(PageTableEntry));
    new_process->page_table_size = total_pages;
    
    if (!new_process->page_table) {
        free(new_process);
        return NULL;
    }
    
    // 为进程分配内存
    for (uint32_t i = 0; i < code_pages + data_pages; i++) {
        uint32_t frame = allocate_frame(new_process->pid, i);
        if (frame == (uint32_t)-1) {
            printf("分配页失败，尝试选择替换页\n");
            // 选择替换页
            frame = select_victim_frame();
            if (frame == (uint32_t)-1) {
                printf("无法选择替换页，进程创建失败\n");
                // 释放进程内存
                for (uint32_t j = 0; j < i; j++) {
                    free_frame(new_process->page_table[j].frame_number);
                }
                free(new_process->page_table);
                free(new_process);
                return NULL;
            }
        }
        // 更新页表项
        new_process->page_table[i].frame_number = frame;
        new_process->page_table[i].flags.present = true;
        new_process->page_table[i].flags.swapped = false;
        new_process->page_table[i].last_access_time = get_current_time();
        
        // 更新内存布局状态
        if (i < code_pages) {
            new_process->memory_layout.code.is_allocated = true;
        } else {
            new_process->memory_layout.data.is_allocated = true;
        }
    }
    
    // 将新进程添加到进程表
    processes[process_index] = *new_process;
    
    // 将新进程添加到就绪队列
    scheduler.total_processes++;
    add_to_ready_queue(&processes[process_index]);
    
    // 释放新进程
    free(new_process);
    
    return &processes[process_index];
}

// 模拟进程内存访问
void simulate_process_memory_access(uint32_t pid, uint32_t access_count) {
    PCB* proc = get_process_by_pid(pid);
    if (!proc) {
        printf("进程 %u 不存在\n", pid);
        return;
    }

    printf("开始模拟进程 %u 内存访问，访问次数 %u\n", pid, access_count);
    
    for (uint32_t i = 0; i < access_count; i++) {
        // 随机选择一个页面进行访问
        uint32_t page_num = rand() % proc->page_table_size;
        uint32_t offset = rand() % PAGE_SIZE;
        uint32_t addr = page_num * PAGE_SIZE + offset;
        
        // 随机选择读或写
        if (rand() % 2) {
            // 读
            access_memory(proc, addr, false);
            printf("进程 %u 读取地址 0x%x\n", pid, addr);
        } else {
            // 写
            access_memory(proc, addr, true);
            printf("进程 %u 写地址 0x%x\n", pid, addr);
        }
    }
    
    printf("进程 %u 内存访问模拟完成\n", pid);
}

// 为进程分配内存
void allocate_process_memory(uint32_t pid, uint32_t size, uint32_t flags) {
    PCB* proc = get_process_by_pid(pid);
    if (!proc) {
        printf("进程 %u 不存在\n", pid);
        return;
    }

    // 需要分配的页数
    uint32_t pages_needed = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    
    // 检查是否超过进程最大页数
    if (proc->page_table_size + pages_needed > MAX_PAGES_PER_PROCESS) {
        printf("内存不足，无法分配 %u 页\n", pages_needed);
        return;
    }

    printf("开始为进程 %u 分配%s内存，大小 %u 字节，%u 页\n", 
           pid, flags ? "堆" : "数据段", size, pages_needed);

    // 分配页表
    for (uint32_t i = 0; i < pages_needed; i++) {
        uint32_t frame = allocate_frame(pid, proc->page_table_size);
        if (frame == (uint32_t)-1) {  // 使用-1作为无效页框
            printf("内存分配失败\n");
            return;
        }
        
        // 分配页表项
        proc->page_table[proc->page_table_size].frame_number = frame;
        proc->page_table[proc->page_table_size].flags.present = 1;
        proc->page_table[proc->page_table_size].flags.swapped = 0;
        proc->page_table[proc->page_table_size].last_access_time = get_current_time();
        proc->page_table_size++;

        // 更新内存布局状态
        if (flags) {
            // 堆内存
            proc->memory_layout.stack.num_pages++;
            proc->memory_layout.stack.is_allocated = true;
        } else {
            // 数据段内存
            proc->memory_layout.heap.num_pages++;
            proc->memory_layout.heap.is_allocated = true;
        }
    }

    printf("进程 %u %s内存分配完成\n", pid, flags ? "堆" : "数据段");
}

// 创建进程
PCB* create_process_with_pid(uint32_t pid, uint32_t priority, uint32_t code_pages, uint32_t data_pages) {
    // 检查进程ID是否有效
    if (pid >= MAX_PROCESSES || get_process_by_pid(pid) != NULL) {
        printf("无效的进程ID，进程创建失败\n");
        return NULL;
    }
    
    // 检查优先级是否有效（0-2）
    printf("\n[DEBUG] 收到优先级参数: %u\n", priority);
    ProcessPriority proc_priority;
    switch(priority) {
        case 0: 
            proc_priority = PRIORITY_HIGH;
            printf("[DEBUG] 设置为高优先级 (PRIORITY_HIGH)\n");
            break;
        case 1: 
            proc_priority = PRIORITY_NORMAL;
            printf("[DEBUG] 设置为普通优先级 (PRIORITY_NORMAL)\n");
            break;
        case 2: 
            proc_priority = PRIORITY_LOW;
            printf("[DEBUG] 设置为低优先级 (PRIORITY_LOW)\n");
            break;
        default:
            printf("警告：优先级 %u 无效，设置为最低优先级\n", priority);
            proc_priority = PRIORITY_LOW;
            break;
    }
    
    // 计算总页数
    uint32_t total_pages = code_pages + data_pages;
    if (total_pages > VIRTUAL_PAGES) {
        printf("虚拟页数 (%u) 超过虚拟内存大小 (%u)\n", 
               total_pages, VIRTUAL_PAGES);
        return NULL;
    }
    
    // 检查是否有足够的空闲页框
    uint32_t free_frames = get_free_frames_count();
    printf("\n=== 创建进程 %u ===\n", pid);
    printf("代码页数 %u (代码段=%u, 数据段=%u)\n", total_pages, code_pages, data_pages);
    printf("当前空闲页数 %u\n", free_frames);
    printf("进程优先级：%u\n", proc_priority);
    
    if (free_frames < MIN(total_pages, PHYSICAL_PAGES/4)) {
        printf("内存不足，进程无法创建\n");
        return NULL;
    }
    
    // 创建PCB
    PCB* process = (PCB*)malloc(sizeof(PCB));
    if (!process) {
        printf("内存分配失败\n");
        return NULL;
    }
    
    // 初始化PCB
    memset(process, 0, sizeof(PCB));  // 清零所有字段
    process->pid = pid;
    process->priority = proc_priority;  // 使用转换后的优先级
    printf("[DEBUG] 设置进程优先级为: %u\n", process->priority);
    process->state = PROCESS_READY;
    process->page_table_size = total_pages;
    process->time_slice = TIME_SLICE;
    
    // 分配页表
    process->page_table = (PageTableEntry*)calloc(total_pages, sizeof(PageTableEntry));
    if (!process->page_table) {
        printf("内存分配失败\n");
        free(process);
        return NULL;
    }
    
    // 初始化内存布局
    process->memory_layout.code.start_page = 0;
    process->memory_layout.code.num_pages = code_pages;
    process->memory_layout.code.is_allocated = true;
    
    process->memory_layout.data.start_page = code_pages;
    process->memory_layout.data.num_pages = data_pages;
    process->memory_layout.data.is_allocated = true;
    
    process->memory_layout.heap.start_page = code_pages + data_pages;
    process->memory_layout.heap.num_pages = 0;
    process->memory_layout.heap.is_allocated = false;
    
    process->memory_layout.stack.start_page = VIRTUAL_PAGES - 1;
    process->memory_layout.stack.num_pages = 0;
    process->memory_layout.stack.is_allocated = false;
    
    // 初始化进程统计信息
    memset(&process->stats, 0, sizeof(ProcessStats));
    
    printf("\n进程创建完成\n");
    printf("PID: %u\n", process->pid);
    printf("优先级: %u\n", process->priority);
    printf("[DEBUG] 最终确认进程优先级: %u\n", process->priority);
    printf("内存布局：\n");
    printf("- 代码段：起始页=%u, 页数=%u\n", 
           process->memory_layout.code.start_page,
           process->memory_layout.code.num_pages);
    printf("- 数据段：起始页=%u, 页数=%u\n",
           process->memory_layout.data.start_page,
           process->memory_layout.data.num_pages);
    
    // 为进程分配物理页框
    printf("\n开始分配物理页框...\n");
    for (uint32_t i = 0; i < total_pages; i++) {
        uint32_t frame = allocate_frame(process->pid, i);
        if (frame == (uint32_t)-1) {
            printf("分配页框失败，尝试选择替换页\n");
            frame = select_victim_frame();
            if (frame == (uint32_t)-1) {
                printf("无法选择替换页，进程创建失败\n");
                // 释放已分配的页框
                for (uint32_t j = 0; j < i; j++) {
                    free_frame(process->page_table[j].frame_number);
                }
                free(process->page_table);
                free(process);
                return NULL;
            }
        }
        
        // 更新页表项
        process->page_table[i].frame_number = frame;
        process->page_table[i].flags.present = true;
        process->page_table[i].flags.swapped = false;
        process->page_table[i].last_access_time = get_current_time();
    }
    
    // 将进程添加到进程表
    uint32_t process_index = MAX_PROCESSES;
    for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state == PROCESS_TERMINATED || processes[i].pid == 0) {
            process_index = i;
            break;
        }
    }
    
    if (process_index == MAX_PROCESSES) {
        printf("进程表已满，无法创建新进程\n");
        // 释放已分配的资源
        for (uint32_t i = 0; i < total_pages; i++) {
            free_frame(process->page_table[i].frame_number);
        }
        free(process->page_table);
        free(process);
        return NULL;
    }
    
    // 复制进程到进程表
    processes[process_index] = *process;
    free(process);  // 释放临时PCB
    
    // 将进程添加到就绪队列
    scheduler.total_processes++;
    add_to_ready_queue(&processes[process_index]);
    
    return &processes[process_index];
}
