#ifdef _WIN32
#include <windows.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "../include/vm.h"
#include "../include/memory.h"

// 在文件开头添加函数声明
uint32_t select_victim_page(PCB* process);
uint32_t select_victim_frame(void);
bool swap_out_page(uint32_t frame);
void clock_tick_handler(void);
uint64_t get_current_time(void);

// 全局虚拟内存管理器实例，用于管理交换区和内存统计
VMManager vm_manager;

/**
 * @brief 初始化虚拟内存管理器，分配交换区并初始化统计信息
 */
void vm_init(void) {
    // 分配交换区信息数组，每个交换块对应一个SwapBlockInfo
    vm_manager.swap_blocks = (SwapBlockInfo*)calloc(SWAP_SIZE, sizeof(SwapBlockInfo));
    // 分配交换区的物理内存空间
    vm_manager.swap_area = malloc(SWAP_SIZE * SWAP_BLOCK_SIZE);
    // 初始化空闲交换块数量
    vm_manager.swap_free_blocks = SWAP_SIZE;
    
    // 初始化内存统计信息为零
    memset(&vm_manager.stats, 0, sizeof(MemoryStats));

    // 检查内存分配是否成功
    if (!vm_manager.swap_blocks || !vm_manager.swap_area) {
        fprintf(stderr, "内存初始化失败\n");
        exit(1); // 内存分配失败，退出程序
    }
}

/**
 * @brief 访问内存，处理读写操作及可能的缺页中断
 * 
 * @param process 指向目标进程的PCB
 * @param virtual_address 要访问的虚拟地址
 * @param is_write 是否为写操作
 */
void access_memory(PCB* process, uint32_t virtual_address, bool is_write) {
    if (!process || !process->page_table) {
        printf("错误：无效的进程或页表\n");
        return;
    }
    
    vm_manager.stats.total_accesses++;
    uint32_t page_num = virtual_address / PAGE_SIZE;
    
    if (page_num >= process->page_table_size) {
        printf("错误：页号 %u 超出页表范围 %u\n", page_num, process->page_table_size);
        return;
    }
    
    PageTableEntry* pte = &process->page_table[page_num];
    
    printf("进程 %d 访问地址 0x%x (页号 %u)\n", process->pid, virtual_address, page_num);
    
    // 更新访问时间
    pte->last_access_time = get_current_time();
    
    // 检查页面是否在内存中
    if (!process->page_table[page_num].flags.present) {
        vm_manager.stats.page_faults++;
        if (!handle_page_fault(process, virtual_address)) {
            printf("页面访问失败，地址 0x%x\n", virtual_address);
            return;
        }
    }
}

/**
 * @brief 处理缺页中断，尝试将页面调入内存
 * 
 * @param process 指向目标进程的PCB
 * @param virtual_address 发生缺页的虚拟地址
 * @return true 处理成功
 * @return false 处理失败
 */
bool handle_page_fault(PCB* process, uint32_t virtual_page) {
    printf("\n=== 开始处理缺页中断 ===\n");
    printf("进程 %u 请求页面 %u\n", process->pid, virtual_page);
    
    // 检查页号是否有效
    if (virtual_page >= process->page_table_size) {
        printf("错误：页号 %u 超出页表范围 %u\n", virtual_page, process->page_table_size);
        return false;
    }
    
    // 先尝试直接分配页框
    uint32_t frame = allocate_frame(process->pid, virtual_page);
    
    // 如果没有空闲页框，需要进行页面置换
    if (frame == (uint32_t)-1) {
        printf("\n=== 内存已满，开始页面置换 ===\n");
        printf("当前空闲页框数: %u\n", memory_manager.free_frames_count);
        
        // 选择要置换出去的页框
        uint32_t victim_frame = select_victim_frame();
        if (victim_frame == (uint32_t)-1) {
            printf("错误：无法找到可替换的页面\n");
            return false;
        }
        
        // 获取被置换页面的信息
        PCB* victim_process = get_process_by_pid(memory_manager.frames[victim_frame].process_id);
        uint32_t victim_page = memory_manager.frames[victim_frame].virtual_page_num;
        
        printf("选择进程 %d 的页面 %d (页框 %d) 进行置换\n", 
               victim_process->pid, victim_page, victim_frame);
        
        // 如果页面被修改过，需要写入交换区
        if (!swap_out_page(victim_frame)) {
            printf("页面换出失败\n");
            return false;
        }
        
        // 为新页面分配刚释放的页框
        frame = victim_frame;
        memory_manager.frames[frame].is_allocated = true;
        memory_manager.frames[frame].process_id = process->pid;
        memory_manager.frames[frame].virtual_page_num = virtual_page;
    }
    
    // 更新页表
    process->page_table[virtual_page].frame_number = frame;
    process->page_table[virtual_page].flags.present = false;
    process->page_table[virtual_page].flags.swapped = true;
    process->page_table[virtual_page].flags.swap_index = frame;
    process->page_table[virtual_page].last_access_time = get_current_time();
    
    return true;
}

