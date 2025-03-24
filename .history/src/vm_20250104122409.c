#ifdef _WIN32
#include <windows.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "../include/vm.h"
#include "../include/memory.h"

// ���ļ���ͷ���Ӻ�������
uint32_t select_victim_page(PCB* process);
bool swap_out_page(uint32_t frame);
void clock_tick_handler(void);
uint64_t get_current_time(void);

// ȫ�������ڴ������ʵ�������ڹ������������ڴ�ͳ��
VMManager vm_manager;

// ȫ��ͳ����Ϣʵ��
PageReplacementStats page_stats = {0};

/**
 * @brief ��ʼ�������ڴ�����������佻��������ʼ��ͳ����Ϣ
 */
void vm_init(void) {
    // ���佻������Ϣ���飬ÿ���������Ӧһ��SwapBlockInfo
    vm_manager.swap_blocks = (SwapBlockInfo*)calloc(SWAP_SIZE, sizeof(SwapBlockInfo));
    // ���佻�����������ڴ�ռ�
    vm_manager.swap_area = malloc(SWAP_SIZE * SWAP_BLOCK_SIZE);
    // ��ʼ�����н���������
    vm_manager.swap_free_blocks = SWAP_SIZE;
    
    // ��ʼ���ڴ�ͳ����ϢΪ��
    memset(&vm_manager.stats, 0, sizeof(MemoryStats));

    // ����ڴ�����Ƿ�ɹ�
    if (!vm_manager.swap_blocks || !vm_manager.swap_area) {
        fprintf(stderr, "�ڴ��ʼ��ʧ��\n");
        exit(1); // �ڴ����ʧ�ܣ��˳�����
    }
}

/**
 * @brief �����ڴ棬������д���������ܵ�ȱҳ�ж�
 * 
 * @param process ָ��Ŀ����̵�PCB
 * @param virtual_address Ҫ���ʵ������ַ
 * @param is_write �Ƿ�Ϊд����
 */
void access_memory(PCB* process, uint32_t virtual_address, bool is_write) {
    if (!process || !process->page_table) {
        printf("������Ч�Ľ��̻�ҳ��\n");
        return;
    }
    
    vm_manager.stats.total_accesses++;
    
    // ����ҳ�ź�ҳ��ƫ��
    uint32_t page_num = (virtual_address & 0xFFFFF000) >> 12;  // ��20λΪҳ��
    uint32_t offset = virtual_address & 0xFFF;                 // ��12λΪҳ��ƫ��
    
    if (page_num >= process->page_table_size) {
        printf("���󣺷��ʵ�ַ 0x%x (ҳ��=%u, ƫ��=0x%x) ��������ҳ����Χ\n", 
               virtual_address, page_num, offset);
        return;
    }
    
    PageTableEntry* pte = &process->page_table[page_num];
    
    // ���ҳ���Ƿ����ڴ���
    if (!pte->flags.present) {
        printf("\n=== ��ʼ����ȱҳ�ж� ===\n");
        vm_manager.stats.page_faults++;
        if (!handle_page_fault(process, page_num)) {
            printf("ҳ�����ʧ�ܣ���ַ 0x%x\n", virtual_address);
            return;
        }
    }
    
    // ����ҳ�������Ϣ
    uint64_t current_time = get_current_time();
    pte->last_access_time = current_time;
    
    // ���¶�Ӧҳ��ķ�����Ϣ
    if (pte->flags.present) {
        uint32_t frame = pte->frame_number;
        memory_manager.frames[frame].last_access_time = current_time;
        
        if (is_write) {
            pte->flags.dirty = true;
            memory_manager.frames[frame].is_dirty = true;
            printf("���� %d д���ַ 0x%x (ҳ��=%u, ƫ��=0x%x, ҳ��=%u)\n", 
                   process->pid, virtual_address, page_num, offset, frame);
        } else {
            printf("���� %d ��ȡ��ַ 0x%x (ҳ��=%u, ƫ��=0x%x, ҳ��=%u)\n", 
                   process->pid, virtual_address, page_num, offset, frame);
        }
    }
}

