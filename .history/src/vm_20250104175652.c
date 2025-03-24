#ifdef _WIN32
#include <windows.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "../include/vm.h"
#include "../include/memory.h"

// ȫ�ֱ����ͽṹ��
bool swap_out_page(uint32_t frame);
void clock_tick_handler(void);
uint64_t get_current_time(void);

// �����ڴ������ʵ��������������ҳ���û��ȹ���
VMManager vm_manager;

// ҳ���û�ͳ����Ϣ
PageReplacementStats page_stats = {0};

/**
 * @brief ��ʼ�������ڴ��������������������ͳ����Ϣ�ĳ�ʼ��
 */
void vm_init(void) {
    // ��ʼ�������������������佻��������Ϣ����
    vm_manager.swap_blocks = (SwapBlockInfo*)calloc(SWAP_SIZE, sizeof(SwapBlockInfo));
    // ���佻����ʵ�ʴ洢�ռ�
    vm_manager.swap_area = malloc(SWAP_SIZE * SWAP_BLOCK_SIZE);
    // ��ʼ�����н���������
    vm_manager.swap_free_blocks = SWAP_SIZE;
    
    // ��ʼ���ڴ����ͳ��
    memset(&vm_manager.stats, 0, sizeof(MemoryStats));

    // ����ڴ�����Ƿ�ɹ�
    if (!vm_manager.swap_blocks || !vm_manager.swap_area) {
        fprintf(stderr, "�����ڴ��ʼ��ʧ��\n");
        exit(1); // �ڴ����ʧ�ܣ��˳�����
    }
}

/**
 * @brief �����ڴ���ʣ�������д������ȱҳ�жϴ���
 * 
 * @param process Ҫ�����ڴ�Ľ���PCB
 * @param virtual_address Ҫ���ʵ������ַ
 * @param is_write �Ƿ���д����
 */
