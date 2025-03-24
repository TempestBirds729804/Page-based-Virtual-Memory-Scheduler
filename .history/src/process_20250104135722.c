#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../include/process.h"
#include "../include/memory.h"
#include "../include/vm.h"

// ���̱�
static PCB processes[MAX_PROCESSES];  // ���̿��ƿ��

// ȫ���̵�����
ProcessScheduler scheduler;

// ��ʼ�����̵�����
void scheduler_init(void) {
    // ��ʼ���������ṹ
    memset(&scheduler, 0, sizeof(ProcessScheduler));
    scheduler.next_pid = 1;
    scheduler.total_runtime = 0;
    scheduler.auto_balance = true;

    // ��ʼ�����̱�
    for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
        processes[i].pid = 0;
        processes[i].state = PROCESS_TERMINATED;
        processes[i].page_table = NULL;
        processes[i].page_table_size = 0;
    }

    // ��ʼ����������
    for (int i = 0; i < 3; i++) {
        scheduler.ready_queue[i] = NULL;
    }
    scheduler.blocked_queue = NULL;
    scheduler.running_process = NULL;
    scheduler.total_processes = 0;
}

// �رս��̵�����
void scheduler_shutdown(void) {
    // ������������
    for (int i = 0; i < 3; i++) {
        PCB* current = scheduler.ready_queue[i];
        while (current) {
            PCB* next = current->next;
            process_destroy(current);
            current = next;
        }
        scheduler.ready_queue[i] = NULL;
    }

    // ������������
    PCB* current = scheduler.blocked_queue;
    while (current) {
        PCB* next = current->next;
        process_destroy(current);
        current = next;
    }
    scheduler.blocked_queue = NULL;

    // ��ǰ���н���
    if (scheduler.running_process) {
        process_destroy(scheduler.running_process);
        scheduler.running_process = NULL;
    }
}

// ����
void schedule(void) {
    // ��ǰû�����н��̣������ȼ���ߵĽ��̿�ʼ����
    if (!scheduler.running_process) {
        printf("\n��ʼ���̵���...\n");
        for (int i = 0; i < 3; i++) {
            if (scheduler.ready_queue[i]) {
                scheduler.running_process = scheduler.ready_queue[i];
                scheduler.ready_queue[i] = scheduler.ready_queue[i]->next;
                scheduler.running_process->next = NULL;
                scheduler.running_process->state = PROCESS_RUNNING;
                printf("���Ƚ��� PID %u (���ȼ� %d) ��ʼ���У�ʱ��Ƭ %u\n", 
                       scheduler.running_process->pid,
                       scheduler.running_process->priority,
                       scheduler.running_process->time_slice);
                return;  // �ҵ����̺���������
            } else {
                printf("���ȼ� %d ����Ϊ��\n", i);
            }
        }
        printf("û�п����еĽ���\n");
    } else {
        printf("��ǰ�н����������У�PID %u�����ȼ� %u��\n",
               scheduler.running_process->pid,
               scheduler.running_process->priority);
    }
}

// ʱ�ӵδ�
void time_tick(void) {
    if (scheduler.running_process) {
        printf("\n��ǰ���н��� PID %u��ʣ��ʱ��Ƭ %u\n", 
               scheduler.running_process->pid,
               scheduler.running_process->time_slice);
        
        scheduler.running_process->time_slice--;
        if (scheduler.running_process->time_slice == 0) {
            printf("���� PID %u ʱ��Ƭ���꣬���̽���\n", scheduler.running_process->pid);
            
            // ʱ��Ƭ���꣬��ֹ����
            PCB* proc = scheduler.running_process;
            process_destroy(proc);
            scheduler.running_process = NULL;
            
            // ������һ������
            schedule();
            
            if (scheduler.running_process) {
                printf("���̵��ȳɹ����½��� PID %u ��ʼ����\n", scheduler.running_process->pid);
            } else {
                printf("û�п����еĽ���\n");
            }
        }
    }
}

// ��ȡ��������
PCB* get_ready_queue(ProcessPriority priority) {
    if (priority >= PRIORITY_HIGH && priority <= PRIORITY_LOW) {
        return scheduler.ready_queue[priority];
    }
    return NULL;
}

// ��ȡ��ǰ���н���
PCB* get_running_process(void) {
    return scheduler.running_process;
}

// ��ȡ��������
uint32_t get_total_processes(void) {
    return scheduler.total_processes;
}

