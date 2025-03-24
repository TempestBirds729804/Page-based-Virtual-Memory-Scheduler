#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../include/process.h"
#include "../include/memory.h"
#include "../include/vm.h"

// 系统初始化完成
memory_init();
vm_init();
scheduler_init();
storage_init();
ui_init();

printf("系统初始化完成\n");
ui_run();

ui_shutdown();
storage_shutdown();
vm_shutdown();

// 进入命令模式
memory_init();
vm_init();
scheduler_init();
storage_init();
ui_init();

// 内存管理器扩展
typedef struct {
    uint32_t total_pages;      // 总页面数
    uint32_t allocated_pages;  // 已分配页面数
    uint32_t free_pages;       // 空闲页面数
    uint32_t swap_pages;       // 交换页面数
} MemoryManagerExt;

// 进程管理器扩展
typedef struct {
    uint32_t total_processes;  // 总进程数
    uint32_t running_processes;// 运行中进程数
    uint32_t waiting_processes;// 等待中进程数
    uint32_t blocked_processes;// 阻塞进程数
} ProcessManagerExt;

// 存储管理器扩展
typedef struct {
    uint32_t total_blocks;     // 总块数
    uint32_t used_blocks;      // 已用块数
    uint32_t free_blocks;      // 空闲块数
    uint32_t reserved_blocks;  // 保留块数
} StorageManagerExt;

// 页面替换算法扩展
typedef struct {
    uint32_t page_faults;      // 缺页次数
    uint32_t page_hits;        // 命中次数
    uint32_t swap_ins;         // 换入次数
    uint32_t swap_outs;        // 换出次数
} PageReplacementExt;

// ???
static PCB processes[MAX_PROCESSES];  // ????

// ????
ProcessScheduler scheduler;

// ??
void scheduler_init(void) {
    // ??
    memset(&scheduler, 0, sizeof(ProcessScheduler));
    scheduler.next_pid = 1;
    scheduler.total_runtime = 0;
    scheduler.auto_balance = true;
    
    // ?
    for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
        processes[i].pid = 0;
        processes[i].state = PROCESS_TERMINATED;
        processes[i].page_table = NULL;
        processes[i].page_table_size = 0;
    }
    
    // ?кн
    for (int i = 0; i < 3; i++) {
        scheduler.ready_queue[i] = NULL;
    }
    scheduler.blocked_queue = NULL;
    scheduler.running_process = NULL;
    scheduler.total_processes = 0;
}

// ??
void scheduler_shutdown(void) {
    // 
    for (int i = 0; i < 3; i++) {
        PCB* current = scheduler.ready_queue[i];
        while (current) {
            PCB* next = current->next;
            process_destroy(current);
            current = next;
        }
        scheduler.ready_queue[i] = NULL;
    }
    
    // 
    PCB* current = scheduler.blocked_queue;
    while (current) {
        PCB* next = current->next;
        process_destroy(current);
        current = next;
    }
    scheduler.blocked_queue = NULL;
    
    // ?н
    if (scheduler.running_process) {
        process_destroy(scheduler.running_process);
        scheduler.running_process = NULL;
    }
}

