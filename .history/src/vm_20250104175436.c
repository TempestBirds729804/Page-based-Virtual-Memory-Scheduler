#ifdef _WIN32
#include <windows.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "../include/vm.h"
#include "../include/memory.h"

// 全局变量和结构体
bool swap_out_page(uint32_t frame);
void clock_tick_handler(void);
uint64_t get_current_time(void);

// 虚拟内存管理器实例，管理交换区和页面置换等功能
VMManager vm_manager;

// 页面置换统计信息
PageReplacementStats page_stats = {0};

/**
 * @brief 初始化虚拟内存管理器，包括交换区和统计信息的初始化
 */
void vm_init(void) {
    // 初始化交换区管理器，分配交换区块信息数组
    vm_manager.swap_blocks = (SwapBlockInfo*)calloc(SWAP_SIZE, sizeof(SwapBlockInfo));
    // 分配交换区实际存储空间
    vm_manager.swap_area = malloc(SWAP_SIZE * SWAP_BLOCK_SIZE);
    // 初始化空闲交换块数量
    vm_manager.swap_free_blocks = SWAP_SIZE;
    
    // 初始化内存访问统计
    memset(&vm_manager.stats, 0, sizeof(MemoryStats));

    // 检查内存分配是否成功
    if (!vm_manager.swap_blocks || !vm_manager.swap_area) {
        fprintf(stderr, "虚拟内存初始化失败\n");
        exit(1); // 内存分配失败，退出程序
    }
}

/**
 * @brief 处理内存访问，包括读写操作和缺页中断处理
 * 
 * @param process 要访问内存的进程PCB
 * @param virtual_address 要访问的虚拟地址
 * @param is_write 是否是写操作
 */
void access_memory(PCB* process, uint32_t virtual_address, bool is_write) {
    if (!process || !process->page_table) {
        printf("错误：无效的进程或页表\n");
        return;
    }
    
    vm_manager.stats.total_accesses++;
    
    // 计算页号和偏移量
    uint32_t page_num = (virtual_address & 0xFFFFF000) >> 12;  // 高20位为页号
    uint32_t offset = virtual_address & 0xFFF;                 // 低12位为页内偏移
    
    if (page_num >= process->page_table_size) {
        printf("错误：访问地址 0x%x (页号=%u, 偏移=0x%x) 超出进程页表范围\n", 
               virtual_address, page_num, offset);
        return;
    }
    
    PageTableEntry* pte = &process->page_table[page_num];
    
    // 如果页面不在内存中
    if (!pte->flags.present) {
        printf("\n=== 处理缺页中断 ===\n");
        vm_manager.stats.page_faults++;
        if (!handle_page_fault(process, page_num)) {
            printf("错误：无法处理地址 0x%x\n", virtual_address);
            return;
        }
    }
    
    // 更新页面访问时间
    uint64_t current_time = get_current_time();
    pte->last_access_time = current_time;
    
    // 更新页框访问信息
    if (pte->flags.present) {
        uint32_t frame = pte->frame_number;
        memory_manager.frames[frame].last_access_time = current_time;
        
        if (is_write) {
            pte->flags.dirty = true;
            memory_manager.frames[frame].is_dirty = true;
            printf("进程 %d 写入地址 0x%x (页号=%u, 偏移=0x%x, 页框=%u)\n", 
                   process->pid, virtual_address, page_num, offset, frame);
        } else {
            printf("进程 %d 读取地址 0x%x (页号=%u, 偏移=0x%x, 页框=%u)\n", 
                   process->pid, virtual_address, page_num, offset, frame);
        }
    }
}

/**
 * @brief 处理缺页中断，包括页面调入和页面置换
 * 
 * @param process 发生缺页中断的进程PCB
 * @param virtual_address 发生缺页中断的地址
 * @return true 处理成功
 * @return false 处理失败
 */