// ���ý������ȼ�
void set_process_priority(PCB* process, ProcessPriority priority) {
    if (!process || priority < PRIORITY_HIGH || priority > PRIORITY_LOW) {
        return;
    }
    
    // �������ȼ����ܸı䣬ֱ�ӷ���
    if (process->priority == priority) {
        return;
    }
    
    // ɾ�����̴Ӿ����ȼ�����
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
    
    // ���ý������ȼ�
    process->priority = priority;
    
    // ��������ӵ������ȼ����еĶ���
    process->next = scheduler.ready_queue[priority];
    scheduler.ready_queue[priority] = process;
}

// ��ӡ������״̬
void print_scheduler_status(void) {
    printf("\n=== ������״̬ ===\n");
    printf("��������%u\n", scheduler.total_processes);
    if (scheduler.running_process) {
        printf("�������еĽ��̣�PID %u\n", scheduler.running_process->pid);
    } else {
        printf("û���������еĽ���\n");
    }
    
    // ��ӡ��������
    for (int i = 0; i < 3; i++) {
        printf("\n���ȼ� %d ���̶��У�", i);
        PCB* current = scheduler.ready_queue[i];
        if (!current) {
            printf("��");
        }
        while (current) {
            printf("PID %u -> ", current->pid);
            current = current->next;
        }
        printf("\n");
    }
}

// ��������
PCB* process_create(uint32_t pid, uint32_t page_table_size) {
    PCB* process = (PCB*)malloc(sizeof(PCB));
    if (!process) return NULL;
    
    // ��ʼ��������Ϣ
    process->pid = pid;
    process->state = PROCESS_READY;
    process->priority = PRIORITY_NORMAL;  // ʹ��ö��ֵPRIORITY_NORMAL��ΪĬ�����ȼ�
    process->time_slice = TIME_SLICE;
    
    // ����ҳ��
    process->page_table = (PageTableEntry*)calloc(page_table_size, sizeof(PageTableEntry));
    if (!process->page_table) {
        printf("�����ڴ����ʧ�ܣ��޷�����ҳ��\n");
        free(process);
        return NULL;
    }
    process->page_table_size = page_table_size;
    
    // ��ʼ������ͳ����Ϣ
    memset(&process->stats, 0, sizeof(ProcessStats));
    
    return process;
}

// ���ٽ���
void process_destroy(PCB* pcb) {
    if (!pcb) return;
    
    printf("���ٽ��� %u\n", pcb->pid);
    
    // 1. �Ƚ����̴ӵ��������Ƴ�
    if (scheduler.running_process == pcb) {
        scheduler.running_process = NULL;
    }
    
    // 2. �Ӿ����������Ƴ�
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
    
    // 3. ���ý���״̬Ϊ��ֹ
    pcb->state = PROCESS_TERMINATED;
    
    // 4. �ͷŽ���ռ�õ�ҳ��
    if (pcb->page_table) {
        for (uint32_t i = 0; i < pcb->page_table_size; i++) {
            if (pcb->page_table[i].flags.present) {
                uint32_t frame = pcb->page_table[i].frame_number;
                pcb->page_table[i].flags.present = false;
                pcb->page_table[i].frame_number = (uint32_t)-1;
                free_frame(frame);
            }
        }
        // 5. �ͷ�ҳ��
        free(pcb->page_table);
        pcb->page_table = NULL;
    }
    
    scheduler.total_processes--;
}

// Ϊ���̷����ڴ�
bool process_allocate_memory(PCB* pcb, uint32_t start_page, uint32_t num_pages) {
    for (uint32_t page = start_page; page < start_page + num_pages; page++) {
        pcb->page_table[page].frame_number = (uint32_t)-1;
        pcb->page_table[page].flags.present = false;
        pcb->page_table[page].flags.swapped = false;
        pcb->page_table[page].last_access_time = get_current_time();
    }
    return true;
}

// �ͷŽ���ռ�õ��ڴ�
void process_free_memory(PCB* pcb, uint32_t start_page, uint32_t num_pages) {
    for (uint32_t page = start_page; page < start_page + num_pages; page++) {
        if (page < pcb->page_table_size) {
            pcb->page_table[page].frame_number = (uint32_t)-1;
            pcb->page_table[page].flags.present = false;
            pcb->page_table[page].flags.swapped = false;
        }
    }
}

// ����PID��ȡ����
PCB* get_process_by_pid(uint32_t pid) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].pid == pid && processes[i].state != PROCESS_TERMINATED) {
            return &processes[i];
        }
    }
    return NULL;
}

