#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../include/process.h"
#include "../include/memory.h"
#include "../include/vm.h"

// ???????????
static PCB processes[MAX_PROCESSES];  // ????????????????????????

// ??????????????
ProcessScheduler scheduler;

// ??????????????
void scheduler_init(void) {
    // ?????????????
    memset(&scheduler, 0, sizeof(ProcessScheduler));
    scheduler.next_pid = 1;
    scheduler.total_runtime = 0;
    scheduler.auto_balance = true;
    
    // ?????????????
    for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
        processes[i].pid = 0;
        processes[i].state = PROCESS_TERMINATED;
        processes[i].page_table = NULL;
        processes[i].page_table_size = 0;
    }
    
    // ????????????к????н???
    for (int i = 0; i < 3; i++) {
        scheduler.ready_queue[i] = NULL;
    }
    scheduler.blocked_queue = NULL;
    scheduler.running_process = NULL;
    scheduler.total_processes = 0;
}

// ???????????
void scheduler_shutdown(void) {
    // ???????н???
    for (int i = 0; i < 3; i++) {
        PCB* current = scheduler.ready_queue[i];
        while (current) {
            PCB* next = current->next;
            process_destroy(current);
            current = next;
        }
        scheduler.ready_queue[i] = NULL;
    }
    
    // ????????????
    PCB* current = scheduler.blocked_queue;
    while (current) {
        PCB* next = current->next;
        process_destroy(current);
        current = next;
    }
    scheduler.blocked_queue = NULL;
    
    // ??????????н???
    if (scheduler.running_process) {
        process_destroy(scheduler.running_process);
        scheduler.running_process = NULL;
    }
}

// ???????
void schedule(void) {
    // ????????????н?????????????????п??????
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

// ???????
void time_tick(void) {
    if (scheduler.running_process) {
        printf("\n??????н??? PID %u???????????%u\n", 
               scheduler.running_process->pid,
               scheduler.running_process->time_slice);
        
        scheduler.running_process->time_slice--;
        if (scheduler.running_process->time_slice == 0) {
            printf("???? PID %u ????????\n", scheduler.running_process->pid);
            
            // ???????????????
            PCB* proc = scheduler.running_process;
            proc->time_slice = TIME_SLICE;
            proc->state = PROCESS_READY;
            
            // ???????????????
            proc->next = scheduler.ready_queue[proc->priority];
            scheduler.ready_queue[proc->priority] = proc;
            scheduler.running_process = NULL;
            
            // ?????????
            schedule();
            
            if (scheduler.running_process) {
                printf("?л??????? PID %u\n", scheduler.running_process->pid);
            } else {
                printf("??о???????????\n");
            }
        }
    }
}

// ???????????
PCB* get_ready_queue(ProcessPriority priority) {
    if (priority >= PRIORITY_HIGH && priority <= PRIORITY_LOW) {
        return scheduler.ready_queue[priority];
    }
    return NULL;
}

// ?????????н???
PCB* get_running_process(void) {
    return scheduler.running_process;
}

// ??????????
uint32_t get_total_processes(void) {
    return scheduler.total_processes;
}

// ????????????
void set_process_priority(PCB* process, ProcessPriority priority) {
    if (!process || priority < PRIORITY_HIGH || priority > PRIORITY_LOW) {
        return;
    }
    
    // ??????????б仯????????
    if (process->priority == priority) {
        return;
    }
    
    // ?????????????????
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
    
    // ?????????
    process->priority = priority;
    
    // ???????????????е????
    process->next = scheduler.ready_queue[priority];
    scheduler.ready_queue[priority] = process;
}

// ???????????
void print_scheduler_status(void) {
    printf("\n=== ???????? ===\n");
    printf("?????????%u\n", scheduler.total_processes);
    if (scheduler.running_process) {
        printf("???н????PID %u\n", scheduler.running_process->pid);
    } else {
        printf("???н??????\n");
    }
    
    // ???????????
    for (int i = 0; i < 3; i++) {
        printf("\n????? %d ???????У?", i);
        PCB* current = scheduler.ready_queue[i];
        if (!current) {
            printf("??");
        }
        while (current) {
            printf("PID %u -> ", current->pid);
            current = current->next;
        }
        printf("\n");
    }
}

// ?????????
PCB* process_create(uint32_t pid, uint32_t page_table_size) {
    PCB* process = (PCB*)malloc(sizeof(PCB));
    if (!process) return NULL;
    
    // ????????????
    process->pid = pid;
    process->state = PROCESS_READY;
    process->priority = PRIORITY_NORMAL;
    process->time_slice = TIME_SLICE;
    
    // ???????
    process->page_table = (PageTableEntry*)calloc(page_table_size, sizeof(PageTableEntry));
    if (!process->page_table) {
        printf("????????????????????????\n");
        free(process);
        return NULL;
    }
    process->page_table_size = page_table_size;
    
    // ???????????
    memset(&process->stats, 0, sizeof(ProcessStats));
    
    return process;
}

// ???????
void process_destroy(PCB* pcb) {
    if (!pcb) return;
    
    printf("??????????? %u\n", pcb->pid);
    
    // ?????????????
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
    
    // ?????????????
    for (uint32_t i = 0; i < pcb->page_table_size; i++) {
        if (pcb->page_table[i].flags.present) {
            free_frame(pcb->page_table[i].frame_number);
        }
    }
    
    // ????????PCB
    free(pcb->page_table);
    free(pcb);
    scheduler.total_processes--;
}

// ???????????
bool process_allocate_memory(PCB* pcb, uint32_t start_page, uint32_t num_pages) {
    for (uint32_t page = start_page; page < start_page + num_pages; page++) {
        pcb->page_table[page].frame_number = (uint32_t)-1;
        pcb->page_table[page].flags.present = false;
        pcb->page_table[page].flags.swapped = false;
        pcb->page_table[page].last_access_time = get_current_time();
    }
    return true;
}

// ??????????
void process_free_memory(PCB* pcb, uint32_t start_page, uint32_t num_pages) {
    for (uint32_t page = start_page; page < start_page + num_pages; page++) {
        if (page < pcb->page_table_size) {
            pcb->page_table[page].frame_number = (uint32_t)-1;
            pcb->page_table[page].flags.present = false;
            pcb->page_table[page].flags.swapped = false;
        }
    }
}

// ???????ID???????
PCB* get_process_by_pid(uint32_t pid) {
    printf("\n=== ??????? PID=%u ===\n", pid);
    printf("????????????\n");
    // ??????з???????????
    for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state != PROCESS_TERMINATED) {
            printf("[%u] PID=%u, ??=%d\n", i, processes[i].pid, processes[i].state);
        }
    }
    
    for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
        // ????PID?????????
        if (processes[i].pid == pid) {
            printf("??????? %u????=%d\n", pid, processes[i].state);
            return &processes[i];
        }
    }
    printf("δ??????? %u\n", pid);
    return NULL;
}