bool handle_page_fault(PCB* process, uint32_t virtual_page) {
    printf("\n=== 处理缺页中断 ===\n");
    printf("进程 %u 访问页面 %u\n", process->pid, virtual_page);
    
    // 更新统计信息
    vm_manager.stats.page_faults++;
    process->stats.page_faults++;
    
    // 检查页号是否有效
    if (virtual_page >= process->page_table_size) {
        printf("错误：页号 %u 超出页表范围 %u\n", virtual_page, process->page_table_size);
        return false;
    }
    
    PageTableEntry* pte = &process->page_table[virtual_page];
    printf("页表项状态：present=%d, swapped=%d\n", 
           pte->flags.present, pte->flags.swapped);
    
    // 尝试分配一个空闲页框
    uint32_t frame = allocate_frame(process->pid, virtual_page);
    
    // 如果没有空闲页框，需要进行页面置换
    if (frame == (uint32_t)-1) {
        printf("\n=== 需要进行页面置换 ===\n");
        printf("当前空闲页框数: %u\n", memory_manager.free_frames_count);
        
        // 更新统计信息
        vm_manager.stats.page_replacements++;
        process->stats.page_replacements++;
        
        // 选择牺牲页框
        uint32_t victim_frame = select_victim_frame();
        if (victim_frame == (uint32_t)-1) {
            printf("错误：无法选择置换页框\n");
            return false;
        }
        
        // 获取牺牲页框的进程
        PCB* victim_process = get_process_by_pid(memory_manager.frames[victim_frame].process_id);
        if (!victim_process) {
            printf("错误：找不到牺牲页框所属的进程\n");
            return false;
        }
        
        uint32_t victim_page = memory_manager.frames[victim_frame].virtual_page_num;
        printf("选择进程 %u 的页面 %u (页框 %u) 进行置换\n", 
               victim_process->pid, victim_page, victim_frame);
        
        // 将牺牲页框的内容写入交换区
        if (!swap_out_page(victim_frame)) {
            printf("错误：无法将页面写入交换区\n");
            return false;
        }
        
        // 分配页框给当前进程
        frame = victim_frame;
        printf("更新页框 %u 的信息：\n", frame);
        printf("  之前 - pid=%u, page=%u, dirty=%d\n",
               memory_manager.frames[frame].process_id,
               memory_manager.frames[frame].virtual_page_num,
               memory_manager.frames[frame].is_dirty);
               
        memory_manager.frames[frame].is_allocated = true;
        memory_manager.frames[frame].process_id = process->pid;
        memory_manager.frames[frame].virtual_page_num = virtual_page;
        memory_manager.frames[frame].last_access_time = get_current_time();
        memory_manager.frames[frame].is_dirty = false;
        
        printf("  之后 - pid=%u, page=%u, dirty=%d\n",
               memory_manager.frames[frame].process_id,
               memory_manager.frames[frame].virtual_page_num,
               memory_manager.frames[frame].is_dirty);
    }
    
    // 如果页面在交换区中，需要调入内存
    if (pte->flags.swapped) {
        printf("页面在交换区中，需要调入内存\n");
        vm_manager.stats.disk_reads++;
        process->stats.pages_swapped_in++;
        if (!swap_in_page(process->pid, virtual_page, frame)) {
            printf("错误：无法从交换区调入页面\n");
            return false;
        }
    }
    
    // 更新页表项
    printf("更新当前进程的页表项：\n");
    printf("  之前 - present=%d, swapped=%d, frame=%u\n",
           pte->flags.present, pte->flags.swapped, pte->frame_number);
           
    pte->frame_number = frame;
    pte->flags.present = true;
    pte->flags.swapped = false;
    pte->last_access_time = get_current_time();
    
    printf("  之后 - present=%d, swapped=%d, frame=%u\n",
           pte->flags.present, pte->flags.swapped, pte->frame_number);
    
    printf("页面 %u 已加载到页框 %u\n", virtual_page, frame);
    
    return true;
}

/**
 * @brief 输出进程的页表信息
 * 
 * @param process 要输出页表信息的进程PCB
 */
void dump_page_table(PCB* process) {
    printf("\n=== 进程 %u 的页表 ===\n", process->pid);
    int present_count = 0, swapped_count = 0;
    
    // 遍历进程的页表项
    for (uint32_t i = 0; i < process->page_table_size; i++) {
        PageTableEntry* pte = &process->page_table[i];
        if (pte->flags.present || pte->flags.swapped) { // 如果页面在内存中或被交换
            printf("页号: %u, 状态: %s%s, 页框: %u\n",
                   i,
                   pte->flags.present ? "在内存" : "不在内存",
                   pte->flags.swapped ? ", 被交换" : "",
                   pte->flags.present ? pte->frame_number : pte->flags.swap_index);
            
            // 更新统计信息
            if (pte->flags.present) present_count++;
            if (pte->flags.swapped) swapped_count++;
        }
    }
    
    // 输出统计信息
    printf("\n在内存的页数: %d, 被交换的页数: %d\n",
           present_count, swapped_count);
}

