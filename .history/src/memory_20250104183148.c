#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "../include/memory.h"
#include "../include/process.h"
#include "../include/vm.h"

// �ڴ������
MemoryManager memory_manager;  // �ڴ������
PhysicalMemory phys_mem;      // �����ڴ�ṹ�壬���������ڴ��ҳ��λͼ

// ҳ��
page_t *pages = NULL;
uint32_t total_pages = 0;

// ��ʼ���ڴ������
void memory_init(void) {
    // ��ʼ���ڴ�������ṹ�壬��ʼ�����г�ԱΪ0
    memset(&memory_manager, 0, sizeof(MemoryManager));
    memory_manager.free_frames_count = PHYSICAL_PAGES;  // ��ʼ��ʱ����ҳ����Ϊ����ҳ����
    memory_manager.strategy = FIRST_FIT;               // Ĭ��ʹ��FIFO�������

    // ��ʼ�������ڴ棬���������ڴ�ռ䣬��ʼ��Ϊ0
    phys_mem.memory = (uint8_t*)calloc(PHYSICAL_PAGES * PAGE_SIZE, 1);  // ���������ڴ�ռ䣬��ʼ��Ϊ0
    phys_mem.frame_map = (bool*)calloc(PHYSICAL_PAGES, sizeof(bool));   // ����ҳ��λͼ����ʼ��Ϊfalse
    phys_mem.total_frames = PHYSICAL_PAGES;                            // ����ҳ����
    phys_mem.free_frames = PHYSICAL_PAGES;                             // ��ʼ��ʱ����ҳ����

    // ����ڴ��ʼ���Ƿ�ɹ�
    if (!phys_mem.memory || !phys_mem.frame_map) {
        fprintf(stderr, "�ڴ��ʼ��ʧ�ܣ�\n");
        exit(1);  // �ڴ��ʼ��ʧ�ܣ��˳�����
    }

    // ��ʼ��ҳ��
    total_pages = PHYSICAL_PAGES;  // ʵ��ҳ����
    pages = (page_t*)calloc(total_pages, sizeof(page_t));
    if (!pages) {
        fprintf(stderr, "ҳ���ʼ��ʧ�ܣ�\n");
        exit(1);
    }
    
    // ��ʼ��ÿ��ҳ��
    uint64_t current_time = get_current_time();
    for (uint32_t i = 0; i < total_pages; i++) {
        pages[i].frame_number = (uint32_t)-1;
        pages[i].is_valid = false;
        pages[i].is_locked = false;
        pages[i].is_dirty = false;
        pages[i].last_access = current_time;
        
        // ͬʱ��ʼ���ڴ�������е�ҳ����Ϣ
        memory_manager.frames[i].last_access_time = current_time;
        memory_manager.frames[i].is_allocated = false;
        memory_manager.frames[i].is_dirty = false;
        memory_manager.frames[i].process_id = 0;
        memory_manager.frames[i].virtual_page_num = 0;
    }
}

// ����ҳ��
uint32_t allocate_frame(uint32_t pid, uint32_t virtual_page) {
    // ���ҿ���ҳ��
    for (uint32_t i = 0; i < PHYSICAL_PAGES; i++) {
        if (!memory_manager.frames[i].is_allocated) {
            // �����ڴ������״̬
            memory_manager.frames[i].is_allocated = true;
            memory_manager.frames[i].process_id = pid;
            memory_manager.frames[i].virtual_page_num = virtual_page;
            memory_manager.frames[i].last_access_time = get_current_time();
            memory_manager.frames[i].is_dirty = false;
            memory_manager.free_frames_count--;
            
            // ���������ڴ�ӳ��
            phys_mem.frame_map[i] = true;
            phys_mem.free_frames--;
            
            return i;
        }
    }
    
    return (uint32_t)-1;
}

// �ͷ�ҳ��
void free_frame(uint32_t frame_number) {
    if (frame_number >= PHYSICAL_PAGES) return;
    
    // 1. �ȼ��ҳ���Ƿ��Ѿ����ͷ�
    if (!memory_manager.frames[frame_number].is_allocated) {
        return;
    }
    
    // 2. ����ҳ��״̬
    memory_manager.frames[frame_number].is_allocated = false;
    memory_manager.frames[frame_number].is_swapping = false;
    memory_manager.frames[frame_number].is_dirty = false;
    memory_manager.frames[frame_number].process_id = 0;
    memory_manager.frames[frame_number].virtual_page_num = 0;
    memory_manager.frames[frame_number].last_access_time = 0;
    
    // 3. ���������ڴ�ӳ��
    phys_mem.frame_map[frame_number] = false;
    
    // 4. ���¼�����
    memory_manager.free_frames_count++;
    phys_mem.free_frames++;
}

