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
    uint64_t current_time = get_current_time();
    for (uint32_t i = 0; i < total_pages; i++) {
        pages[i].frame_number = (uint32_t)-1;
        pages[i].is_valid = false;
        pages[i].is_locked = false;
        pages[i].is_dirty = false;
        pages[i].last_access = current_time;
        
        // 同时初始化内存管理器中的页框信息
        memory_manager.frames[i].last_access_time = current_time;
        memory_manager.frames[i].is_allocated = false;
        memory_manager.frames[i].is_dirty = false;
        memory_manager.frames[i].process_id = 0;
        memory_manager.frames[i].virtual_page_num = 0;
    }
}

// 分配页框
uint32_t allocate_frame(uint32_t pid, uint32_t virtual_page) {
    // 查找空闲页框
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
            
            return i;
        }
    }
    
    return (uint32_t)-1;
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
    uint64_t oldest_access = 0;  // 初始化为0，这样我们会找到最早的访问时间
    uint64_t current_time = get_current_time();
    
    printf("\n=== LRU页面置换 ===\n");
    
    // 第一次扫描：寻找未修改且最久未使用的页面
    for (uint32_t i = 0; i < PHYSICAL_PAGES; i++) {
        if (!memory_manager.frames[i].is_allocated || 
            memory_manager.frames[i].process_id == 0) {
            continue;
        }
        
        // 获取该页框对应的进程和页表项
        PCB* process = get_process_by_pid(memory_manager.frames[i].process_id);
        if (!process || memory_manager.frames[i].virtual_page_num >= process->page_table_size) {
            continue;
        }
        
        // 检查页表项的present标志
        PageTableEntry* pte = &process->page_table[memory_manager.frames[i].virtual_page_num];
        if (!pte->flags.present) {
            continue;  // 如果页面不在内存中，跳过
        }
        
        uint64_t time_diff = current_time - memory_manager.frames[i].last_access_time;
        if (!memory_manager.frames[i].is_dirty && 
            (victim_frame == (uint32_t)-1 || time_diff > oldest_access)) {
            oldest_access = time_diff;
            victim_frame = i;
        }
    }
    
    // 如果没找到未修改的页面，寻找最久未使用的页面（不考虑是否修改）
    if (victim_frame == (uint32_t)-1) {
        oldest_access = 0;
        
        for (uint32_t i = 0; i < PHYSICAL_PAGES; i++) {
            if (!memory_manager.frames[i].is_allocated || 
                memory_manager.frames[i].process_id == 0) {
                continue;
            }
            
            // 获取该页框对应的进程和页表项
            PCB* process = get_process_by_pid(memory_manager.frames[i].process_id);
            if (!process || memory_manager.frames[i].virtual_page_num >= process->page_table_size) {
                continue;
            }
            
            // 检查页表项的present标志
            PageTableEntry* pte = &process->page_table[memory_manager.frames[i].virtual_page_num];
            if (!pte->flags.present) {
                continue;  // 如果页面不在内存中，跳过
            }
            
            uint64_t time_diff = current_time - memory_manager.frames[i].last_access_time;
            if (victim_frame == (uint32_t)-1 || time_diff > oldest_access) {
                oldest_access = time_diff;
                victim_frame = i;
            }
        }
    }
    
    if (victim_frame != (uint32_t)-1) {
        // 更新统计信息
        vm_manager.stats.page_replacements++;
        if (memory_manager.frames[victim_frame].is_dirty) {
            vm_manager.stats.disk_writes++;
            vm_manager.stats.pages_swapped_out++;
        }
        
        printf("\n选择页框 %u 进行置换 (PID=%u, 页号=0x%04x, 脏=%s)\n", 
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

uint32_t select_victim_page(PCB* process) {
    printf("\n=== 选择牺牲页面 ===\n");
    
    // 遍历所有进程查找可以置换的页面
    for (uint32_t pid = 1; pid < scheduler.next_pid; pid++) {
        PCB* current_process = get_process_by_pid(pid);
        if (!current_process || current_process == process) {
            continue;
        }
        
        printf("检查进程 %u 的页面\n", pid);
        
        // 计算进程当前的物理页面比例
        float physical_ratio = calculate_physical_pages_ratio(current_process);
        printf("进程 %u 当前物理页面比例: %.2f%%\n", pid, physical_ratio);
        
        // 如果目标进程物理页面比例过低（小于15%），则跳过
        // 这里设置更低的阈值，以确保在紧急情况下仍然可以进行页面置换
        if (physical_ratio <= 15.0f) {
            printf("进程 %u 的物理页面比例过低，跳过\n", pid);
            continue;
        }
        
        // 查找该进程中最久未使用的页面
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
            printf("在进程 %u 中找到可置换页面：虚拟页 %u -> 物理页框 %u\n",
                   pid, oldest_page, oldest_frame);
            
            // 将页面写入交换区
            if (current_process->page_table[oldest_page].flags.dirty) {
                printf("页面已修改，需要写入交换区\n");
                if (!swap_out_page(oldest_frame)) {
                    printf("写入交换区失败，尝试下一个页面\n");
                    continue;
                }
            }
            
            current_process->page_table[oldest_page].flags.present = false;
            current_process->page_table[oldest_page].flags.swapped = true;
            free_frame(oldest_frame);  // 释放物理页框
            
            printf("页面已处理，释放物理页框 %u\n", oldest_frame);
            return oldest_frame;
        }
    }
    
    // 如果所有进程的物理页面比例都很低，则选择占用内存最多的进程进行置换
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
            printf("所有进程物理页面比例都较低，从占用内存最多的进程 %u 中选择页面\n", 
                   max_pages_process->pid);
            
            // 查找最久未使用的页面
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
                    printf("页面已修改，需要写入交换区\n");
                    if (!swap_out_page(oldest_frame)) {
                        printf("写入交换区失败\n");
                        return (uint32_t)-1;
                    }
                }
                
                max_pages_process->page_table[oldest_page].flags.present = false;
                max_pages_process->page_table[oldest_page].flags.swapped = true;
                free_frame(oldest_frame);
                
                printf("页面已处理，释放物理页框 %u\n", oldest_frame);
                return oldest_frame;
            }
        }
    }
    
    printf("未找到可置换的页面\n");
    return (uint32_t)-1;
}
