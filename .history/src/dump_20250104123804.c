#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/dump.h"
#include "../include/memory.h"
#include "../include/vm.h"
#include "../include/process.h"

// �����ⲿ����
extern VMManager vm_manager;

// ת��ϵͳ״̬
bool dump_system_state(const char* filename) {
    FILE* fp = fopen(filename, "wb");
    if (!fp) {
        printf("�޷�����ת���ļ���%s\n", filename);
        return false;
    }

    // 1. ׼����д��ͷ����Ϣ
    SystemStateHeader header = {
        .total_pages = VIRTUAL_PAGES,
        .physical_pages = PHYSICAL_PAGES,
        .swap_size = SWAP_SIZE,
        .memory_stats = get_memory_stats(),
        .process_count = get_total_processes(),
        .current_pid = get_running_process() ? get_running_process()->pid : 0
    };

    if (fwrite(&header, sizeof(header), 1, fp) != 1) {
        printf("д��ͷ����Ϣʧ��\n");
        fclose(fp);
        return false;
    }

    // 2. �������н��̵�״̬
    for (int i = 0; i < 3; i++) {  // �����������ȼ�����
        PCB* proc = get_ready_queue(i);
        while (proc) {
            // ������̻�����Ϣ
            if (fwrite(proc, sizeof(PCB), 1, fp) != 1) {
                fclose(fp);
                return false;
            }
            // ������̵�ҳ��
            if (fwrite(proc->page_table, sizeof(PageTableEntry) * proc->page_table_size, 1, fp) != 1) {
                fclose(fp);
                return false;
            }
            proc = proc->next;
        }
    }

    // 3. ���������ڴ�״̬
    uint8_t* memory = get_physical_memory();
    bool* frame_map = get_frame_map();
    if (!memory || !frame_map ||
        fwrite(memory, PAGE_SIZE, PHYSICAL_PAGES, fp) != PHYSICAL_PAGES ||
        fwrite(frame_map, sizeof(bool), PHYSICAL_PAGES, fp) != PHYSICAL_PAGES) {
        printf("���������ڴ�״̬ʧ��\n");
        fclose(fp);
        return false;
    }

    // 4. ���潻����״̬
    SwapBlockInfo* swap_blocks = get_swap_blocks();
    void* swap_area = get_swap_area();
    if (!swap_blocks || !swap_area ||
        fwrite(swap_blocks, sizeof(SwapBlockInfo), SWAP_SIZE, fp) != SWAP_SIZE ||
        fwrite(swap_area, SWAP_BLOCK_SIZE, SWAP_SIZE, fp) != SWAP_SIZE) {
        printf("���潻����״̬ʧ��\n");
        fclose(fp);
        return false;
    }

    fclose(fp);
    return true;
}

// �ָ�ϵͳ״̬
bool restore_system_state(const char* filename) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        printf("�޷���ת���ļ���%s\n", filename);
        return false;
    }

    // 1. ��ȡ����֤ͷ����Ϣ
    SystemStateHeader header;
    if (fread(&header, sizeof(header), 1, fp) != 1) {
        printf("��ȡͷ����Ϣʧ��\n");
        fclose(fp);
        return false;
    }

    // 2. ������ǰ״̬
    scheduler_shutdown();
    vm_shutdown();
    
    // 3. ���³�ʼ��ϵͳ
    scheduler_init();
    vm_init();
    memory_init();

    // 4. �ָ�����״̬
    for (uint32_t i = 0; i < header.process_count; i++) {
        PCB proc_info;
        if (fread(&proc_info, sizeof(PCB), 1, fp) != 1) {
            printf("��ȡ������Ϣʧ��\n");
            fclose(fp);
            return false;
        }

        // 创建新进程
        PCB* new_proc = process_create(proc_info.priority, proc_info.code_pages, proc_info.data_pages);
        if (!new_proc) {
            printf("ʧ\n");
            fclose(fp);
            return false;
        }

        // ƽ
        new_proc->state = proc_info.state;
        new_proc->priority = proc_info.priority;
        new_proc->time_slice = proc_info.time_slice;
        new_proc->wait_time = proc_info.wait_time;

        // ȡָҳ
        if (fread(new_proc->page_table, sizeof(PageTableEntry) * new_proc->page_table_size, 1, fp) != 1) {
            printf("ȡҳʧ\n");
            fclose(fp);
            return false;
        }
    }

    // 5. ָڴ״̬
    uint8_t* memory = get_physical_memory();
    bool* frame_map = get_frame_map();
    if (!memory || !frame_map ||
        fread(memory, PAGE_SIZE, PHYSICAL_PAGES, fp) != PHYSICAL_PAGES ||
        fread(frame_map, sizeof(bool), PHYSICAL_PAGES, fp) != PHYSICAL_PAGES) {
        printf("ָڴ״̬ʧ\n");
        fclose(fp);
        return false;
    }

    // 6. ָ״̬
    SwapBlockInfo* swap_blocks = get_swap_blocks();
    void* swap_area = get_swap_area();
    if (!swap_blocks || !swap_area ||
        fread(swap_blocks, sizeof(SwapBlockInfo), SWAP_SIZE, fp) != SWAP_SIZE ||
        fread(swap_area, SWAP_BLOCK_SIZE, SWAP_SIZE, fp) != SWAP_SIZE) {
        printf("ָ״̬ʧ\n");
        fclose(fp);
        return false;
    }

    // ָ״̬¼
    uint32_t used_blocks = 0;
    for (uint32_t i = 0; i < SWAP_SIZE; i++) {
        if (vm_manager.swap_blocks[i].is_used) {
            used_blocks++;
        }
    }
    vm_manager.swap_free_blocks = SWAP_SIZE - used_blocks;

    // 7. ָͳϢ
    vm_manager.stats = header.memory_stats;

    fclose(fp);
    return true;
} 