// ���ҳ���Ƿ��ѷ���
bool is_frame_allocated(uint32_t frame_number) {
    if (frame_number >= PHYSICAL_PAGES) {
        return false;
    }
    return memory_manager.frames[frame_number].is_allocated;
}

// ���úͻ�ȡ�������
void set_allocation_strategy(AllocationStrategy strategy) {
    memory_manager.strategy = strategy;  // �����ڴ�������
}

AllocationStrategy get_allocation_strategy(void) {
    return memory_manager.strategy;
}

// ��ȡ�����ڴ�ָ��
uint8_t* get_physical_memory(void) {
    return phys_mem.memory;  // ���������ڴ�ָ��
}

// ��ȡҳ��ӳ��
bool* get_frame_map(void) {
    return phys_mem.frame_map;  // ����ҳ��λͼָ��
}

void update_free_frames_count(uint32_t free_count) {
    phys_mem.free_frames = free_count;  // ���������ڴ��еĿ���ҳ����
}

// ��ӡ�ڴ沼��
void print_memory_map(void) {
    // ͳ����ʹ�õ�ҳ����ڴ���Ƭ
    uint32_t used_frames = 0;
    uint32_t fragments = 0;
    bool in_fragment = false;
    
    // ������ʹ��ҳ��������Ƭ��
    for (uint32_t i = 0; i < PHYSICAL_PAGES; i++) {
        if (memory_manager.frames[i].is_allocated) {
            used_frames++;
            if (!in_fragment) {
                fragments++;
                in_fragment = true;
            }
        } else {
            in_fragment = false;
        }
    }

    printf("\n=== �����ڴ沼�� ===\n");
    printf("��ҳ������%u\n\n", PHYSICAL_PAGES);
    
    // ��ʾ�ѷ���ҳ���б�
    printf("�ѷ���ҳ���б�\n");
    for (uint32_t i = 0; i < PHYSICAL_PAGES; i++) {
        if (memory_manager.frames[i].is_allocated) {
            printf("[%3u] PID=%-4u ����ҳ��=0x%04x\n", 
                   i, memory_manager.frames[i].process_id, memory_manager.frames[i].virtual_page_num);
        }
    }
    
    printf("\n�ڴ�ʹ�����飺\n");
    printf("�������ڴ棺%u �ֽ�\n", PHYSICAL_PAGES * PAGE_SIZE);
    printf("�����ڴ棺%u �ֽ� (%.1f%%)\n", 
           used_frames * PAGE_SIZE,
           (float)(used_frames * 100) / PHYSICAL_PAGES);
    printf("��Ƭ����%u\n", fragments);
    
    // ASCIIͼ����ʾ
    printf("\n�ڴ�ʹ��ͼʾ��\n");
    printf("������ʾ���ã�����ʾ���У�\n");
    for (uint32_t i = 0; i < PHYSICAL_PAGES; i++) {
        if (i % 32 == 0) {
            printf("\n%3u: ", i);
        }
        printf("%s", memory_manager.frames[i].is_allocated ? "��" : "��");
        if ((i + 1) % 8 == 0) {
            printf(" ");
        }
    }
    printf("\n");
}

// ��ȡ����ҳ������
uint32_t get_free_frames_count(void) {
    return memory_manager.free_frames_count;
}

// �����ڴ���Ƭ����
uint32_t count_memory_fragments(void) {
    uint32_t fragments = 0;
    bool in_fragment = false;
    
    // ��������ҳ��
    for (uint32_t i = 0; i < PHYSICAL_PAGES; i++) {
        if (!memory_manager.frames[i].is_allocated) {
            // �ҵ�����ҳ��
            if (!in_fragment) {
                // �µ���Ƭ��ʼ
                fragments++;
                in_fragment = true;
            }
        } else {
            // �ҵ��ѷ���ҳ��
            in_fragment = false;
        }
    }
    
    return fragments;
}

/**
 * @brief ��ȡҳ���Ӧ�������ַ
 * 
 * @param frame ҳ���
 * @return void* ���������ַ�����ҳ����Ч�򷵻�NULL
 */
void* get_physical_address(uint32_t frame) {
    if (frame >= PHYSICAL_PAGES) {
        return NULL;
    }
    // ���������ַ = ����ַ + ҳ��� * ҳ���С
    return (void*)(phys_mem.memory + (frame * PAGE_SIZE));
}

