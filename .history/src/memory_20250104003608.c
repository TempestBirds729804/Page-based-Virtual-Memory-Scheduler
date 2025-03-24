#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "../include/memory.h"
#include "../include/process.h"
#include "../include/vm.h"

// ����ȫ���ڴ������
MemoryManager memory_manager;  // ȫ�ֱ�������
PhysicalMemory phys_mem;      // �����ڴ�ṹ�������ڴ�ռ��ҳ��λͼ

// ���ļ���ͷ����ȫ�ֱ�������
page_t *pages = NULL;
uint32_t total_pages = 0;

// ��ʼ���ڴ������
void memory_init(void) {
    // ��ʼ���ڴ�������ṹ�壬�������г�Ա����
    memset(&memory_manager, 0, sizeof(MemoryManager));
    memory_manager.free_frames_count = PHYSICAL_PAGES;  // ���ó�ʼ����ҳ������Ϊ��ҳ����
    memory_manager.strategy = FIRST_FIT;               // Ĭ��ʹ���״���Ӧ�㷨

    // ��ʼ�������ڴ棬���������ڴ�ռ��ҳ��λͼ
    phys_mem.memory = (uint8_t*)calloc(PHYSICAL_PAGES * PAGE_SIZE, 1);  // ���������ڴ�ռ䣬��ʼ��Ϊ0
    phys_mem.frame_map = (bool*)calloc(PHYSICAL_PAGES, sizeof(bool));   // ����ҳ��λͼ����ʼ��Ϊfalse�����У�
    phys_mem.total_frames = PHYSICAL_PAGES;                            // ������ҳ����
    phys_mem.free_frames = PHYSICAL_PAGES;                             // ���ó�ʼ����ҳ����

    // ����ڴ�����Ƿ�ɹ�
    if (!phys_mem.memory || !phys_mem.frame_map) {
        fprintf(stderr, "�����ڴ��ʼ��ʧ�ܣ�\n");
        exit(1);  // �ڴ����ʧ�ܣ��˳�����
    }

    // ��ʼ��ҳ������
    total_pages = PHYSICAL_PAGES;  // �������ʵ���ҳ������
    pages = (page_t*)calloc(total_pages, sizeof(page_t));
    if (!pages) {
        fprintf(stderr, "ҳ�������ʼ��ʧ�ܣ�\n");
        exit(1);
    }
    
    // ��ʼ��ÿ��ҳ��
    for (uint32_t i = 0; i < total_pages; i++) {
        pages[i].frame_number = (uint32_t)-1;
        pages[i].is_valid = false;
        pages[i].is_locked = false;
        pages[i].is_dirty = false;
        pages[i].last_access = 0;
    }
}

// ����ҳ��
uint32_t allocate_frame(uint32_t process_id, uint32_t virtual_page_num) {
    // �ȳ����ҿ���ҳ��
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
    
    // ���û�п���ҳ�򣬳���ҳ���û�
    uint32_t victim_frame = select_victim_frame();

    if (victim_frame != (uint32_t)-1) {
        // ��ѡ�е�ҳ�滻����������
        if (swap_out_page(victim_frame)) {
            memory_manager.frames[victim_frame].is_allocated = true;
            memory_manager.frames[victim_frame].process_id = process_id;
            memory_manager.frames[victim_frame].virtual_page_num = virtual_page_num;
            return victim_frame;
        }
    }
    
    return (uint32_t)-1;
}

// �ͷ�ҳ��
void free_frame(uint32_t frame_number) {
    if (frame_number >= PHYSICAL_PAGES) return;
    
    // ͬ����������״̬
    memory_manager.frames[frame_number].is_allocated = false;
    memory_manager.frames[frame_number].is_swapping = false;
    memory_manager.frames[frame_number].process_id = 0;
    memory_manager.frames[frame_number].virtual_page_num = 0;
    memory_manager.free_frames_count++;
    
    phys_mem.frame_map[frame_number] = false;
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

// �����ڴ��д����
bool read_physical_memory(uint32_t frame, uint32_t offset, void* buffer, size_t size) {
    // 检查参数有效性
    if (frame >= PHYSICAL_PAGES || offset + size > PAGE_SIZE || !buffer) {
        return false;  // 无效的参数，返回失败
    }

    // 检查页框是否已被使用
    if (!phys_mem.frame_map[frame]) {
        return false;  // 页框未被使用，返回失败
    }

    update_frame_access_time(frame);  // 更新访问时间
    
    // 计算源地址并执行内存拷贝
    uint8_t* src = phys_mem.memory + (frame * PAGE_SIZE) + offset;
    memcpy(buffer, src, size);
    return true;  // 成功读取，返回 true
}

bool write_physical_memory(uint32_t frame, uint32_t offset, const void* data, size_t size) {
    // 检查参数有效性
    if (frame >= PHYSICAL_PAGES || offset + size > PAGE_SIZE || !data) {
        return false;  // 无效的参数，返回失败
    }

    // 检查页框是否已被使用
    if (!phys_mem.frame_map[frame]) {
        return false;  // 页框未被使用，返回失败
    }

    update_frame_access_time(frame);  // 更新访问时间
    memory_manager.frames[frame].is_dirty = true;  // 标记为已修改
    
    // 计算目标地址并执行内存拷贝
    uint8_t* dst = phys_mem.memory + (frame * PAGE_SIZE) + offset;
    memcpy(dst, data, size);
    return true;  // 成功写入，返回 true
}

// ȡڴָ
uint8_t* get_physical_memory(void) {
    return phys_mem.memory;  // ڴָ
}

// ȡҳӳ
bool* get_frame_map(void) {
    return phys_mem.frame_map;  // ҳλͼָ
}

void update_free_frames_count(uint32_t free_count) {
    phys_mem.free_frames = free_count;  // ڴеĿҳ
}

// ʾڴ沼
void print_memory_map(void) {
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

// ȡǰҳ
uint32_t get_free_frames_count(void) {
    return memory_manager.free_frames_count;
}

// ڴƬ
uint32_t count_memory_fragments(void) {
    uint32_t fragments = 0;
    bool in_fragment = false;
    
    // ҳ
    for (uint32_t i = 0; i < PHYSICAL_PAGES; i++) {
        if (!memory_manager.frames[i].is_allocated) {
            // ҵҳ
            if (!in_fragment) {
                // µƬʼ
                fragments++;
                in_fragment = true;
            }
        } else {
            // ҵѷҳ
            in_fragment = false;
        }
    }
    
    return fragments;
}

/**
 * @brief ȡҳӦַ
 * 
 * @param frame ҳ
 * @return void* ַҳЧ򷵻NULL
 */
void* get_physical_address(uint32_t frame) {
    if (frame >= PHYSICAL_PAGES) {
        return NULL;
    }
    // ַ + ҳ * ҳС
    return (void*)(phys_mem.memory + (frame * PAGE_SIZE));
}

// ڴ״̬һ
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
        
        // ״̬һ
        if (memory_manager.frames[i].is_allocated != phys_mem.frame_map[i]) {
            printf("ҳ %u ״̬һ£=%d, ӳ=%d\n",
                   i, memory_manager.frames[i].is_allocated, phys_mem.frame_map[i]);
        }
    }
    
    if (allocated_count != mapped_count || 
        PHYSICAL_PAGES - allocated_count != memory_manager.free_frames_count) {
        printf("ҳڴһ£=%u, ӳ=%u, ҳм=%u\n",
               allocated_count, mapped_count, memory_manager.free_frames_count);
    }
}