void access_memory(PCB* process, uint32_t virtual_address, bool is_write) {
    if (!process || !process->page_table) {
        printf("������Ч�Ľ��̻�ҳ��\n");
        return;
    }
    
    vm_manager.stats.total_accesses++;
    
    // ����ҳ�ź�ƫ����
    uint32_t page_num = (virtual_address & 0xFFFFF000) >> 12;  // ��20λΪҳ��
    uint32_t offset = virtual_address & 0xFFF;                 // ��12λΪҳ��ƫ��
    
    if (page_num >= process->page_table_size) {
        printf("���󣺷��ʵ�ַ 0x%x (ҳ��=%u, ƫ��=0x%x) ��������ҳ��Χ\n", 
               virtual_address, page_num, offset);
        return;
    }
    
    PageTableEntry* pte = &process->page_table[page_num];
    
    // ���ҳ�治���ڴ���
    if (!pte->flags.present) {
        printf("\n=== ����ȱҳ�ж� ===\n");
        vm_manager.stats.page_faults++;
        if (!handle_page_fault(process, page_num)) {
            printf("�����޷������ַ 0x%x\n", virtual_address);
            return;
        }
    }
    
    // ����ҳ�����ʱ��
    uint64_t current_time = get_current_time();
    pte->last_access_time = current_time;
    
    // ����ҳ�������Ϣ
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
 * @brief ����ȱҳ�жϣ�����ҳ������ҳ���û�
 * 
 * @param process ����ȱҳ�жϵĽ���PCB
 * @param virtual_address ����ȱҳ�жϵĵ�ַ
 * @return true ����ɹ�
 * @return false ����ʧ��
 */
bool handle_page_fault(PCB* process, uint32_t virtual_page) {
    printf("\n=== ����ȱҳ�ж� ===\n");
    printf("���� %u ����ҳ�� %u\n", process->pid, virtual_page);
    
    // ����ͳ����Ϣ
    vm_manager.stats.page_faults++;
    process->stats.page_faults++;
    
    // ���ҳ���Ƿ���Ч
    if (virtual_page >= process->page_table_size) {
        printf("����ҳ�� %u ����ҳ��Χ %u\n", virtual_page, process->page_table_size);
        return false;
    }
    
    PageTableEntry* pte = &process->page_table[virtual_page];
    printf("ҳ����״̬��present=%d, swapped=%d\n", 
           pte->flags.present, pte->flags.swapped);
    
    // ���Է���һ������ҳ��
    uint32_t frame = allocate_frame(process->pid, virtual_page);
    
    // ���û�п���ҳ����Ҫ����ҳ���û�
    if (frame == (uint32_t)-1) {
        printf("\n=== ��Ҫ����ҳ���û� ===\n");
        printf("��ǰ����ҳ����: %u\n", memory_manager.free_frames_count);
        
        // ����ͳ����Ϣ
        vm_manager.stats.page_replacements++;
        process->stats.page_replacements++;
        
        // ѡ������ҳ��
        uint32_t victim_frame = select_victim_frame();
        if (victim_frame == (uint32_t)-1) {
            printf("�����޷�ѡ���û�ҳ��\n");
            return false;
        }
        
        // ��ȡ����ҳ��Ľ���
        PCB* victim_process = get_process_by_pid(memory_manager.frames[victim_frame].process_id);
        if (!victim_process) {
            printf("�����Ҳ�������ҳ�������Ľ���\n");
            return false;
        }
        
        uint32_t victim_page = memory_manager.frames[victim_frame].virtual_page_num;
        printf("ѡ����� %u ��ҳ�� %u (ҳ�� %u) �����û�\n", 
               victim_process->pid, victim_page, victim_frame);
        
        // ������ҳ�������д�뽻����
        if (!swap_out_page(victim_frame)) {
            printf("�����޷���ҳ��д�뽻����\n");
            return false;
        }
        
        // ����ҳ�����ǰ����
        frame = victim_frame;
        printf("����ҳ�� %u ����Ϣ��\n", frame);
        printf("  ֮ǰ - pid=%u, page=%u, dirty=%d\n",
               memory_manager.frames[frame].process_id,
               memory_manager.frames[frame].virtual_page_num,
               memory_manager.frames[frame].is_dirty);
               
        memory_manager.frames[frame].is_allocated = true;
        memory_manager.frames[frame].process_id = process->pid;
        memory_manager.frames[frame].virtual_page_num = virtual_page;
        memory_manager.frames[frame].last_access_time = get_current_time();
        memory_manager.frames[frame].is_dirty = false;
        
        printf("  ֮�� - pid=%u, page=%u, dirty=%d\n",
               memory_manager.frames[frame].process_id,
               memory_manager.frames[frame].virtual_page_num,
               memory_manager.frames[frame].is_dirty);
    }
    
    // ���ҳ���ڽ������У���Ҫ�����ڴ�
    if (pte->flags.swapped) {
        printf("ҳ���ڽ������У���Ҫ�����ڴ�\n");
        vm_manager.stats.disk_reads++;
        process->stats.pages_swapped_in++;
        if (!swap_in_page(process->pid, virtual_page, frame)) {
            printf("�����޷��ӽ���������ҳ��\n");
            return false;
        }
    }
    
    // ����ҳ����
    printf("���µ�ǰ���̵�ҳ���\n");
    printf("  ֮ǰ - present=%d, swapped=%d, frame=%u\n",
           pte->flags.present, pte->flags.swapped, pte->frame_number);
           
    pte->frame_number = frame;
    pte->flags.present = true;
    pte->flags.swapped = false;
    pte->last_access_time = get_current_time();
    
    printf("  ֮�� - present=%d, swapped=%d, frame=%u\n",
           pte->flags.present, pte->flags.swapped, pte->frame_number);
    
    printf("ҳ�� %u �Ѽ��ص�ҳ�� %u\n", virtual_page, frame);
    
    return true;
}

/**
 * @brief ������̵�ҳ����Ϣ
 * 
 * @param process Ҫ���ҳ����Ϣ�Ľ���PCB
 */
void dump_page_table(PCB* process) {
    printf("\n=== ���� %u ��ҳ�� ===\n", process->pid);
    int present_count = 0, swapped_count = 0;
    
    // �������̵�ҳ����
    for (uint32_t i = 0; i < process->page_table_size; i++) {
        PageTableEntry* pte = &process->page_table[i];
        if (pte->flags.present || pte->flags.swapped) { // ���ҳ�����ڴ��л򱻽���
            printf("ҳ��: %u, ״̬: %s%s, ҳ��: %u\n",
                   i,
                   pte->flags.present ? "���ڴ�" : "�����ڴ�",
                   pte->flags.swapped ? ", ������" : "",
                   pte->flags.present ? pte->frame_number : pte->flags.swap_index);
            
            // ����ͳ����Ϣ
            if (pte->flags.present) present_count++;
            if (pte->flags.swapped) swapped_count++;
        }
    }
    
    // ���ͳ����Ϣ
    printf("\n���ڴ��ҳ��: %d, ��������ҳ��: %d\n",
           present_count, swapped_count);
}

/**
 * @brief �ر������ڴ���������ͷ���Դ
 */
void vm_shutdown(void) {
    free(vm_manager.swap_blocks); // �ͷŽ���������Ϣ����
    free(vm_manager.swap_area);   // �ͷŽ�����ʵ�ʴ洢�ռ�
}

/**
 * @brief ����һ���������飬���ؽ�����������
 * 
 * @param process_id ����ID
 * @param virtual_page ����ҳ��
 * @return uint32_t ���������������������ʧ�ܷ���-1
 */
uint32_t allocate_swap_block(uint32_t process_id, uint32_t virtual_page) {
    if (vm_manager.swap_free_blocks == 0) { // ���û�п��н�����
        return (uint32_t)-1;
    }

    // ��������������Ϣ���飬�ҵ�һ�����еĽ�������
    for (uint32_t i = 0; i < SWAP_SIZE; i++) {
        if (!vm_manager.swap_blocks[i].is_used) {
            vm_manager.swap_blocks[i].is_used = true; // ���Ϊ��ʹ��
            vm_manager.swap_blocks[i].process_id = process_id; // ���ý���ID
            vm_manager.swap_blocks[i].virtual_page = virtual_page; // ��������ҳ��
            vm_manager.swap_free_blocks--; // ���ٿ��н���������
            return i; // ���ؽ�����������
        }
    }
    return (uint32_t)-1; // ������н������鶼��ʹ�ã�����-1��ʾ����ʧ��
}

/**
 * @brief �ͷ�һ����������
 * 
 * @param swap_index ������������
 */
void free_swap_block(uint32_t swap_index) {
    // ��齻�����������Ƿ���Ч
    if (swap_index < SWAP_SIZE && vm_manager.swap_blocks[swap_index].is_used) {
        vm_manager.swap_blocks[swap_index].is_used = false; // ���Ϊδʹ��
        vm_manager.swap_blocks[swap_index].process_id = 0; // ���ý���IDΪ0
        vm_manager.swap_blocks[swap_index].virtual_page = 0; // ��������ҳ��Ϊ0
        vm_manager.swap_free_blocks++; // ���ӿ��н���������
    }
}

/**
 * @brief ��ҳ��д�뽻����
 * 
 * @param process Ҫд��ҳ��Ľ���PCB
 * @param page_num Ҫд��ҳ���ҳ��
 * @return true д��ɹ�
 * @return false д��ʧ��
 */
bool page_out(PCB* process, uint32_t page_num) {
    // ���ҳ���Ƿ���Ч
    if (!process || page_num >= process->page_table_size ||
        !process->page_table[page_num].flags.present) {
        printf("����ҳ�� %u ��Ч\n", page_num);
        return false;
    }

    PageTableEntry* pte = &process->page_table[page_num];
    // ���ҳ���Ƿ����ڴ���
    if (!pte->flags.present) {
        printf("����ҳ�� %u �����ڴ���\n", page_num);
        return false;
    }

    // ����һ�����н�������
    uint32_t swap_index = allocate_swap_block(process->pid, page_num);
    if (swap_index == (uint32_t)-1) { // �������ʧ��
        printf("����û�п��н�������\n");
        return false;
    }

    // ��ȡҳ������
    uint8_t page_data[PAGE_SIZE];
    if (!read_physical_memory(pte->frame_number, 0, page_data, PAGE_SIZE)) {
        printf("���󣺶�ȡ�����ڴ�ʧ��\n");
        free_swap_block(swap_index); // �ͷŽ�������
        return false;
    }
    
    // ������д�뽻����
    if (!write_to_swap(swap_index, page_data)) {
        printf("����д�뽻����ʧ��\n");
        free_swap_block(swap_index); // �ͷŽ�������
        return false;
    }

    // �ͷ�ҳ��
    free_frame(pte->frame_number); // �ͷ�ҳ��
    pte->frame_number = (uint32_t)-1; // ����ҳ��Ϊ-1��ʾ�����ڴ���
    pte->flags.present = false; // ����ҳ�治���ڴ���
    pte->flags.swapped = true; // ����ҳ�汻����
    pte->flags.swap_index = swap_index; // ���ý�����������
    
    // ����ͳ����Ϣ
    vm_manager.stats.page_replacements++;
    printf("ҳ�� %u ��д�뽻������������������: %u\n", page_num, swap_index);
    return true;
}

/**
 * @brief ��ҳ������ڴ�
 * 
 * @param process Ҫ����ҳ��Ľ���PCB
 * @param virtual_page Ҫ����ҳ�������ҳ��
 * @return true ����ɹ�
 * @return false ����ʧ��
 */
bool page_in(PCB* process, uint32_t virtual_page) {
    // ���ҳ���Ƿ���Ч
    if (!process || virtual_page >= process->page_table_size) {
        return false;
    }

    PageTableEntry* pte = &process->page_table[virtual_page];
    // ���ҳ���Ƿ񱻽���
    if (!pte->flags.swapped) {
        return false; // ���ҳ�治�ڽ�����������false��ʾ����Ҫ����
    }

    // ���Է���һ������ҳ��
    uint32_t frame = allocate_frame(process->pid, virtual_page);
    if (frame == (uint32_t)-1) { // �������ʧ��
        uint32_t victim_page = select_victim_page(process); // ѡ��һ������ҳ��
        if (victim_page != (uint32_t)-1) {
            printf("ѡ����� %u ��ҳ�� %u �����û�\n", process->pid, victim_page);
            if (page_out(process, victim_page)) { // ������ҳ��д�뽻����
                frame = allocate_frame(process->pid, virtual_page); // ���·���ҳ��
            }
        }
    }

    if (frame == (uint32_t)-1) { // �������ʧ��
        return false;
    }

    // ��ȡҳ������
    uint8_t page_data[PAGE_SIZE];
    uint32_t swap_index = pte->flags.swap_index;
    if (!read_from_swap(swap_index, page_data)) {
        free_frame(frame); // �ͷ�ҳ��
        return false;
    }

    // ������д�������ڴ�
    if (!write_physical_memory(frame, 0, page_data, PAGE_SIZE)) {
        free_frame(frame); // �ͷ�ҳ��
        return false;
    }

    // ����ҳ����
    pte->frame_number = frame;
    pte->flags.present = true; // ����ҳ�����ڴ���
    pte->flags.swapped = false; // ����ҳ�治�ڽ�����
    
    printf("ҳ�� %u �Ѽ��ص�ҳ�� %u\n", virtual_page, frame);
    return true;
}

/**
 * @brief ��ȡ���н�����������
 * 
 * @return uint32_t ���н�����������
 */
uint32_t get_free_swap_blocks(void) {
    return vm_manager.swap_free_blocks;
}

/**
 * @brief ��������ڴ��������ͳ����Ϣ
 */
void print_vm_stats(void) {
    float page_fault_rate = vm_manager.stats.total_accesses > 0 ? 
        (float)vm_manager.stats.page_faults * 100 / vm_manager.stats.total_accesses : 0;
    
    printf("=== �����ڴ������ͳ����Ϣ ===\n");
    printf("�ܷ��ʴ���: %u\n", vm_manager.stats.total_accesses);
    printf("ȱҳ����: %u (%.1f%%)\n", vm_manager.stats.page_faults, page_fault_rate);
    printf("ҳ���û�����: %u\n", vm_manager.stats.page_replacements);
    printf("����д�����: %u\n", vm_manager.stats.disk_writes);
    printf("ҳ���������: %u\n", vm_manager.stats.pages_swapped_out);
    printf("ҳ��������: %u\n", vm_manager.stats.pages_swapped_in);
    
    printf("\n=== ������ͳ����Ϣ ===\n");
    printf("������������: %u\n", SWAP_SIZE);
    printf("���н���������: %u\n", SWAP_SIZE - vm_manager.swap_free_blocks);
    printf("��ʹ�ý���������: %u\n", vm_manager.swap_free_blocks);
    
    float fragmentation = 0;
    if (memory_manager.free_frames_count > 0) {
        uint32_t fragments = count_memory_fragments();
        fragmentation = (float)fragments * 100 / memory_manager.free_frames_count;
    }
    
    printf("\n=== �ڴ���Ƭͳ����Ϣ ===\n");
    printf("����ҳ����: %u\n", memory_manager.free_frames_count);
    printf("�ڴ���Ƭ��: %u\n", count_memory_fragments());
    printf("�ڴ���Ƭ��: %.1f%%\n", fragmentation);
}

/**
 * @brief ��ҳ������д�������ڴ�
 * 
 * @param process Ҫд��ҳ��Ľ���PCB
 * @param virtual_address Ҫд��ҳ��������ַ
 * @param data Ҫд�������
 * @param size Ҫд������ݴ�С
 * @return true д��ɹ�
 * @return false д��ʧ��
 */
bool write_page_data(PCB* process, uint32_t virtual_address, const void* data, size_t size) {
    uint32_t page_num = virtual_address / PAGE_SIZE;
    if (!process || page_num >= process->page_table_size) {
        return false;
    }

    // ���ҳ���Ƿ����ڴ���
    if (!process->page_table[page_num].flags.present) {
        if (!handle_page_fault(process, virtual_address)) {
            return false;
        }
    }

    // ��ȡҳ���
    uint32_t frame = process->page_table[page_num].frame_number;
    if (!write_physical_memory(frame, 0, data, size)) {
        return false;
    }

    // ����ҳ�����ʱ��
    process->page_table[page_num].last_access_time = get_current_time();
    return true;
}

/**
 * @brief ��ҳ�����ݴ������ڴ��ȡ�������ڴ�
 * 
 * @param process Ҫ��ȡҳ��Ľ���PCB
 * @param virtual_address Ҫ��ȡҳ��������ַ
 * @param buffer Ҫ��ȡ�����ݻ�����
 * @param size Ҫ��ȡ�����ݴ�С
 * @return true ��ȡ�ɹ�
 * @return false ��ȡʧ��
 */
bool read_page_data(PCB* process, uint32_t virtual_address, void* buffer, size_t size) {
    uint32_t page_num = virtual_address / PAGE_SIZE;
    if (!process || page_num >= process->page_table_size) {
        return false;
    }

    // ���ҳ���Ƿ����ڴ���
    if (!process->page_table[page_num].flags.present) {
        if (!handle_page_fault(process, virtual_address)) {
            return false;
        }
    }

    // ��ȡҳ���
    uint32_t frame = process->page_table[page_num].frame_number;
    if (!read_physical_memory(frame, 0, buffer, size)) {
        return false;
    }

    // ����ҳ�����ʱ��
    process->page_table[page_num].last_access_time = get_current_time();
    return true;
}

/**
 * @brief ������д�뽻����
 * 
 * @param swap_index ������������
 * @param data Ҫд�������
 * @return true д��ɹ�
 * @return false д��ʧ��
 */
bool write_to_swap(uint32_t swap_index, const void* data) {
    if (swap_index >= SWAP_SIZE || !data) { // ��齻�����������Ƿ���Ч
        return false;
    }

    // ��齻�������Ƿ�ʹ��
    if (!vm_manager.swap_blocks[swap_index].is_used) {
        return false;
    }

    // ������д�뽻����
    uint8_t* dest = (uint8_t*)vm_manager.swap_area + (swap_index * SWAP_BLOCK_SIZE);
    memcpy(dest, data, SWAP_BLOCK_SIZE); // ��������
    vm_manager.stats.writes_to_disk++; // ���Ӵ���д�����

    return true; // д��ɹ�
}

/**
 * @brief �ӽ�������ȡ����
 * 
 * @param swap_index ������������
 * @param buffer Ҫ��ȡ�����ݻ�����
 * @return true ��ȡ�ɹ�
 * @return false ��ȡʧ��
 */
bool read_from_swap(uint32_t swap_index, void* buffer) {
    if (swap_index >= SWAP_SIZE || !buffer) { // ��齻�����������Ƿ���Ч
        return false;
    }

    // ��齻�������Ƿ�ʹ��
    if (!vm_manager.swap_blocks[swap_index].is_used) {
        return false;
    }

    // �����ݴӽ�������ȡ��������
    uint8_t* src = (uint8_t*)vm_manager.swap_area + (swap_index * SWAP_BLOCK_SIZE);
    memcpy(buffer, src, SWAP_BLOCK_SIZE); // ��������

    return true; // ��ȡ�ɹ�
}

/**
 * @brief ��ȡ����������Ϣ����
 * 
 * @return SwapBlockInfo* ����������Ϣ����
 */
SwapBlockInfo* get_swap_blocks(void) {
    return vm_manager.swap_blocks;
}

/**
 * @brief ��ȡ������ʵ�ʴ洢�ռ�
 * 
 * @return void* ������ʵ�ʴ洢�ռ�
 */
void* get_swap_area(void) {
    return vm_manager.swap_area;
}

/**
 * @brief ��ȡ�ڴ����ͳ����Ϣ
 * 
 * @return MemoryStats �ڴ����ͳ����Ϣ
 */
MemoryStats get_memory_stats(void) {
    return vm_manager.stats;
}

/**
 * @brief �����������ͷ����н�������
 */
void clean_swap_area(void) {
    for (uint32_t i = 0; i < SWAP_BLOCKS; i++) {
        if (vm_manager.swap_blocks[i].is_used) { // ����������鱻ʹ��
            vm_manager.swap_blocks[i].is_used = false; // ���Ϊδʹ��
            vm_manager.swap_blocks[i].process_id = 0; // ���ý���IDΪ0
            vm_manager.swap_blocks[i].virtual_page = 0; // ��������ҳ��Ϊ0
            vm_manager.swap_free_blocks++; // ���ӿ��н���������
        }
    }
}

/**
 * @brief ���������״̬
 */
void print_swap_status(void) {
    uint32_t used_blocks = 0;
    // ��������������Ϣ���飬ͳ����ʹ�ý�����������
    for (uint32_t i = 0; i < SWAP_BLOCKS; i++) {
        if (vm_manager.swap_blocks[i].is_used) {
            used_blocks++;
        }
    }
    printf("��ʹ�ý���������: %u\n", SWAP_BLOCKS);
    printf("���н���������: %u\n", SWAP_BLOCKS - used_blocks);
    printf("��ʹ�ý���������: %u\n", used_blocks);
    
    printf("\n��������״̬\n");
    // ��������������Ϣ���飬���ÿ�����������״̬
    for (uint32_t i = 0; i < SWAP_BLOCKS; i++) {
        if (vm_manager.swap_blocks[i].is_used) {
            printf(" %u: PID=%u, ����ҳ��=%u\n",
                   i,
                   vm_manager.swap_blocks[i].process_id,
                   vm_manager.swap_blocks[i].virtual_page);
        }
    }
}

/**
 * @brief ��ȡ���һ�����û���ҳ��
 * 
 * @return uint32_t ���һ�����û���ҳ��
 */
uint32_t get_last_replaced_page(void) {
    return vm_manager.stats.last_replaced_page;
}

/**
 * @brief ����ҳ���û�ͳ����Ϣ
 * 
 * @param page_num ���û���ҳ��
 */
void update_replacement_stats(uint32_t page_num) {
    vm_manager.stats.page_replacements++; // ����ҳ���û�����
    vm_manager.stats.last_replaced_page = page_num; // �������һ�����û���ҳ��
    printf("ҳ�� %u ���û�����ǰҳ���û�����: %u\n", page_num, vm_manager.stats.page_replacements);
}

/**
 * @brief ��ҳ��д�뽻����
 * 
 * @param frame Ҫд��ҳ���ҳ���
 * @return true д��ɹ�
 * @return false д��ʧ��
 */
bool swap_out_page(uint32_t frame) {
    if (frame >= PHYSICAL_PAGES || !memory_manager.frames[frame].is_allocated) {
        printf("����ҳ��� %u ��Ч\n", frame);
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
        printf("��������ҳ�� %u ����ҳ��Χ %u\n", virtual_page, process->page_table_size);
        return false;
    }

    // ����һ�����н�������
    uint32_t swap_index = allocate_swap_block(process->pid, virtual_page);
    if (swap_index == (uint32_t)-1) {
        printf("�����޷����佻������\n");
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

    // ���ҳ�汻�޸Ĺ�����Ҫд�����
    if (frame_info->is_dirty) {
        vm_manager.stats.disk_writes++;
    }
    vm_manager.stats.pages_swapped_out++;

    // ����ҳ����Ϣ
    frame_info->is_allocated = false;
    frame_info->process_id = 0;
    frame_info->virtual_page_num = 0;
    frame_info->is_dirty = false;
    memory_manager.free_frames_count++;

    printf("ҳ�� %u ��д�뽻������������������: %u\n", virtual_page, swap_index);

    return true;
}

/**
 * @brief ��ȡ��ǰʱ��
 * 
 * @return uint64_t ��ǰʱ��
 */
uint64_t get_current_time(void) {
#ifdef _WIN32
    // Windowsϵͳ����GetSystemTimeAsFileTime
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    // ת��Ϊ΢�루Windowsʱ���1601��1��1�յ����ڵ�΢������
    return (uli.QuadPart - 116444736000000000ULL) / 10;
#else
    // ��Windowsϵͳ����clock_gettime
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000000) + (ts.tv_nsec / 1000);
#endif
}

/**
 * @brief ���ҳ���û�ͳ����Ϣ
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
 * @brief �ӽ�������ҳ����ص��ڴ�
 * 
 * @param pid ����ID
 * @param virtual_page ����ҳ��
 * @param frame Ŀ��ҳ���
 * @return true ���سɹ�
 * @return false ����ʧ��
 */
bool swap_in_page(uint32_t pid, uint32_t virtual_page, uint32_t frame) {
    // ��������Ч��
    if (frame >= PHYSICAL_PAGES) {
        printf("������Ч��ҳ��� %u\n", frame);
        return false;
    }

    // ���Ҷ�Ӧ�Ľ�������
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
        printf("�����ڽ��������Ҳ������� %u ��ҳ�� %u\n", pid, virtual_page);
        return false;
    }

    // ��ȡ����������
    uint8_t page_data[PAGE_SIZE];
    if (!read_from_swap(swap_index, page_data)) {
        printf("���󣺴ӽ�������ȡ����ʧ��\n");
        return false;
    }

    // ������д�������ڴ�
    if (!write_physical_memory(frame, 0, page_data, PAGE_SIZE)) {
        printf("����д�������ڴ�ʧ��\n");
        return false;
    }

    // �ͷŽ�������
    free_swap_block(swap_index);

    // ����ͳ����Ϣ
    vm_manager.stats.disk_reads++;
    vm_manager.stats.pages_swapped_in++;

    printf("ҳ��ɹ��ӽ��������ص��ڴ棺PID=%u, ����ҳ��=%u, ҳ��=%u\n", 
           pid, virtual_page, frame);
    return true;
}