// ����ڴ������״̬һ����
void check_memory_state(void) {
    uint32_t allocated_count = 0;
    uint32_t mapped_count = 0;
    
    for (uint32_t i = 0; i < PHYSICAL_PAGES; i++) {
        if (memory_manager.frames[i].is_allocated) {
            allocated_count++;
        }
        if (phys_mem.frame_map[i]) {
            mapped_count++;
        }
        
        // ���״̬һ����
        if (memory_manager.frames[i].is_allocated != phys_mem.frame_map[i]) {
            printf("���棺ҳ�� %u ״̬��һ�£�����=%d, ӳ��=%d\n",
                   i, memory_manager.frames[i].is_allocated, phys_mem.frame_map[i]);
        }
    }
    
    if (allocated_count != mapped_count || 
        PHYSICAL_PAGES - allocated_count != memory_manager.free_frames_count) {
        printf("���棺�ڴ����һ�£��ѷ���=%u, ��ӳ��=%u, ���м���=%u\n",
               allocated_count, mapped_count, memory_manager.free_frames_count);
    }
}

// ����ҳ�����ʱ���
void update_page_access(page_t *page) {
    if (page) {
        page->last_access = get_current_time();
    }
}

// LRU�㷨ѡ������ҳ��
page_t* find_lru_victim(void) {
    uint32_t victim_frame = select_victim_frame(); // ʹ��select_victim_frameѡ������ҳ��
    if (victim_frame == (uint32_t)-1) {
        return NULL;
    }
    
    // �ҵ���Ӧ��ҳ��
    for (uint32_t i = 0; i < total_pages; i++) {
        if (pages[i].frame_number == victim_frame) {
            return &pages[i];
        }
    }
    
    return NULL;
}

// �滻ҳ��
int replace_page(page_t *victim, page_t *new_page) {
    if (!victim || !new_page) {
        return -1;
    }
    
    // ��ȡ���̿��ƿ�
    PCB* process = get_process_by_pid(memory_manager.frames[victim->frame_number].process_id);
    if (!process) {
        return -1;
    }
    
    // ʹ��ҳ��
    if (victim->is_dirty) {
        // ʹ��page_out
        if (!page_out(process, memory_manager.frames[victim->frame_number].virtual_page_num)) {
            return -1;
        }
    }
    
    // ����ҳ����Ϣ
    victim->frame_number = new_page->frame_number;
    victim->last_access = get_current_time(); // ʹ��vm.h��get_current_time
    victim->is_valid = true;
    victim->is_dirty = false;
    
    return 0;
}

void memory_shutdown(void) {
    // �ͷ������ڴ��ҳ��λͼ
    free(phys_mem.memory);
    free(phys_mem.frame_map);
    
    // �ͷ�ҳ��
    if (pages) {
        free(pages);
        pages = NULL;
    }
}

// ����ҳ�����ʱ��
void update_frame_access_time(uint32_t frame_number) {
    if (frame_number < PHYSICAL_PAGES && memory_manager.frames[frame_number].is_allocated) {
        uint64_t old_time = memory_manager.frames[frame_number].last_access_time;
        memory_manager.frames[frame_number].last_access_time = get_current_time();
        printf("����ҳ�� %u �ķ���ʱ�䣺%llu -> %llu\n", 
               frame_number, 
               old_time,
               memory_manager.frames[frame_number].last_access_time);
    }
}