// ?????????????
void print_process_stats(PCB* process) {
    if (!process) return;
    
    printf("\n=== ???? %u ????????? ===\n", process->pid);
    printf("\n??沼???\n");
    printf("????Σ???????=%u, ???=%u, ?????=%s\n",
           process->memory_layout.code.start_page,
           process->memory_layout.code.num_pages,
           process->memory_layout.code.is_allocated ? "??" : "??");
    printf("????Σ???????=%u, ???=%u, ?????=%s\n",
           process->memory_layout.data.start_page,
           process->memory_layout.data.num_pages,
           process->memory_layout.data.is_allocated ? "??" : "??");
    printf("??????  ??????=%u, ???=%u, ?????=%s\n",
           process->memory_layout.heap.start_page,
           process->memory_layout.heap.num_pages,
           process->memory_layout.heap.is_allocated ? "??" : "??");
    printf("?????  ??????=%u, ???=%u, ?????=%s\n",
           process->memory_layout.stack.start_page,
           process->memory_layout.stack.num_pages,
           process->memory_layout.stack.is_allocated ? "??" : "??");
    
    printf("\n???????\n");
    printf("  ?????????%u ???\n", process->stats.memory_usage);
    printf("  ????????%u\n", process->stats.page_faults);
    printf("  ????????%u\n", process->stats.mem_stats.page_replacements);
    
    printf("\n????????\n");
    printf("  ?????????%u\n", process->stats.total_runtime);
    printf("  ??????%u\n", process->stats.wait_time);
    
    printf("\n??????????\n");
    uint32_t total_accesses = process->stats.mem_stats.total_accesses;
    if (total_accesses > 0) {
        printf("????η????%u (%.1f%%)\n", 
               process->stats.mem_stats.code_accesses,
               (float)process->stats.mem_stats.code_accesses * 100.0 / total_accesses);
    } else {
        printf("????η????0 (0.0%%)\n");
    }
    // ... ?????????ε???????
}

