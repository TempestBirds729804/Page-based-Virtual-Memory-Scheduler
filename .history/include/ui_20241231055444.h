#ifndef UI_H
#define UI_H

#include <stdbool.h>
#include "types.h"

// �������ͷ���
typedef enum {
    // ϵͳ������
    CMD_HELP,           // ��ʾ����
    CMD_EXIT,           // �˳�ϵͳ
    CMD_STATUS,         // ��ʾϵͳ״̬
    
    // ���̹���������
    CMD_PROC_CREATE,    // ��������
    CMD_PROC_KILL,      // ��ֹ����
    CMD_PROC_LIST,      // �г�����
    CMD_PROC_INFO,      // ��ʾ��������
    CMD_PROC_PRIO,      // ���ý������ȼ�
    
    // �ڴ����������
    CMD_MEM_ALLOC,      // �����ڴ�
    CMD_MEM_FREE,       // �ͷ��ڴ�
    CMD_MEM_READ,       // ��ȡ�ڴ�
    CMD_MEM_WRITE,      // д���ڴ�
    CMD_MEM_MAP,        // ��ʾ�ڴ�ӳ��
    CMD_MEM_STAT,       // �ڴ�ͳ��
    
    // �����ڴ�������
    CMD_VM_PAGE,        // ҳ�����
    CMD_VM_SWAP,        // ����������
    CMD_VM_STAT,        // �����ڴ�ͳ��
    
    // �洢����������
    CMD_DISK_ALLOC,     // ������̿ռ�
    CMD_DISK_FREE,      // �ͷŴ��̿ռ�
    CMD_DISK_READ,      // ��ȡ����
    CMD_DISK_WRITE,     // д�����
    CMD_DISK_STAT,      // ����״̬
    
    // ϵͳ״̬����������
    CMD_STATE_SAVE,     // ����ϵͳ״̬
    CMD_STATE_LOAD,     // ����ϵͳ״̬
    CMD_STATE_RESET,    // ����ϵͳ״̬
    CMD_DEMO_SCHEDULE,  // ������ʾ����
    CMD_DEMO_MEMORY,    // �ڴ������ʾ
    CMD_UNKNOWN,        // δ֪
    
    // 新增高级命令
    CMD_APP_CREATE,     // 创建应用程序
    CMD_APP_RUN,        // 运行应用程序
    CMD_APP_MONITOR,    // 监控应用程序
    CMD_MEM_PROFILE,    // 内存使用分析
    CMD_MEM_OPTIMIZE,   // 内存布局优化
    CMD_MEM_BALANCE,    // 内存使用平衡
    CMD_TIME_TICK,      // 时间片轮转命令
    // 演示命令
    CMD_DEMO_FULL_LOAD,    // 系统满负载运行演示
    CMD_DEMO_MULTI_PROC,   // 多进程竞争演示
    CMD_DEMO_PAGE_THRASH,  // 页面抖动演示
} CommandType;

// ṹ
typedef struct {
    uint32_t pid;       // ID
    uint32_t addr;      // ַ
    uint32_t size;      // С
    uint32_t flags;     // ־λ
    char* text;         //ı
    uint32_t code_pages;  // 代码段页数
    uint32_t data_pages;  // 数据段页数
} CommandArgs;

// ṹ
typedef struct {
    CommandType type;   // 
    CommandArgs args;   // 
} Command;

//û溯
void ui_init(void);
void ui_run(void);
void ui_shutdown(void);

// 
void handle_command(Command* cmd);
void show_help(void);
void show_detailed_help(CommandType cmd_type);

// 演示函数声明
void demo_full_load(void);
void demo_multi_process(void);
void demo_page_thrash(void);

// UI 函数声明
void print_system_status_graph(void);
void print_segment_ascii(PCB* process, uint32_t start_addr, uint32_t size);

// 内存访问检查
bool check_memory_access(PCB* process, uint32_t addr);

#endif // UI_H 