// ?
void schedule(void) {
    // ??н??п?
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

// ???
void time_tick(void) {
    if (scheduler.running_process) {
        printf("\n?н PID %u???%u\n", 
               scheduler.running_process->pid,
               scheduler.running_process->time_slice);
        
        scheduler.running_process->time_slice--;
        if (scheduler.running_process->time_slice == 0) {
            printf(" PID %u ??\n", scheduler.running_process->pid);
            
            // ????
            PCB* proc = scheduler.running_process;
            proc->time_slice = TIME_SLICE;
            proc->state = PROCESS_READY;
            
            // ???
            proc->next = scheduler.ready_queue[proc->priority];
            scheduler.ready_queue[proc->priority] = proc;
            scheduler.running_process = NULL;
            
            // ?
            schedule();
            
            if (scheduler.running_process) {
                printf("л PID %u\n", scheduler.running_process->pid);
            } else {
                printf("?о??\n");
            }
        }
    }
}

// ?
PCB* get_ready_queue(ProcessPriority priority) {
    if (priority >= PRIORITY_HIGH && priority <= PRIORITY_LOW) {
        return scheduler.ready_queue[priority];
    }
    return NULL;
}

// ??н
PCB* get_running_process(void) {
    return scheduler.running_process;
}

// ??
uint32_t get_total_processes(void) {
    return scheduler.total_processes;
}

// ??
void set_process_priority(PCB* process, ProcessPriority priority) {
    if (!process || priority < PRIORITY_HIGH || priority > PRIORITY_LOW) {
        return;
    }
    
    // ??б仯??
    if (process->priority == priority) {
        return;
    }
    
    // ???
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
    
    // ?
    process->priority = priority;
    
    // ???
    process->next = scheduler.ready_queue[priority];
    scheduler.ready_queue[priority] = process;
}

// ???
void print_scheduler_status(void) {
    printf("\n=== ?? ===\n");
    printf("?%u\n", scheduler.total_processes);
    if (scheduler.running_process) {
        printf("нPID %u\n", scheduler.running_process->pid);
    } else {
        printf("н\n");
    }
    
    // ?
    for (int i = 0; i < 3; i++) {
        printf("\n? %d  У", i);
        PCB* current = scheduler.ready_queue[i];
        if (!current) {
            printf("");
        }
        while (current) {
            printf("PID %u -> ", current->pid);
            current = current->next;
        }
        printf("\n");
    }
}

// ?
PCB* process_create(uint32_t pid, uint32_t page_table_size) {
    PCB* process = (PCB*)malloc(sizeof(PCB));
    if (!process) return NULL;
    
    // ??
    process->pid = pid;
    process->state = PROCESS_READY;
    process->priority = PRIORITY_NORMAL;
    process->time_slice = TIME_SLICE;
    
    // ?
    process->page_table = (PageTableEntry*)calloc(page_table_size, sizeof(PageTableEntry));
    if (!process->page_table) {
        printf("?????\n");
        free(process);
        return NULL;
    }
    process->page_table_size = page_table_size;
    
    // ???
    memset(&process->stats, 0, sizeof(ProcessStats));
    
    return process;
}

// ?
void process_destroy(PCB* pcb) {
    if (!pcb) return;
    
    printf("? %u\n", pcb->pid);
    
    // ???
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
    
    // ????
    for (uint32_t i = 0; i < pcb->page_table_size; i++) {
        if (pcb->page_table[i].flags.present) {
            free_frame(pcb->page_table[i].frame_number);
        }
    }
    
    // ??PCB
    free(pcb->page_table);
    free(pcb);
    scheduler.total_processes--;
}

// ???
bool process_allocate_memory(PCB* pcb, uint32_t start_page, uint32_t num_pages) {
    for (uint32_t page = start_page; page < start_page + num_pages; page++) {
        pcb->page_table[page].frame_number = (uint32_t)-1;
        pcb->page_table[page].flags.present = false;
        pcb->page_table[page].flags.swapped = false;
        pcb->page_table[page].last_access_time = get_current_time();
    }
    return true;
}

// ????
void process_free_memory(PCB* pcb, uint32_t start_page, uint32_t num_pages) {
    for (uint32_t page = start_page; page < start_page + num_pages; page++) {
        if (page < pcb->page_table_size) {
            pcb->page_table[page].frame_number = (uint32_t)-1;
            pcb->page_table[page].flags.present = false;
            pcb->page_table[page].flags.swapped = false;
        }
    }
}

// ?ID?
PCB* get_process_by_pid(uint32_t pid) {
    printf("\n=== ? PID=%u ===\n", pid);
    printf("??\n");
    // ?з????
    for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state != PROCESS_TERMINATED) {
            printf("[%u] PID=%u, ??=%d\n", i, processes[i].pid, processes[i].state);
        }
    }
    
    for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
        // ?PID????
        if (processes[i].pid == pid) {
            printf("? %u??=%d\n", pid, processes[i].state);
            return &processes[i];
        }
    }
    printf("δ? %u\n", pid);
    return NULL;
}

