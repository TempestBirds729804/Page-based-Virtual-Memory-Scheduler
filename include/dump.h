#ifndef DUMP_H
#define DUMP_H

#include <stdio.h>
#include <stdbool.h>
#include "types.h"

// ת���ṹ����
typedef struct {
    // ϵͳ״̬
    uint32_t total_pages;          // ��ҳ����
    uint32_t physical_pages;       // ����ҳ����
    uint32_t swap_size;           // ��������С
    
    // ͳ����Ϣ
    MemoryStats memory_stats;     // �ڴ�ͳ��
    
    // ������Ϣ
    uint32_t process_count;       // ��������
    uint32_t current_pid;         // ��ǰ���н���ID
} SystemStateHeader;

// ����ת����ָ��ӿ�
bool dump_system_state(const char* filename);
bool restore_system_state(const char* filename);

#endif // DUMP_H 