/**
 * @brief 关闭虚拟内存管理器，释放资源
 */
void vm_shutdown(void) {
    free(vm_manager.swap_blocks); // 释放交换区块信息数组
    free(vm_manager.swap_area);   // 释放交换区实际存储空间
}

/**
 * @brief 分配一个交换区块，返回交换区块索引
 * 
 * @param process_id 进程ID
 * @param virtual_page 虚拟页号
 * @return uint32_t 交换区块索引，如果分配失败返回-1
 */
uint32_t allocate_swap_block(uint32_t process_id, uint32_t virtual_page) {
    if (vm_manager.swap_free_blocks == 0) { // 如果没有空闲交换块
        return (uint32_t)-1;
    }

    // 遍历交换区块信息数组，找到一个空闲的交换区块
    for (uint32_t i = 0; i < SWAP_SIZE; i++) {
        if (!vm_manager.swap_blocks[i].is_used) {
            vm_manager.swap_blocks[i].is_used = true; // 标记为已使用
            vm_manager.swap_blocks[i].process_id = process_id; // 设置进程ID
            vm_manager.swap_blocks[i].virtual_page = virtual_page; // 设置虚拟页号
            vm_manager.swap_free_blocks--; // 减少空闲交换块数量
            return i; // 返回交换区块索引
        }
    }
    return (uint32_t)-1; // 如果所有交换区块都被使用，返回-1表示分配失败
}

/**
 * @brief 释放一个交换区块
 * 
 * @param swap_index 交换区块索引
 */
void free_swap_block(uint32_t swap_index) {
    // 检查交换区块索引是否有效
    if (swap_index < SWAP_SIZE && vm_manager.swap_blocks[swap_index].is_used) {
        vm_manager.swap_blocks[swap_index].is_used = false; // 标记为未使用
        vm_manager.swap_blocks[swap_index].process_id = 0; // 设置进程ID为0
        vm_manager.swap_blocks[swap_index].virtual_page = 0; // 设置虚拟页号为0
        vm_manager.swap_free_blocks++; // 增加空闲交换块数量
    }
}

/**
 * @brief 将页面写入交换区
 * 
 * @param process 要写入页面的进程PCB
 * @param page_num 要写入页面的页号
 * @return true 写入成功
 * @return false 写入失败
 */
bool page_out(PCB* process, uint32_t page_num) {
    // 检查页号是否有效
    if (!process || page_num >= process->page_table_size ||
        !process->page_table[page_num].flags.present) {
        printf("错误：页号 %u 无效\n", page_num);
        return false;
    }

    PageTableEntry* pte = &process->page_table[page_num];
    // 检查页面是否在内存中
    if (!pte->flags.present) {
        printf("错误：页号 %u 不在内存中\n", page_num);
        return false;
    }

    // 分配一个空闲交换区块
    uint32_t swap_index = allocate_swap_block(process->pid, page_num);
    if (swap_index == (uint32_t)-1) { // 如果分配失败
        printf("错误：没有空闲交换区块\n");
        return false;
    }

    // 读取页面数据
    uint8_t page_data[PAGE_SIZE];
    if (!read_physical_memory(pte->frame_number, 0, page_data, PAGE_SIZE)) {
        printf("错误：读取物理内存失败\n");
        free_swap_block(swap_index); // 释放交换区块
        return false;
    }
    
    // 将数据写入交换区
    if (!write_to_swap(swap_index, page_data)) {
        printf("错误：写入交换区失败\n");
        free_swap_block(swap_index); // 释放交换区块
        return false;
    }

    // 释放页面
    free_frame(pte->frame_number); // 释放页框
    pte->frame_number = (uint32_t)-1; // 设置页框为-1表示不在内存中
    pte->flags.present = false; // 设置页面不在内存中
    pte->flags.swapped = true; // 设置页面被交换
    pte->flags.swap_index = swap_index; // 设置交换区块索引
    
    // 更新统计信息
    vm_manager.stats.page_replacements++;
    printf("页面 %u 已写入交换区，交换区块索引: %u\n", page_num, swap_index);
    return true;
}

/**
 * @brief 将页面调入内存
 * 
 * @param process 要调入页面的进程PCB
 * @param virtual_page 要调入页面的虚拟页号
 * @return true 调入成功
 * @return false 调入失败
 */
