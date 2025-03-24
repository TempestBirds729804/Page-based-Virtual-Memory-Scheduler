#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/storage.h"

static StorageManager storage_manager;

// 初始化存储管理器
void storage_init(void) {
    // 分配模拟磁盘空间
    storage_manager.disk_space = malloc(STORAGE_SIZE);
    storage_manager.blocks = (StorageBlock*)calloc(MAX_BLOCKS, sizeof(StorageBlock));
    storage_manager.total_blocks = MAX_BLOCKS;
    storage_manager.free_blocks = MAX_BLOCKS;
    storage_manager.block_size = BLOCK_SIZE;

    if (!storage_manager.disk_space || !storage_manager.blocks) {
        fprintf(stderr, "存储空间初始化失败！\n");
        exit(1);
    }

    // 初始化为一个大的空闲块
    storage_manager.blocks[0].start_block = 0;
    storage_manager.blocks[0].block_count = MAX_BLOCKS;
    storage_manager.blocks[0].is_free = true;
    storage_manager.blocks[0].process_id = 0;
}

// 清理存储管理器
void storage_shutdown(void) {
    free(storage_manager.disk_space);
    free(storage_manager.blocks);
}

// 分配存储空间（首次适应算法）
uint32_t storage_allocate(uint32_t size) {
    uint32_t blocks_needed = (size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    
    if (blocks_needed > storage_manager.free_blocks) {
        return (uint32_t)-1;
    }

    // 查找足够大的空闲块
    for (uint32_t i = 0; i < storage_manager.total_blocks; i++) {
        StorageBlock* block = &storage_manager.blocks[i];
        if (block->is_free && block->block_count >= blocks_needed) {
            // 找到合适的块
            uint32_t start_block = block->start_block;
            
            // 如果块大小正好，直接分配
            if (block->block_count == blocks_needed) {
                block->is_free = false;
            } else {
                // 分割块
                StorageBlock new_block = {
                    .start_block = start_block + blocks_needed,
                    .block_count = block->block_count - blocks_needed,
                    .is_free = true,
                    .process_id = 0
                };
                
                block->block_count = blocks_needed;
                block->is_free = false;
                
                // 插入新的空闲块
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

// 释放存储空间
void storage_free(uint32_t start_block) {
    // 查找对应的块
    for (uint32_t i = 0; i < storage_manager.total_blocks; i++) {
        StorageBlock* block = &storage_manager.blocks[i];
        if (block->start_block == start_block && !block->is_free) {
            // 释放块
            block->is_free = true;
            block->process_id = 0;
            storage_manager.free_blocks += block->block_count;
            
            // 尝试合并相邻的空闲块
            // 与后一个块合并
            if (i + 1 < storage_manager.total_blocks && 
                storage_manager.blocks[i + 1].is_free) {
                block->block_count += storage_manager.blocks[i + 1].block_count;
                memmove(&storage_manager.blocks[i + 1], 
                       &storage_manager.blocks[i + 2],
                       (storage_manager.total_blocks - i - 2) * sizeof(StorageBlock));
            }
            
            // 与前一个块合并
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

// 读取存储块
bool storage_read(uint32_t block_num, void* buffer) {
    if (block_num >= storage_manager.total_blocks || !buffer) {
        return false;
    }

    // 查找块是否已分配
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
        printf("错误：尝试读取未分配的块 %u\n", block_num);
        return false;
    }
    
    uint8_t* src = (uint8_t*)storage_manager.disk_space + (block_num * BLOCK_SIZE);
    memcpy(buffer, src, BLOCK_SIZE);
    return true;
}

// 写入存储块
bool storage_write(uint32_t block_num, const void* data) {
    if (block_num >= storage_manager.total_blocks || !data) {
        return false;
    }

    // 查找块是否已分配
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
        printf("错误：尝试写入未分配的块 %u\n", block_num);
        return false;
    }
    
    uint8_t* dest = (uint8_t*)storage_manager.disk_space + (block_num * BLOCK_SIZE);
    memcpy(dest, data, BLOCK_SIZE);
    return true;
}

// 获取空闲块数量
uint32_t get_free_blocks_count(void) {
    return storage_manager.free_blocks;
}

// 打印存储状态
void print_storage_status(void) {
    printf("\n=== 存储状态 ===\n");
    printf("总块数: %u\n", storage_manager.total_blocks);
    printf("空闲块数: %u\n", storage_manager.free_blocks);
    printf("块大小: %u bytes\n", storage_manager.block_size);
    
    printf("\n块列表:\n");
    for (uint32_t i = 0; i < storage_manager.total_blocks; i++) {
        StorageBlock* block = &storage_manager.blocks[i];
        if (block->block_count > 0) {
            printf("块 %u: 起始=%u, 大小=%u blocks, %s\n",
                   i, block->start_block, block->block_count,
                   block->is_free ? "空闲" : "已用");
        }
    }
} 