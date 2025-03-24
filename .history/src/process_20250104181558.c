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
    // 如果当前没有运行进程，从优先级最高的进程开始调度
    if (!scheduler.running_process) {
        printf("\n开始进程调度...\n");
        for (int i = 0; i < 3; i++) {
            if (scheduler.ready_queue[i]) {
                PCB* process = scheduler.ready_queue[i];
                scheduler.ready_queue[i] = process->next;
                set_running_process(process);
                
                printf("调度进程 PID %u (优先级 %d) 开始运行，时间片 %u\n", 
                       process->pid,
                       process->priority,
                       process->time_slice);
                return;
            } else {
                printf("优先级 %d 队列为空\n", i);
            }
        }
        printf("没有可运行的进程\n");
    } else {
        // 检查是否有更高优先级的进程
        for (int i = 0; i < scheduler.running_process->priority; i++) {
            if (scheduler.ready_queue[i]) {
                PCB* high_priority_process = scheduler.ready_queue[i];
                scheduler.ready_queue[i] = high_priority_process->next;
                preempt_process(scheduler.running_process, high_priority_process);
                return;
            }
        }
        printf("当前进程 PID %u 继续运行（优先级 %u）\n",
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
            printf("进程 PID %u 时间片用完\n", scheduler.running_process->pid);
            
            // 将进程放回就绪队列而不是终止
            PCB* proc = scheduler.running_process;
            proc->state = PROCESS_READY;
            proc->time_slice = proc->total_time_slice;  // 重置时间片
            
            // 将进程加入就绪队列
            proc->next = scheduler.ready_queue[proc->priority];
            scheduler.ready_queue[proc->priority] = proc;
            
            scheduler.running_process = NULL;
            
            // 调度下一个进程
            schedule();
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

// 计算进程的物理页框占比
float calculate_physical_pages_ratio(PCB* process) {
    if (!process || !process->page_table || process->page_table_size == 0) {
        return 0.0f;
    }
    
    uint32_t physical_pages = 0;
    uint32_t total_allocated_pages = 0;
    
    // 统计已分配的页面数和在物理内存中的页面数
    for (uint32_t i = 0; i < process->page_table_size; i++) {
        if (process->page_table[i].flags.present || process->page_table[i].flags.swapped) {
            total_allocated_pages++;
            if (process->page_table[i].flags.present) {
                physical_pages++;
            }
        }
    }
    
    if (total_allocated_pages == 0) return 0.0f;
    return (float)physical_pages / total_allocated_pages * 100.0f;
}

// 随机选择一个页面调入物理内存
bool swap_in_random_page(PCB* process) {
    if (!process || !process->page_table) return false;
    
    // 创建一个数组存储所有可调入的页面索引
    uint32_t* swapped_pages = (uint32_t*)malloc(process->page_table_size * sizeof(uint32_t));
    uint32_t swapped_count = 0;
    
    // 收集所有在交换区的页面
    for (uint32_t i = 0; i < process->page_table_size; i++) {
        if (process->page_table[i].flags.swapped && !process->page_table[i].flags.present) {
            swapped_pages[swapped_count++] = i;
        }
    }
    
    // 如果没有可调入的页面
    if (swapped_count == 0) {
        free(swapped_pages);
        return false;
    }
    
    // 随机选择一个页面
    uint32_t random_index = rand() % swapped_count;
    uint32_t page_to_swap = swapped_pages[random_index];
    
    // 分配物理页框
    uint32_t frame = allocate_frame(process->pid, page_to_swap);
    if (frame == (uint32_t)-1) {
        // 如果没有空闲页框，需要进行页面置换
        frame = select_victim_page(process);
        if (frame == (uint32_t)-1) {
            free(swapped_pages);
            return false;
        }
    }
    
    // 更新页表项
    process->page_table[page_to_swap].frame_number = frame;
    process->page_table[page_to_swap].flags.present = true;
    process->page_table[page_to_swap].flags.swapped = false;
    process->page_table[page_to_swap].last_access_time = get_current_time();
    
    printf("将进程 %u 的页面 %u 调入物理页框 %u\n", 
           process->pid, page_to_swap, frame);
    
    free(swapped_pages);
    return true;
}

// 确保进程有足够的物理页框
bool ensure_minimum_physical_pages(PCB* process) {
    const float MINIMUM_RATIO = 25.0f;  // 最小物理页框比例
    float current_ratio;
    
    do {
        current_ratio = calculate_physical_pages_ratio(process);
        printf("进程 %u 当前物理页框占比: %.2f%%\n", process->pid, current_ratio);
        
        if (current_ratio >= MINIMUM_RATIO) {
            return true;
        }
        
        // 尝试调入一个页面
        if (!swap_in_random_page(process)) {
            printf("警告：无法为进程 %u 调入更多页面\n", process->pid);
            return false;
        }
        
    } while (current_ratio < MINIMUM_RATIO);
    
    return true;
}

// 设置运行进程
void set_running_process(PCB* process) {
    if (!process) return;
    
    // 确保进程有足够的物理页框
    printf("\n检查进程 %u 的物理页框占比...\n", process->pid);
    if (!ensure_minimum_physical_pages(process)) {
        printf("错误：无法确保进程 %u 有足够的物理页框，无法设置为运行状态\n", process->pid);
        return;
    }
    
    // 如果进程在就绪队列中，先将其移除
    PCB** queue = &scheduler.ready_queue[process->priority];
    if (*queue == process) {
        *queue = process->next;
    } else {
        PCB* current = *queue;
        while (current && current->next != process) {
            current = current->next;
        }
        if (current) {
            current->next = process->next;
        }
    }
    
    // 设置进程状态
    process->state = PROCESS_RUNNING;
    process->next = NULL;
    scheduler.running_process = process;
    
    printf("进程 %u 已设置为运行状态\n", process->pid);
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
    
    // 如果进程已经在运行，不要添加到就绪队列
    if (process->state == PROCESS_RUNNING) return;
    
    // 设置进程状态为就绪
    process->state = PROCESS_READY;
    
    // 将进程添加到对应优先级的就绪队列
    process->next = scheduler.ready_queue[process->priority];
    scheduler.ready_queue[process->priority] = process;
}

// 创建进程
PCB* create_process(uint32_t priority, uint32_t code_pages, uint32_t data_pages) {
    printf("\n=== 创建进程 ===\n");
    printf("代码页数 %u (代码段=%u, 数据段=%u)\n", code_pages + data_pages, code_pages, data_pages);
    uint32_t free_frames = get_free_frames_count();
    printf("当前空闲页数 %u\n", free_frames);
    
    // 计算总页数
    uint32_t total_pages = code_pages + data_pages + 10;  // 预分配页数
    if (total_pages > VIRTUAL_PAGES) {
        printf("总页数 %u 超过虚拟内存页数 %u\n", total_pages, VIRTUAL_PAGES);
        return NULL;
    }
    
    // 计算需要的物理页框数（25%的页面需要在物理内存中）
    uint32_t required_frames = (total_pages * 25 + 99) / 100;  // 向上取整
    printf("需要的物理页框数：%u（总页数的25%%）\n", required_frames);
    printf("当前可用的物理页框数：%u\n", free_frames);
    
    if (free_frames < required_frames) {
        printf("尝试通过页面置换获取更多物理页框...\n");
        uint32_t frames_needed = required_frames - free_frames;
        printf("需要额外置换 %u 个页框\n", frames_needed);
        
        // 尝试通过页面置换获取足够的页框
        uint32_t frames_freed = 0;
        for (uint32_t i = 0; i < frames_needed; i++) {
            uint32_t victim_frame = select_victim_page(NULL);
            if (victim_frame != (uint32_t)-1) {
                frames_freed++;
                printf("成功通过置换获得页框 %u\n", victim_frame);
            } else {
                printf("页面置换失败，无法获得更多页框\n");
                break;
            }
        }
        
        printf("通过页面置换获得 %u 个页框\n", frames_freed);
        if (frames_freed < frames_needed) {
            printf("无法获得足够的物理页框（需要 %u 个，只获得 %u 个）\n", 
                   frames_needed, frames_freed);
            return NULL;
        }
    }
    
    // 创建新进程
    PCB* new_process = (PCB*)malloc(sizeof(PCB));
    if (!new_process) return NULL;
    
    // 初始化进程
    new_process->pid = scheduler.next_pid++;
    new_process->priority = priority;
    new_process->state = PROCESS_READY;
    new_process->time_slice = DEFAULT_TIME_SLICE;
    new_process->total_time_slice = DEFAULT_TIME_SLICE;
    new_process->wait_time = 0;
    new_process->was_preempted = false;
    new_process->next = NULL;
    
    // 初始化页表
    init_page_table(new_process, total_pages);
    
    printf("\n开始为进程 %u 分配物理页框...\n", new_process->pid);
    
    // 分配所需的物理页框
    bool allocation_failed = false;
    uint32_t allocated_frames = 0;
    
    for (uint32_t i = 0; i < required_frames; i++) {
        printf("尝试为进程 %u 的虚拟页 %u 分配物理页框\n", 
               new_process->pid, i);
        
        uint32_t frame = allocate_frame(new_process->pid, i);
        if (frame == (uint32_t)-1) {
            printf("直接分配失败，尝试页面置换\n");
            frame = select_victim_page(NULL);
            if (frame == (uint32_t)-1) {
                printf("页面置换失败，无法获取更多物理页框\n");
                allocation_failed = true;
                break;
            }
            printf("通过页面置换获得页框 %u\n", frame);
        }
        
        new_process->page_table[i].frame_number = frame;
        new_process->page_table[i].flags.present = true;
        allocated_frames++;
        printf("成功为虚拟页 %u 分配物理页框 %u\n", i, frame);
    }
    
    printf("成功分配 %u 个物理页框（需要 %u 个）\n", 
           allocated_frames, required_frames);
    
    if (allocation_failed) {
        printf("内存分配失败，释放已分配的资源\n");
        // 释放已分配的页框
        for (uint32_t i = 0; i < allocated_frames; i++) {
            free_frame(new_process->page_table[i].frame_number);
        }
        free(new_process->page_table);
        free(new_process);
        return NULL;
    }
    
    // 将剩余页面标记为在交换区
    for (uint32_t i = required_frames; i < total_pages; i++) {
        new_process->page_table[i].flags.swapped = true;
        new_process->page_table[i].flags.present = false;
    }
    
    // 找到空闲进程槽位
    uint32_t process_index = MAX_PROCESSES;
    for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state == PROCESS_TERMINATED || processes[i].pid == 0) {
            process_index = i;
            break;
        }
    }
    
    if (process_index == MAX_PROCESSES) {
        printf("进程表已满，无法创建新进程\n");
        // 释放已分配的页框
        for (uint32_t i = 0; i < allocated_frames; i++) {
            free_frame(new_process->page_table[i].frame_number);
        }
        free(new_process->page_table);
        free(new_process);
        return NULL;
    }
    
    // 复制进程到进程表
    processes[process_index] = *new_process;
    PCB* final_process = &processes[process_index];
    free(new_process);
    
    // 将进程添加到就绪队列
    scheduler.total_processes++;
    add_to_ready_queue(final_process);
    
    printf("\n进程 %u 创建成功\n", final_process->pid);
    printf("已分配 %u 个物理页框\n", allocated_frames);
    printf("剩余 %u 个页面在交换区\n", total_pages - required_frames);
    
    return final_process;
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
    ProcessPriority proc_priority;
    switch(priority) {
        case 0: 
            proc_priority = PRIORITY_HIGH;
            break;
        case 1: 
            proc_priority = PRIORITY_NORMAL;
            break;
        case 2: 
            proc_priority = PRIORITY_LOW;
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
    
    //if (free_frames < MIN(total_pages, PHYSICAL_PAGES/4)) {
    //    printf("内存不足，进程无法创建\n");
   //     return NULL;
    //}
    
    // 创建PCB
    PCB* process = (PCB*)malloc(sizeof(PCB));
    if (!process) {
        printf("内存分配失败\n");
        return NULL;
    }
    
    // 初始化PCB
    memset(process, 0, sizeof(PCB));  // 清零所有字段
    process->pid = pid;
    process->priority = proc_priority;
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
            
            // 获取被置换页框的信息
            FrameInfo* victim_frame = &memory_manager.frames[frame];
            PCB* victim_process = get_process_by_pid(victim_frame->process_id);
            if (victim_process) {
                // 如果是脏页，需要写入交换区
                if (victim_frame->is_dirty) {
                    printf("页框 %u 是脏页，将其写入交换区\n", frame);
                    if (!swap_out_page(frame)) {
                        printf("写入交换区失败，进程创建失败\n");
                        // 释放已分配的页框
                        for (uint32_t j = 0; j < i; j++) {
                            free_frame(process->page_table[j].frame_number);
                        }
                        free(process->page_table);
                        free(process);
                        return NULL;
                    }
                }
                
                // 更新被置换页的页表项
                PageTableEntry* victim_pte = &victim_process->page_table[victim_frame->virtual_page_num];
                victim_pte->flags.present = false;
                victim_pte->flags.swapped = true;
                printf("更新进程 %u 的页表项：virtual_page=%u, present=false, swapped=true\n",
                       victim_process->pid, victim_frame->virtual_page_num);
            }
            
            // 释放并重新分配页框
            free_frame(frame);
            frame = allocate_frame(process->pid, i);
            if (frame == (uint32_t)-1) {
                printf("重新分配页框失败，进程创建失败\n");
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
    PCB* new_process = &processes[process_index];  // 保存新进程的引用
    free(process);  // 释放临时PCB
    
    // 将进程添加到就绪队列
    scheduler.total_processes++;
    add_to_ready_queue(new_process);
    
    // 检查是否需要抢占当前运行的进程
    check_preemption(new_process);
    
    return new_process;
}

// 判断是否应该进行抢占
bool should_preempt(PCB* current, PCB* new_proc) {
    if (!current || !new_proc) return false;
    return new_proc->priority < current->priority;  // 优先级数字越小优先级越高
}

// 检查是否需要进程抢占
void check_preemption(PCB* new_process) {
    if (!new_process || !scheduler.running_process) return;
    
    if (should_preempt(scheduler.running_process, new_process)) {
        preempt_process(scheduler.running_process, new_process);
    }
}

// 执行进程抢占
void preempt_process(PCB* current, PCB* new_proc) {
    if (!current || !new_proc) return;
    
    printf("\n=== 进程抢占 ===\n");
    printf("当前运行进程 PID %u (优先级 %u) 被进程 PID %u (优先级 %u) 抢占\n",
           current->pid, current->priority, new_proc->pid, new_proc->priority);
    
    // 保存当前运行进程的状态
    current->was_preempted = true;
    current->state = PROCESS_READY;
    current->next = NULL;  // 清除next指针，防止链表混乱
    
    // 将被抢占进程加入就绪队列
    add_to_ready_queue(current);
    
    // 设置新进程为运行进程
    set_running_process(new_proc);
    
    printf("进程切换完成\n");
    printf("被抢占进程 PID %u 已加入优先级 %u 就绪队列\n", 
           current->pid, current->priority);
}

// 恢复被抢占的进程
void restore_preempted_process(PCB* process) {
    if (!process || !process->was_preempted) return;
    
    printf("\n=== 恢复被抢占进程 ===\n");
    printf("恢复进程 PID %u (优先级 %u)\n", process->pid, process->priority);
    
    process->was_preempted = false;
    process->state = PROCESS_RUNNING;
    scheduler.running_process = process;
}