bool page_in(PCB* process, uint32_t virtual_page) {
    // 检查页号是否有效
    if (!process || virtual_page >= process->page_table_size) {
        return false;
    }

    PageTableEntry* pte = &process->page_table[virtual_page];
    // 检查页面是否被交换
    if (!pte->flags.swapped) {
        return false; // 如果页面不在交换区，返回false表示不需要调入
    }

    // 尝试分配一个空闲页框
    uint32_t frame = allocate_frame(process->pid, virtual_page);
    if (frame == (uint32_t)-1) { // 如果分配失败
        uint32_t victim_page = select_victim_page(process); // 选择一个牺牲页面
        if (victim_page != (uint32_t)-1) {
            printf("选择进程 %u 的页面 %u 进行置换\n", process->pid, victim_page);
            if (page_out(process, victim_page)) { // 将牺牲页面写入交换区
                frame = allocate_frame(process->pid, virtual_page); // 重新分配页框
            }
        }
    }

    if (frame == (uint32_t)-1) { // 如果分配失败
        return false;
    }

    // 读取页面数据
    uint8_t page_data[PAGE_SIZE];
    uint32_t swap_index = pte->flags.swap_index;
    if (!read_from_swap(swap_index, page_data)) {
        free_frame(frame); // 释放页框
        return false;
    }

    // 将数据写入物理内存
    if (!write_physical_memory(frame, 0, page_data, PAGE_SIZE)) {
        free_frame(frame); // 释放页框
        return false;
    }

    // 更新页表项
    pte->frame_number = frame;
    pte->flags.present = true; // 设置页面在内存中
    pte->flags.swapped = false; // 设置页面不在交换区
    
    printf("页面 %u 已加载到页框 %u\n", virtual_page, frame);
    return true;
}

/**
 * @brief 获取空闲交换区块数量
 * 
 * @return uint32_t 空闲交换区块数量
 */
uint32_t get_free_swap_blocks(void) {
    return vm_manager.swap_free_blocks;
}

/**
 * @brief 输出虚拟内存管理器的统计信息
 */
void print_vm_stats(void) {
    float page_fault_rate = vm_manager.stats.total_accesses > 0 ? 
        (float)vm_manager.stats.page_faults * 100 / vm_manager.stats.total_accesses : 0;
    
    printf("=== 虚拟内存管理器统计信息 ===\n");
    printf("总访问次数: %u\n", vm_manager.stats.total_accesses);
    printf("缺页次数: %u (%.1f%%)\n", vm_manager.stats.page_faults, page_fault_rate);
    printf("页面置换次数: %u\n", vm_manager.stats.page_replacements);
    printf("磁盘写入次数: %u\n", vm_manager.stats.disk_writes);
    printf("页面调出次数: %u\n", vm_manager.stats.pages_swapped_out);
    printf("页面调入次数: %u\n", vm_manager.stats.pages_swapped_in);
    
    printf("\n=== 交换区统计信息 ===\n");
    printf("交换区块总数: %u\n", SWAP_SIZE);
    printf("空闲交换区块数: %u\n", SWAP_SIZE - vm_manager.swap_free_blocks);
    printf("已使用交换区块数: %u\n", vm_manager.swap_free_blocks);
    
    float fragmentation = 0;
    if (memory_manager.free_frames_count > 0) {
        uint32_t fragments = count_memory_fragments();
        fragmentation = (float)fragments * 100 / memory_manager.free_frames_count;
    }
    
    printf("\n=== 内存碎片统计信息 ===\n");
    printf("空闲页框数: %u\n", memory_manager.free_frames_count);
    printf("内存碎片数: %u\n", count_memory_fragments());
    printf("内存碎片率: %.1f%%\n", fragmentation);
}

/**
 * @brief 将页面数据写入物理内存
 * 
 * @param process 要写入页面的进程PCB
 * @param virtual_address 要写入页面的虚拟地址
 * @param data 要写入的数据
 * @param size 要写入的数据大小
 * @return true 写入成功
 * @return false 写入失败
 */
bool write_page_data(PCB* process, uint32_t virtual_address, const void* data, size_t size) {
    uint32_t page_num = virtual_address / PAGE_SIZE;
    if (!process || page_num >= process->page_table_size) {
        return false;
    }

    // 检查页面是否在内存中
    if (!process->page_table[page_num].flags.present) {
        if (!handle_page_fault(process, virtual_address)) {
            return false;
        }
    }

    // 获取页框号
    uint32_t frame = process->page_table[page_num].frame_number;
    if (!write_physical_memory(frame, 0, data, size)) {
        return false;
    }

    // 更新页面访问时间
    process->page_table[page_num].last_access_time = get_current_time();
    return true;
}