uint32_t select_victim_frame(void) {
    uint32_t victim_frame = (uint32_t)-1;
    uint64_t oldest_access = 0;  // ��ʼ��Ϊ0���������ǻ��ҵ�����ķ���ʱ��
    uint64_t current_time = get_current_time();
    
    printf("\n=== LRUҳ���û� ===\n");
    
    // ��һ��ɨ�裺Ѱ��δ�޸������δʹ�õ�ҳ��
    for (uint32_t i = 0; i < PHYSICAL_PAGES; i++) {
        if (!memory_manager.frames[i].is_allocated || 
            memory_manager.frames[i].process_id == 0) {
            continue;
        }
        
        // ��ȡ��ҳ���Ӧ�Ľ��̺�ҳ����
        PCB* process = get_process_by_pid(memory_manager.frames[i].process_id);
        if (!process || memory_manager.frames[i].virtual_page_num >= process->page_table_size) {
            continue;
        }
        
        // ���ҳ�����present��־
        PageTableEntry* pte = &process->page_table[memory_manager.frames[i].virtual_page_num];
        if (!pte->flags.present) {
            continue;  // ���ҳ�治���ڴ��У�����
        }
        
        uint64_t time_diff = current_time - memory_manager.frames[i].last_access_time;
        if (!memory_manager.frames[i].is_dirty && 
            (victim_frame == (uint32_t)-1 || time_diff > oldest_access)) {
            oldest_access = time_diff;
            victim_frame = i;
        }
    }
    
    // ���û�ҵ�δ�޸ĵ�ҳ�棬Ѱ�����δʹ�õ�ҳ�棨�������Ƿ��޸ģ�
    if (victim_frame == (uint32_t)-1) {
        oldest_access = 0;
        
        for (uint32_t i = 0; i < PHYSICAL_PAGES; i++) {
            if (!memory_manager.frames[i].is_allocated || 
                memory_manager.frames[i].process_id == 0) {
                continue;
            }
            
            // ��ȡ��ҳ���Ӧ�Ľ��̺�ҳ����
            PCB* process = get_process_by_pid(memory_manager.frames[i].process_id);
            if (!process || memory_manager.frames[i].virtual_page_num >= process->page_table_size) {
                continue;
            }
            
            // ���ҳ�����present��־
            PageTableEntry* pte = &process->page_table[memory_manager.frames[i].virtual_page_num];
            if (!pte->flags.present) {
                continue;  // ���ҳ�治���ڴ��У�����
            }
            
            uint64_t time_diff = current_time - memory_manager.frames[i].last_access_time;
            if (victim_frame == (uint32_t)-1 || time_diff > oldest_access) {
                oldest_access = time_diff;
                victim_frame = i;
            }
        }
    }
    
    if (victim_frame != (uint32_t)-1) {
        // ����ͳ����Ϣ
        vm_manager.stats.page_replacements++;
        if (memory_manager.frames[victim_frame].is_dirty) {
            vm_manager.stats.disk_writes++;
            vm_manager.stats.pages_swapped_out++;
        }
        
        printf("\nѡ��ҳ�� %u �����û� (PID=%u, ҳ��=0x%04x, ��=%s)\n", 
               victim_frame,
               memory_manager.frames[victim_frame].process_id,
               memory_manager.frames[victim_frame].virtual_page_num,
               memory_manager.frames[victim_frame].is_dirty ? "��" : "��");
    } else {
        printf("�����޷��ҵ����ʵ�ҳ������û�\n");
    }
    
    return victim_frame;
}

// �������ڴ��ж�ȡ
bool read_physical_memory(uint32_t frame, uint32_t offset, void* buffer, size_t size) {
    // ��������Ч��
    if (frame >= PHYSICAL_PAGES || offset + size > PAGE_SIZE || !buffer) {
        return false;  // ��Ч�Ĳ���������ʧ��
    }

    // ���ҳ���Ƿ��ѱ�ʹ��
    if (!phys_mem.frame_map[frame]) {
        return false;  // ҳ��δ��ʹ�ã�����ʧ��
    }

    // ���·���ʱ��
    memory_manager.frames[frame].last_access_time = get_current_time();
    
    // ����Դ��ַ��ִ���ڴ濽��
    uint8_t* src = phys_mem.memory + (frame * PAGE_SIZE) + offset;
    memcpy(buffer, src, size);
    return true;  // �ɹ���ȡ������ true
}

// д�������ڴ�
bool write_physical_memory(uint32_t frame, uint32_t offset, const void* data, size_t size) {
    // ��������Ч��
    if (frame >= PHYSICAL_PAGES || offset + size > PAGE_SIZE || !data) {
        return false;  // ��Ч�Ĳ���������ʧ��
    }

    // ���ҳ���Ƿ��ѱ�ʹ��
    if (!phys_mem.frame_map[frame]) {
        return false;  // ҳ��δ��ʹ�ã�����ʧ��
    }

    // ���·���ʱ������־
    memory_manager.frames[frame].last_access_time = get_current_time();
    memory_manager.frames[frame].is_dirty = true;
    
    // ����Ŀ���ַ��ִ���ڴ濽��
    uint8_t* dst = phys_mem.memory + (frame * PAGE_SIZE) + offset;
    memcpy(dst, data, size);
    return true;  // �ɹ�д�룬���� true
}

