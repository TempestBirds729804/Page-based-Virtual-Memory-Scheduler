#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../include/process.h"
#include "../include/memory.h"
#include "../include/vm.h"

// 全局进程调度器实例
ProcessScheduler scheduler;

// 初始化进程调度器
void scheduler_init(void) {
    // 初始化调度器结构
    memset(&scheduler, 0, sizeof(ProcessScheduler));
    scheduler.next_pid = 1;
    scheduler.total_runtime = 0;
    scheduler.auto_balance = true;
    
    // 初始化就绪队列和运行进程
    for (int i = 0; i < 3; i++) {
        scheduler.ready_queue[i] = NULL;
    }
    scheduler.blocked_queue = NULL;
    scheduler.running_process = NULL;
    scheduler.total_processes = 0;
}

// 关闭进程调度器
void scheduler_shutdown(void) {
    // 清理所有进程
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

// 进程调度
void schedule(void) {
    // 如果当前没有运行进程，从最高优先级队列开始查找
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
    
    // 从原优先级队列中移除
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

// 创建新进程
PCB* process_create(uint32_t pid, uint32_t page_table_size) {
    PCB* process = (PCB*)malloc(sizeof(PCB));
    if (!process) return NULL;
    
    // 初始化基本信息
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
    
    // 从调度队列中移除
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
    
    // 释放进程占用的内存
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

// 释放进程的内存
void process_free_memory(PCB* pcb, uint32_t start_page, uint32_t num_pages) {
    for (uint32_t page = start_page; page < start_page + num_pages; page++) {
        if (page < pcb->page_table_size) {
            pcb->page_table[page].frame_number = (uint32_t)-1;
            pcb->page_table[page].flags.present = false;
            pcb->page_table[page].flags.swapped = false;
        }
    }
}

// 根据进程ID查找进程
PCB* get_process_by_pid(uint32_t pid) {
    // 检查当前运行的进程
    if (scheduler.running_process && scheduler.running_process->pid == pid) {
        return scheduler.running_process;
    }
    
    // 检查就绪队列
    for (int i = 0; i < 3; i++) {
        PCB* current = scheduler.ready_queue[i];
        while (current) {
            if (current->pid == pid) {
                return current;
            }
            current = current->next;
        }
    }
    
    // 检查阻塞队列
    PCB* current = scheduler.blocked_queue;
    while (current) {
        if (current->pid == pid) {
            return current;
        }
        current = current->next;
    }
    
    return NULL;  // 未找到进程
}

// 打印进程统计信息
void print_process_stats(PCB* process) {
    if (!process) return;
    
    printf("\n进程统计信息：\n");
    printf("内存布局：\n");
    printf("  代码段：0x%08x - 0x%08x\n", 
           process->memory_layout.code_start,
           process->memory_layout.code_start + process->memory_layout.code_size);
    printf("  数据段：0x%08x - 0x%08x\n",
           process->memory_layout.data_start,
           process->memory_layout.data_start + process->memory_layout.data_size);
    printf("  堆区：  0x%08x - 0x%08x\n",
           process->memory_layout.heap_start,
           process->memory_layout.heap_start + process->memory_layout.heap_size);
    printf("  栈区：  0x%08x - 0x%08x\n",
           process->memory_layout.stack_start,
           process->memory_layout.stack_start + process->memory_layout.stack_size);
    
    printf("\n内存使用：\n");
    printf("  总内存使用：%u 字节\n", process->stats.memory_usage);
    printf("  缺页次数：%u\n", process->stats.page_faults);
    printf("  页面置换：%u\n", process->stats.mem_stats.page_replacements);
    
    printf("\n运行统计：\n");
    printf("  总运行时间：%u\n", process->stats.total_runtime);
    printf("  等待时间：%u\n", process->stats.wait_time);
    
    printf("\n内存访问统计：\n");
    uint32_t total_accesses = process->stats.mem_stats.total_accesses;
    if (total_accesses > 0) {
        printf("代码段访问：%u (%.1f%%)\n", 
               process->stats.mem_stats.code_accesses,
               (float)process->stats.mem_stats.code_accesses * 100.0 / total_accesses);
    } else {
        printf("代码段访问：0 (0.0%%)\n");
    }
    // ... 添加其他段的访问统计
}

// 创建应用程序
PCB* create_application(const char* name, AppConfig* config) {
    if (!name || !config) return NULL;
    
    // 计算需要的总页表大小
    config->page_table_size = (config->layout.stack_start + config->layout.stack_size) / PAGE_SIZE;
    
    PCB* process = process_create(scheduler.next_pid++, config->page_table_size);
    if (!process) return NULL;
    
    // 设置进程名称
    strncpy(process->name, name, MAX_PROCESS_NAME - 1);
    
    // 设置内存布局
    process->memory_layout = config->layout;
    
    // 初始化进程内存
    if (!setup_process_memory(process, &config->layout)) {
        process_destroy(process);
        return NULL;
    }
    
    return process;
}

// 设置进程内存布局
bool setup_process_memory(PCB* process, MemoryLayout* layout) {
    if (!process || !layout) {
        printf("设置进程内存失败：无效参数\n");
        return false;
    }
    
    printf("\n=== 进程内存布局 ===\n");
    printf("代码段: 0x%x - 0x%x (%u页)\n",
           layout->code_start, layout->code_start + layout->code_size,
           layout->code_size / PAGE_SIZE);
    printf("数据段: 0x%x - 0x%x (%u页)\n",
           layout->data_start, layout->data_start + layout->data_size,
           layout->data_size / PAGE_SIZE);
    printf("堆区:   0x%x - 0x%x (%u页)\n",
           layout->heap_start, layout->heap_start + layout->heap_size,
           layout->heap_size / PAGE_SIZE);
    printf("栈区:   0x%x - 0x%x (%u页)\n",
           layout->stack_start, layout->stack_start + layout->stack_size,
           layout->stack_size / PAGE_SIZE);

    // 分配代码段的所有页框
    uint32_t code_pages = layout->code_size / PAGE_SIZE;
    for (uint32_t i = 0; i < code_pages; i++) {
        if (!handle_page_fault(process, i)) {
            printf("分配代码段页框失败\n");
            return false;
        }
    }
    
    // 分配数据段的所有页框
    uint32_t data_pages = layout->data_size / PAGE_SIZE;
    for (uint32_t i = 0; i < data_pages; i++) {
        uint32_t page_num = code_pages + i;
        if (!handle_page_fault(process, page_num)) {
            printf("分配数据段页框失败\n");
            return false;
        }
    }
    
    // 分配堆区的所有页框
    uint32_t heap_pages = layout->heap_size / PAGE_SIZE;
    for (uint32_t i = 0; i < heap_pages; i++) {
        uint32_t page_num = code_pages + data_pages + i;
        if (!handle_page_fault(process, page_num)) {
            printf("分配堆区页框失败\n");
            return false;
        }
    }
    
    // 分配栈区的所有页框
    uint32_t stack_pages = layout->stack_size / PAGE_SIZE;
    for (uint32_t i = 0; i < stack_pages; i++) {
        uint32_t page_num = code_pages + data_pages + heap_pages + i;
        if (!handle_page_fault(process, page_num)) {
            printf("分配栈区页框失败\n");
            return false;
        }
    }
    
    printf("\n已分配物理页框：代码段 %u 页，数据段 %u 页，堆区 %u 页，栈区 %u 页\n", 
           code_pages, data_pages, heap_pages, stack_pages);

    return true;
}

// 监控进程
void monitor_process(PCB* process) {
    if (!process) return;
    
    printf("\n=== 进程监控信息 ===\n");
    printf("PID: %u\n", process->pid);
    printf("名称: %s\n", process->app_config.name);
    print_process_status(process->state);
    
    // 打印内存使用情况
    print_process_stats(process);
    
    // 如果启用了访问模式跟踪
    if (process->monitor_config.track_access_pattern) {
        printf("\n内存访问模式：\n");
        printf("代码段访问：%u\n", process->stats.mem_stats.code_accesses);
        printf("数据段访问：%u\n", process->stats.mem_stats.data_accesses);
        printf("堆访问：%u\n", process->stats.mem_stats.heap_accesses);
        printf("栈访问：%u\n", process->stats.mem_stats.stack_accesses);
    }
    
    // 如果启用了统计收集
    if (process->monitor_config.collect_stats) {
        printf("\n性能统计：\n");
        printf("总访问次数：%u\n", process->stats.mem_stats.total_accesses);
        printf("缺页次数：%u\n", process->stats.mem_stats.page_faults);
        printf("页面置换次数：%u\n", process->stats.mem_stats.page_replacements);
        printf("磁盘写入次数：%u\n", process->stats.mem_stats.writes_to_disk);
        printf("磁盘读取次数：%u\n", process->stats.mem_stats.reads_from_disk);
    }
}

// 分析进程内存使用情况
void analyze_memory_usage(PCB* process) {
    if (!process) return;
    
    uint32_t total_pages = 0;
    uint32_t used_pages = 0;
    uint64_t current_time = get_current_time();
    uint32_t active_pages = 0;  // 最近访问的页面数
    
    for (uint32_t i = 0; i < process->page_table_size; i++) {
        if (process->page_table[i].flags.present) {
            used_pages++;
            total_pages++;
            // 检查是否是最近访问的页面（比如1000个时间单位内）
            if (current_time - process->page_table[i].last_access_time < 1000) {
                active_pages++;
            }
        }
    }
    
    // 更新统计信息
    process->stats.memory_usage = used_pages * PAGE_SIZE;
    
    printf("\n=== 内存使用分析 (PID=%u) ===\n", process->pid);
    printf("总页面数：%u\n", process->page_table_size);
    printf("已使用页面：%u\n", used_pages);
    printf("活跃页面：%u\n", active_pages);
    printf("内存使用率：%.2f%%\n", 
           (float)used_pages * 100 / process->page_table_size);
    printf("页面活跃率：%.2f%%\n",
           (float)active_pages * 100 / used_pages);
}

// 优化进程内存布局
void optimize_memory_layout(PCB* process) {
    if (!process) return;
    
    printf("\n=== 优化内存布局 ===\n");
    printf("进程 %u (%s)\n", process->pid, process->app_config.name);
    
    // 1. 合并连续的空闲页面
    uint32_t free_start = (uint32_t)-1;
    uint32_t free_count = 0;
    
    for (uint32_t i = 0; i < process->page_table_size; i++) {
        if (!process->page_table[i].flags.present) {
            if (free_start == (uint32_t)-1) {
                free_start = i;
            }
            free_count++;
        } else if (free_count > 0) {
            printf("合并空闲页面：起始=%u，数量=%u\n", free_start, free_count);
            free_start = (uint32_t)-1;
            free_count = 0;
        }
    }
    
    // 2. 重新排列内存区域
    printf("\n重新排列内存区域...\n");
    // 实际的重排代码需要更复杂的实现
}

// 平衡进程内存使用
void balance_memory_usage(void) {
    for (uint32_t pid = 1; pid <= scheduler.next_pid; pid++) {
        PCB* process = get_process_by_pid(pid);
        if (!process) continue;
        
        for (uint32_t i = 0; i < process->page_table_size; i++) {
            if (process->page_table[i].flags.present) {
                // 检查是否长时间未访问
                uint64_t current_time = get_current_time();
                if (current_time - process->page_table[i].last_access_time > 1000) {
                    // 可以考虑换出这个页面
                    // ...其余代码保持不变
                }
            }
        }
    }
}

void set_running_process(PCB* process) {
    if (!process) return;
    
    // 将当前运行进程放回就绪队列
    if (scheduler.running_process) {
        scheduler.running_process->state = PROCESS_READY;
        scheduler.running_process->next = scheduler.ready_queue[scheduler.running_process->priority];
        scheduler.ready_queue[scheduler.running_process->priority] = scheduler.running_process;
    }
    
    // 设置新的运行进程
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

// 检查进程是否已被分析
bool is_process_analyzed(PCB* process, PCB** analyzed_list, int analyzed_count) {
    if (!process || !analyzed_list) {
        return false;
    }
    
    // 在已分析列表中查找进程
    for (int i = 0; i < analyzed_count; i++) {
        if (analyzed_list[i] == process) {
            return true;  // 找到进程，表示已分析过
        }
    }
    
    return false;  // 未找到进程，表示未分析过
}

void initialize_process_memory(PCB* process) {
    if (!process) return;
    
    // 只分配前24个页框
    for (uint32_t i = 0; i < 24; i++) {
        uint32_t page_num = i + 1;  // 从1开始，0页保留
        uint32_t frame = allocate_frame(process->pid, page_num);
        if (frame == (uint32_t)-1) {
            printf("分配页框失败\n");
            return;
        }
        process->page_table[page_num].frame_number = frame;
        process->page_table[page_num].flags.present = true;
    }
    printf("为进程 %d 分配了 24 个物理页框\n", process->pid);
}

void init_page_table(PCB* process, uint32_t size) {
    process->page_table = (PageTableEntry*)calloc(size, sizeof(PageTableEntry));
    process->page_table_size = size;
    
    // 初始化每个页表项
    for (uint32_t i = 0; i < size; i++) {
        process->page_table[i].frame_number = (uint32_t)-1;
        process->page_table[i].flags.present = false;
        process->page_table[i].flags.swapped = false;
        process->page_table[i].last_access_time = get_current_time();
    }
}