/**
 * @brief ����ȱҳ�жϣ����Խ�ҳ������ڴ�
 * 
 * @param process ָ��Ŀ����̵�PCB
 * @param virtual_address ����ȱҳ�������ַ
 * @return true �����ɹ�
 * @return false ����ʧ��
 */
bool handle_page_fault(PCB* process, uint32_t virtual_page) {
    printf("\n=== ��ʼ����ȱҳ�ж� ===\n");
    printf("���� %u ����ҳ�� %u\n", process->pid, virtual_page);
    
    // ����ͳ����Ϣ
    vm_manager.stats.page_faults++;
    process->stats.page_faults++;
    
    // ���ҳ���Ƿ���Ч
    if (virtual_page >= process->page_table_size) {
        printf("����ҳ�� %u ����ҳ����Χ %u\n", virtual_page, process->page_table_size);
        return false;
    }
    
    PageTableEntry* pte = &process->page_table[virtual_page];
    printf("ҳ��״̬��present=%d, swapped=%d\n", 
           pte->flags.present, pte->flags.swapped);
    
    // �ȳ���ֱ�ӷ���ҳ��
    uint32_t frame = allocate_frame(process->pid, virtual_page);
    
    // ���û�п���ҳ����Ҫ����ҳ���û�
    if (frame == (uint32_t)-1) {
        printf("\n=== �ڴ���������ʼҳ���û� ===\n");
        printf("��ǰ����ҳ����: %u\n", memory_manager.free_frames_count);
        
        // ����ͳ����Ϣ
        vm_manager.stats.page_replacements++;
        process->stats.page_replacements++;
        
        // ѡ��Ҫ�û���ȥ��ҳ��
        uint32_t victim_frame = select_victim_frame();
        if (victim_frame == (uint32_t)-1) {
            printf("�����޷��ҵ����滻��ҳ��\n");
            return false;
        }
        
        // ��ȡ���û�ҳ�����Ϣ
        PCB* victim_process = get_process_by_pid(memory_manager.frames[victim_frame].process_id);
        if (!victim_process) {
            printf("�����Ҳ������û�ҳ�������Ľ���\n");
            return false;
        }
        
        uint32_t victim_page = memory_manager.frames[victim_frame].virtual_page_num;
        printf("ѡ����� %u ��ҳ�� %u (ҳ�� %u) �����û�\n", 
               victim_process->pid, victim_page, victim_frame);
        
        // ���ҳ�汻�޸Ĺ�����Ҫд�뽻����
        if (memory_manager.frames[victim_frame].is_dirty) {
            vm_manager.stats.disk_writes++;
            if (!swap_out_page(victim_frame)) {
                printf("����ҳ�滻��ʧ��\n");
                return false;
            }
            vm_manager.stats.pages_swapped_out++;
            victim_process->stats.pages_swapped_out++;
        }
        
        // ���±��û�ҳ���ҳ����
        PageTableEntry* victim_pte = &victim_process->page_table[victim_page];
        victim_pte->flags.present = false;
        victim_pte->flags.swapped = true;
        victim_pte->frame_number = (uint32_t)-1;
        
        // Ϊ��ҳ�������ͷŵ�ҳ��
        frame = victim_frame;
        memory_manager.frames[frame].is_allocated = true;
        memory_manager.frames[frame].process_id = process->pid;
        memory_manager.frames[frame].virtual_page_num = virtual_page;
        memory_manager.frames[frame].last_access_time = get_current_time();
        memory_manager.frames[frame].is_dirty = false;
    }
    
    // ���ҳ���ڽ���������Ҫ����
    if (pte->flags.swapped) {
        vm_manager.stats.disk_reads++;
        process->stats.pages_swapped_in++;
        if (!swap_in_page(process->pid, virtual_page, frame)) {
            printf("����ҳ�����ʧ��\n");
            return false;
        }
    }
    
    // ����ҳ����
    pte->frame_number = frame;
    pte->flags.present = true;
    pte->flags.swapped = false;
    pte->last_access_time = get_current_time();
    
    printf("ҳ�� %u �Ѽ��ص�ҳ�� %u\n", virtual_page, frame);
    
    return true;
}