// ??????ó???
PCB* create_application(const char* name, AppConfig* config) {
    if (!config) return NULL;
    
    PCB* process = (PCB*)malloc(sizeof(PCB));
    if (!process) return NULL;
    
    // ????????????
    process->pid = scheduler.next_pid++;
    strncpy(process->name, name, MAX_PROCESS_NAME - 1);
    process->name[MAX_PROCESS_NAME - 1] = '\0';
    process->state = PROCESS_READY;
    process->priority = config->priority;
    process->time_slice = TIME_SLICE;
    process->next = NULL;
    
    // ??????????沼??
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
    
    // ???????????
    process->app_config = *config;
    
    return process;
}

// ?????????沼??
bool setup_process_memory(PCB* process, ProcessMemoryLayout* layout) {
    if (!process || !layout) return false;
    
    // ?????????
    uint32_t total_pages = layout->code.num_pages + layout->data.num_pages +
                           layout->heap.num_pages + layout->stack.num_pages;
    
    // ???????
    process->page_table = (PageTableEntry*)calloc(total_pages, sizeof(PageTableEntry));
    if (!process->page_table) {
        return false;
    }
    process->page_table_size = total_pages;
    
    // ??????沼??
    process->memory_layout = *layout;
    
    return true;
}

// ??????
void monitor_process(PCB* process) {
    if (!process) return;
    
    printf("\n=== ????????? ===\n");
    printf("PID: %u\n", process->pid);
    printf("????: %s\n", process->app_config.name);
    print_process_status(process->state);
    
    // ????????????
    print_process_stats(process);
    
    // ??????????????????
    if (process->monitor_config.track_access_pattern) {
        printf("\n??????????\n");
        printf("????η????%u\n", process->stats.mem_stats.code_accesses);
        printf("????η????%u\n", process->stats.mem_stats.data_accesses);
        printf("??????%u\n", process->stats.mem_stats.heap_accesses);
        printf("??????%u\n", process->stats.mem_stats.stack_accesses);
    }
    
    // ???????????????
    if (process->monitor_config.collect_stats) {
        printf("\n????????\n");
        printf("??????????%u\n", process->stats.mem_stats.total_accesses);
        printf("????????%u\n", process->stats.mem_stats.page_faults);
        printf("????????????%u\n", process->stats.mem_stats.page_replacements);
        printf("????д???????%u\n", process->stats.mem_stats.writes_to_disk);
        printf("????????????%u\n", process->stats.mem_stats.reads_from_disk);
    }
}

// ?????????????????
void analyze_memory_usage(PCB* process) {
    if (!process) return;
    
    uint32_t total_pages = 0;
    uint32_t used_pages = 0;
    uint64_t current_time = get_current_time();
    uint32_t active_pages = 0;  // ?????????????
    
    for (uint32_t i = 0; i < process->page_table_size; i++) {
        if (process->page_table[i].flags.present) {
            used_pages++;
            total_pages++;
            // ??????????????????棨????1000?????λ???
            if (current_time - process->page_table[i].last_access_time < 1000) {
                active_pages++;
            }
        }
    }
    
    // ??????????
    process->stats.memory_usage = used_pages * PAGE_SIZE;
    
    printf("\n=== ?????÷??? (PID=%u) ===\n", process->pid);
    printf("?????????%u\n", process->page_table_size);
    printf("???????棺%u\n", used_pages);
    printf("?????棺%u\n", active_pages);
    printf("?????????%.2f%%\n", 
           (float)used_pages * 100 / process->page_table_size);
    printf("????????%.2f%%\n",
           (float)active_pages * 100 / used_pages);
}

// ?????????沼??
void optimize_memory_layout(PCB* process) {
    if (!process) return;
    
    printf("\n=== ?????沼?? ===\n");
    printf("???? %u (%s)\n", process->pid, process->app_config.name);
    
    // 1. ???????????????
    uint32_t free_start = (uint32_t)-1;
    uint32_t free_count = 0;
    
    for (uint32_t i = 0; i < process->page_table_size; i++) {
        if (!process->page_table[i].flags.present) {
            if (free_start == (uint32_t)-1) {
                free_start = i;
            }
            free_count++;
        } else if (free_count > 0) {
            printf("?????????棺???=%u??????=%u\n", free_start, free_count);
            free_start = (uint32_t)-1;
            free_count = 0;
        }
    }
    
    // 2. ???????????????
    printf("\n???????????????...\n");
    // ????????????????????????
}

// ????????????
void balance_memory_usage(void) {
    for (uint32_t pid = 1; pid <= scheduler.next_pid; pid++) {
        PCB* process = get_process_by_pid(pid);
        if (!process) continue;
        
        for (uint32_t i = 0; i < process->page_table_size; i++) {
            if (process->page_table[i].flags.present) {
                // ?????????δ????
                uint64_t current_time = get_current_time();
                if (current_time - process->page_table[i].last_access_time > 1000) {
                    // ????????????????
                    // ...????????????
                }
            }
        }
    }
}

