#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "../include/memory.h"
#include "../include/process.h"
#include "../include/vm.h"

// 内存管理器
MemoryManager memory_manager;  // 内存管理器
PhysicalMemory phys_mem;      // 物理内存结构体，包含物理内存和页框位图

// 页表
page_t *pages = NULL;
uint32_t total_pages = 0;

// 初始化内存管理器
void memory_init(void) {
    // 初始化内存管理器结构体，初始化所有成员为0
    memset(&memory_manager, 0, sizeof(MemoryManager));
    memory_manager.free_frames_count = PHYSICAL_PAGES;  // 初始化时物理页框数为物理页框数
    memory_manager.strategy = FIRST_FIT;               // 默认使用FIFO分配策略

    // 初始化物理内存，分配物理内存空间，初始化为0
    phys_mem.memory = (uint8_t*)calloc(PHYSICAL_PAGES * PAGE_SIZE, 1);  // 分配物理内存空间，初始化为0
    phys_mem.frame_map = (bool*)calloc(PHYSICAL_PAGES, sizeof(bool));   // 分配页框位图，初始化为false
    phys_mem.total_frames = PHYSICAL_PAGES;                            // 物理页框数
    phys_mem.free_frames = PHYSICAL_PAGES;                             // 初始化时物理页框数

    // 检查内存初始化是否成功
    if (!phys_mem.memory || !phys_mem.frame_map) {
        fprintf(stderr, "内存初始化失败！\n");
        exit(1);  // 内存初始化失败，退出程序
    }

    // 初始化页表
    total_pages = PHYSICAL_PAGES;  // 实际页框数
    pages = (page_t*)calloc(total_pages, sizeof(page_t));
    if (!pages) {
        fprintf(stderr, "页表初始化失败！\n");
        exit(1);
    }
    
    // 初始化每个页框
    for (uint32_t i = 0; i < total_pages; i++) {
        pages[i].frame_number = (uint32_t)-1;
        pages[i].is_valid = false;
        pages[i].is_locked = false;
        pages[i].is_dirty = false;
        pages[i].last_access = 0;
    }
}

// 分配页框
uint32_t allocate_frame(uint32_t pid, uint32_t virtual_page) {
    printf("尝试为进程 %u 的虚拟页 %u 分配物理页框\n", pid, virtual_page);
    printf("当前空闲页框数：%u\n", memory_manager.free_frames_count);
    
    // 如果空闲页框太少（小于总页框的10%），先尝试页面置换
    if (memory_manager.free_frames_count < PHYSICAL_PAGES / 10) {
        printf("空闲页框数量过低，尝试页面置换...\n");
        uint32_t victim_frame = select_victim_page(NULL);
        if (victim_frame != (uint32_t)-1) {
            printf("成功置换出页框 %u\n", victim_frame);
            // 页框已经在 select_victim_page 中被释放
        }
    }
    
    // 先尝试分配空闲页框
    for (uint32_t i = 0; i < PHYSICAL_PAGES; i++) {
        if (!memory_manager.frames[i].is_allocated) {
            // 更新内存管理器状态
            memory_manager.frames[i].is_allocated = true;
            memory_manager.frames[i].process_id = pid;
            memory_manager.frames[i].virtual_page_num = virtual_page;
            memory_manager.frames[i].last_access_time = get_current_time();
            memory_manager.frames[i].is_dirty = false;
            memory_manager.free_frames_count--;
            
            // 更新物理内存映射
            phys_mem.frame_map[i] = true;
            phys_mem.free_frames--;
            
            printf("成功分配页框 %u\n", i);
            return i;
        }
    }
    
    // 如果没有空闲页框，强制进行页面置换
    printf("没有空闲页框，强制进行页面置换\n");
    uint32_t victim_frame = select_victim_page(NULL);
    if (victim_frame == (uint32_t)-1) {
        printf("错误：无法选择替换页框\n");
        return (uint32_t)-1;
    }
    
    printf("选择页框 %u 进行置换\n", victim_frame);
    
    // 获取被置换页面的信息
    PCB* victim_process = get_process_by_pid(memory_manager.frames[victim_frame].process_id);
    if (!victim_process) {
        printf("错误：找不到被置换页面所属的进程\n");
        return (uint32_t)-1;
    }
    
    uint32_t victim_page = memory_manager.frames[victim_frame].virtual_page_num;
    PageTableEntry* victim_pte = &victim_process->page_table[victim_page];
    
    // 如果页面被修改过，需要写入交换区
    if (memory_manager.frames[victim_frame].is_dirty) {
        if (!swap_out_page(victim_frame)) {
            printf("错误：页面换出失败\n");
            return (uint32_t)-1;
        }
        victim_pte->flags.swapped = true;
        printf("页面已写入交换区\n");
    }
    
    // 更新被置换页面的页表项
    victim_pte->flags.present = false;
    victim_pte->frame_number = (uint32_t)-1;
    
    // 分配页框给新页面
    memory_manager.frames[victim_frame].process_id = pid;
    memory_manager.frames[victim_frame].virtual_page_num = virtual_page;
    memory_manager.frames[victim_frame].last_access_time = get_current_time();
    memory_manager.frames[victim_frame].is_dirty = false;
    memory_manager.frames[victim_frame].is_allocated = true;
    
    printf("页框 %u 已分配给进程 %u 的虚拟页 %u\n", victim_frame, pid, virtual_page);
    return victim_frame;
}