/**
 * @brief ���ָ�����̵�ҳ���״̬
 * 
 * @param process ָ��Ŀ����̵�PCB
 */
void dump_page_table(PCB* process) {
    printf("\n=== ҳ��״̬ (PID=%u) ===\n", process->pid);
    int present_count = 0, swapped_count = 0;
    
    // ��������ҳ�棬��ӡ״̬
    for (uint32_t i = 0; i < process->page_table_size; i++) {
        PageTableEntry* pte = &process->page_table[i];
        if (pte->flags.present || pte->flags.swapped) { // ֻ��ӡ���ڴ��л��ѽ�����ҳ��
            printf("ҳ��: %u, ״̬: %s%s, ֡��: %u\n",
                   i,
                   pte->flags.present ? "�ѷ���" : "δ����",
                   pte->flags.swapped ? ", �ѽ���" : "",
                   pte->flags.present ? pte->frame_number : pte->flags.swap_index);
            
            // ͳ�Ƹ�״̬ҳ������
            if (pte->flags.present) present_count++;
            if (pte->flags.swapped) swapped_count++;
        }
    }
    
    // ���ͳ����Ϣ
    printf("\n��ҳ����: %d, �ѽ���ҳ����: %d\n",
           present_count, swapped_count);
}

/**
 * @brief �ر������ڴ���������ͷ����з�����ڴ�
 */
void vm_shutdown(void) {
    free(vm_manager.swap_blocks); // �ͷŽ�������Ϣ����
    free(vm_manager.swap_area);   // �ͷŽ����������ڴ�
}

/**
 * @brief ����һ�������飬����ҳ�潻��
 * 
 * @param process_id ����ID
 * @param virtual_page ����ҳ��
 * @return uint32_t ����Ľ�����������ʧ�ܷ��� -1
 */
uint32_t allocate_swap_block(uint32_t process_id, uint32_t virtual_page) {
    if (vm_manager.swap_free_blocks == 0) { // ����Ƿ��п��н�����
        return (uint32_t)-1;
    }

    // ���������飬�ҵ���һ��δʹ�õĽ�����
    for (uint32_t i = 0; i < SWAP_SIZE; i++) {
        if (!vm_manager.swap_blocks[i].is_used) {
            vm_manager.swap_blocks[i].is_used = true; // ���Ϊ��ʹ��
            vm_manager.swap_blocks[i].process_id = process_id; // ��¼����ID
            vm_manager.swap_blocks[i].virtual_page = virtual_page; // ��¼����ҳ��
            vm_manager.swap_free_blocks--; // ���ٿ��н��������
            return i; // ���ؽ���������
        }
    }
    return (uint32_t)-1; // δ�ҵ����н�����
}

/**
 * @brief �ͷ�һ��������
 * 
 * @param swap_index Ҫ�ͷŵĽ���������
 */
void free_swap_block(uint32_t swap_index) {
    // ��齻����������Ч���ѱ�ʹ��
    if (swap_index < SWAP_SIZE && vm_manager.swap_blocks[swap_index].is_used) {
        vm_manager.swap_blocks[swap_index].is_used = false; // ���Ϊδʹ��
        vm_manager.swap_blocks[swap_index].process_id = 0; // �������ID
        vm_manager.swap_blocks[swap_index].virtual_page = 0; // �������ҳ��
        vm_manager.swap_free_blocks++; // ���ӿ��н��������
    }
}

/**
 * @brief ��ҳ�潻������������
 * 
 * @param process ָ��Ŀ����̵�PCB
 * @param page_num Ҫ��������ҳ���
 * @return true �����ɹ�
 * @return false ����ʧ��
 */