// ��ӡ����ͳ����Ϣ
void print_process_stats(PCB* process) {
    if (!process) return;
    
    printf("\n=== ���� %u �ڴ�ʹ��ͳ�� ===\n", process->pid);
    printf("\n�ڴ沼�֣�\n");
    printf("����Σ���ʼҳ=%u, ҳ��=%u, ״̬=%s\n",
           process->memory_layout.code.start_page,
           process->memory_layout.code.num_pages,
           process->memory_layout.code.is_allocated ? "�ѷ���" : "δ����");
    printf("���ݶΣ���ʼҳ=%u, ҳ��=%u, ״̬=%s\n",
           process->memory_layout.data.start_page,
           process->memory_layout.data.num_pages,
           process->memory_layout.data.is_allocated ? "�ѷ���" : "δ����");
    printf("�ѣ���ʼҳ=%u, ҳ��=%u, ״̬=%s\n",
           process->memory_layout.heap.start_page,
           process->memory_layout.heap.num_pages,
           process->memory_layout.heap.is_allocated ? "�ѷ���" : "δ����");
    printf("ջ����ʼҳ=%u, ҳ��=%u, ״̬=%s\n",
           process->memory_layout.stack.start_page,
           process->memory_layout.stack.num_pages,
           process->memory_layout.stack.is_allocated ? "�ѷ���" : "δ����");
    
    printf("\n�ڴ����ͳ�ƣ�\n");
    uint32_t total_accesses = process->stats.mem_stats.total_accesses;
    if (total_accesses > 0) {
        printf("����η��ʴ�����%u�� (%.1f%%)\n",
               process->stats.mem_stats.code_accesses,
               (float)process->stats.mem_stats.code_accesses * 100.0 / total_accesses);
        printf("���ݶη��ʴ�����%u�� (%.1f%%)\n",
               process->stats.mem_stats.data_accesses,
               (float)process->stats.mem_stats.data_accesses * 100.0 / total_accesses);
        printf("�ѷ��ʴ�����%u�� (%.1f%%)\n",
               process->stats.mem_stats.heap_accesses,
               (float)process->stats.mem_stats.heap_accesses * 100.0 / total_accesses);
        printf("ջ���ʴ�����%u�� (%.1f%%)\n",
               process->stats.mem_stats.stack_accesses,
               (float)process->stats.mem_stats.stack_accesses * 100.0 / total_accesses);
    }
    
    printf("\nҳ��ͳ�ƣ�\n");
    printf("ȱҳ������%u\n", process->stats.page_faults);
    printf("ҳ���滻������%u\n", process->stats.page_replacements);
    printf("ҳ�滻�������%u\n", process->stats.pages_swapped_in);
    printf("ҳ�滻��������%u\n", process->stats.pages_swapped_out);
    printf("�Ӵ��̶�ȡ��ҳ����%u\n", process->stats.mem_stats.reads_from_disk);
}

// ����Ӧ�ó���
PCB* create_application(const char* name, AppConfig* config) {
    if (!config) return NULL;
    
    PCB* process = (PCB*)malloc(sizeof(PCB));
    if (!process) return NULL;
    
    // ��ʼ��������Ϣ
    process->pid = scheduler.next_pid++;
    strncpy(process->name, name, MAX_PROCESS_NAME - 1);
    process->name[MAX_PROCESS_NAME - 1] = '\0';
    process->state = PROCESS_READY;
    process->priority = config->priority;
    process->time_slice = TIME_SLICE;
    process->next = NULL;
    
    // ���ý����ڴ沼��
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
    
    // ����Ӧ�ó�����Ϣ
    process->app_config = *config;
    
    return process;
}

// ���ý����ڴ沼��
bool setup_process_memory(PCB* process, ProcessMemoryLayout* layout) {
    if (!process || !layout) return false;
    
    // ����ҳ��
    uint32_t total_pages = layout->code.num_pages + layout->data.num_pages +
                           layout->heap.num_pages + layout->stack.num_pages;
    
    // ����ҳ��
    process->page_table = (PageTableEntry*)calloc(total_pages, sizeof(PageTableEntry));
    if (!process->page_table) {
        return false;
    }
    process->page_table_size = total_pages;
    
    // �����ڴ沼��
    process->memory_layout = *layout;
    
    return true;
}

