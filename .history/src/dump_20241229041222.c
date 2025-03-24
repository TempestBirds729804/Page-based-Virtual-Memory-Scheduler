#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/dump.h"
#include "../include/memory.h"
#include "../include/vm.h"
#include "../include/process.h"

// 声明外部变量
extern VMManager vm_manager;

// 转储系统状态
bool dump_system_state(const char* filename) {
    FILE* fp = fopen(filename, "wb");
    if (!fp) {
        printf("无法创建转储文件：%s\n", filename);
        return false;
    }

    // 1. 准备并写入头部信息
    SystemStateHeader header = {
        .total_pages = VIRTUAL_PAGES,
        .physical_pages = PHYSICAL_PAGES,
        .swap_size = SWAP_SIZE,
        .memory_stats = get_memory_stats(),
        .process_count = get_total_processes(),
        .current_pid = get_running_process() ? get_running_process()->pid : 0
    };

    if (fwrite(&header, sizeof(header), 1, fp) != 1) {
        printf("写入头部信息失败\n");
        fclose(fp);
        return false;
    }

    // 2. 保存所有进程的状态
    for (int i = 0; i < 3; i++) {  // 遍历所有优先级队列
        PCB* proc = get_ready_queue(i);
        while (proc) {
            // 保存进程基本信息
            if (fwrite(proc, sizeof(PCB), 1, fp) != 1) {
                fclose(fp);
                return false;
            }
            // 保存进程的页表
            if (fwrite(proc->page_table, sizeof(PageTableEntry) * proc->page_table_size, 1, fp) != 1) {
                fclose(fp);
                return false;
            }
            proc = proc->next;
        }
    }

    // 3. 保存物理内存状态
    uint8_t* memory = get_physical_memory();
    bool* frame_map = get_frame_map();
    if (!memory || !frame_map ||
        fwrite(memory, PAGE_SIZE, PHYSICAL_PAGES, fp) != PHYSICAL_PAGES ||
        fwrite(frame_map, sizeof(bool), PHYSICAL_PAGES, fp) != PHYSICAL_PAGES) {
        printf("保存物理内存状态失败\n");
        fclose(fp);
        return false;
    }

    // 4. 保存交换区状态
    SwapBlockInfo* swap_blocks = get_swap_blocks();
    void* swap_area = get_swap_area();
    if (!swap_blocks || !swap_area ||
        fwrite(swap_blocks, sizeof(SwapBlockInfo), SWAP_SIZE, fp) != SWAP_SIZE ||
        fwrite(swap_area, SWAP_BLOCK_SIZE, SWAP_SIZE, fp) != SWAP_SIZE) {
        printf("保存交换区状态失败\n");
        fclose(fp);
        return false;
    }

    fclose(fp);
    return true;
}

// 恢复系统状态
bool restore_system_state(const char* filename) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        printf("无法打开转储文件：%s\n", filename);
        return false;
    }

    // 1. 读取并验证头部信息
    SystemStateHeader header;
    if (fread(&header, sizeof(header), 1, fp) != 1) {
        printf("读取头部信息失败\n");
        fclose(fp);
        return false;
    }

    // 2. 清理当前状态
    scheduler_shutdown();
    vm_shutdown();
    
    // 3. 重新初始化系统
    scheduler_init();
    vm_init();
    memory_init();

    // 4. 恢复进程状态
    for (uint32_t i = 0; i < header.process_count; i++) {
        PCB proc_info;
        if (fread(&proc_info, sizeof(PCB), 1, fp) != 1) {
            printf("读取进程信息失败\n");
            fclose(fp);
            return false;
        }

        // 创建新进程
        PCB* new_proc = process_create(proc_info.pid);
        if (!new_proc) {
            printf("创建进程失败\n");
            fclose(fp);
            return false;
        }

        // 复制进程属性
        new_proc->state = proc_info.state;
        new_proc->priority = proc_info.priority;
        new_proc->time_slice = proc_info.time_slice;
        new_proc->wait_time = proc_info.wait_time;

        // 读取并恢复页表
        if (fread(new_proc->page_table, sizeof(PageTableEntry) * new_proc->page_table_size, 1, fp) != 1) {
            printf("读取页表失败\n");
            fclose(fp);
            return false;
        }
    }

    // 5. 恢复物理内存状态
    uint8_t* memory = get_physical_memory();
    bool* frame_map = get_frame_map();
    if (!memory || !frame_map ||
        fread(memory, PAGE_SIZE, PHYSICAL_PAGES, fp) != PHYSICAL_PAGES ||
        fread(frame_map, sizeof(bool), PHYSICAL_PAGES, fp) != PHYSICAL_PAGES) {
        printf("恢复物理内存状态失败\n");
        fclose(fp);
        return false;
    }

    // 6. 恢复交换区状态
    SwapBlockInfo* swap_blocks = get_swap_blocks();
    void* swap_area = get_swap_area();
    if (!swap_blocks || !swap_area ||
        fread(swap_blocks, sizeof(SwapBlockInfo), SWAP_SIZE, fp) != SWAP_SIZE ||
        fread(swap_area, SWAP_BLOCK_SIZE, SWAP_SIZE, fp) != SWAP_SIZE) {
        printf("恢复交换区状态失败\n");
        fclose(fp);
        return false;
    }

    // 恢复交换区状态后更新计数
    uint32_t used_blocks = 0;
    for (uint32_t i = 0; i < SWAP_SIZE; i++) {
        if (vm_manager.swap_blocks[i].is_used) {
            used_blocks++;
        }
    }
    vm_manager.swap_free_blocks = SWAP_SIZE - used_blocks;

    // 7. 恢复统计信息
    vm_manager.stats = header.memory_stats;

    fclose(fp);
    return true;
} 