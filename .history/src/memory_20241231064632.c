#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "../include/memory.h"
#include "../include/process.h"
#include "../include/vm.h"

// 定义全局内存管理器
MemoryManager memory_manager;  // 全局变量定义
PhysicalMemory phys_mem;      // 物理内存结构，包含内存空间和页框位图

// 在文件开头添加全局变量定义
page_t *pages = NULL;
uint32_t total_pages = 0;

// 初始化内存管理器
void memory_init(void) {
    // 初始化内存管理器结构体，清零所有成员变量
    memset(&memory_manager, 0, sizeof(MemoryManager));
    memory_manager.free_frames_count = PHYSICAL_PAGES;  // 设置初始空闲页框数量为总页框数
    memory_manager.strategy = FIRST_FIT;               // 默认使用首次适应算法

    // 初始化物理内存，分配物理内存空间和页框位图
    phys_mem.memory = (uint8_t*)calloc(PHYSICAL_PAGES * PAGE_SIZE, 1);  // 分配物理内存空间，初始化为0
    phys_mem.frame_map = (bool*)calloc(PHYSICAL_PAGES, sizeof(bool));   // 分配页框位图，初始化为false（空闲）
    phys_mem.total_frames = PHYSICAL_PAGES;                            // 设置总页框数
    phys_mem.free_frames = PHYSICAL_PAGES;                             // 设置初始空闲页框数

    // 检查内存分配是否成功
    if (!phys_mem.memory || !phys_mem.frame_map) {
        fprintf(stderr, "物理内存初始化失败！\n");
        exit(1);  // 内存分配失败，退出程序
    }

    // 初始化页面数组
    total_pages = PHYSICAL_PAGES;  // 或其他适当的页面数量
    pages = (page_t*)calloc(total_pages, sizeof(page_t));
    if (!pages) {
        fprintf(stderr, "页面数组初始化失败！\n");
        exit(1);
    }
    
    // 初始化每个页面
    for (uint32_t i = 0; i < total_pages; i++) {
        pages[i].frame_number = (uint32_t)-1;
        pages[i].is_valid = false;
        pages[i].is_locked = false;
        pages[i].is_dirty = false;
        pages[i].last_access = 0;
    }
}

// 分配页框
uint32_t allocate_frame(uint32_t process_id, uint32_t virtual_page_num) {
    // 先尝试找空闲页框
    for (uint32_t i = 0; i < PHYSICAL_PAGES; i++) {
        if (!memory_manager.frames[i].is_allocated) {
            memory_manager.frames[i].is_allocated = true;
            memory_manager.frames[i].process_id = process_id;
            memory_manager.frames[i].virtual_page_num = virtual_page_num;
            memory_manager.frames[i].last_access_time = get_current_time();
            memory_manager.free_frames_count--;
            return i;
        }
    }
    
    // 如果没有空闲页框，尝试页面置换
    uint32_t victim_frame = select_victim_frame();

    if (victim_frame != (uint32_t)-1) {
        // 将选中的页面换出到交换区
        if (swap_out_page(victim_frame)) {
            memory_manager.frames[victim_frame].is_allocated = true;
            memory_manager.frames[victim_frame].process_id = process_id;
            memory_manager.frames[victim_frame].virtual_page_num = virtual_page_num;
            return victim_frame;
        }
    }
    
    return (uint32_t)-1;
}

// 释放页框
void free_frame(uint32_t frame_number) {
    if (frame_number >= PHYSICAL_PAGES) return;
    
    // 同步更新所有状态
    memory_manager.frames[frame_number].is_allocated = false;
    memory_manager.frames[frame_number].is_swapping = false;
    memory_manager.frames[frame_number].process_id = 0;
    memory_manager.frames[frame_number].virtual_page_num = 0;
    memory_manager.free_frames_count++;
    
    phys_mem.frame_map[frame_number] = false;
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

// 物理内存读写操作
bool read_physical_memory(uint32_t frame, uint32_t offset, void* buffer, size_t size) {
    // 检查参数有效性
    if (frame >= PHYSICAL_PAGES || offset + size > PAGE_SIZE || !buffer) {
        return false;  // 无效的参数，返回失败
    }

    // 检查页框是否已被使用
    if (!phys_mem.frame_map[frame]) {
        return false;  // 页框未被使用，返回失败
    }

    // 计算源地址并执行内存拷贝
    uint8_t* src = phys_mem.memory + (frame * PAGE_SIZE) + offset;
    memcpy(buffer, src, size);
    return true;  // 成功读取，返回 true
}

bool write_physical_memory(uint32_t frame, uint32_t offset, const void* data, size_t size) {
    // 检查参数有效性
    if (frame >= PHYSICAL_PAGES || offset + size > PAGE_SIZE || !data) {
        return false;
    }

    // 计算目标地址并执行内存拷贝
    uint8_t* dest = phys_mem.memory + (frame * PAGE_SIZE) + offset;
    memcpy(dest, data, size);
    return true;
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

// 显示内存布局
void print_memory_map(void) {
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
    
    // 内存使用详情
    uint32_t used_frames = 0;
    uint32_t fragments = 0;
    bool in_fragment = false;
    
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
    
    printf("\n内存使用详情：\n");
    printf("总物理内存：%u 字节\n", PHYSICAL_PAGES * PAGE_SIZE);
    printf("已用内存：%u 字节 (%.1f%%)\n", 
           used_frames * PAGE_SIZE,
           (float)(used_frames * 100) / PHYSICAL_PAGES);
    printf("碎片数：%u\n", fragments);
    
    // 添加ASCII图形显示
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

// 获取当前空闲页框数量
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
 * @return void* 返回物理地址，如果页框号无效则返回NULL
 */
void* get_physical_address(uint32_t frame) {
    if (frame >= PHYSICAL_PAGES) {
        return NULL;
    }
    // 计算物理地址：基地址 + 页框号 * 页面大小
    return (void*)(phys_mem.memory + (frame * PAGE_SIZE));
}

// 新增：检查内存管理器状态一致性
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
            printf("警告：页框 %u 状态不一致：分配=%d, 映射=%d\n",
                   i, memory_manager.frames[i].is_allocated, phys_mem.frame_map[i]);
        }
    }
    
    if (allocated_count != mapped_count || 
        PHYSICAL_PAGES - allocated_count != memory_manager.free_frames_count) {
        printf("警告：内存计数不一致：已分配=%u, 已映射=%u, 空闲计数=%u\n",
               allocated_count, mapped_count, memory_manager.free_frames_count);
    }
}