/**
 * @brief 将页面数据从物理内存读取到虚拟内存
 * 
 * @param process 要读取页面的进程PCB
 * @param virtual_address 要读取页面的虚拟地址
 * @param buffer 要读取的数据缓冲区
 * @param size 要读取的数据大小
 * @return true 读取成功
 * @return false 读取失败
 */
bool read_page_data(PCB* process, uint32_t virtual_address, void* buffer, size_t size) {
    uint32_t page_num = virtual_address / PAGE_SIZE;
    if (!process || page_num >= process->page_table_size) {
        return false;
    }

    // 检查页面是否在内存中
    if (!process->page_table[page_num].flags.present) {
        if (!handle_page_fault(process, virtual_address)) {
            return false;
        }
    }

    // 获取页框号
    uint32_t frame = process->page_table[page_num].frame_number;
    if (!read_physical_memory(frame, 0, buffer, size)) {
        return false;
    }

    // 更新页面访问时间
    process->page_table[page_num].last_access_time = get_current_time();
    return true;
}

/**
 * @brief 将数据写入交换区
 * 
 * @param swap_index 交换区块索引
 * @param data 要写入的数据
 * @return true 写入成功
 * @return false 写入失败
 */
bool write_to_swap(uint32_t swap_index, const void* data) {
    if (swap_index >= SWAP_SIZE || !data) { // 检查交换区块索引是否有效
        return false;
    }

    // 检查交换区块是否被使用
    if (!vm_manager.swap_blocks[swap_index].is_used) {
        return false;
    }

    // 将数据写入交换区
    uint8_t* dest = (uint8_t*)vm_manager.swap_area + (swap_index * SWAP_BLOCK_SIZE);
    memcpy(dest, data, SWAP_BLOCK_SIZE); // 复制数据
    vm_manager.stats.writes_to_disk++; // 增加磁盘写入次数

    return true; // 写入成功
}

/**
 * @brief 从交换区读取数据
 * 
 * @param swap_index 交换区块索引
 * @param buffer 要读取的数据缓冲区
 * @return true 读取成功
 * @return false 读取失败
 */
bool read_from_swap(uint32_t swap_index, void* buffer) {
    if (swap_index >= SWAP_SIZE || !buffer) { // 检查交换区块索引是否有效
        return false;
    }

    // 检查交换区块是否被使用
    if (!vm_manager.swap_blocks[swap_index].is_used) {
        return false;
    }

    // 将数据从交换区读取到缓冲区
    uint8_t* src = (uint8_t*)vm_manager.swap_area + (swap_index * SWAP_BLOCK_SIZE);
    memcpy(buffer, src, SWAP_BLOCK_SIZE); // 复制数据

    return true; // 读取成功
}

/**
 * @brief 获取交换区块信息数组
 * 
 * @return SwapBlockInfo* 交换区块信息数组
 */
SwapBlockInfo* get_swap_blocks(void) {
    return vm_manager.swap_blocks;
}

/**
 * @brief 获取交换区实际存储空间
 * 
 * @return void* 交换区实际存储空间
 */
void* get_swap_area(void) {
    return vm_manager.swap_area;
}

/**
 * @brief 获取内存访问统计信息
 * 
 * @return MemoryStats 内存访问统计信息
 */
MemoryStats get_memory_stats(void) {
    return vm_manager.stats;
}

/**
 * @brief 清理交换区，释放所有交换区块
 */
void clean_swap_area(void) {
    for (uint32_t i = 0; i < SWAP_BLOCKS; i++) {
        if (vm_manager.swap_blocks[i].is_used) { // 如果交换区块被使用
            vm_manager.swap_blocks[i].is_used = false; // 标记为未使用
            vm_manager.swap_blocks[i].process_id = 0; // 设置进程ID为0
            vm_manager.swap_blocks[i].virtual_page = 0; // 设置虚拟页号为0
            vm_manager.swap_free_blocks++; // 增加空闲交换块数量
        }
    }
}

/**
 * @brief 输出交换区状态
 */
