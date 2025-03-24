#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../include/process.h"
#include "../include/memory.h"
#include "../include/vm.h"

// 进程管理
static PCB processes[MAX_PROCESSES];  // 进程控制块数组

// 全局进程调度器实例
ProcessScheduler scheduler;

// 初始化进程调度器
void scheduler_init(void) {
    // 初始化调度器结构
    memset(&scheduler, 0, sizeof(ProcessScheduler));
    scheduler.next_pid = 1;
    scheduler.total_runtime = 0;
    scheduler.auto_balance = true;

    // 初始化进程数组
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
    // 清理就绪队列
    for (int i = 0; i < 3; i++) {
        PCB* current = scheduler.ready_queue[i];
        while (current) {
            PCB* next = current->next;
            process_destroy(current);
            current = next;
        }
        scheduler.ready_queue[i] = NULL;
    }

    // 清理阻塞队列
    PCB* current = scheduler.blocked_queue;
    while (current) {
        PCB* next = current->next;
        process_destroy(current);
        current = next;
    }
    scheduler.blocked_queue = NULL;

    // 清理当前运行进程
    if (scheduler.running_process) {
        process_destroy(scheduler.running_process);
        scheduler.running_process = NULL;
    }
}

// 调度进程
void schedule(void) {
    // 如果当前没有运行进程，从高优先级队列开始调度
    if (!scheduler.running_process) {
        for (int i = 0; i < 3; i++) {
            if (scheduler.ready_queue[i]) {
                scheduler.running_process = scheduler.ready_queue[i];
                scheduler.ready_queue[i] = scheduler.ready_queue[i]->next;
                scheduler.running_process->next = NULL;
                scheduler.running_process->state = PROCESS_RUNNING;
                break;
            }
        }
    }
}