bool page_out(PCB* process, uint32_t page_num) {
    // ���ҳ���Ƿ�ȷʵ���ڴ���
    if (!process || page_num >= process->page_table_size ||
        !process->page_table[page_num].flags.present) {
        printf("����ҳ�� %u �����ڴ���\n", page_num);
        return false;
    }

    PageTableEntry* pte = &process->page_table[page_num];
    // ���ҳ���Ƿ����ڴ���
    if (!pte->flags.present) {
        printf("����ҳ�� %u �����ڴ���\n", page_num);
        return false;
    }

    // ���佻����
    uint32_t swap_index = allocate_swap_block(process->pid, page_num);
    if (swap_index == (uint32_t)-1) { // ����Ƿ����ɹ�
        printf("�����޷����佻���飨ʣ�ཻ���飺%u��\n", 
               vm_manager.swap_free_blocks);
        return false;
    }

    // ��ȡҳ�����ݵ���ʱ������
    uint8_t page_data[PAGE_SIZE];
    if (!read_physical_memory(pte->frame_number, 0, page_data, PAGE_SIZE)) {
        printf("�����޷��������ڴ��ȡҳ�����ݣ�ҳ��ţ�%u��\n", 
               pte->frame_number);
        free_swap_block(swap_index); // ��ȡʧ�ܣ��ͷŽ�����
        return false;
    }
    
    // ��ҳ������д�뽻����
    if (!write_to_swap(swap_index, page_data)) {
        printf("�����޷�д�뽻������������ţ�%u��\n", swap_index);
        free_swap_block(swap_index); // д��ʧ�ܣ��ͷŽ�����
        return false;
    }

    // �ͷ�����֡������ҳ����
    free_frame(pte->frame_number); // �ͷ�����֡
    pte->frame_number = (uint32_t)-1; // ����֡��
    pte->flags.present = false; // ���ҳ�治���ڴ���
    pte->flags.swapped = true; // ���ҳ���ѽ���
    pte->flags.swap_index = swap_index; // ��¼����������
    
    // �����ڴ�ͳ����Ϣ
    vm_manager.stats.page_replacements++;
    printf("ҳ�� %u �ѳ����������� %u\n", page_num, swap_index);
    return true;
}

/**
 * @brief ��ҳ������ڴ�
 * 
 * @param process ָ��Ŀ����̵�PCB
 * @param virtual_page Ҫ���������ҳ��
 * @return true ����ɹ�
 * @return false ����ʧ��
 */
bool page_in(PCB* process, uint32_t virtual_page) {
    // �����̺�ҳ�ŵ���Ч��
    if (!process || virtual_page >= process->page_table_size) {
        return false;
    }

    PageTableEntry* pte = &process->page_table[virtual_page];
    // ���ҳ���Ƿ񱻽�����
    if (!pte->flags.swapped) {
        return false; // ҳ��δ���������޷�����
    }

    // ���Է�������֡
    uint32_t frame = allocate_frame(process->pid, virtual_page);
    if (frame == (uint32_t)-1) { // �ڴ治��
        uint32_t victim_page = select_victim_page(process); // ѡ���ܺ�ҳ��
        if (victim_page != (uint32_t)-1) {
            printf("ѡ��ҳ�� %u ���н���\n", victim_page);
            if (page_out(process, victim_page)) { // ���ܺ�ҳ�潻����
                frame = allocate_frame(process->pid, virtual_page); // �ٴγ��Է���֡
            }
        }
    }

    if (frame == (uint32_t)-1) { // �����Ȼ�޷�����֡
        return false;
    }

    // ��ȡҳ�����ݴӽ�����
    uint8_t page_data[PAGE_SIZE];
    uint32_t swap_index = pte->flags.swap_index;
    if (!read_from_swap(swap_index, page_data)) {
        free_frame(frame); // ��ȡʧ�ܣ��ͷ�֡
        return false;
    }

    // ��ҳ������д�������ڴ�
    if (!write_physical_memory(frame, 0, page_data, PAGE_SIZE)) {
        free_frame(frame); // д��ʧ�ܣ��ͷ�֡
        return false;
    }

    // ����ҳ������ҳ�����ڴ���
    pte->frame_number = frame;
    pte->flags.present = true; // ���ҳ�����ڴ���
    pte->flags.swapped = false; // �������λ
    
    printf("ҳ�� %u �ѵ����ڴ棬������ %u\n", 
           virtual_page, swap_index);
    return true;
}