void print_swap_status(void) {
    uint32_t used_blocks = 0;
    // 遍历交换区块信息数组，统计已使用交换区块数量
    for (uint32_t i = 0; i < SWAP_BLOCKS; i++) {
        if (vm_manager.swap_blocks[i].is_used) {
            used_blocks++;
        }
    }
    printf("已使用交换区块数: %u\n", SWAP_BLOCKS);
    printf("空闲交换区块数: %u\n", SWAP_BLOCKS - used_blocks);
    printf("已使用交换区块数: %u\n", used_blocks);
    
    printf("\n交换区块状态\n");
    // 遍历交换区块信息数组，输出每个交换区块的状态
    for (uint32_t i = 0; i < SWAP_BLOCKS; i++) {
        if (vm_manager.swap_blocks[i].is_used) {
            printf(" %u: PID=%u, 虚拟页号=%u\n",
                   i,
                   vm_manager.swap_blocks[i].process_id,
                   vm_manager.swap_blocks[i].virtual_page);
        }
    }
}

/**
 * @brief 获取最后一个被置换的页面
 * 
 * @return uint32_t 最后一个被置换的页面
 */
uint32_t get_last_replaced_page(void) {
    return vm_manager.stats.last_replaced_page;
}

/**
 * @brief 更新页面置换统计信息
 * 
 * @param page_num 被置换的页面
 */
void update_replacement_stats(uint32_t page_num) {
    vm_manager.stats.page_replacements++; // 增加页面置换次数
    vm_manager.stats.last_replaced_page = page_num; // 设置最后一个被置换的页面
    printf("页面 %u 被置换，当前页面置换次数: %u\n", page_num, vm_manager.stats.page_replacements);
}

/**
 * @brief 将页面写入交换区
 * 
 * @param frame 要写入页面的页框号
 * @return true 写入成功
 * @return false 写入失败
 */
bool swap_out_page(uint32_t frame) {
    if (frame >= PHYSICAL_PAGES || !memory_manager.frames[frame].is_allocated) {
        printf("错误：页框号 %u 无效\n", frame);
        return false;
    }

    FrameInfo* frame_info = &memory_manager.frames[frame];
    PCB* process = get_process_by_pid(frame_info->process_id);
    if (!process) {
        printf("错误：找不到进程 %u\n", frame_info->process_id);
        return false;
    }

    uint32_t virtual_page = frame_info->virtual_page_num;
    if (virtual_page >= process->page_table_size) {
        printf("错误：虚拟页号 %u 超出页表范围 %u\n", virtual_page, process->page_table_size);
        return false;
    }

    // 分配一个空闲交换区块
    uint32_t swap_index = allocate_swap_block(process->pid, virtual_page);
    if (swap_index == (uint32_t)-1) {
        printf("错误：无法分配交换区块\n");
        return false;
    }

    // 将页面数据写入交换区
    void* page_data = get_physical_memory() + (frame * PAGE_SIZE);
    if (!write_to_swap(swap_index, page_data)) {
        printf("错误：写入交换区失败\n");
        free_swap_block(swap_index);
        return false;
    }

    // 更新页表项
    PageTableEntry* pte = &process->page_table[virtual_page];
    pte->flags.present = false;
    pte->flags.swapped = true;
    pte->flags.swap_index = swap_index;

    // 如果页面被修改过，需要写入磁盘
    if (frame_info->is_dirty) {
        vm_manager.stats.disk_writes++;
    }
    vm_manager.stats.pages_swapped_out++;

    // 更新页框信息
    frame_info->is_allocated = false;
    frame_info->process_id = 0;
    frame_info->virtual_page_num = 0;
    frame_info->is_dirty = false;
    memory_manager.free_frames_count++;

    printf("页面 %u 已写入交换区，交换区块索引: %u\n", virtual_page, swap_index);

    return true;
}

/**
 * @brief 获取当前时间
 * 
 * @return uint64_t 当前时间
 */
uint64_t get_current_time(void) {
#ifdef _WIN32
    // Windows系统调用GetSystemTimeAsFileTime
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    // 转换为微秒（Windows时间从1601年1月1日到现在的微秒数）
    return (uli.QuadPart - 116444736000000000ULL) / 10;
#else
    // 非Windows系统调用clock_gettime
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000000) + (ts.tv_nsec / 1000);
#endif
}

/**
 * @brief 输出页面置换统计信息
 */
void print_page_replacement_stats(void) {
    printf("\n=== 页面置换统计信息 ===\n");
    printf("总缺页次数: %u\n", page_stats.total_page_faults);
    printf("页面置换次数: %u\n", page_stats.page_replacements);
    printf("磁盘读取次数: %u\n", page_stats.disk_reads);
    printf("磁盘写入次数: %u\n", page_stats.disk_writes);
    printf("LRU命中率: %.2f%%\n", 
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