// ��ؽ���
void monitor_process(PCB* process) {
    if (!process) return;
    
    printf("\n=== ��ؽ��� %u ��Ϣ ===\n", process->pid);
    
    // �����ڴ�ʹ�����
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
    
    printf("\n�ڴ�ʹ�������\n");
    printf("��ҳ����%u\n", process->page_table_size);
    printf("���ڴ��е�ҳ����%u\n", present_pages);
    printf("�ڴ����е�ҳ����%u\n", swapped_pages);
    
    printf("\n�ڴ����ͳ�ƣ�\n");
    printf("�ܷ��ʴ�����%u\n", process->stats.mem_stats.total_accesses);
    printf("����η��ʴ�����%u\n", process->stats.mem_stats.code_accesses);
    printf("���ݶη��ʴ�����%u\n", process->stats.mem_stats.data_accesses);
    printf("�ѷ��ʴ�����%u\n", process->stats.mem_stats.heap_accesses);
    printf("ջ���ʴ�����%u\n", process->stats.mem_stats.stack_accesses);
    
    printf("\nҳ��ͳ�ƣ�\n");
    printf("ȱҳ������%u\n", process->stats.page_faults);
    printf("ҳ���滻������%u\n", process->stats.page_replacements);
    printf("ҳ�滻�������%u\n", process->stats.pages_swapped_in);
    printf("ҳ�滻��������%u\n", process->stats.pages_swapped_out);
    printf("�Ӵ��̶�ȡ��ҳ����%u\n", process->stats.mem_stats.reads_from_disk);
    
    if (process->stats.mem_stats.total_accesses > 0) {
        float fault_rate = (float)process->stats.page_faults * 100.0 / 
                          process->stats.mem_stats.total_accesses;
        printf("\nҳ������ʣ�\n");
        printf("ȱҳ�ʣ�%.2f%%\n", fault_rate);
    }
}

// �����ڴ�ʹ�����
void analyze_memory_usage(PCB* process) {
    if (!process) return;
    
    uint32_t used_pages = 0;
    uint64_t current_time = get_current_time();
    uint32_t active_pages = 0;  // ��ǰ��Ծҳ��
    
    // ͳ���ڴ�ʹ�����
    for (uint32_t i = 0; i < process->page_table_size; i++) {
        if (process->page_table[i].flags.present) {
            used_pages++;
            // �ж��Ƿ�Ϊ��Ծҳ��1000ms�ڱ����ʹ�
            if (current_time - process->page_table[i].last_access_time < 1000) {
                active_pages++;
            }
        }
    }
    
    printf("\n=== ���� %u �ڴ�ʹ����� ===\n", process->pid);
    printf("��ҳ����%u\n", process->page_table_size);
    printf("��Ծҳ����%u (%.1f%%)\n", 
           used_pages, 
           (float)used_pages * 100 / process->page_table_size);
    printf("����Ծҳ����%u (%.1f%%)\n", 
           active_pages,
           (float)active_pages * 100 / used_pages);
    
    // �ڴ����ͳ��
    uint32_t total_accesses = process->stats.mem_stats.total_accesses;
    if (total_accesses > 0) {
        printf("\n�ڴ���ʷֲ���\n");
        printf("����Σ�%.1f%%\n", 
               (float)process->stats.mem_stats.code_accesses * 100 / total_accesses);
        printf("���ݶΣ�%.1f%%\n", 
               (float)process->stats.mem_stats.data_accesses * 100 / total_accesses);
        printf("�ѣ�%.1f%%\n", 
               (float)process->stats.mem_stats.heap_accesses * 100 / total_accesses);
        printf("ջ��%.1f%%\n", 
               (float)process->stats.mem_stats.stack_accesses * 100 / total_accesses);
    }
    
    // ҳ�����ͳ��
    printf("\nҳ�����ͳ�ƣ�\n");
    printf("ȱҳ������%u\n", process->stats.page_faults);
    printf("ҳ���滻������%u\n", process->stats.page_replacements);
    if (total_accesses > 0) {
        printf("ȱҳ�ʣ�%.2f%%\n", 
               (float)process->stats.page_faults * 100 / total_accesses);
    }
}

// �Ż��ڴ沼��
void optimize_memory_layout(PCB* process) {
    if (!process) return;
    
    printf("\n=== �ڴ沼���Ż� ===\n");
    printf("���� %u (%s)\n", process->pid, process->app_config.name);
    
    // 1. �ϲ�����ҳ
    uint32_t free_start = (uint32_t)-1;
    uint32_t free_count = 0;
    
    for (uint32_t i = 0; i < process->page_table_size; i++) {
        if (!process->page_table[i].flags.present) {
            if (free_start == (uint32_t)-1) {
                free_start = i;
            }
            free_count++;
        } else if (free_count > 0) {
            printf("�ϲ�����ҳ����ʼҳ=%u��ҳ��=%u\n", free_start, free_count);
            free_start = (uint32_t)-1;
            free_count = 0;
        }
    }
    
    // 2. �ƶ�����ҳ
    printf("\n�ƶ�����ҳ...\n");
    // ʵ��ʵ����Ҫ���ݾ�������ʵ��
}

