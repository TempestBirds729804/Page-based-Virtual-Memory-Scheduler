#ifndef STORAGE_H
#define STORAGE_H

#include <stdint.h>
#include <stdbool.h>
#include "types.h"

// �洢���ó���
#define STORAGE_SIZE (64 * 1024 * 1024)  // 64MB �洢�ռ�
#define BLOCK_SIZE (4 * 1024)            // 4KB ���С
#define MAX_BLOCKS (STORAGE_SIZE / BLOCK_SIZE)

// �洢��״̬
typedef struct {
    uint32_t start_block;     // ��ʼ���
    uint32_t block_count;     // ������
    bool is_free;            // �Ƿ����
    uint32_t process_id;      // ռ�ý���ID
} StorageBlock;

// �洢������
typedef struct {
    void* disk_space;                    // ģ����̿ռ�
    StorageBlock* blocks;                // �洢����Ϣ����
    uint32_t total_blocks;               // �ܿ���
    uint32_t free_blocks;                // ���п���
    uint32_t block_size;                 // ���С
} StorageManager;

// �洢����ӿ�
void storage_init(void);
void storage_shutdown(void);

// �洢���������
uint32_t storage_allocate(uint32_t size);
void storage_free(uint32_t start_block);

// ���ݶ�д
bool storage_read(uint32_t block_num, void* buffer);
bool storage_write(uint32_t block_num, const void* data);

// ״̬��ѯ
uint32_t get_free_blocks_count(void);
void print_storage_status(void);

#endif // STORAGE_H 