uint32_t select_victim_page(PCB* process) {
    printf("\n=== ѡ������ҳ�� ===\n");
    
    // �������н��̲��ҿ����û���ҳ��
    for (uint32_t pid = 1; pid < scheduler.next_pid; pid++) {
        PCB* current_process = get_process_by_pid(pid);
        if (!current_process || current_process == process) {
            continue;
        }
        
        printf("������ %u ��ҳ��\n", pid);
        
        // ������̵�ǰ������ҳ�����
        float physical_ratio = calculate_physical_pages_ratio(current_process);
        printf("���� %u ��ǰ����ҳ�����: %.2f%%\n", pid, physical_ratio);
        
        // ���Ŀ���������ҳ��������ͣ�С��15%����������
        // �������ø��͵���ֵ����ȷ���ڽ����������Ȼ���Խ���ҳ���û�
        if (physical_ratio <= 15.0f) {
            printf("���� %u ������ҳ��������ͣ�����\n", pid);
            continue;
        }
        
        // ���Ҹý��������δʹ�õ�ҳ��
        uint32_t oldest_page = (uint32_t)-1;
        uint64_t oldest_time = UINT64_MAX;
        uint32_t oldest_frame = (uint32_t)-1;
        
        for (uint32_t i = 0; i < current_process->page_table_size; i++) {
            if (current_process->page_table[i].flags.present) {
                if (current_process->page_table[i].last_access_time < oldest_time) {
                    oldest_time = current_process->page_table[i].last_access_time;
                    oldest_page = i;
                    oldest_frame = current_process->page_table[i].frame_number;
                }
            }
        }
        
        if (oldest_page != (uint32_t)-1) {
            printf("�ڽ��� %u ���ҵ����û�ҳ�棺����ҳ %u -> ����ҳ�� %u\n",
                   pid, oldest_page, oldest_frame);
            
            // ��ҳ��д�뽻����
            if (current_process->page_table[oldest_page].flags.dirty) {
                printf("ҳ�����޸ģ���Ҫд�뽻����\n");
                if (!swap_out_page(oldest_frame)) {
                    printf("д�뽻����ʧ�ܣ�������һ��ҳ��\n");
                    continue;
                }
            }
            
            current_process->page_table[oldest_page].flags.present = false;
            current_process->page_table[oldest_page].flags.swapped = true;
            free_frame(oldest_frame);  // �ͷ�����ҳ��
            
            printf("ҳ���Ѵ����ͷ�����ҳ�� %u\n", oldest_frame);
            return oldest_frame;
        }
    }
    
    // ������н��̵�����ҳ��������ܵͣ���ѡ��ռ���ڴ����Ľ��̽����û�
    if (process) {
        PCB* max_pages_process = NULL;
        uint32_t max_present_pages = 0;
        
        for (uint32_t pid = 1; pid < scheduler.next_pid; pid++) {
            PCB* current_process = get_process_by_pid(pid);
            if (!current_process || current_process == process) {
                continue;
            }
            
            uint32_t present_pages = 0;
            for (uint32_t i = 0; i < current_process->page_table_size; i++) {
                if (current_process->page_table[i].flags.present) {
                    present_pages++;
                }
            }
            
            if (present_pages > max_present_pages) {
                max_present_pages = present_pages;
                max_pages_process = current_process;
            }
        }
        
        if (max_pages_process) {
            printf("���н�������ҳ��������ϵͣ���ռ���ڴ����Ľ��� %u ��ѡ��ҳ��\n", 
                   max_pages_process->pid);
            
            // �������δʹ�õ�ҳ��
            uint32_t oldest_page = (uint32_t)-1;
            uint64_t oldest_time = UINT64_MAX;
            uint32_t oldest_frame = (uint32_t)-1;
            
            for (uint32_t i = 0; i < max_pages_process->page_table_size; i++) {
                if (max_pages_process->page_table[i].flags.present) {
                    if (max_pages_process->page_table[i].last_access_time < oldest_time) {
                        oldest_time = max_pages_process->page_table[i].last_access_time;
                        oldest_page = i;
                        oldest_frame = max_pages_process->page_table[i].frame_number;
                    }
                }
            }
            
            if (oldest_page != (uint32_t)-1) {
                if (max_pages_process->page_table[oldest_page].flags.dirty) {
                    printf("ҳ�����޸ģ���Ҫд�뽻����\n");
                    if (!swap_out_page(oldest_frame)) {
                        printf("д�뽻����ʧ��\n");
                        return (uint32_t)-1;
                    }
                }
                
                max_pages_process->page_table[oldest_page].flags.present = false;
                max_pages_process->page_table[oldest_page].flags.swapped = true;
                free_frame(oldest_frame);
                
                printf("ҳ���Ѵ����ͷ�����ҳ�� %u\n", oldest_frame);
                return oldest_frame;
            }
        }
    }
    
    printf("δ�ҵ����û���ҳ��\n");
    return (uint32_t)-1;
}