/**
 * @brief ��ȡ��ǰ���еĽ���������
 * 
 * @return uint32_t ���н���������
 */
uint32_t get_free_swap_blocks(void) {
    return vm_manager.swap_free_blocks;
}

/**
 * @brief ��������ڴ��ͳ����Ϣ
 */
void print_vm_stats(void) {
    float page_fault_rate = vm_manager.stats.total_accesses > 0 ? 
        (float)vm_manager.stats.page_faults * 100 / vm_manager.stats.total_accesses : 0;
    
    printf("=== �ڴ����ͳ�� ===\n");
    printf("�ܷ��ʴ���: %u\n", vm_manager.stats.total_accesses);
    printf("ȱҳ����: %u (%.1f%%)\n", vm_manager.stats.page_faults, page_fault_rate);
    printf("ҳ���滻����: %u\n", vm_manager.stats.page_replacements);
    printf("д����̴���: %u\n", vm_manager.stats.disk_writes);
    printf("ҳ�滻������: %u\n", vm_manager.stats.pages_swapped_out);
    printf("ҳ�滻�����: %u\n", vm_manager.stats.pages_swapped_in);
    
    printf("\n=== ������״̬ ===\n");
    printf("�ܽ�������: %u\n", SWAP_SIZE);
    printf("���ý�����: %u\n", SWAP_SIZE - vm_manager.swap_free_blocks);
    printf("���н�����: %u\n", vm_manager.swap_free_blocks);
    
    float fragmentation = 0;
    if (memory_manager.free_frames_count > 0) {
        uint32_t fragments = count_memory_fragments();
        fragmentation = (float)fragments * 100 / memory_manager.free_frames_count;
    }
    
    printf("\n=== �ڴ���Ƭͳ�� ===\n");
    printf("����ҳ����: %u\n", memory_manager.free_frames_count);
    printf("��Ƭ��: %u\n", count_memory_fragments());
    printf("��Ƭ��: %.1f%%\n", fragmentation);
}

/**
 * @brief ѡ��һ���ܺ�ҳ������û�
 * 
 * @param process ָ��Ŀ����̵�PCB
 * @return uint32_t ѡ����ܺ�ҳ�ţ�δ�ҵ��򷵻� -1
 */
uint32_t select_victim_page(PCB* process) {
    uint32_t victim_page = (uint32_t)-1;
    uint64_t oldest_access = UINT64_MAX;
    
    for (uint32_t i = 0; i < process->page_table_size; i++) {
        if (process->page_table[i].flags.present) {
            if (process->page_table[i].last_access_time < oldest_access) {
                oldest_access = process->page_table[i].last_access_time;
                victim_page = i;
            }
        }
    }
    return victim_page;
}

/**
 * @brief ��ҳ������д��ָ�������ַ
 * 
 * @param process ָ��Ŀ����̵�PCB
 * @param virtual_address Ҫд��������ַ
 * @param data Ҫд�������ָ��
 * @param size Ҫд������ݴ�С
 * @return true д��ɹ�
 * @return false д��ʧ��
 */
bool write_page_data(PCB* process, uint32_t virtual_address, const void* data, size_t size) {
    uint32_t page_num = virtual_address / PAGE_SIZE;
    if (!process || page_num >= process->page_table_size) {
        return false;
    }

    // ȷ��ҳ�����ڴ���
    if (!process->page_table[page_num].flags.present) {
        if (!handle_page_fault(process, virtual_address)) {
            return false;
        }
    }

    // ��ȡ������ַ��д������
    uint32_t frame = process->page_table[page_num].frame_number;
    if (!write_physical_memory(frame, 0, data, size)) {
        return false;
    }

    // ����ҳ����
    process->page_table[page_num].last_access_time = get_current_time();
    return true;
}