// 在页面访问时更新时间戳
void update_page_access(page_t *page) {
    if (page) {
        page->last_access = get_current_time();
    }
}

// LRU算法查找受害者页面
page_t* find_lru_victim(void) {
    uint32_t victim_frame = select_victim_frame(); // 使用现有的victim选择函数
    if (victim_frame == (uint32_t)-1) {
        return NULL;
    }
    
    // 找到对应的页面
    for (uint32_t i = 0; i < total_pages; i++) {
        if (pages[i].frame_number == victim_frame) {
            return &pages[i];
        }
    }
    
    return NULL;
}

// 页面置换操作
int replace_page(page_t *victim, page_t *new_page) {
    if (!victim || !new_page) {
        return -1;
    }
    
    // 获取进程信息
    PCB* process = get_process_by_pid(memory_manager.frames[victim->frame_number].process_id);
    if (!process) {
        return -1;
    }
    
    // 使用现有的页面置换机制
    if (victim->is_dirty) {
        // 使用现有的page_out函数
        if (!page_out(process, memory_manager.frames[victim->frame_number].virtual_page_num)) {
            return -1;
        }
    }
    
    // 更新页面信息
    victim->frame_number = new_page->frame_number;
    victim->last_access = get_current_time(); // 使用vm.h中定义的get_current_time
    victim->is_valid = true;
    victim->is_dirty = false;
    
    return 0;
}

void memory_shutdown(void) {
    // 释放物理内存和页框位图
    free(phys_mem.memory);
    free(phys_mem.frame_map);
    
    // 释放页面数组
    if (pages) {
        free(pages);
        pages = NULL;
    }
}

uint32_t select_victim_frame(void) {
    uint32_t victim_frame = (uint32_t)-1;
    uint64_t oldest_access = UINT64_MAX;
    
    printf("\n=== 开始LRU页面置换 ===\n");
    printf("查找最久未使用的页框...\n");
    
    for (uint32_t i = 0; i < PHYSICAL_PAGES; i++) {
        if (memory_manager.frames[i].is_allocated) {
            // 检查是否是最旧的访问时间，并且不是当前正在访问的页框
            if (memory_manager.frames[i].last_access_time < oldest_access &&
                memory_manager.frames[i].process_id != 0) {  // 不选择系统页框
                oldest_access = memory_manager.frames[i].last_access_time;
                victim_frame = i;
                printf("找到更旧的页框 %u (访问时间: %llu, 当前时间: %llu)\n", 
                       i, oldest_access, get_current_time());
            }
        }
    }
    
    if (victim_frame != (uint32_t)-1) {
        printf("选择页框 %u 进行置换 (PID=%u, 虚拟页号=0x%04x, 最后访问时间=%llu)\n",
               victim_frame,
               memory_manager.frames[victim_frame].process_id,
               memory_manager.frames[victim_frame].virtual_page_num,
               memory_manager.frames[victim_frame].last_access_time);
        printf("页框访问时间差: %llu\n", 
               get_current_time() - memory_manager.frames[victim_frame].last_access_time);
    }
    
    return victim_frame;
}

// 在访问页框时更新时间戳
void update_frame_access(uint32_t frame_number) {
    if (frame_number < PHYSICAL_PAGES && memory_manager.frames[frame_number].is_allocated) {
        memory_manager.frames[frame_number].last_access_time = get_current_time();
        printf("更新页框 %u 的访问时间为 %llu\n", frame_number, 
               memory_manager.frames[frame_number].last_access_time);
    }
}
