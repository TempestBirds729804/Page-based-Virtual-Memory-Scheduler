#ifndef STORAGE_H
#define STORAGE_H

#include <stdint.h>
#include <stdbool.h>
#include "types.h"

// 存储配置常量
#define STORAGE_SIZE (64 * 1024 * 1024)  // 64MB 存储空间
#define BLOCK_SIZE (4 * 1024)            // 4KB 块大小
#define MAX_BLOCKS (STORAGE_SIZE / BLOCK_SIZE)

// 存储块状态
typedef struct {
    uint32_t start_block;     // 起始块号
    uint32_t block_count;     // 块数量
    bool is_free;            // 是否空闲
    uint32_t process_id;      // 占用进程ID
} StorageBlock;

// 存储管理器
typedef struct {
    void* disk_space;                    // 模拟磁盘空间
    StorageBlock* blocks;                // 存储块信息数组
    uint32_t total_blocks;               // 总块数
    uint32_t free_blocks;                // 空闲块数
    uint32_t block_size;                 // 块大小
} StorageManager;

// 存储管理接口
void storage_init(void);
void storage_shutdown(void);

// 存储分配与回收
uint32_t storage_allocate(uint32_t size);
void storage_free(uint32_t start_block);

// 数据读写
bool storage_read(uint32_t block_num, void* buffer);
bool storage_write(uint32_t block_num, const void* data);

// 状态查询
uint32_t get_free_blocks_count(void);
void print_storage_status(void);

#endif // STORAGE_H 