// 时间片轮转
void time_tick(void) {
    if (scheduler.running_process) {
        printf("\n当前运行进程 PID %u，剩余时间片：%u\n", 
               scheduler.running_process->pid,
               scheduler.running_process->time_slice);
        
        scheduler.running_process->time_slice--;
        if (scheduler.running_process->time_slice == 0) {
            printf("进程 PID %u 时间片用完\n", scheduler.running_process->pid);
            
            // 时间片用完，重新调度
            PCB* proc = scheduler.running_process;
            proc->time_slice = TIME_SLICE;
            proc->state = PROCESS_READY;
            
            // 将进程放回就绪队列
            proc->next = scheduler.ready_queue[proc->priority];
            scheduler.ready_queue[proc->priority] = proc;
            scheduler.running_process = NULL;
            
            // 调度新进程
            schedule();
            
            if (scheduler.running_process) {
                printf("切换到进程 PID %u\n", scheduler.running_process->pid);
            } else {
                printf("没有就绪进程可调度\n");
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

// 获取总进程数
uint32_t get_total_processes(void) {
    return scheduler.total_processes;
}

// 设置进程优先级
void set_process_priority(PCB* process, ProcessPriority priority) {
    if (!process || priority < PRIORITY_HIGH || priority > PRIORITY_LOW) {
        return;
    }
    
    // 如果优先级没有变化，直接返回
    if (process->priority == priority) {
        return;
    }
    
    // 从原优先级队列移除
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
    
    // 更新优先级
    process->priority = priority;
    
    // 添加到新优先级队列的头部
    process->next = scheduler.ready_queue[priority];
    scheduler.ready_queue[priority] = process;
}

// 打印调度器状态
void print_scheduler_status(void) {
    printf("\n=== 调度器状态 ===\n");
    printf("总进程数：%u\n", scheduler.total_processes);
    if (scheduler.running_process) {
        printf("运行进程：PID %u\n", scheduler.running_process->pid);
    } else {
        printf("运行进程：无\n");
    }
    
    // 打印就绪队列
    for (int i = 0; i < 3; i++) {
        printf("\n优先级 %d 就绪队列：", i);
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
    process->priority = PRIORITY_NORMAL;
    process->time_slice = TIME_SLICE;
    
    // 分配页表
    process->page_table = (PageTableEntry*)calloc(page_table_size, sizeof(PageTableEntry));
    if (!process->page_table) {
        printf("进程创建失败：无法分配页表内存\n");
        free(process);
        return NULL;
    }
    process->page_table_size = page_table_size;
    
    // 初始化统计信息
    memset(&process->stats, 0, sizeof(ProcessStats));
    
    return process;
}

// 销毁进程
void process_destroy(PCB* pcb) {
    if (!pcb) return;
    
    printf("正在销毁进程 %u\n", pcb->pid);
    
    // 从就绪队列中移除
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
    
    // 释放占用的页框
    for (uint32_t i = 0; i < pcb->page_table_size; i++) {
        if (pcb->page_table[i].flags.present) {
            free_frame(pcb->page_table[i].frame_number);
        }
    }
    
    // 释放页表和PCB
    free(pcb->page_table);
    free(pcb);
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

// 释放进程内存
void process_free_memory(PCB* pcb, uint32_t start_page, uint32_t num_pages) {
    for (uint32_t page = start_page; page < start_page + num_pages; page++) {
        if (page < pcb->page_table_size) {
            pcb->page_table[page].frame_number = (uint32_t)-1;
            pcb->page_table[page].flags.present = false;
            pcb->page_table[page].flags.swapped = false;
        }
    }
}

// 根据PID查找进程
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
    printf("代码段起始页=%u, 页数=%u, 状态=%s\n",
           process->memory_layout.code.start_page,
           process->memory_layout.code.num_pages,
           process->memory_layout.code.is_allocated ? "已分配" : "未分配");
    printf("数据段起始页=%u, 页数=%u, 状态=%s\n",
           process->memory_layout.data.start_page,
           process->memory_layout.data.num_pages,
           process->memory_layout.data.is_allocated ? "已分配" : "未分配");
    printf("堆段起始页=%u, 页数=%u, 状态=%s\n",
           process->memory_layout.heap.start_page,
           process->memory_layout.heap.num_pages,
           process->memory_layout.heap.is_allocated ? "已分配" : "未分配");
    printf("栈段起始页=%u, 页数=%u, 状态=%s\n",
           process->memory_layout.stack.start_page,
           process->memory_layout.stack.num_pages,
           process->memory_layout.stack.is_allocated ? "已分配" : "未分配");
    
    printf("\n内存使用：\n");
    printf("  内存使用：%u 字节\n", process->stats.memory_usage);
    printf("  缺页数：%u\n", process->stats.page_faults);
    printf("  页替换数：%u\n", process->stats.mem_stats.page_replacements);
    
    printf("\n进程统计：\n");
    printf("  总运行时间：%u\n", process->stats.total_runtime);
    printf("  等待时间：%u\n", process->stats.wait_time);
    
    printf("\n内存访问统计：\n");
    uint32_t total_accesses = process->stats.mem_stats.total_accesses;
    if (total_accesses > 0) {
        printf("代码访问率：%u (%.1f%%)\n", 
               process->stats.mem_stats.code_accesses,
               (float)process->stats.mem_stats.code_accesses * 100.0 / total_accesses);
    } else {
        printf("代码访问率：0 (0.0%%)\n");
    }
    // ... 其他访问统计
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
    
    printf("\n=== 进程监控信息 ===\n");
    printf("PID: %u\n", process->pid);
    printf("应用程序：%s\n", process->app_config.name);
    print_process_status(process->state);
    
    // 打印进程内存使用
    print_process_stats(process);
    
    // 根据进程监控配置打印访问模式
    if (process->monitor_config.track_access_pattern) {
        printf("\n内存访问模式\n");
        printf("代码访问率：%u\n", process->stats.mem_stats.code_accesses);
        printf("数据访问率：%u\n", process->stats.mem_stats.data_accesses);
        printf("堆访问率：%u\n", process->stats.mem_stats.heap_accesses);
        printf("栈访问率：%u\n", process->stats.mem_stats.stack_accesses);
    }
    
    // 根据进程监控配置收集统计信息
    if (process->monitor_config.collect_stats) {
        printf("\n内存统计信息\n");
        printf("总访问次数：%u\n", process->stats.mem_stats.total_accesses);
        printf("缺页数：%u\n", process->stats.mem_stats.page_faults);
        printf("页替换数：%u\n", process->stats.mem_stats.page_replacements);
        printf("写入磁盘次数：%u\n", process->stats.mem_stats.writes_to_disk);
        printf("从磁盘读取次数：%u\n", process->stats.mem_stats.reads_from_disk);
    }
}

// 分析内存使用
void analyze_memory_usage(PCB* process) {
    if (!process) return;
    
    uint32_t total_pages = 0;
    uint32_t used_pages = 0;
    uint64_t current_time = get_current_time();
    uint32_t active_pages = 0;  // 当前活跃页数
    
    for (uint32_t i = 0; i < process->page_table_size; i++) {
        if (process->page_table[i].flags.present) {
            used_pages++;
            total_pages++;
            // 判断是否为当前活跃页（1000毫秒内访问）
            if (current_time - process->page_table[i].last_access_time < 1000) {
                active_pages++;
            }
        }
    }
    
    // 计算内存使用统计信息
    process->stats.memory_usage = used_pages * PAGE_SIZE;
    
    printf("\n=== 内存使用统计 (PID=%u) ===\n", process->pid);
    printf("总页数：%u\n", process->page_table_size);
    printf("已使用页数：%u\n", used_pages);
    printf("活跃页数：%u\n", active_pages);
    printf("内存使用率：%.2f%%\n", 
           (float)used_pages * 100 / process->page_table_size);
    printf("活跃页比例：%.2f%%\n",
           (float)active_pages * 100 / used_pages);
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
            printf("合并空闲页：起始=%u，数量=%u\n", free_start, free_count);
            free_start = (uint32_t)-1;
            free_count = 0;
        }
    }
    
    // 2. 移动进程内存
    printf("\n移动进程内存...\n");
    // 实际实现需要考虑内存分配器的具体实现
}

// 平衡内存使用
void balance_memory_usage(void) {
    for (uint32_t pid = 1; pid <= scheduler.next_pid; pid++) {
        PCB* process = get_process_by_pid(pid);
        if (!process) continue;
        
        for (uint32_t i = 0; i < process->page_table_size; i++) {
            if (process->page_table[i].flags.present) {
                // 判断是否为未分配
                uint64_t current_time = get_current_time();
                if (current_time - process->page_table[i].last_access_time > 1000) {
                    // 可能为未分配的页
                    // ...保持不变
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
    
    // 设置新运行进程
    scheduler.running_process = process;
    scheduler.running_process->state = PROCESS_RUNNING;
    scheduler.running_process->next = NULL;
}

// 打印进程状态
void print_process_status(ProcessState state) {
    const char* state_str[] = {
        "就绪",     // PROCESS_READY
        "运行",     // PROCESS_RUNNING
        "阻塞",     // PROCESS_BLOCKED
        "终止",     // PROCESS_TERMINATED
        "等待"      // PROCESS_WAITING
    };
    
    if (state >= PROCESS_READY && state <= PROCESS_WAITING) {
        printf("状态：%s\n", state_str[state]);
    } else {
        printf("状态：未知(%d)\n", state);
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
            return true;  // 找到进程，表示已分析
        }
    }
    
    return false;  // 未找到进程，表示未分析
}

// 初始化进程内存
void initialize_process_memory(PCB* process) {
    if (!process) return;
    
    // 只初始化前24页
    for (uint32_t i = 0; i < 24; i++) {
        uint32_t page_num = i + 1;  // 从1开始，0页为空
        uint32_t frame = allocate_frame(process->pid, page_num);
        if (frame == (uint32_t)-1) {
            printf("分配页失败\n");
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

// 销毁进程
void destroy_process(PCB* process) {
    if (!process) return;
    
    printf("\n=== 销毁进程 PID=%u ===\n", process->pid);
    printf("进程当前状态=%d\n", process->state);
    
    // 从就绪队列中移除
    if (scheduler.running_process == process) {
        scheduler.running_process = NULL;
    }
    
    // 从就绪队列中移除
    for (int i = 0; i < 3; i++) {
        PCB* prev = NULL;
        PCB* current = scheduler.ready_queue[i];
        while (current) {
            if (current == process) {
                if (prev) {
                    prev->next = current->next;
                } else {
                    scheduler.ready_queue[i] = current->next;
                }
                break;
            }
            prev = current;
            current = current->next;
        }
    }
    
    // 释放占用的页框
    for (uint32_t i = 0; i < PHYSICAL_PAGES; i++) {
        if (memory_manager.frames[i].process_id == process->pid) {
            free_frame(i);
        }
    }
    
    // 释放页表
    if (process->page_table) {
        free(process->page_table);
        process->page_table = NULL;
    }
    
    // 设置进程状态
    process->state = PROCESS_TERMINATED;
    process->pid = 0;  // 进程ID
    scheduler.total_processes--;
}

// 添加到就绪队列
void add_to_ready_queue(PCB* process) {
    if (!process) return;
    
    // 确保优先级有效范围
    uint32_t priority = process->priority;
    if (priority >= 3) priority = 2;  // 确保优先级在0-2范围内，优先级为2
    
    // 如果就绪队列为空，直接添加
    if (!scheduler.ready_queue[priority]) {
        scheduler.ready_queue[priority] = process;
        process->next = NULL;
        return;
    }
    
    // 添加到就绪队列尾部
    PCB* current = scheduler.ready_queue[priority];
    while (current->next) {
        current = current->next;
    }
    current->next = process;
    process->next = NULL;
    
    printf("进程 %u 加入优先级 %u 就绪队列\n", process->pid, priority);
}

// 创建进程
PCB* create_process(uint32_t priority, uint32_t code_pages, uint32_t data_pages) {
    printf("\n=== 创建进程 ===\n");
    printf("代码页数 %u 页，数据页数 %u 页\n", code_pages, data_pages);
    
    // 计算总页数
    uint32_t total_pages = code_pages + data_pages + 10;  // 预留堆页
    if (total_pages > PHYSICAL_PAGES) {
        printf("总页数 %u 超过系统页数 %u\n", total_pages, PHYSICAL_PAGES);
        return NULL;
    }
    
    PCB* new_process = (PCB*)malloc(sizeof(PCB));
    if (!new_process) return NULL;
    
    // 查找空闲进程
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
    
    // 初始化新进程
    new_process->pid = scheduler.next_pid++;
    new_process->priority = priority;
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
            printf("分配页失败：无法选择替换页\n");
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
            printf("成功选择替换页 %u\n", frame);
        }
        new_process->page_table[i].frame_number = frame;
        new_process->page_table[i].flags.present = true;
    }
    
    // 新进程加入进程表
    processes[process_index] = *new_process;
    
    // 更新进程总数
    scheduler.total_processes++;
    add_to_ready_queue(&processes[process_index]);
    
    // 释放新进程
    free(new_process);
    
    return &processes[process_index];
}

// 模拟进程访存
void simulate_process_memory_access(uint32_t pid, uint32_t access_count) {
    PCB* proc = get_process_by_pid(pid);
    if (!proc) {
        printf("错误：进程 %u 不存在\n", pid);
        return;
    }

    printf("开始模拟进程 %u 的内存访问，访问次数：%u\n", pid, access_count);
    
    for (uint32_t i = 0; i < access_count; i++) {
        // 随机选择一个页面进行访问
        uint32_t page_num = rand() % proc->page_table_size;
        uint32_t offset = rand() % PAGE_SIZE;
        uint32_t addr = page_num * PAGE_SIZE + offset;
        
        // 随机决定是读还是写操作
        if (rand() % 2) {
            // 读操作
            access_memory(proc, addr, false);
            printf("进程 %u 读取地址 0x%x\n", pid, addr);
        } else {
            // 写操作
            access_memory(proc, addr, true);
            printf("进程 %u 写入地址 0x%x\n", pid, addr);
        }
    }
    
    printf("进程 %u 的内存访问模拟完成\n", pid);
}

// 为进程分配堆/栈空间
void allocate_process_memory(uint32_t pid, uint32_t size, uint32_t flags) {
    PCB* proc = get_process_by_pid(pid);
    if (!proc) {
        printf("错误：进程 %u 不存在\n", pid);
        return;
    }

    // 计算需要分配的页面数
    uint32_t pages_needed = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    
    // 检查是否超过进程最大页面限制
    if (proc->page_table_size + pages_needed > MAX_PAGES_PER_PROCESS) {
        printf("错误：超出进程最大页面限制\n");
        return;
    }

    printf("开始为进程 %u 分配%s空间，大小：%u 字节（%u 页）\n", 
           pid, flags ? "栈" : "堆", size, pages_needed);

    // 分配页面
    for (uint32_t i = 0; i < pages_needed; i++) {
        uint32_t frame = allocate_frame(pid, proc->page_table_size);
        if (frame == (uint32_t)-1) {  // 使用-1作为无效帧号
            printf("错误：无法分配物理页面\n");
            return;
        }
        
        // 更新页表
        proc->page_table[proc->page_table_size].frame_number = frame;
        proc->page_table[proc->page_table_size].flags.present = 1;
        proc->page_table[proc->page_table_size].flags.swapped = 0;
        proc->page_table[proc->page_table_size].last_access_time = get_current_time();
        proc->page_table_size++;

        // 更新内存布局
        if (flags) {
            // 栈空间
            proc->memory_layout.stack.num_pages++;
            proc->memory_layout.stack.is_allocated = true;
        } else {
            // 堆空间
            proc->memory_layout.heap.num_pages++;
            proc->memory_layout.heap.is_allocated = true;
        }
    }

    printf("进程 %u 的%s空间分配完成\n", pid, flags ? "栈" : "堆");
}

// 创建指定PID的进程
PCB* create_process_with_pid(uint32_t pid, uint32_t priority, uint32_t code_pages, uint32_t data_pages) {
    // 检查进程ID是否有效
    if (pid >= MAX_PROCESSES || get_process_by_pid(pid) != NULL) {
        printf("错误：无效的进程ID或进程已存在\n");
        return NULL;
    }
    
    // 检查内存请求的合理性
    uint32_t total_pages = code_pages + data_pages;
    if (total_pages > VIRTUAL_PAGES) {
        printf("错误：请求的内存页数 (%u) 超过虚拟内存限制 (%u)\n", 
               total_pages, VIRTUAL_PAGES);
        return NULL;
    }
    
    // 检查是否有足够的物理内存
    uint32_t free_frames = get_free_frames_count();
    printf("\n=== 创建进程 %u ===\n", pid);
    printf("请求页面数：%u (代码段=%u, 数据段=%u)\n", total_pages, code_pages, data_pages);
    printf("当前可用页框数：%u\n", free_frames);
    
    if (free_frames < MIN(total_pages, PHYSICAL_PAGES/4)) {
        printf("警告：可用物理内存不足，进程创建可能导致系统负载过重\n");
    }
    
    // 分配PCB
    PCB* process = (PCB*)malloc(sizeof(PCB));
    if (!process) {
        printf("错误：无法分配PCB内存\n");
        return NULL;
    }
    
    // 初始化PCB
    process->pid = pid;
    process->priority = priority;
    process->state = PROCESS_READY;
    process->page_table_size = total_pages;
    
    // 分配页表
    process->page_table = (PageTableEntry*)calloc(total_pages, sizeof(PageTableEntry));
    if (!process->page_table) {
        printf("错误：无法分配页表内存\n");
        free(process);
        return NULL;
    }
    
    // 初始化内存布局
    process->memory_layout.code_start = 0;
    process->memory_layout.code_size = code_pages * PAGE_SIZE;
    process->memory_layout.data_start = code_pages * PAGE_SIZE;
    process->memory_layout.data_size = data_pages * PAGE_SIZE;
    process->memory_layout.heap_start = (code_pages + data_pages) * PAGE_SIZE;
    process->memory_layout.heap_size = 0;
    process->memory_layout.stack_start = VIRTUAL_MEMORY_SIZE - PAGE_SIZE;
    process->memory_layout.stack_size = 0;
    
    // 为代码段和数据段分配页框
    printf("\n开始分配内存：\n");
    for (uint32_t i = 0; i < total_pages; i++) {
        uint32_t frame = allocate_frame(pid, i);
        if (frame == (uint32_t)-1) {
            printf("警告：无法直接分配页框 %u，尝试页面置换\n", i);
            continue;
        }
        
        process->page_table[i].frame_number = frame;
        process->page_table[i].flags.present = true;
        process->page_table[i].flags.swapped = false;
        process->page_table[i].last_access_time = get_current_time();
        
        printf("页面 %u 分配到页框 %u\n", i, frame);
    }
    
    // 初始化进程统计信息
    process->stats.page_faults = 0;
    process->stats.page_replacements = 0;
    
    printf("\n进程创建完成：\n");
    printf("PID: %u\n", pid);
    printf("优先级: %u\n", priority);
    printf("内存布局：\n");
    printf("- 代码段：起始=0x%x, 大小=%u字节 (%u页)\n", 
           process->memory_layout.code_start,
           process->memory_layout.code_size,
           code_pages);
    printf("- 数据段：起始=0x%x, 大小=%u字节 (%u页)\n",
           process->memory_layout.data_start,
           process->memory_layout.data_size,
           data_pages);
    
    return process;
}