/**
 * @brief ��ָ�������ַ��ȡҳ������
 * 
 * @param process ָ��Ŀ����̵�PCB
 * @param virtual_address Ҫ��ȡ�������ַ
 * @param buffer �洢��ȡ���ݵĻ�����
 * @param size Ҫ��ȡ�����ݴ�С
 * @return true ��ȡ�ɹ�
 * @return false ��ȡʧ��
 */
bool read_page_data(PCB* process, uint32_t virtual_address, void* buffer, size_t size) {
    uint32_t page_num = virtual_address / PAGE_SIZE;
    if (!process || page_num >= process->page_table_size) {
        return false;
    }

    // ȷ��ҳ�����ڴ���
    if (!process->page_table[page_num].flags.present) {
        if (!handle_page_fault(process, virtual_address)) {
            return false;
        }
    }

    // ��ȡ������ַ����ȡ����
    uint32_t frame = process->page_table[page_num].frame_number;
    if (!read_physical_memory(frame, 0, buffer, size)) {
        return false;
    }

    // ����ҳ����
    process->page_table[page_num].last_access_time = get_current_time();
    return true;
}

/**
 * @brief ������д�뽻������ָ��������
 * 
 * @param swap_index ����������
 * @param data Ҫд�������ָ��
 * @return true д��ɹ�
 * @return false д��ʧ��
 */
bool write_to_swap(uint32_t swap_index, const void* data) {
    if (swap_index >= SWAP_SIZE || !data) { // ��齻����������������Ч��
        return false;
    }

    // ��齻�����Ƿ��ѱ�ʹ��
    if (!vm_manager.swap_blocks[swap_index].is_used) {
        return false;
    }

    // ���㽻�����н��������ʼ��ַ
    uint8_t* dest = (uint8_t*)vm_manager.swap_area + (swap_index * SWAP_BLOCK_SIZE);
    memcpy(dest, data, SWAP_BLOCK_SIZE); // �����ݸ��Ƶ�������
    vm_manager.stats.writes_to_disk++; // ����д����̴���

    return true; // д��ɹ�
}

/**
 * @brief �ӽ�������ȡ���ݵ�������
 * 
 * @param swap_index ����������
 * @param buffer �洢��ȡ���ݵĻ�����
 * @return true ��ȡ�ɹ�
 * @return false ��ȡʧ��
 */
bool read_from_swap(uint32_t swap_index, void* buffer) {
    if (swap_index >= SWAP_SIZE || !buffer) { // ��齻���������ͻ�������Ч��
        return false;
    }

    // ��齻�����Ƿ��ѱ�ʹ��
    if (!vm_manager.swap_blocks[swap_index].is_used) {
        return false;
    }

    // ���㽻�����н��������ʼ��ַ
    uint8_t* src = (uint8_t*)vm_manager.swap_area + (swap_index * SWAP_BLOCK_SIZE);
    memcpy(buffer, src, SWAP_BLOCK_SIZE); // �����ݸ��Ƶ�������

    return true; // ��ȡ�ɹ�
}

/**
 * @brief ��ȡ��������Ϣ����
 * 
 * @return SwapBlockInfo* ָ�򽻻�����Ϣ�����ָ��
 */
SwapBlockInfo* get_swap_blocks(void) {
    return vm_manager.swap_blocks;
}

/**
 * @brief ��ȡ�������������ڴ�ָ��
 * 
 * @return void* ָ�򽻻��������ڴ��ָ��
 */
void* get_swap_area(void) {
    return vm_manager.swap_area;
}

/**
 * @brief ��ȡ�ڴ������ͳ����Ϣ
 * 
 * @return MemoryStats �����ڴ�ͳ����Ϣ
 */