// ҳʱʱ
void update_page_access(page_t *page) {
    if (page) {
        page->last_access = get_current_time();
    }
}

// LRU㷨ܺҳ
page_t* find_lru_victim(void) {
    uint32_t victim_frame = select_victim_frame(); // ʹеvictimѡ
    if (victim_frame == (uint32_t)-1) {
        return NULL;
    }
    
    // ҵӦҳ
    for (uint32_t i = 0; i < total_pages; i++) {
        if (pages[i].frame_number == victim_frame) {
            return &pages[i];
        }
    }
    
    return NULL;
}

// ҳû
int replace_page(page_t *victim, page_t *new_page) {
    if (!victim || !new_page) {
        return -1;
    }
    
    // ȡϢ
    PCB* process = get_process_by_pid(memory_manager.frames[victim->frame_number].process_id);
    if (!process) {
        return -1;
    }
    
    // ʹеҳû
    if (victim->is_dirty) {
        // ʹеpage_out
        if (!page_out(process, memory_manager.frames[victim->frame_number].virtual_page_num)) {
            return -1;
        }
    }
    
    // ҳϢ
    victim->frame_number = new_page->frame_number;
    victim->last_access = get_current_time(); // ʹvm.hжget_current_time
    victim->is_valid = true;
    victim->is_dirty = false;
    
    return 0;
}

void memory_shutdown(void) {
    // ͷڴҳλͼ
    free(phys_mem.memory);
    free(phys_mem.frame_map);
    
    // ͷҳ
    if (pages) {
        free(pages);
        pages = NULL;
    }
}

// 更新页框访问时间
void update_frame_access_time(uint32_t frame_number) {
    if (frame_number < PHYSICAL_PAGES && memory_manager.frames[frame_number].is_allocated) {
        memory_manager.frames[frame_number].last_access_time = get_current_time();
        printf("更新页框 %u 的访问时间为 %llu\n", frame_number, 
               memory_manager.frames[frame_number].last_access_time);
    }
}

uint32_t select_victim_frame(void) {
    uint32_t victim_frame = (uint32_t)-1;
    uint64_t oldest_access = UINT64_MAX;
    
    printf("\n=== 开始LRU页面置换 ===\n");
    printf("查找最久未使用的页框...\n");
    
    // 第一次扫描：寻找未修改的页面
    for (uint32_t i = 0; i < PHYSICAL_PAGES; i++) {
        if (memory_manager.frames[i].is_allocated && 
            !memory_manager.frames[i].is_dirty &&
            memory_manager.frames[i].process_id != 0) {
            if (memory_manager.frames[i].last_access_time < oldest_access) {
                oldest_access = memory_manager.frames[i].last_access_time;
                victim_frame = i;
                printf("找到更旧的未修改页框 %u (访问时间: %llu, 当前时间: %llu)\n", 
                       i, oldest_access, get_current_time());
            }
        }
    }
    
    // 如果没找到未修改的页面，寻找最旧的已修改页面
    if (victim_frame == (uint32_t)-1) {
        oldest_access = UINT64_MAX;
        for (uint32_t i = 0; i < PHYSICAL_PAGES; i++) {
            if (memory_manager.frames[i].is_allocated && 
                memory_manager.frames[i].process_id != 0) {
                if (memory_manager.frames[i].last_access_time < oldest_access) {
                    oldest_access = memory_manager.frames[i].last_access_time;
                    victim_frame = i;
                    printf("找到更旧的已修改页框 %u (访问时间: %llu, 当前时间: %llu)\n", 
                           i, oldest_access, get_current_time());
                }
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