// 打印进程统计信息
void print_process_stats(PCB* process) {
    if (!process) return;
    
    printf("\n=== 进程统计信息 (PID=%u) ===\n", process->pid);
    printf("总访问次数：%u\n", process->stats.mem_stats.total_accesses);
    printf("缺页次数：%u\n", process->stats.mem_stats.page_faults);
    printf("页面置换次数：%u\n", process->stats.mem_stats.page_replacements);
    printf("写入磁盘次数：%u\n", process->stats.mem_stats.writes_to_disk);
    printf("从磁盘读取次数：%u\n", process->stats.mem_stats.reads_from_disk);
}

// 分析进程内存使用情况
void analyze_memory_usage(PCB* process) {
    if (!process) return;
    
    uint32_t total_pages = 0;
    uint32_t used_pages = 0;
    uint64_t current_time = get_current_time();
    uint32_t active_pages = 0;  // 活跃页面计数
    
    for (uint32_t i = 0; i < process->page_table_size; i++) {
        if (process->page_table[i].flags.present) {
            used_pages++;
            total_pages++;
            // 检查是否是活跃页面（最近1000个时间单位内）
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
    
    // 2. 重新组织内存布局
    printf("\n重新组织内存布局...\n");
    // 实际的布局调整需要根据具体实现
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
                    // 可以考虑回收该页面
                    // ...具体实现保持不变
                }
            }
        }
    }
}