MemoryStats get_memory_stats(void) {
    return vm_manager.stats;
}

/**
 * @brief �������������ͷ�����ʹ���еĽ�����
 */
void clean_swap_area(void) {
    for (uint32_t i = 0; i < SWAP_BLOCKS; i++) {
        if (vm_manager.swap_blocks[i].is_used) { // ��齻�����Ƿ�ʹ��
            vm_manager.swap_blocks[i].is_used = false; // ���Ϊδʹ��
            vm_manager.swap_blocks[i].process_id = 0; // �������ID
            vm_manager.swap_blocks[i].virtual_page = 0; // �������ҳ��
            vm_manager.swap_free_blocks++; // ���ӿ��н��������
        }
    }
}

/**
 * @brief �����ǰ��������״̬�������ܽ������������к���ʹ�ý�����
 */
void print_swap_status(void) {
    uint32_t used_blocks = 0;
    // ͳ����ʹ�õĽ���������
    for (uint32_t i = 0; i < SWAP_BLOCKS; i++) {
        if (vm_manager.swap_blocks[i].is_used) {
            used_blocks++;
        }
    }
    printf("�ܽ�����: %u\n", SWAP_BLOCKS);
    printf("���н�����: %u\n", SWAP_BLOCKS - used_blocks);
    printf("��ʹ�ý�����: %u\n", used_blocks);
    
    printf("\n��ʹ��ҳ��\n");
    // ��ӡÿ����ʹ�õĽ��������Ϣ
    for (uint32_t i = 0; i < SWAP_BLOCKS; i++) {
        if (vm_manager.swap_blocks[i].is_used) {
            printf(" %u: PID=%u, ҳ��=%u\n",
                   i,
                   vm_manager.swap_blocks[i].process_id,
                   vm_manager.swap_blocks[i].virtual_page);
        }
    }
}

/**
 * @brief ��ȡ����滻��ҳ���
 * 
 * @return uint32_t ����滻��ҳ���
 */
uint32_t get_last_replaced_page(void) {
    return vm_manager.stats.last_replaced_page;
}

/**
 * @brief ����ҳ���滻��ͳ����Ϣ
 * 
 * @param page_num ���滻��ҳ���
 */
void update_replacement_stats(uint32_t page_num) {
    vm_manager.stats.page_replacements++; // ����ҳ���滻����
    vm_manager.stats.last_replaced_page = page_num; // ��¼����滻��ҳ���
    printf("ҳ���滻��ҳ�� %u ���滻���滻���� %u\n", 
           page_num, vm_manager.stats.page_replacements);
}

/**
 * @brief ��ҳ��д�뽻����
 * 
 * @param frame Ҫд���ҳ���
 * @return true д��ɹ�
 * @return false д��ʧ��
 */
bool swap_out_page(uint32_t frame) {
    if (frame >= PHYSICAL_PAGES || !memory_manager.frames[frame].is_allocated) {
        printf("������Ч��ҳ��� %u\n", frame);
        return false;
    }

    FrameInfo* frame_info = &memory_manager.frames[frame];
    PCB* process = get_process_by_pid(frame_info->process_id);
    if (!process) {
        printf("�����Ҳ������� %u\n", frame_info->process_id);
        return false;
    }

    uint32_t virtual_page = frame_info->virtual_page_num;
    if (virtual_page >= process->page_table_size) {
        printf("��������ҳ�� %u ������Χ\n", virtual_page);
        return false;
    }

    // ���佻����
    uint32_t swap_index = allocate_swap_block(process->pid, virtual_page);
    if (swap_index == (uint32_t)-1) {
        printf("�����޷����佻����\n");
        return false;
    }

    // ��ҳ������д�뽻����
    void* page_data = get_physical_memory() + (frame * PAGE_SIZE);
    if (!write_to_swap(swap_index, page_data)) {
        printf("����д�뽻����ʧ��\n");
        free_swap_block(swap_index);
        return false;
    }

    // ����ҳ����
    PageTableEntry* pte = &process->page_table[virtual_page];
    pte->flags.present = false;
    pte->flags.swapped = true;
    pte->flags.swap_index = swap_index;

    // ����ͳ����Ϣ
    if (frame_info->is_dirty) {
        vm_manager.stats.disk_writes++;
    }
    vm_manager.stats.pages_swapped_out++;

    // ����ҳ��״̬
    frame_info->is_allocated = false;
    frame_info->process_id = 0;
    frame_info->virtual_page_num = 0;
    frame_info->is_dirty = false;
    memory_manager.free_frames_count++;

    printf("ҳ��ɹ�д�뽻����������=%u, ����ҳ��=%u, ������=%u\n", 
           process->pid, virtual_page, swap_index);

    return true;
}