/**
 * @brief 输出指定进程的页面表状态
 * 
 * @param process 指向目标进程的PCB
 */
void dump_page_table(PCB* process) {
    printf("\n=== 页面状态 (PID=%u) ===\n", process->pid);
    int present_count = 0, swapped_count = 0;
    
    // 遍历所有页面，打印状态
    for (uint32_t i = 0; i < process->page_table_size; i++) {
        PageTableEntry* pte = &process->page_table[i];
        if (pte->flags.present || pte->flags.swapped) { // 只打印在内存中或已交换的页面
            printf("页面: %u, 状态: %s%s, 帧号: %u\n",
                   i,
                   pte->flags.present ? "已分配" : "未分配",
                   pte->flags.swapped ? ", 已交换" : "",
                   pte->flags.present ? pte->frame_number : pte->flags.swap_index);
            
            // 统计各状态页面数量
            if (pte->flags.present) present_count++;
            if (pte->flags.swapped) swapped_count++;
        }
    }
    
    // 输出统计信息
    printf("\n总页面数: %d, 已交换页面数: %d\n",
           present_count, swapped_count);
}

/**
 * @brief 关闭虚拟内存管理器，释放所有分配的内存
 */
void vm_shutdown(void) {
    free(vm_manager.swap_blocks); // 释放交换块信息数组
    free(vm_manager.swap_area);   // 释放交换区物理内存
}

/**
 * @brief 分配一个交换块，用于页面交换
 * 
 * @param process_id 进程ID
 * @param virtual_page 虚拟页号
 * @return uint32_t 分配的交换块索引，失败返回 -1
 */
uint32_t allocate_swap_block(uint32_t process_id, uint32_t virtual_page) {
    if (vm_manager.swap_free_blocks == 0) { // 检查是否有空闲交换块
        return (uint32_t)-1;
    }

    // 遍历交换块，找到第一个未使用的交换块
    for (uint32_t i = 0; i < SWAP_SIZE; i++) {
        if (!vm_manager.swap_blocks[i].is_used) {
            vm_manager.swap_blocks[i].is_used = true; // 标记为已使用
            vm_manager.swap_blocks[i].process_id = process_id; // 记录进程ID
            vm_manager.swap_blocks[i].virtual_page = virtual_page; // 记录虚拟页号
            vm_manager.swap_free_blocks--; // 减少空闲交换块计数
            return i; // 返回交换块索引
        }
    }
    return (uint32_t)-1; // 未找到空闲交换块
}

/**
 * @brief 释放一个交换块
 * 
 * @param swap_index 要释放的交换块索引
 */
void free_swap_block(uint32_t swap_index) {
    // 检查交换块索引有效且已被使用
    if (swap_index < SWAP_SIZE && vm_manager.swap_blocks[swap_index].is_used) {
        vm_manager.swap_blocks[swap_index].is_used = false; // 标记为未使用
        vm_manager.swap_blocks[swap_index].process_id = 0; // 清除进程ID
        vm_manager.swap_blocks[swap_index].virtual_page = 0; // 清除虚拟页号
        vm_manager.swap_free_blocks++; // 增加空闲交换块计数
    }
}

/**
 * @brief 将页面交换出到交换区
 * 
 * @param process 指向目标进程的PCB
 * @param page_num 要交换出的页面号
 * @return true 交换成功
 * @return false 交换失败
 */