// 释放页框
void free_frame(uint32_t frame_number) {
    if (frame_number >= PHYSICAL_PAGES) return;
    
    // 1. 先检查页框是否已经被释放
    if (!memory_manager.frames[frame_number].is_allocated) {
        return;
    }
    
    // 2. 更新页框状态
    memory_manager.frames[frame_number].is_allocated = false;
    memory_manager.frames[frame_number].is_swapping = false;
    memory_manager.frames[frame_number].is_dirty = false;
    memory_manager.frames[frame_number].process_id = 0;
    memory_manager.frames[frame_number].virtual_page_num = 0;
    memory_manager.frames[frame_number].last_access_time = 0;
    
    // 3. 更新物理内存映射
    phys_mem.frame_map[frame_number] = false;
    
    // 4. 更新计数器
    memory_manager.free_frames_count++;
    phys_mem.free_frames++;
}

// 检查页框是否已分配
bool is_frame_allocated(uint32_t frame_number) {
    if (frame_number >= PHYSICAL_PAGES) {
        return false;
    }
    return memory_manager.frames[frame_number].is_allocated;
}

// 设置和获取分配策略
void set_allocation_strategy(AllocationStrategy strategy) {
    memory_manager.strategy = strategy;  // 设置内存分配策略
}

AllocationStrategy get_allocation_strategy(void) {
    return memory_manager.strategy;
}

// 获取物理内存指针
uint8_t* get_physical_memory(void) {
    return phys_mem.memory;  // 返回物理内存指针
}

// 获取页框映射
bool* get_frame_map(void) {
    return phys_mem.frame_map;  // 返回页框位图指针
}

void update_free_frames_count(uint32_t free_count) {
    phys_mem.free_frames = free_count;  // 更新物理内存中的空闲页框数
}

// 打印内存布局
void print_memory_map(void) {
    // 统计已使用的页框和内存碎片
    uint32_t used_frames = 0;
    uint32_t fragments = 0;
    bool in_fragment = false;
    
    // 计算已使用页框数和碎片数
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

    printf("\n=== 物理内存布局 ===\n");
    printf("总页框数：%u\n\n", PHYSICAL_PAGES);
    
    // 显示已分配页框列表
    printf("已分配页框列表：\n");
    for (uint32_t i = 0; i < PHYSICAL_PAGES; i++) {
        if (memory_manager.frames[i].is_allocated) {
            printf("[%3u] PID=%-4u 虚拟页号=0x%04x\n", 
                   i, memory_manager.frames[i].process_id, memory_manager.frames[i].virtual_page_num);
        }
    }
    
    printf("\n内存使用详情：\n");
    printf("总物理内存：%u 字节\n", PHYSICAL_PAGES * PAGE_SIZE);
    printf("已用内存：%u 字节 (%.1f%%)\n", 
           used_frames * PAGE_SIZE,
           (float)(used_frames * 100) / PHYSICAL_PAGES);
    printf("碎片数：%u\n", fragments);
    
    // ASCII图形显示
    printf("\n内存使用图示：\n");
    printf("（■表示已用，□表示空闲）\n");
    for (uint32_t i = 0; i < PHYSICAL_PAGES; i++) {
        if (i % 32 == 0) {
            printf("\n%3u: ", i);
        }
        printf("%s", memory_manager.frames[i].is_allocated ? "■" : "□");
        if ((i + 1) % 8 == 0) {
            printf(" ");
        }
    }
    printf("\n");
}