/**
 * @brief ��ȡ��ǰʱ��
 * 
 * @return uint64_t ��ǰʱ��
 */
uint64_t get_current_time(void) {
#ifdef _WIN32
    // Windowsϵͳʹ��GetSystemTimeAsFileTime
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    // ת��Ϊ΢�� (Windows�ļ�ʱ���Ǵ�1601�꿪ʼ��100������)
    return (uli.QuadPart - 116444736000000000ULL) / 10;
#else
    // ��Windowsϵͳʹ��clock_gettime
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000000) + (ts.tv_nsec / 1000);
#endif
}

/**
 * @brief ��ӡҳ���û�ͳ����Ϣ
 */
void print_page_replacement_stats(void) {
    printf("\n=== ҳ���û�ͳ����Ϣ ===\n");
    printf("��ȱҳ����: %u\n", page_stats.total_page_faults);
    printf("ҳ���û�����: %u\n", page_stats.page_replacements);
    printf("���̶�ȡ����: %u\n", page_stats.disk_reads);
    printf("����д�����: %u\n", page_stats.disk_writes);
    printf("LRU������: %.2f%%\n", 
           page_stats.lru_hits + page_stats.lru_misses > 0 ?
           (float)page_stats.lru_hits / (page_stats.lru_hits + page_stats.lru_misses) * 100 : 0);
}

/**
 * @brief 从交换区将页面加载到内存
 * 
 * @param pid 进程ID
 * @param virtual_page 虚拟页号
 * @param frame 目标页框号
 * @return true 加载成功
 * @return false 加载失败
 */
bool swap_in_page(uint32_t pid, uint32_t virtual_page, uint32_t frame) {
    // 检查参数有效性
    if (frame >= PHYSICAL_PAGES) {
        printf("错误：无效的页框号 %u\n", frame);
        return false;
    }

    // 查找对应的交换区块
    uint32_t swap_index = (uint32_t)-1;
    for (uint32_t i = 0; i < SWAP_SIZE; i++) {
        if (vm_manager.swap_blocks[i].is_used &&
            vm_manager.swap_blocks[i].process_id == pid &&
            vm_manager.swap_blocks[i].virtual_page == virtual_page) {
            swap_index = i;
            break;
        }
    }

    if (swap_index == (uint32_t)-1) {
        printf("错误：在交换区中找不到进程 %u 的页面 %u\n", pid, virtual_page);
        return false;
    }

    // 读取交换区数据
    uint8_t page_data[PAGE_SIZE];
    if (!read_from_swap(swap_index, page_data)) {
        printf("错误：从交换区读取数据失败\n");
        return false;
    }

    // 将数据写入物理内存
    if (!write_physical_memory(frame, 0, page_data, PAGE_SIZE)) {
        printf("错误：写入物理内存失败\n");
        return false;
    }

    // 释放交换区块
    free_swap_block(swap_index);

    // 更新统计信息
    vm_manager.stats.disk_reads++;
    vm_manager.stats.pages_swapped_in++;

    printf("页面成功从交换区加载到内存：PID=%u, 虚拟页号=%u, 页框=%u\n", 
           pid, virtual_page, frame);
    return true;
}