// 设置当前运行进程
void set_running_process(PCB* process) {
    if (!process) return;
    
    // 如果当前有运行进程，放回就绪队列
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

// ??
bool is_process_analyzed(PCB* process, PCB** analyzed_list, int analyzed_count) {
    if (!process || !analyzed_list) {
        return false;
    }
    
    // ?бв?
    for (int i = 0; i < analyzed_count; i++) {
        if (analyzed_list[i] == process) {
            return true;  // ????
        }
    }
    
    return false;  // δ???δ
}

void initialize_process_memory(PCB* process) {
    if (!process) return;
    
    // ??24?
    for (uint32_t i = 0; i < 24; i++) {
        uint32_t page_num = i + 1;  // 1?0?
        uint32_t frame = allocate_frame(process->pid, page_num);
        if (frame == (uint32_t)-1) {
            printf("??\n");
            return;
        }
        process->page_table[page_num].frame_number = frame;
        process->page_table[page_num].flags.present = true;
    }
    printf("? %d  24 ?\n", process->pid);
}

void init_page_table(PCB* process, uint32_t size) {
    process->page_table = (PageTableEntry*)calloc(size, sizeof(PageTableEntry));
    process->page_table_size = size;
    
    // ???
    for (uint32_t i = 0; i < size; i++) {
        process->page_table[i].frame_number = (uint32_t)-1;
        process->page_table[i].flags.present = false;
        process->page_table[i].flags.swapped = false;
        process->page_table[i].last_access_time = get_current_time();
    }
}

void destroy_process(PCB* process) {
    if (!process) return;
    
    printf("\n=== ? PID=%u ===\n", process->pid);
    printf("????：%d\n", process->state);
    
    // ???
    if (scheduler.running_process == process) {
        scheduler.running_process = NULL;
    }
    
    // ??
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
    
    // ?????
    for (uint32_t i = 0; i < PHYSICAL_PAGES; i++) {
        if (memory_manager.frames[i].process_id == process->pid) {
            free_frame(i);
        }
    }
    
    // ??
    if (process->page_table) {
        free(process->page_table);
        process->page_table = NULL;
    }
    
    // ???
    process->state = PROCESS_TERMINATED;
    process->pid = 0;  // PID
    scheduler.total_processes--;
}

// ??
void add_to_ready_queue(PCB* process) {
    if (!process) return;
    
    // ??ЧΧ
    uint32_t priority = process->priority;
    if (priority >= 3) priority = 2;  // 0-2Χ
    
    // ????
    if (!scheduler.ready_queue[priority]) {
        scheduler.ready_queue[priority] = process;
        process->next = NULL;
        return;
    }
    
    // ?β
    PCB* current = scheduler.ready_queue[priority];
    while (current->next) {
        current = current->next;
    }
    current->next = process;
    process->next = NULL;
    
    printf(" %u ?? %u ?\n", process->pid, priority);
}

PCB* create_process(uint32_t priority, uint32_t code_pages, uint32_t data_pages) {
    printf("\n=== 创建新进程 ===\n");
    printf("请求页面数：代码段 %u 页，数据段 %u 页\n", code_pages, data_pages);
    
    // 先在进程数组中找位置
    uint32_t process_index = MAX_PROCESSES;
    for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state == PROCESS_TERMINATED || processes[i].pid == 0) {
            process_index = i;
            break;
        }
    }
    
    if (process_index == MAX_PROCESSES) {
        printf("错误：进程数量已达到最大值 %u\n", MAX_PROCESSES);
        return NULL;
    }

    // 检查总页面数是否超过系统限制
    uint32_t total_pages = code_pages + data_pages + 10;  // 包括堆栈预留页面
    if (total_pages > MAX_PAGES_PER_PROCESS) {
        printf("错误：请求的总页面数 %u 超过进程限制 %u\n", total_pages, MAX_PAGES_PER_PROCESS);
        return NULL;
    }

    // 检查当前可用物理页框数量
    uint32_t available_frames = get_free_frames_count();
    printf("当前可用页框数：%u，需要页框数：%u\n", available_frames, code_pages + data_pages);
    
    PCB* new_process = (PCB*)malloc(sizeof(PCB));
    if (!new_process) {
        printf("错误：内存分配失败\n");
        return NULL;
    }
    
    // 初始化新进程
    new_process->pid = scheduler.next_pid++;
    new_process->priority = priority;
    new_process->state = PROCESS_READY;
    new_process->time_slice = DEFAULT_TIME_SLICE;
    new_process->next = NULL;
    
    // 初始化页表
    new_process->page_table = (PageTableEntry*)calloc(total_pages, sizeof(PageTableEntry));
    new_process->page_table_size = total_pages;
    
    if (!new_process->page_table) {
        printf("错误：页表分配失败\n");
        free(new_process);
        return NULL;
    }

    // 为代码段和数据段分配物理内存
    uint32_t allocated_frames = 0;
    for (uint32_t i = 0; i < code_pages + data_pages; i++) {
        uint32_t frame = allocate_frame(new_process->pid, i);
        if (frame == (uint32_t)-1) {
            printf("需要页面置换来获取新页框（已分配 %u/%u 页）\n", 
                   allocated_frames, code_pages + data_pages);
            frame = select_victim_frame();
            if (frame == (uint32_t)-1) {
                printf("错误：无法获取页框，进程创建失败\n");
                free(new_process->page_table);
                free(new_process);
                return NULL;
            }
            allocated_frames++;
            printf("成功通过页面置换获取页框 %u\n", frame);
        } else {
            allocated_frames++;
            printf("直接分配页框 %u（%u/%u）\n", frame, allocated_frames, code_pages + data_pages);
        }
        new_process->page_table[i].frame_number = frame;
        new_process->page_table[i].flags.present = true;
        update_frame_access(frame);
    }

    // 设置内存布局
    new_process->memory_layout.code.start_page = 0;
    new_process->memory_layout.code.num_pages = code_pages;
    new_process->memory_layout.code.is_allocated = true;
    
    new_process->memory_layout.data.start_page = code_pages;
    new_process->memory_layout.data.num_pages = data_pages;
    new_process->memory_layout.data.is_allocated = true;
    
    new_process->memory_layout.heap.start_page = code_pages + data_pages;
    new_process->memory_layout.heap.num_pages = 5;  // 预留5页
    new_process->memory_layout.heap.is_allocated = false;
    
    new_process->memory_layout.stack.start_page = code_pages + data_pages + 5;
    new_process->memory_layout.stack.num_pages = 5;  // 预留5页
    new_process->memory_layout.stack.is_allocated = false;
    
    // 复制进程信息到数组
    processes[process_index] = *new_process;
    
    // 更新调度器状态
    scheduler.total_processes++;
    add_to_ready_queue(&processes[process_index]);
    
    // 释放临时PCB
    free(new_process);
    
    printf("进程 %u 已加入优先级 %u 的就绪队列\n", processes[process_index].pid, priority);
    printf("创建进程成功，PID=%u\n", processes[process_index].pid);
    printf("内存布局：\n");
    printf("代码段：%u页\n", code_pages);
    printf("数据段：%u页\n", data_pages);
    printf("堆：5页（未分配）\n");
    printf("栈：5页（未分配）\n");
    
    return &processes[process_index];
}