// 获取空闲页框数量
uint32_t get_free_frames_count(void) {
    return memory_manager.free_frames_count;
}

// 计算内存碎片数量
uint32_t count_memory_fragments(void) {
    uint32_t fragments = 0;
    bool in_fragment = false;
    
    // 遍历所有页框
    for (uint32_t i = 0; i < PHYSICAL_PAGES; i++) {
        if (!memory_manager.frames[i].is_allocated) {
            // 找到空闲页框
            if (!in_fragment) {
                // 新的碎片开始
                fragments++;
                in_fragment = true;
            }
        } else {
            // 找到已分配页框
            in_fragment = false;
        }
    }
    
    return fragments;
}

/**
 * @brief 获取页框对应的物理地址
 * 
 * @param frame 页框号
 * @return void* 返回物理地址，如果页框无效则返回NULL
 */
void* get_physical_address(uint32_t frame) {
    if (frame >= PHYSICAL_PAGES) {
        return NULL;
    }
    // 返回物理地址 = 基地址 + 页框号 * 页面大小
    return (void*)(phys_mem.memory + (frame * PAGE_SIZE));
}

// 检查内存管理器状态一致性
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
        
        // 检查状态一致性
        if (memory_manager.frames[i].is_allocated != phys_mem.frame_map[i]) {
            printf("警告：页框 %u 状态不一致，分配=%d, 映射=%d\n",
                   i, memory_manager.frames[i].is_allocated, phys_mem.frame_map[i]);
        }
    }
    
    if (allocated_count != mapped_count || 
        PHYSICAL_PAGES - allocated_count != memory_manager.free_frames_count) {
        printf("警告：内存管理不一致，已分配=%u, 已映射=%u, 空闲计数=%u\n",
               allocated_count, mapped_count, memory_manager.free_frames_count);
    }
}

// 更新页面访问时间戳
void update_page_access(page_t *page) {
    if (page) {
        page->last_access = get_current_time();
    }
}

// LRU算法选择牺牲页框
page_t* find_lru_victim(void) {
    uint32_t victim_frame = select_victim_frame(); // 使用select_victim_frame选择牺牲页框
    if (victim_frame == (uint32_t)-1) {
        return NULL;
    }
    
    // 找到对应的页框
    for (uint32_t i = 0; i < total_pages; i++) {
        if (pages[i].frame_number == victim_frame) {
            return &pages[i];
        }
    }
    
    return NULL;
}

// 替换页框
int replace_page(page_t *victim, page_t *new_page) {
    if (!victim || !new_page) {
        return -1;
    }
    
    // 获取进程控制块
    PCB* process = get_process_by_pid(memory_manager.frames[victim->frame_number].process_id);
    if (!process) {
        return -1;
    }
    
    // 使用页框
    if (victim->is_dirty) {
        // 使用page_out
        if (!page_out(process, memory_manager.frames[victim->frame_number].virtual_page_num)) {
            return -1;
        }
    }
    
    // 更新页框信息
    victim->frame_number = new_page->frame_number;
    victim->last_access = get_current_time(); // 使用vm.h的get_current_time
    victim->is_valid = true;
    victim->is_dirty = false;
    
    return 0;
}

void memory_shutdown(void) {
    // 释放物理内存和页框位图
    free(phys_mem.memory);
    free(phys_mem.frame_map);
    
    // 释放页表
    if (pages) {
        free(pages);
        pages = NULL;
    }
}