void set_running_process(PCB* process) {
    if (!process) return;
    
    // ????????н????????????
    if (scheduler.running_process) {
        scheduler.running_process->state = PROCESS_READY;
        scheduler.running_process->next = scheduler.ready_queue[scheduler.running_process->priority];
        scheduler.ready_queue[scheduler.running_process->priority] = scheduler.running_process;
    }
    
    // ??????????н???
    scheduler.running_process = process;
    scheduler.running_process->state = PROCESS_RUNNING;
    scheduler.running_process->next = NULL;
}

// ?????????
void print_process_status(ProcessState state) {
    const char* state_str[] = {
        "????",     // PROCESS_READY
        "????",     // PROCESS_RUNNING
        "????",     // PROCESS_BLOCKED
        "???",     // PROCESS_TERMINATED
        "???"      // PROCESS_WAITING
    };
    
    if (state >= PROCESS_READY && state <= PROCESS_WAITING) {
        printf("????%s\n", state_str[state]);
    } else {
        printf("????δ?(%d)\n", state);
    }
}

// ????????????????
bool is_process_analyzed(PCB* process, PCB** analyzed_list, int analyzed_count) {
    if (!process || !analyzed_list) {
        return false;
    }
    
    // ????????б??в??????
    for (int i = 0; i < analyzed_count; i++) {
        if (analyzed_list[i] == process) {
            return true;  // ??????????????????
        }
    }
    
    return false;  // δ???????????δ??????
}

void initialize_process_memory(PCB* process) {
    if (!process) return;
    
    // ??????24?????
    for (uint32_t i = 0; i < 24; i++) {
        uint32_t page_num = i + 1;  // ??1?????0?????
        uint32_t frame = allocate_frame(process->pid, page_num);
        if (frame == (uint32_t)-1) {
            printf("??????????\n");
            return;
        }
        process->page_table[page_num].frame_number = frame;
        process->page_table[page_num].flags.present = true;
    }
    printf("????? %d ?????? 24 ?????????\n", process->pid);
}

void init_page_table(PCB* process, uint32_t size) {
    process->page_table = (PageTableEntry*)calloc(size, sizeof(PageTableEntry));
    process->page_table_size = size;
    
    // ?????????????
    for (uint32_t i = 0; i < size; i++) {
        process->page_table[i].frame_number = (uint32_t)-1;
        process->page_table[i].flags.present = false;
        process->page_table[i].flags.swapped = false;
        process->page_table[i].last_access_time = get_current_time();
    }
}

void destroy_process(PCB* process) {
    if (!process) return;
    
    printf("\n=== ??????? PID=%u ===\n", process->pid);
    printf("??????????%d\n", process->state);
    
    // ?????????????????
    if (scheduler.running_process == process) {
        scheduler.running_process = NULL;
    }
    
    // ????????????
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
    
    // ?????????????????
    for (uint32_t i = 0; i < PHYSICAL_PAGES; i++) {
        if (memory_manager.frames[i].process_id == process->pid) {
            free_frame(i);
        }
    }
    
    // ??????
    if (process->page_table) {
        free(process->page_table);
        process->page_table = NULL;
    }
    
    // ?????????
    process->state = PROCESS_TERMINATED;
    process->pid = 0;  // ???PID
    scheduler.total_processes--;
}

// ???????????????????
void add_to_ready_queue(PCB* process) {
    if (!process) return;
    
    // ????????????Ч??Χ??
    uint32_t priority = process->priority;
    if (priority >= 3) priority = 2;  // ??????0-2??Χ??
    
    // ?????????????????????
    if (!scheduler.ready_queue[priority]) {
        scheduler.ready_queue[priority] = process;
        process->next = NULL;
        return;
    }
    
    // ?????????????β??
    PCB* current = scheduler.ready_queue[priority];
    while (current->next) {
        current = current->next;
    }
    current->next = process;
    process->next = NULL;
    
    printf("???? %u ?????????? %u ?????????\n", process->pid, priority);
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
    if (!new_process) return NULL;
    
    // 初始化新进程
    new_process->pid = scheduler.next_pid++;
    new_process->priority = priority;
    new_process->state = PROCESS_READY;
    new_process->time_slice = DEFAULT_TIME_SLICE;
    new_process->next = NULL;
    
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
    
    // 复制进程信息到数组
    processes[process_index] = *new_process;
    
    // 更新调度器状态
    scheduler.total_processes++;
    add_to_ready_queue(&processes[process_index]);
    
    // 释放临时PCB
    free(new_process);
    
    return &processes[process_index];
}