// ƽ���ڴ�ʹ��
void balance_memory_usage(void) {
    for (uint32_t pid = 1; pid <= scheduler.next_pid; pid++) {
        PCB* process = get_process_by_pid(pid);
        if (!process) continue;
        
        for (uint32_t i = 0; i < process->page_table_size; i++) {
            if (process->page_table[i].flags.present) {
                // �ж��Ƿ�Ϊδ����ҳ
                uint64_t current_time = get_current_time();
                if (current_time - process->page_table[i].last_access_time > 1000) {
                    // ����Ϊδ����ҳ
                    // ...
                }
            }
        }
    }
}

// ���õ�ǰ���н���
void set_running_process(PCB* process) {
    if (!process) return;
    
    // ����ǰ���н��̷Żؾ�������
    if (scheduler.running_process) {
        scheduler.running_process->state = PROCESS_READY;
        scheduler.running_process->next = scheduler.ready_queue[scheduler.running_process->priority];
        scheduler.ready_queue[scheduler.running_process->priority] = scheduler.running_process;
    }
    
    // �������н���
    scheduler.running_process = process;
    scheduler.running_process->state = PROCESS_RUNNING;
    scheduler.running_process->next = NULL;
}

// ��ӡ����״̬
void print_process_status(ProcessState state) {
    printf("����״̬��");
    switch (state) {
        case PROCESS_NEW:
            printf("�½�\n");
            break;
        case PROCESS_READY:
            printf("����\n");
            break;
        case PROCESS_RUNNING:
            printf("����\n");
            break;
        case PROCESS_BLOCKED:
            printf("����\n");
            break;
        case PROCESS_TERMINATED:
            printf("��ֹ\n");
            break;
        default:
            printf("δ֪״̬\n");
            break;
    }
}

// �жϽ����Ƿ��ѷ���
bool is_process_analyzed(PCB* process, PCB** analyzed_list, int analyzed_count) {
    if (!process || !analyzed_list) {
        return false;
    }
    
    // �����ѷ��������б�
    for (int i = 0; i < analyzed_count; i++) {
        if (analyzed_list[i] == process) {
            return true;  // �ҵ����̣���ʾ�ѷ���
        }
    }
    
    return false;  // δ�ҵ����̣���ʾδ����
}

// ��ʼ�������ڴ�
void initialize_process_memory(PCB* process) {
    if (!process) return;
    
    // ֻ��ʼ��ǰ24ҳ
    for (uint32_t i = 0; i < 24; i++) {
        uint32_t page_num = i + 1;  // ��1��ʼ��0ҳΪ��
        uint32_t frame = allocate_frame(process->pid, page_num);
        if (frame == (uint32_t)-1) {
            printf("����ҳʧ�ܣ����̴���ʧ��\n");
            return;
        }
        process->page_table[page_num].frame_number = frame;
        process->page_table[page_num].flags.present = true;
    }
    printf("Ϊ���� %d ������ 24 ҳ�ڴ�\n", process->pid);
}

// ��ʼ��ҳ��
void init_page_table(PCB* process, uint32_t size) {
    process->page_table = (PageTableEntry*)calloc(size, sizeof(PageTableEntry));
    process->page_table_size = size;
    
    // ��ʼ��ÿҳ
    for (uint32_t i = 0; i < size; i++) {
        process->page_table[i].frame_number = (uint32_t)-1;
        process->page_table[i].flags.present = false;
        process->page_table[i].flags.swapped = false;
        process->page_table[i].last_access_time = get_current_time();
    }
}