// 更新页框访问时间
void update_frame_access_time(uint32_t frame_number) {
    if (frame_number < PHYSICAL_PAGES && memory_manager.frames[frame_number].is_allocated) {
        uint64_t old_time = memory_manager.frames[frame_number].last_access_time;
        memory_manager.frames[frame_number].last_access_time = get_current_time();
        printf("更新页框 %u 的访问时间：%llu -> %llu\n", 
               frame_number, 
               old_time,
               memory_manager.frames[frame_number].last_access_time);
    }
}

uint32_t select_victim_frame(void) {
    uint32_t victim_frame = (uint32_t)-1;
    uint64_t oldest_access = UINT64_MAX;
    uint64_t current_time = get_current_time();
    
    printf("\n=== LRU页面置换 ===\n");
    
    // 第一次扫描：寻找未修改且最久未使用的页面
    for (uint32_t i = 0; i < PHYSICAL_PAGES; i++) {
        if (!memory_manager.frames[i].is_allocated || 
            memory_manager.frames[i].process_id == 0) {
            continue;
        }
        
        uint64_t time_diff = current_time - memory_manager.frames[i].last_access_time;
        if (!memory_manager.frames[i].is_dirty && time_diff > oldest_access) {
            oldest_access = time_diff;
            victim_frame = i;
        }
    }
    
    // 如果没找到未修改的页面，寻找最久未使用的页面（不考虑是否修改）
    if (victim_frame == (uint32_t)-1) {
        oldest_access = 0;
        printf("没有找到未修改页面，寻找最久未使用的页面...\n");
        
        for (uint32_t i = 0; i < PHYSICAL_PAGES; i++) {
            if (!memory_manager.frames[i].is_allocated || 
                memory_manager.frames[i].process_id == 0) {
                continue;
            }
            
            uint64_t time_diff = current_time - memory_manager.frames[i].last_access_time;
            if (time_diff > oldest_access) {
                oldest_access = time_diff;
                victim_frame = i;
            }
        }
    }
    
    if (victim_frame != (uint32_t)-1) {
        printf("选择页框 %u 进行置换 (PID=%u, 页号=0x%04x, 脏=%s)\n", 
               victim_frame,
               memory_manager.frames[victim_frame].process_id,
               memory_manager.frames[victim_frame].virtual_page_num,
               memory_manager.frames[victim_frame].is_dirty ? "是" : "否");
    } else {
        printf("错误：无法找到合适的页框进行置换\n");
    }
    
    return victim_frame;
}

// 从物理内存中读取
bool read_physical_memory(uint32_t frame, uint32_t offset, void* buffer, size_t size) {
    // 检查参数有效性
    if (frame >= PHYSICAL_PAGES || offset + size > PAGE_SIZE || !buffer) {
        return false;  // 无效的参数，返回失败
    }

    // 检查页框是否已被使用
    if (!phys_mem.frame_map[frame]) {
        return false;  // 页框未被使用，返回失败
    }

    // 更新访问时间
    memory_manager.frames[frame].last_access_time = get_current_time();
    
    // 计算源地址并执行内存拷贝
    uint8_t* src = phys_mem.memory + (frame * PAGE_SIZE) + offset;
    memcpy(buffer, src, size);
    return true;  // 成功读取，返回 true
}

// 写入物理内存
bool write_physical_memory(uint32_t frame, uint32_t offset, const void* data, size_t size) {
    // 检查参数有效性
    if (frame >= PHYSICAL_PAGES || offset + size > PAGE_SIZE || !data) {
        return false;  // 无效的参数，返回失败
    }

    // 检查页框是否已被使用
    if (!phys_mem.frame_map[frame]) {
        return false;  // 页框未被使用，返回失败
    }

    // 更新访问时间和脏标志
    memory_manager.frames[frame].last_access_time = get_current_time();
    memory_manager.frames[frame].is_dirty = true;
    
    // 计算目标地址并执行内存拷贝
    uint8_t* dst = phys_mem.memory + (frame * PAGE_SIZE) + offset;
    memcpy(dst, data, size);
    return true;  // 成功写入，返回 true
}