bool page_out(PCB* process, uint32_t page_num) {
    // 检查页面是否确实在内存中
    if (!process || page_num >= process->page_table_size ||
        !process->page_table[page_num].flags.present) {
        printf("错误：页面 %u 不在内存中\n", page_num);
        return false;
    }

    PageTableEntry* pte = &process->page_table[page_num];
    // 检查页面是否在内存中
    if (!pte->flags.present) {
        printf("错误：页面 %u 不在内存中\n", page_num);
        return false;
    }

    // 分配交换块
    uint32_t swap_index = allocate_swap_block(process->pid, page_num);
    if (swap_index == (uint32_t)-1) { // 检查是否分配成功
        printf("错误：无法分配交换块（剩余交换块：%u）\n", 
               vm_manager.swap_free_blocks);
        return false;
    }

    // 读取页面数据到临时缓冲区
    uint8_t page_data[PAGE_SIZE];
    if (!read_physical_memory(pte->frame_number, 0, page_data, PAGE_SIZE)) {
        printf("错误：无法从物理内存读取页面数据（页框号：%u）\n", 
               pte->frame_number);
        free_swap_block(swap_index); // 读取失败，释放交换块
        return false;
    }
    
    // 将页面数据写入交换区
    if (!write_to_swap(swap_index, page_data)) {
        printf("错误：无法写入交换区（交换块号：%u）\n", swap_index);
        free_swap_block(swap_index); // 写入失败，释放交换块
        return false;
    }

    // 释放物理帧并更新页表项
    free_frame(pte->frame_number); // 释放物理帧
    pte->frame_number = (uint32_t)-1; // 重置帧号
    pte->flags.present = false; // 标记页面不在内存中
    pte->flags.swapped = true; // 标记页面已交换
    pte->flags.swap_index = swap_index; // 记录交换块索引
    
    // 更新内存统计信息
    vm_manager.stats.page_replacements++;
    printf("页面 %u 已出到交换区块 %u\n", page_num, swap_index);
    return true;
}

/**
 * @brief 将页面调入内存
 * 
 * @param process 指向目标进程的PCB
 * @param virtual_page 要调入的虚拟页号
 * @return true 调入成功
 * @return false 调入失败
 */
bool page_in(PCB* process, uint32_t virtual_page) {
    // 检查进程和页号的有效性
    if (!process || virtual_page >= process->page_table_size) {
        return false;
    }

    PageTableEntry* pte = &process->page_table[virtual_page];
    // 检查页面是否被交换出
    if (!pte->flags.swapped) {
        return false; // 页面未被交换，无法调入
    }

    // 尝试分配物理帧
    uint32_t frame = allocate_frame(process->pid, virtual_page);
    if (frame == (uint32_t)-1) { // 内存不足
        uint32_t victim_page = select_victim_page(process); // 选择受害页面
        if (victim_page != (uint32_t)-1) {
            printf("选择页面 %u 进行交换\n", victim_page);
            if (page_out(process, victim_page)) { // 将受害页面交换出
                frame = allocate_frame(process->pid, virtual_page); // 再次尝试分配帧
            }
        }
    }

    if (frame == (uint32_t)-1) { // 如果仍然无法分配帧
        return false;
    }

    // 读取页面数据从交换区
    uint8_t page_data[PAGE_SIZE];
    uint32_t swap_index = pte->flags.swap_index;
    if (!read_from_swap(swap_index, page_data)) {
        free_frame(frame); // 读取失败，释放帧
        return false;
    }

    // 将页面数据写入物理内存
    if (!write_physical_memory(frame, 0, page_data, PAGE_SIZE)) {
        free_frame(frame); // 写入失败，释放帧
        return false;
    }

    // 更新页表项，标记页面在内存中
    pte->frame_number = frame;
    pte->flags.present = true; // 标记页面在内存中
    pte->flags.swapped = false; // 清除交换位
    
    printf("页面 %u 已调入内存，交换块 %u\n", 
           virtual_page, swap_index);
    return true;
}

/**
 * @brief 获取当前空闲的交换块数量
 * 
 * @return uint32_t 空闲交换块数量
 */
uint32_t get_free_swap_blocks(void) {
    return vm_manager.swap_free_blocks;
}

/**
 * @brief 输出虚拟内存的统计信息
 */
void print_vm_stats(void) {
    printf("总访问次数: %u, 缺页次数: %u (%.1f%%), 页面替换次数: %u, 写入磁盘次数: %u\n",
           vm_manager.stats.total_accesses,
           vm_manager.stats.page_faults,
           (vm_manager.stats.total_accesses > 0) ?
           (vm_manager.stats.page_faults * 100.0 / vm_manager.stats.total_accesses) : 0,
           vm_manager.stats.page_replacements,
           vm_manager.stats.writes_to_disk);
    printf("已用交换块: %u/%u\n",
           vm_manager.swap_free_blocks,
           SWAP_SIZE);
}

/**
 * @brief 选择一个受害页面进行置换
 * 
 * @param process 指向目标进程的PCB
 * @return uint32_t 选择的受害页号，未找到则返回 -1
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
 * @brief 将页面数据写入指定虚拟地址
 * 
 * @param process 指向目标进程的PCB
 * @param virtual_address 要写入的虚拟地址
 * @param data 要写入的数据指针
 * @param size 要写入的数据大小
 * @return true 写入成功
 * @return false 写入失败
 */