// ��ӽ��̵���������
void add_to_ready_queue(PCB* process) {
    if (!process) return;
    
    printf("[DEBUG] add_to_ready_queue - ���� %u �����ȼ�: %u (%s)\n", 
           process->pid, process->priority,
           process->priority == PRIORITY_HIGH ? "�����ȼ�" :
           process->priority == PRIORITY_NORMAL ? "��ͨ���ȼ�" :
           process->priority == PRIORITY_LOW ? "�����ȼ�" : "δ֪���ȼ�");
    
    // ȷ�����ȼ���Ч��0-2��
    if (process->priority >= 3) {
        printf("���棺���� %u ���ȼ� %u ��Ч������Ϊ������ȼ�\n", 
               process->pid, process->priority);
        process->priority = PRIORITY_LOW;
    }
    
    printf("������ %u (���ȼ� %u) ��ӵ���������\n", 
           process->pid, process->priority);
    
    // ����������ж�Ӧ���ȼ�Ϊ�գ�ֱ�Ӳ���
    if (!scheduler.ready_queue[process->priority]) {
        scheduler.ready_queue[process->priority] = process;
        process->next = NULL;
        printf("���� %u ��ӵ����ȼ� %u ��������ͷ��\n", 
               process->pid, process->priority);
        
        // �����ǰû�����н��̣��������е���
        if (!scheduler.running_process) {
            schedule();
        }
        return;
    }
    
    // ��ӽ��̵���������β��
    PCB* current = scheduler.ready_queue[process->priority];
    while (current->next) {
        current = current->next;
    }
    current->next = process;
    process->next = NULL;
    
    printf("���� %u ��ӵ����ȼ� %u ��������β��\n", 
           process->pid, process->priority);
    
    // �����ǰû�����н��̣��������е���
    if (!scheduler.running_process) {
        schedule();
    }
}

// ��������
PCB* create_process(uint32_t priority, uint32_t code_pages, uint32_t data_pages) {
    printf("\n=== �������� ===\n");
    printf("����ҳ�� %u ҳ������ҳ�� %u ҳ\n", code_pages, data_pages);
    
    // ת�����ȼ�Ϊö��ֵ
    ProcessPriority proc_priority;
    switch(priority) {
        case 0: proc_priority = PRIORITY_HIGH; break;
        case 1: proc_priority = PRIORITY_NORMAL; break;
        case 2: proc_priority = PRIORITY_LOW; break;
        default: 
            printf("���棺���ȼ� %u ��Ч������Ϊ��ͨ���ȼ�\n", priority);
            proc_priority = PRIORITY_NORMAL;
            break;
    }
    
    // ������ҳ��
    uint32_t total_pages = code_pages + data_pages + 10;  // Ԥ����ҳ��
    if (total_pages > PHYSICAL_PAGES) {
        printf("��ҳ�� %u ����ϵͳҳ�� %u\n", total_pages, PHYSICAL_PAGES);
        return NULL;
    }
    
    PCB* new_process = (PCB*)malloc(sizeof(PCB));
    if (!new_process) return NULL;
    
    // �ҵ����н���
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
    
    // ��ʼ������
    new_process->pid = scheduler.next_pid++;
    new_process->priority = proc_priority;  // ʹ��ת��������ȼ�ö��ֵ
    new_process->state = PROCESS_READY;
    new_process->time_slice = TIME_SLICE;
    new_process->next = NULL;
    
    // ���ý����ڴ沼��
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
    
    // ����ҳ��
    new_process->page_table = (PageTableEntry*)calloc(total_pages, sizeof(PageTableEntry));
    new_process->page_table_size = total_pages;
    
    if (!new_process->page_table) {
        free(new_process);
        return NULL;
    }
    
    // Ϊ���̷����ڴ�
    for (uint32_t i = 0; i < code_pages + data_pages; i++) {
        uint32_t frame = allocate_frame(new_process->pid, i);
        if (frame == (uint32_t)-1) {
            printf("����ҳʧ�ܣ�����ѡ���滻ҳ\n");
            // ѡ���滻ҳ
            frame = select_victim_frame();
            if (frame == (uint32_t)-1) {
                printf("�޷�ѡ���滻ҳ�����̴���ʧ��\n");
                // �ͷŽ����ڴ�
                for (uint32_t j = 0; j < i; j++) {
                    free_frame(new_process->page_table[j].frame_number);
                }
                free(new_process->page_table);
                free(new_process);
                return NULL;
            }
        }
        // ����ҳ����
        new_process->page_table[i].frame_number = frame;
        new_process->page_table[i].flags.present = true;
        new_process->page_table[i].flags.swapped = false;
        new_process->page_table[i].last_access_time = get_current_time();
        
        // �����ڴ沼��״̬
        if (i < code_pages) {
            new_process->memory_layout.code.is_allocated = true;
        } else {
            new_process->memory_layout.data.is_allocated = true;
        }
    }
    
    // ���½�����ӵ����̱�
    processes[process_index] = *new_process;
    
    // ���½�����ӵ���������
    scheduler.total_processes++;
    add_to_ready_queue(&processes[process_index]);
    
    // �ͷ��½���
    free(new_process);
    
    return &processes[process_index];
}

