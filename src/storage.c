#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/storage.h"

static StorageManager storage_manager;

// ��ʼ���洢������
void storage_init(void) {
    // ����ģ����̿ռ�
    storage_manager.disk_space = malloc(STORAGE_SIZE);
    storage_manager.blocks = (StorageBlock*)calloc(MAX_BLOCKS, sizeof(StorageBlock));
    storage_manager.total_blocks = MAX_BLOCKS;
    storage_manager.free_blocks = MAX_BLOCKS;
    storage_manager.block_size = BLOCK_SIZE;

    if (!storage_manager.disk_space || !storage_manager.blocks) {
        fprintf(stderr, "�洢�ռ��ʼ��ʧ�ܣ�\n");
        exit(1);
    }

    // ��ʼ��Ϊһ����Ŀ��п�
    storage_manager.blocks[0].start_block = 0;
    storage_manager.blocks[0].block_count = MAX_BLOCKS;
    storage_manager.blocks[0].is_free = true;
    storage_manager.blocks[0].process_id = 0;
}

// ����洢������
void storage_shutdown(void) {
    free(storage_manager.disk_space);
    free(storage_manager.blocks);
}

// ����洢�ռ䣨�״���Ӧ�㷨��
uint32_t storage_allocate(uint32_t size) {
    uint32_t blocks_needed = (size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    
    if (blocks_needed > storage_manager.free_blocks) {
        return (uint32_t)-1;
    }

    // �����㹻��Ŀ��п�
    for (uint32_t i = 0; i < storage_manager.total_blocks; i++) {
        StorageBlock* block = &storage_manager.blocks[i];
        if (block->is_free && block->block_count >= blocks_needed) {
            // �ҵ����ʵĿ�
            uint32_t start_block = block->start_block;
            
            // ������С���ã�ֱ�ӷ���
            if (block->block_count == blocks_needed) {
                block->is_free = false;
            } else {
                // �ָ��
                StorageBlock new_block = {
                    .start_block = start_block + blocks_needed,
                    .block_count = block->block_count - blocks_needed,
                    .is_free = true,
                    .process_id = 0
                };
                
                block->block_count = blocks_needed;
                block->is_free = false;
                
                // �����µĿ��п�
                memmove(&storage_manager.blocks[i + 2], 
                       &storage_manager.blocks[i + 1],
                       (storage_manager.total_blocks - i - 2) * sizeof(StorageBlock));
                storage_manager.blocks[i + 1] = new_block;
            }
            
            storage_manager.free_blocks -= blocks_needed;
            return start_block;
        }
    }
    
    return (uint32_t)-1;
}

// �ͷŴ洢�ռ�
void storage_free(uint32_t start_block) {
    // ���Ҷ�Ӧ�Ŀ�
    for (uint32_t i = 0; i < storage_manager.total_blocks; i++) {
        StorageBlock* block = &storage_manager.blocks[i];
        if (block->start_block == start_block && !block->is_free) {
            // �ͷſ�
            block->is_free = true;
            block->process_id = 0;
            storage_manager.free_blocks += block->block_count;
            
            // ���Ժϲ����ڵĿ��п�
            // ���һ����ϲ�
            if (i + 1 < storage_manager.total_blocks && 
                storage_manager.blocks[i + 1].is_free) {
                block->block_count += storage_manager.blocks[i + 1].block_count;
                memmove(&storage_manager.blocks[i + 1], 
                       &storage_manager.blocks[i + 2],
                       (storage_manager.total_blocks - i - 2) * sizeof(StorageBlock));
            }
            
            // ��ǰһ����ϲ�
            if (i > 0 && storage_manager.blocks[i - 1].is_free) {
                storage_manager.blocks[i - 1].block_count += block->block_count;
                memmove(&storage_manager.blocks[i], 
                       &storage_manager.blocks[i + 1],
                       (storage_manager.total_blocks - i - 1) * sizeof(StorageBlock));
            }
            
            break;
        }
    }
}

// ��ȡ�洢��
bool storage_read(uint32_t block_num, void* buffer) {
    if (block_num >= storage_manager.total_blocks || !buffer) {
        return false;
    }

    // ���ҿ��Ƿ��ѷ���
    bool block_found = false;
    for (uint32_t i = 0; i < storage_manager.total_blocks; i++) {
        StorageBlock* block = &storage_manager.blocks[i];
        if (!block->is_free && 
            block_num >= block->start_block && 
            block_num < block->start_block + block->block_count) {
            block_found = true;
            break;
        }
    }

    if (!block_found) {
        printf("���󣺳��Զ�ȡδ����Ŀ� %u\n", block_num);
        return false;
    }
    
    uint8_t* src = (uint8_t*)storage_manager.disk_space + (block_num * BLOCK_SIZE);
    memcpy(buffer, src, BLOCK_SIZE);
    return true;
}

// д��洢��
bool storage_write(uint32_t block_num, const void* data) {
    if (block_num >= storage_manager.total_blocks || !data) {
        return false;
    }

    // ���ҿ��Ƿ��ѷ���
    bool block_found = false;
    for (uint32_t i = 0; i < storage_manager.total_blocks; i++) {
        StorageBlock* block = &storage_manager.blocks[i];
        if (!block->is_free && 
            block_num >= block->start_block && 
            block_num < block->start_block + block->block_count) {
            block_found = true;
            break;
        }
    }

    if (!block_found) {
        printf("���󣺳���д��δ����Ŀ� %u\n", block_num);
        return false;
    }
    
    uint8_t* dest = (uint8_t*)storage_manager.disk_space + (block_num * BLOCK_SIZE);
    memcpy(dest, data, BLOCK_SIZE);
    return true;
}

// ��ȡ���п�����
uint32_t get_free_blocks_count(void) {
    return storage_manager.free_blocks;
}

// ��ӡ�洢״̬
void print_storage_status(void) {
    printf("\n=== �洢״̬ ===\n");
    printf("�ܿ���: %u\n", storage_manager.total_blocks);
    printf("���п���: %u\n", storage_manager.free_blocks);
    printf("���С: %u bytes\n", storage_manager.block_size);
    
    printf("\n���б�:\n");
    for (uint32_t i = 0; i < storage_manager.total_blocks; i++) {
        StorageBlock* block = &storage_manager.blocks[i];
        if (block->block_count > 0) {
            printf("�� %u: ��ʼ=%u, ��С=%u blocks, %s\n",
                   i, block->start_block, block->block_count,
                   block->is_free ? "����" : "����");
        }
    }
} 