bool write_page_data(PCB* process, uint32_t virtual_address, const void* data, size_t size) {
    uint32_t page_num = virtual_address / PAGE_SIZE;
    if (!process || page_num >= process->page_table_size) {
        return false;
    }

    // 确保页面在内存中
    if (!process->page_table[page_num].flags.present) {
        if (!handle_page_fault(process, virtual_address)) {
            return false;
        }
    }

    // 获取物理地址并写入数据
    uint32_t frame = process->page_table[page_num].frame_number;
    if (!write_physical_memory(frame, 0, data, size)) {
        return false;
    }

    // 更新页表项
    process->page_table[page_num].last_access_time = get_current_time();
    return true;
}

/**
 * @brief 从指定虚拟地址读取页面数据
 * 
 * @param process 指向目标进程的PCB
 * @param virtual_address 要读取的虚拟地址
 * @param buffer 存储读取数据的缓冲区
 * @param size 要读取的数据大小
 * @return true 读取成功
 * @return false 读取失败
 */
bool read_page_data(PCB* process, uint32_t virtual_address, void* buffer, size_t size) {
    uint32_t page_num = virtual_address / PAGE_SIZE;
    if (!process || page_num >= process->page_table_size) {
        return false;
    }

    // 确保页面在内存中
    if (!process->page_table[page_num].flags.present) {
        if (!handle_page_fault(process, virtual_address)) {
            return false;
        }
    }

    // 获取物理地址并读取数据
    uint32_t frame = process->page_table[page_num].frame_number;
    if (!read_physical_memory(frame, 0, buffer, size)) {
        return false;
    }

    // 更新页表项
    process->page_table[page_num].last_access_time = get_current_time();
    return true;
}

/**
 * @brief 将数据写入交换区的指定交换块
 * 
 * @param swap_index 交换块索引
 * @param data 要写入的数据指针
 * @return true 写入成功
 * @return false 写入失败
 */
bool write_to_swap(uint32_t swap_index, const void* data) {
    if (swap_index >= SWAP_SIZE || !data) { // 检查交换块索引和数据有效性
        return false;
    }

    // 检查交换块是否已被使用
    if (!vm_manager.swap_blocks[swap_index].is_used) {
        return false;
    }

    // 计算交换区中交换块的起始地址
    uint8_t* dest = (uint8_t*)vm_manager.swap_area + (swap_index * SWAP_BLOCK_SIZE);
    memcpy(dest, data, SWAP_BLOCK_SIZE); // 将数据复制到交换区
    vm_manager.stats.writes_to_disk++; // 增加写入磁盘次数

    return true; // 写入成功
}

/**
 * @brief 从交换区读取数据到缓冲区
 * 
 * @param swap_index 交换块索引
 * @param buffer 存储读取数据的缓冲区
 * @return true 读取成功
 * @return false 读取失败
 */
bool read_from_swap(uint32_t swap_index, void* buffer) {
    if (swap_index >= SWAP_SIZE || !buffer) { // 检查交换块索引和缓冲区有效性
        return false;
    }

    // 检查交换块是否已被使用
    if (!vm_manager.swap_blocks[swap_index].is_used) {
        return false;
    }

    // 计算交换区中交换块的起始地址
    uint8_t* src = (uint8_t*)vm_manager.swap_area + (swap_index * SWAP_BLOCK_SIZE);
    memcpy(buffer, src, SWAP_BLOCK_SIZE); // 将数据复制到缓冲区

    return true; // 读取成功
}

/**
 * @brief 获取交换块信息数组
 * 
 * @return SwapBlockInfo* 指向交换块信息数组的指针
 */
SwapBlockInfo* get_swap_blocks(void) {
    return vm_manager.swap_blocks;
}

/**
 * @brief 获取交换区的物理内存指针
 * 
 * @return void* 指向交换区物理内存的指针
 */
void* get_swap_area(void) {
    return vm_manager.swap_area;
}

/**
 * @brief 获取内存管理的统计信息
 * 
 * @return MemoryStats 返回内存统计信息
 */
MemoryStats get_memory_stats(void) {
    return vm_manager.stats;
}

/**
 * @brief 清理交换区，释放所有使用中的交换块
 */