// ģ������ڴ����
void simulate_process_memory_access(uint32_t pid, uint32_t access_count) {
    PCB* proc = get_process_by_pid(pid);
    if (!proc) {
        printf("���� %u ������\n", pid);
        return;
    }

    printf("��ʼģ����� %u �ڴ���ʣ����ʴ��� %u\n", pid, access_count);
    
    for (uint32_t i = 0; i < access_count; i++) {
        // ���ѡ��һ��ҳ����з���
        uint32_t page_num = rand() % proc->page_table_size;
        uint32_t offset = rand() % PAGE_SIZE;
        uint32_t addr = page_num * PAGE_SIZE + offset;
        
        // ���ѡ�����д
        if (rand() % 2) {
            // ��
            access_memory(proc, addr, false);
            printf("���� %u ��ȡ��ַ 0x%x\n", pid, addr);
        } else {
            // д
            access_memory(proc, addr, true);
            printf("���� %u д��ַ 0x%x\n", pid, addr);
        }
    }
    
    printf("���� %u �ڴ����ģ�����\n", pid);
}

// Ϊ���̷����ڴ�
void allocate_process_memory(uint32_t pid, uint32_t size, uint32_t flags) {
    PCB* proc = get_process_by_pid(pid);
    if (!proc) {
        printf("���� %u ������\n", pid);
        return;
    }

    // ��Ҫ�����ҳ��
    uint32_t pages_needed = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    
    // ����Ƿ񳬹��������ҳ��
    if (proc->page_table_size + pages_needed > MAX_PAGES_PER_PROCESS) {
        printf("�ڴ治�㣬�޷����� %u ҳ\n", pages_needed);
        return;
    }

    printf("��ʼΪ���� %u ����%s�ڴ棬��С %u �ֽڣ�%u ҳ\n", 
           pid, flags ? "��" : "���ݶ�", size, pages_needed);

    // ����ҳ��
    for (uint32_t i = 0; i < pages_needed; i++) {
        uint32_t frame = allocate_frame(pid, proc->page_table_size);
        if (frame == (uint32_t)-1) {  // ʹ��-1��Ϊ��Чҳ��
            printf("�ڴ����ʧ��\n");
            return;
        }
        
        // ����ҳ����
        proc->page_table[proc->page_table_size].frame_number = frame;
        proc->page_table[proc->page_table_size].flags.present = 1;
        proc->page_table[proc->page_table_size].flags.swapped = 0;
        proc->page_table[proc->page_table_size].last_access_time = get_current_time();
        proc->page_table_size++;

        // �����ڴ沼��״̬
        if (flags) {
            // ���ڴ�
            proc->memory_layout.stack.num_pages++;
            proc->memory_layout.stack.is_allocated = true;
        } else {
            // ���ݶ��ڴ�
            proc->memory_layout.heap.num_pages++;
            proc->memory_layout.heap.is_allocated = true;
        }
    }

    printf("���� %u %s�ڴ�������\n", pid, flags ? "��" : "���ݶ�");
}

// ��������
PCB* create_process_with_pid(uint32_t pid, uint32_t priority, uint32_t code_pages, uint32_t data_pages) {
    // ������ID�Ƿ���Ч
    if (pid >= MAX_PROCESSES || get_process_by_pid(pid) != NULL) {
        printf("��Ч�Ľ���ID�����̴���ʧ��\n");
        return NULL;
    }
    
    // ������ȼ��Ƿ���Ч��0-2��
    printf("\n[DEBUG] �յ����ȼ�����: %u\n", priority);
    ProcessPriority proc_priority;
    switch(priority) {
        case 0: 
            proc_priority = PRIORITY_HIGH;
            printf("[DEBUG] ����Ϊ�����ȼ� (PRIORITY_HIGH)\n");
            break;
        case 1: 
            proc_priority = PRIORITY_NORMAL;
            printf("[DEBUG] ����Ϊ��ͨ���ȼ� (PRIORITY_NORMAL)\n");
            break;
        case 2: 
            proc_priority = PRIORITY_LOW;
            printf("[DEBUG] ����Ϊ�����ȼ� (PRIORITY_LOW)\n");
            break;
        default:
            printf("���棺���ȼ� %u ��Ч������Ϊ������ȼ�\n", priority);
            proc_priority = PRIORITY_LOW;
            break;
    }
    
    // ������ҳ��
    uint32_t total_pages = code_pages + data_pages;
    if (total_pages > VIRTUAL_PAGES) {
        printf("����ҳ�� (%u) ���������ڴ��С (%u)\n", 
               total_pages, VIRTUAL_PAGES);
        return NULL;
    }
    
    // ����Ƿ����㹻�Ŀ���ҳ��
    uint32_t free_frames = get_free_frames_count();
    printf("\n=== �������� %u ===\n", pid);
    printf("����ҳ�� %u (�����=%u, ���ݶ�=%u)\n", total_pages, code_pages, data_pages);
    printf("��ǰ����ҳ�� %u\n", free_frames);
    printf("�������ȼ���%u\n", proc_priority);
    
    if (free_frames < MIN(total_pages, PHYSICAL_PAGES/4)) {
        printf("�ڴ治�㣬�����޷�����\n");
        return NULL;
    }
    
    // ����PCB
    PCB* process = (PCB*)malloc(sizeof(PCB));
    if (!process) {
        printf("�ڴ����ʧ��\n");
        return NULL;
    }
    
    // ��ʼ��PCB
    memset(process, 0, sizeof(PCB));  // ���������ֶ�
    process->pid = pid;
    process->priority = proc_priority;  // ʹ��ת��������ȼ�
    printf("[DEBUG] ���ý������ȼ�Ϊ: %u\n", process->priority);
    process->state = PROCESS_READY;
    process->page_table_size = total_pages;
    process->time_slice = TIME_SLICE;
    
    // ����ҳ��
    process->page_table = (PageTableEntry*)calloc(total_pages, sizeof(PageTableEntry));
    if (!process->page_table) {
        printf("�ڴ����ʧ��\n");
        free(process);
        return NULL;
    }
    
    // ��ʼ���ڴ沼��
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
    
    // ��ʼ������ͳ����Ϣ
    memset(&process->stats, 0, sizeof(ProcessStats));
    
    printf("\n���̴������\n");
    printf("PID: %u\n", process->pid);
    printf("���ȼ�: %u\n", process->priority);
    printf("[DEBUG] ����ȷ�Ͻ������ȼ�: %u\n", process->priority);
    printf("�ڴ沼�֣�\n");
    printf("- ����Σ���ʼҳ=%u, ҳ��=%u\n", 
           process->memory_layout.code.start_page,
           process->memory_layout.code.num_pages);
    printf("- ���ݶΣ���ʼҳ=%u, ҳ��=%u\n",
           process->memory_layout.data.start_page,
           process->memory_layout.data.num_pages);
    
    // Ϊ���̷�������ҳ��
    printf("\n��ʼ��������ҳ��...\n");
    for (uint32_t i = 0; i < total_pages; i++) {
        uint32_t frame = allocate_frame(process->pid, i);
        if (frame == (uint32_t)-1) {
            printf("����ҳ��ʧ�ܣ�����ѡ���滻ҳ\n");
            frame = select_victim_frame();
            if (frame == (uint32_t)-1) {
                printf("�޷�ѡ���滻ҳ�����̴���ʧ��\n");
                // �ͷ��ѷ����ҳ��
                for (uint32_t j = 0; j < i; j++) {
                    free_frame(process->page_table[j].frame_number);
                }
                free(process->page_table);
                free(process);
                return NULL;
            }
        }
        
        // ����ҳ����
        process->page_table[i].frame_number = frame;
        process->page_table[i].flags.present = true;
        process->page_table[i].flags.swapped = false;
        process->page_table[i].last_access_time = get_current_time();
    }
    
    // ��������ӵ����̱�
    uint32_t process_index = MAX_PROCESSES;
    for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state == PROCESS_TERMINATED || processes[i].pid == 0) {
            process_index = i;
            break;
        }
    }
    
    if (process_index == MAX_PROCESSES) {
        printf("���̱��������޷������½���\n");
        // �ͷ��ѷ������Դ
        for (uint32_t i = 0; i < total_pages; i++) {
            free_frame(process->page_table[i].frame_number);
        }
        free(process->page_table);
        free(process);
        return NULL;
    }
    
    // ���ƽ��̵����̱�
    processes[process_index] = *process;
    free(process);  // �ͷ���ʱPCB
    
    // ��������ӵ���������
    scheduler.total_processes++;
    add_to_ready_queue(&processes[process_index]);
    
    return &processes[process_index];
}