void clean_swap_area(void) {
    for (uint32_t i = 0; i < SWAP_BLOCKS; i++) {
        if (vm_manager.swap_blocks[i].is_used) { // 检查交换块是否被使用
            vm_manager.swap_blocks[i].is_used = false; // 标记为未使用
            vm_manager.swap_blocks[i].process_id = 0; // 清除进程ID
            vm_manager.swap_blocks[i].virtual_page = 0; // 清除虚拟页号
            vm_manager.swap_free_blocks++; // 增加空闲交换块计数
        }
    }
}

/**
 * @brief 输出当前交换区的状态，包括总交换块数、空闲和已使用交换块
 */
void print_swap_status(void) {
    uint32_t used_blocks = 0;
    // 统计已使用的交换块数量
    for (uint32_t i = 0; i < SWAP_BLOCKS; i++) {
        if (vm_manager.swap_blocks[i].is_used) {
            used_blocks++;
        }
    }
    printf("总交换块: %u\n", SWAP_BLOCKS);
    printf("空闲交换块: %u\n", SWAP_BLOCKS - used_blocks);
    printf("已使用交换块: %u\n", used_blocks);
    
    printf("\n已使用页面\n");
    // 打印每个已使用的交换块的信息
    for (uint32_t i = 0; i < SWAP_BLOCKS; i++) {
        if (vm_manager.swap_blocks[i].is_used) {
            printf(" %u: PID=%u, 页面=%u\n",
                   i,
                   vm_manager.swap_blocks[i].process_id,
                   vm_manager.swap_blocks[i].virtual_page);
        }
    }
}

/**
 * @brief 获取最后被替换的页面号
 * 
 * @return uint32_t 最后被替换的页面号
 */
uint32_t get_last_replaced_page(void) {
    return vm_manager.stats.last_replaced_page;
}

/**
 * @brief 更新页面替换的统计信息
 * 
 * @param page_num 被替换的页面号
 */
void update_replacement_stats(uint32_t page_num) {
    vm_manager.stats.page_replacements++; // 增加页面替换次数
    vm_manager.stats.last_replaced_page = page_num; // 记录最后被替换的页面号
    printf("页面替换：页面 %u 被替换，替换次数 %u\n", 
           page_num, vm_manager.stats.page_replacements);
}

/**
 * @brief 使用时钟算法选择要替换的页框
 * 
 * @return uint32_t 选择的页框号，未找到则返回 -1
 */
uint32_t select_victim_frame(void) {
    uint32_t victim_frame = (uint32_t)-1;
    uint64_t oldest_access = UINT64_MAX;
    
    for (uint32_t i = 0; i < PHYSICAL_PAGES; i++) {
        if (memory_manager.frames[i].is_allocated) {
            PCB* process = get_process_by_pid(memory_manager.frames[i].process_id);
            uint32_t page_num = memory_manager.frames[i].virtual_page_num;
            if (process && page_num < process->page_table_size) {
                if (process->page_table[page_num].last_access_time < oldest_access) {
                    oldest_access = process->page_table[page_num].last_access_time;
                    victim_frame = i;
                }
            }
        }
    }
    return victim_frame;
}

/**
 * @brief 将页面写入交换区
 * 
 * @param frame 要写入的页框号
 * @return true 写入成功
 * @return false 写入失败
 */
bool swap_out_page(uint32_t frame) {
    PCB* process = get_process_by_pid(memory_manager.frames[frame].process_id);
    if (!process) {
        return false;
    }
    
    uint32_t virtual_page = memory_manager.frames[frame].virtual_page_num;
    
    // 分配交换区空间
    uint32_t swap_index = allocate_swap_block(process->pid, virtual_page);
    if (swap_index == (uint32_t)-1) {
        return false;
    }
    
    // 将页面写入交换区
    uint8_t page_data[PAGE_SIZE];
    if (!read_physical_memory(frame, 0, page_data, PAGE_SIZE) ||
        !write_to_swap(swap_index, page_data)) {
        free_swap_block(swap_index);
        return false;
    }
    
    // 更新页表
    process->page_table[virtual_page].flags.present = false;
    process->page_table[virtual_page].flags.swapped = true;
    process->page_table[virtual_page].flags.swap_index = swap_index;
    
    // 释放页框
    free_frame(frame);
    return true;
}

/**
 * @brief 获取当前时间
 * 
 * @return uint64_t 当前时间
 */
uint64_t get_current_time(void) {
#ifdef _WIN32
    // Windows系统使用GetSystemTimeAsFileTime
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    // 转换为微秒 (Windows文件时间是从1601年开始的100纳秒间隔)
    return (uli.QuadPart - 116444736000000000ULL) / 10;
#else
    // 非Windows系统使用clock_gettime
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000000) + (ts.tv_nsec / 1000);
#endif
}
