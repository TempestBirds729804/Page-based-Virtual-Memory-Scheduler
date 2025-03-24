#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ��ƽ̨����ʱ����֧��
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#define Sleep(x) usleep((x)*1000)
#endif

#include "../include/ui.h"
#include "../include/process.h"
#include "../include/memory.h"
#include "../include/vm.h"
#include "../include/storage.h"
#include "../include/dump.h"

#define MAX_CMD_LEN 256
#define MAX_ARGS 4

// �����ⲿ����
extern MemoryManager memory_manager;

// �������
static Command parse_command(char* cmd_str) {
    Command cmd = {CMD_UNKNOWN, {0}};  // ��ʼ�������ֶ�Ϊ0
    char* token = strtok(cmd_str, " \n");
    
    if (!token) return cmd;
    
    // ��������
    if (strcmp(token, "help") == 0) {
        cmd.type = CMD_HELP;
    } else if (strcmp(token, "status") == 0) {
        cmd.type = CMD_STATUS;
    } else if (strcmp(token, "exit") == 0) {
        cmd.type = CMD_EXIT;
    } else if (strcmp(token, "proc") == 0) {
        // ���̹�������
        token = strtok(NULL, " \n");
        if (token) {
            if (strcmp(token, "create") == 0) {
                cmd.type = CMD_PROC_CREATE;
                token = strtok(NULL, " \n");
                if (token) cmd.args.pid = (uint32_t)strtoul(token, NULL, 0);
            } else if (strcmp(token, "kill") == 0) {
                cmd.type = CMD_PROC_KILL;
                token = strtok(NULL, " \n");
                if (token) cmd.args.pid = (uint32_t)strtoul(token, NULL, 0);
            } else if (strcmp(token, "list") == 0) {
                cmd.type = CMD_PROC_LIST;
            } else if (strcmp(token, "info") == 0) {
                cmd.type = CMD_PROC_INFO;
                token = strtok(NULL, " \n");
                if (token) cmd.args.pid = (uint32_t)strtoul(token, NULL, 0);
            } else if (strcmp(token, "prio") == 0) {
                cmd.type = CMD_PROC_PRIO;
                token = strtok(NULL, " \n");  // pid
                if (token) cmd.args.pid = (uint32_t)strtoul(token, NULL, 0);
                token = strtok(NULL, " \n");  // priority
                if (token) cmd.args.flags = (uint32_t)strtoul(token, NULL, 0);
            } else if (strcmp(token, "access") == 0) {
                cmd.type = CMD_PROC_ACCESS;
                token = strtok(NULL, " \n");  // pid
                if (token) cmd.args.pid = (uint32_t)strtoul(token, NULL, 0);
                token = strtok(NULL, " \n");  // access_count
                if (token) cmd.args.size = (uint32_t)strtoul(token, NULL, 0);
            } else if (strcmp(token, "alloc") == 0) {
                cmd.type = CMD_PROC_ALLOC;
                token = strtok(NULL, " \n");  // pid
                if (token) cmd.args.pid = (uint32_t)strtoul(token, NULL, 0);
                token = strtok(NULL, " \n");  // size
                if (token) cmd.args.size = (uint32_t)strtoul(token, NULL, 0);
                token = strtok(NULL, " \n");  // flags
                if (token) cmd.args.flags = (uint32_t)strtoul(token, NULL, 0);
            } else if (strcmp(token, "time") == 0) {
                cmd.type = CMD_PROC_TIME;
                token = strtok(NULL, " \n");  // pid
                if (token) cmd.args.pid = (uint32_t)strtoul(token, NULL, 0);
                token = strtok(NULL, " \n");  // time_slice
                if (token) cmd.args.time_slice = (uint32_t)strtoul(token, NULL, 0);
            }
        }
    } else if (strcmp(token, "mem") == 0) {
        // �ڴ��������
        token = strtok(NULL, " \n");
        if (token) {
            if (strcmp(token, "alloc") == 0) {
                cmd.type = CMD_MEM_ALLOC;
                token = strtok(NULL, " \n");  // pid
                if (token) cmd.args.pid = (uint32_t)strtoul(token, NULL, 0);
                token = strtok(NULL, " \n");  // addr
                if (token) cmd.args.addr = (uint32_t)strtoul(token, NULL, 0);
                token = strtok(NULL, " \n");  // size
                if (token) cmd.args.size = (uint32_t)strtoul(token, NULL, 0);
            } else if (strcmp(token, "free") == 0) {
                cmd.type = CMD_MEM_FREE;
                token = strtok(NULL, " \n");  // pid
                if (token) cmd.args.pid = (uint32_t)strtoul(token, NULL, 0);
                token = strtok(NULL, " \n");  // addr
                if (token) cmd.args.addr = (uint32_t)strtoul(token, NULL, 0);
                token = strtok(NULL, " \n");  // size
                if (token) cmd.args.size = (uint32_t)strtoul(token, NULL, 0);
            } else if (strcmp(token, "write") == 0) {
                cmd.type = CMD_MEM_WRITE;
                token = strtok(NULL, " \n");  // pid
                if (token) cmd.args.pid = (uint32_t)strtoul(token, NULL, 0);
                token = strtok(NULL, " \n");  // addr
                if (token) cmd.args.addr = (uint32_t)strtoul(token, NULL, 0);
                token = strtok(NULL, "\"");   // text (in quotes)
                if (token) cmd.args.text = strdup(token);
            } else if (strcmp(token, "read") == 0) {
                cmd.type = CMD_MEM_READ;
                token = strtok(NULL, " \n");  // pid
                if (token) cmd.args.pid = (uint32_t)strtoul(token, NULL, 0);
                token = strtok(NULL, " \n");  // addr
                if (token) cmd.args.addr = (uint32_t)strtoul(token, NULL, 0);
            } else if (strcmp(token, "map") == 0) {
                cmd.type = CMD_MEM_MAP;
                token = strtok(NULL, " \n");  // pid
                if (token) cmd.args.pid = (uint32_t)strtoul(token, NULL, 0);
            } else if (strcmp(token, "stat") == 0) {
                cmd.type = CMD_MEM_STAT;
            } else if (strcmp(token, "profile") == 0) {
                cmd.type = CMD_MEM_PROFILE;
            } else if (strcmp(token, "optimize") == 0) {
                cmd.type = CMD_MEM_OPTIMIZE;
                token = strtok(NULL, " \n");  // pid
                if (token) cmd.args.pid = (uint32_t)strtoul(token, NULL, 0);
            } else if (strcmp(token, "balance") == 0) {
                cmd.type = CMD_MEM_BALANCE;
                token = strtok(NULL, " \n");  // pid
                if (token) cmd.args.pid = (uint32_t)strtoul(token, NULL, 0);
            } else if (strcmp(token, "strategy") == 0) {
                cmd.type = CMD_MEM_STRATEGY;
                token = strtok(NULL, " \n");  // strategy
                if (token) {
                    if (strcmp(token, "first") == 0) cmd.args.flags = FIRST_FIT;
                    else if (strcmp(token, "best") == 0) cmd.args.flags = BEST_FIT;
                    else if (strcmp(token, "worst") == 0) cmd.args.flags = WORST_FIT;
                }
            }
        }
    } else if (strcmp(token, "vm") == 0) {
        // �����ڴ��������
        token = strtok(NULL, " \n");
        if (token) {
            if (strcmp(token, "page") == 0) {
                cmd.type = CMD_VM_PAGE;
                token = strtok(NULL, " \n");  // in/out
                if (token) {
                    cmd.args.flags = (strcmp(token, "in") == 0) ? 1 : 0;
                    token = strtok(NULL, " \n");  // pid
                    if (token) cmd.args.pid = (uint32_t)strtoul(token, NULL, 0);
                    token = strtok(NULL, " \n");  // page
                    if (token) cmd.args.addr = (uint32_t)strtoul(token, NULL, 0);
                }
            } else if (strcmp(token, "swap") == 0) {
                cmd.type = CMD_VM_SWAP;
                token = strtok(NULL, " \n");  // list/clean
                if (token) cmd.args.flags = (strcmp(token, "clean") == 0) ? 1 : 0;
            } else if (strcmp(token, "stat") == 0) {
                cmd.type = CMD_VM_STAT;
            }
        }
    } else if (strcmp(token, "disk") == 0) {
        // ���̹�������
        token = strtok(NULL, " \n");
        if (token) {
            if (strcmp(token, "alloc") == 0) {
                cmd.type = CMD_DISK_ALLOC;
                token = strtok(NULL, " \n");  // size
                if (token) cmd.args.size = (uint32_t)strtoul(token, NULL, 0);
            } else if (strcmp(token, "free") == 0) {
                cmd.type = CMD_DISK_FREE;
                token = strtok(NULL, " \n");  // block
                if (token) cmd.args.addr = (uint32_t)strtoul(token, NULL, 0);
            } else if (strcmp(token, "write") == 0) {
                cmd.type = CMD_DISK_WRITE;
                token = strtok(NULL, " \n");  // block
                if (token) cmd.args.addr = (uint32_t)strtoul(token, NULL, 0);
                token = strtok(NULL, "\"");   // text (in quotes)
                if (token) cmd.args.text = strdup(token);
            } else if (strcmp(token, "read") == 0) {
                cmd.type = CMD_DISK_READ;
                token = strtok(NULL, " \n");  // block
                if (token) cmd.args.addr = (uint32_t)strtoul(token, NULL, 0);
            } else if (strcmp(token, "stat") == 0) {
                cmd.type = CMD_DISK_STAT;
            }
        }
    } else if (strcmp(token, "state") == 0) {
        // ϵͳ״̬��������
        token = strtok(NULL, " \n");
        if (token) {
            if (strcmp(token, "save") == 0) {
                cmd.type = CMD_STATE_SAVE;
                token = strtok(NULL, " \n");  // optional filename
                if (token) cmd.args.text = strdup(token);
            } else if (strcmp(token, "load") == 0) {
                cmd.type = CMD_STATE_LOAD;
                token = strtok(NULL, " \n");  // optional filename
                if (token) cmd.args.text = strdup(token);
            } else if (strcmp(token, "reset") == 0) {
                cmd.type = CMD_STATE_RESET;
            }
        }
    } else if (strcmp(token, "app") == 0) {
        token = strtok(NULL, " \n");
        if (token) {
            if (strcmp(token, "create") == 0) {
                cmd.type = CMD_APP_CREATE;
                // ����PID
                token = strtok(NULL, " \n");
                if (token) cmd.args.pid = (uint32_t)strtoul(token, NULL, 0);
                // �������ȼ�
                token = strtok(NULL, " \n");
                if (token) cmd.args.priority = (uint32_t)strtoul(token, NULL, 0);
                // ���������ҳ��
                token = strtok(NULL, " \n");
                if (token) cmd.args.code_pages = (uint32_t)strtoul(token, NULL, 0);
                // �������ݶ�ҳ��
                token = strtok(NULL, " \n");
                if (token) cmd.args.data_pages = (uint32_t)strtoul(token, NULL, 0);
            } else if (strcmp(token, "run") == 0) {
                cmd.type = CMD_APP_RUN;
                token = strtok(NULL, " \n");
                if (token) cmd.args.pid = (uint32_t)strtoul(token, NULL, 0);
            } else if (strcmp(token, "monitor") == 0) {
                cmd.type = CMD_APP_MONITOR;
                token = strtok(NULL, " \n");
                if (token) cmd.args.pid = (uint32_t)strtoul(token, NULL, 0);
            }
        }
    } else if (strcmp(token, "time") == 0) {
        token = strtok(NULL, " \n");
        if (token) {
            if (strcmp(token, "tick") == 0) {
                cmd.type = CMD_TIME_TICK;
                token = strtok(NULL, " \n");  // ��ѡ��tick����
                if (token) {
                    cmd.args.size = (uint32_t)strtoul(token, NULL, 0);
                } else {
                    cmd.args.size = 1;  // Ĭ��һ��ʱ��Ƭ
                }
            }
        }
    } else if (strncmp(cmd_str, CMD_STR_PROC_ACCESS, strlen(CMD_STR_PROC_ACCESS)) == 0) {
        cmd.type = CMD_PROC_ACCESS;
        sscanf(cmd_str, "proc access %u %u", &cmd.args.pid, &cmd.args.size);
    } else if (strncmp(cmd_str, CMD_STR_PROC_ALLOC, strlen(CMD_STR_PROC_ALLOC)) == 0) {
        cmd.type = CMD_PROC_ALLOC;
        sscanf(cmd_str, "proc alloc %u %u %u", &cmd.args.pid, &cmd.args.size, &cmd.args.flags);
    }
    
    return cmd;
}

void show_help(void) {
    printf("\n=== ϵͳ���� ===\n");
    printf("\n��������\n");
    printf("help [command]          - ��ʾ������Ϣ\n");
    printf("status                  - ��ʾϵͳ״̬\n");
    printf("exit                    - �˳�ϵͳ\n");
    
    printf("\n���̹���\n");
    printf("app create <pid> <prio> <code_pages> <data_pages> - ��������\n");
    printf("proc kill <pid>         - ��ֹ����\n");
    printf("proc list               - ��ʾ�����б�\n");
    printf("proc info <pid>         - ��ʾ������Ϣ\n");
    printf("proc prio <pid> <prio>  - �������ȼ�\n");
    printf("proc access <pid> <count> - ģ������ڴ����\n");
    printf("proc alloc <pid> <size> <type> - �����ڴ�(type:0��/1ջ)\n");
    
    printf("\n�ڴ����\n");
    printf("mem map                 - ��ʾ�ڴ�ӳ��\n");
    printf("mem stat                - ��ʾ�ڴ�ͳ��\n");
    
    printf("\n�����ڴ����\n");
    printf("vm page <in/out> <pid> <page> - ҳ�����\n");
    printf("vm swap <list/clean>    - ����������\n");
    printf("vm stat                 - ��ʾ�����ڴ�ͳ��\n");
    
    printf("\n��ʾ��\n");
    printf("1. �ڴ��С��λΪ�ֽ�\n");
    printf("2. ���ȼ���Χ��0(��) - 2(��)\n");
    printf("3. ҳ��С��4096�ֽ�\n");
    
    printf("proc time <pid> <time>  - ���ý���ʱ��Ƭ\n");
    printf("mem strategy <type>      - �����ڴ�������(first/best/worst)\n");
}

void show_detailed_help(CommandType cmd_type) {
    switch (cmd_type) {
        case CMD_MEM_ALLOC:
            printf("\n�ڴ����������ϸ˵��\n");
            printf("mem alloc <pid> <addr> <size> [flags]\n");
            printf("  pid  - ����ID\n");
            printf("  addr - ��ʼ��ַ����ʽ��0x1000\n");
            printf("  size - ��С���ֽ�����\n");
            printf("  flags - ��ѡ��\n");
            printf("    -r  ֻ��\n");
            printf("    -w  д��\n");
            printf("    -x  ִ��\n");
            printf("ʾ����mem alloc 1 0x1000 4096 -w\n");
            break;
            
        default:
            printf("��Ч����\n");
            break;
    }
}

void ui_init(void) {
    printf("��ӭʹ���ڴ����ϵͳ\n");
    printf("���� 'help' ��ȡ����\n");
}

void ui_run(void) {
    char cmd_str[MAX_CMD_LEN];
    Command cmd;
    
    while (1) {
        printf("\n> ");
        if (!fgets(cmd_str, MAX_CMD_LEN, stdin)) break;
        
        cmd = parse_command(cmd_str);
        if (cmd.type == CMD_EXIT) break;
        
        handle_command(&cmd);
        
        // �ͷŲ���
        if (cmd.args.text) {
            free(cmd.args.text);
            cmd.args.text = NULL;
        }
    }
}

void ui_shutdown(void) {
    printf("ϵͳ�˳�...\n");
}

// �ڴ���ʼ�麯��
bool check_memory_access(PCB* process, uint32_t addr) {
    if (!process) return false;
    
    // ����ҳ��
    uint32_t page_num = addr / PAGE_SIZE;
    
    // �������εķ�Χ
    // �����
    if (page_num >= process->memory_layout.code.start_page && 
        page_num < process->memory_layout.code.start_page + process->memory_layout.code.num_pages) {
        return true;
    }
    
    // ���ݶ�
    if (page_num >= process->memory_layout.data.start_page && 
        page_num < process->memory_layout.data.start_page + process->memory_layout.data.num_pages) {
        return true;
    }
    
    // ����
    if (page_num >= process->memory_layout.heap.start_page && 
        page_num < process->memory_layout.heap.start_page + process->memory_layout.heap.num_pages) {
        return true;
    }
    
    // ջ��
    if (page_num >= process->memory_layout.stack.start_page && 
        page_num < process->memory_layout.stack.start_page + process->memory_layout.stack.num_pages) {
        return true;
    }
    
    return false;
}

// ��ӡ�ڴ��״̬�� ASCII ͼ��
void print_segment_ascii(PCB* process, uint32_t start_page, uint32_t num_pages) {
    for (uint32_t i = 0; i < num_pages; i++) {
        uint32_t page = start_page + i;
        if (page < process->page_table_size) {
            if (process->page_table[page].flags.present) {
                printf("�� ");
            } else if (process->page_table[page].flags.swapped) {
                printf("S ");
            } else {
                printf("�� ");
            }
        }
    }
}

// ��ӡ�ڴ沼��
void print_memory_layout(PCB* process) {
    if (!process) return;
    
    printf("\n=== ���� %u �ڴ沼�� ===\n", process->pid);
    printf("����Σ�ҳ�� %u-%u (%u ҳ)%s\n",
           process->memory_layout.code.start_page,
           process->memory_layout.code.start_page + process->memory_layout.code.num_pages - 1,
           process->memory_layout.code.num_pages,
           process->memory_layout.code.is_allocated ? " [�ѷ���]" : "");
    
    printf("���ݶΣ�ҳ�� %u-%u (%u ҳ)%s\n",
           process->memory_layout.data.start_page,
           process->memory_layout.data.start_page + process->memory_layout.data.num_pages - 1,
           process->memory_layout.data.num_pages,
           process->memory_layout.data.is_allocated ? " [�ѷ���]" : "");
    
    printf("������  ҳ�� %u-%u (%u ҳ)%s\n",
           process->memory_layout.heap.start_page,
           process->memory_layout.heap.start_page + process->memory_layout.heap.num_pages - 1,
           process->memory_layout.heap.num_pages,
           process->memory_layout.heap.is_allocated ? " [�ѷ���]" : "");
    
    printf("ջ����  ҳ�� %u-%u (%u ҳ)%s\n",
           process->memory_layout.stack.start_page,
           process->memory_layout.stack.start_page + process->memory_layout.stack.num_pages - 1,
           process->memory_layout.stack.num_pages,
           process->memory_layout.stack.is_allocated ? " [�ѷ���]" : "");
}

// ϵͳ״̬ͼ����ʾ
void print_system_status_graph(void) {
    printf("\n=== ϵͳ�ڴ�ʹ����� ===\n\n");
    
    // ��ʾͼ��
    printf("ͼ��: ������ ������ ?���� ?δ����\n\n");
    
    // ��ʾ�����ڴ�ʹ�����
    printf("�����ڴ�ʹ��:\n");
    for (int i = 0; i < PHYSICAL_PAGES; i++) {
        if (i % 32 == 0) printf("%3d: ", i);
        if (memory_manager.frames[i].is_allocated)
            printf("��");
        else
            printf("��");
        if ((i + 1) % 32 == 0) printf("\n");
    }
    
    printf("\n=== �����ڴ�ӳ�� ===\n");
    for (uint32_t i = 0; i < scheduler.total_processes; i++) {
        PCB* process = get_process_by_pid(i + 1);
        if (process) {
            printf("\n���� %d (%s):\n", process->pid, process->name);
            
            // ��ʾ�ڴ��ʹ�����
            printf("     0  1  2  3  4  5  6  7  8  9\n");
            printf("���� ");
            print_segment_ascii(process, process->memory_layout.code.start_page,
                              process->memory_layout.code.num_pages);
            printf("\n���� ");
            print_segment_ascii(process, process->memory_layout.data.start_page,
                              process->memory_layout.data.num_pages);
            printf("\n��   ");
            print_segment_ascii(process, process->memory_layout.heap.start_page,
                              process->memory_layout.heap.num_pages);
            printf("\nջ   ");
            print_segment_ascii(process, process->memory_layout.stack.start_page,
                              process->memory_layout.stack.num_pages);
            printf("\n");
        }
    }
}

// �����û�����
void handle_command(Command* cmd) {
    PCB* process;
    uint32_t block;
    
    switch (cmd->type) {
        case CMD_HELP:
            show_help();
            break;
            
        case CMD_STATUS:
            printf("\n=== ϵͳ״̬ ===\n");
            print_scheduler_status();
            print_memory_map();
            print_vm_stats();
            print_storage_status();
            break;
            
        case CMD_PROC_CREATE:
            // Ϊ�½�������Ĭ���ڴ沼��
            MemoryLayout default_layout = {
                .code_start = 0x1000,
                .code_size = PAGE_SIZE,
                .data_start = 0x2000,
                .data_size = PAGE_SIZE,
                .heap_start = 0x3000,
                .heap_size = PAGE_SIZE * 5,  // 5ҳ�ѿռ�
                .stack_start = 0x8000,       // �����������㹻�ռ�
                .stack_size = PAGE_SIZE * 5   // 5ҳջ�ռ�
            };
            
            // ������Ҫ��ҳ���С
            uint32_t page_table_size = (default_layout.stack_start + default_layout.stack_size) / PAGE_SIZE;
            
            process = process_create(cmd->args.pid, page_table_size);
            if (process) {
                printf("���� %u �����ɹ�\n", cmd->args.pid);
                // ����Ĭ���ڴ沼��
                ProcessMemoryLayout default_memory_layout = {
                    .code = {
                        .start_page = 0,
                        .num_pages = 1,
                        .is_allocated = true
                    },
                    .data = {
                        .start_page = 1,
                        .num_pages = 1,
                        .is_allocated = true
                    },
                    .heap = {
                        .start_page = 2,
                        .num_pages = 5,
                        .is_allocated = false
                    },
                    .stack = {
                        .start_page = 7,
                        .num_pages = 5,
                        .is_allocated = false
                    }
                };
                process->memory_layout = default_memory_layout;
            } else {
                printf("���̴���ʧ��\n");
            }
            break;
            
        case CMD_PROC_KILL:
            process = get_process_by_pid(cmd->args.pid);
            if (process) {
                process_destroy(process);
                printf("���� %u ����ֹ\n", cmd->args.pid);
            } else {
                printf("�Ҳ������� %u\n", cmd->args.pid);
            }
            break;
            
        case CMD_PROC_LIST:
            print_scheduler_status();
            break;
            
        case CMD_PROC_INFO:
            process = get_process_by_pid(cmd->args.pid);
            if (process) {
                printf("\n���� %u ��Ϣ��\n", process->pid);
                print_process_status(process->state);
                printf("���ȼ���%d\n", process->priority);
                printf("ʱ��Ƭ��%u\n", process->time_slice);
                print_process_stats(process);
            } else {
                printf("�Ҳ������� %u\n", cmd->args.pid);
            }
            break;
            
        case CMD_PROC_PRIO:
            process = get_process_by_pid(cmd->args.pid);
            if (process) {
                ProcessPriority new_priority = (ProcessPriority)cmd->args.flags;
                if (new_priority >= PRIORITY_HIGH && new_priority <= PRIORITY_LOW) {
                    set_process_priority(process, new_priority);
                    printf("���� %u �����ȼ��Ѹ���Ϊ %d\n", cmd->args.pid, new_priority);
                } else {
                    printf("��Ч�����ȼ�ֵ��ӦΪ 0-2��\n");
                }
            } else {
                printf("�Ҳ������� %u\n", cmd->args.pid);
            }
            break;
            
        case CMD_MEM_ALLOC:
            process = get_process_by_pid(cmd->args.pid);
            if (process) {
                if (process_allocate_memory(process, cmd->args.addr, cmd->args.size)) {
                    printf("�ڴ����ɹ�\n");
                }
            } else {
                printf("�Ҳ������� %u\n", cmd->args.pid);
            }
            break;
            
        case CMD_MEM_FREE:
            process = get_process_by_pid(cmd->args.pid);
            if (process) {
                process_free_memory(process, cmd->args.addr, cmd->args.size);
                printf("�ڴ����ͷ�\n");
            } else {
                printf("�Ҳ������� %u\n", cmd->args.pid);
            }
            break;
            
        case CMD_MEM_MAP:
            print_memory_map();
            break;
            
        case CMD_VM_PAGE:
            process = get_process_by_pid(cmd->args.pid);
            if (process) {
                if (cmd->args.flags) {  // page in
                    if (page_in(process, cmd->args.addr / PAGE_SIZE)) {
                        printf("ҳ�����ɹ�\n");
                    }
                } else {  // page out
                    if (page_out(process, cmd->args.addr / PAGE_SIZE)) {
                        printf("ҳ������ɹ�\n");
                    }
                }
            } else {
                printf("�Ҳ������� %u\n", cmd->args.pid);
            }
            break;
            
        case CMD_VM_SWAP:
            if (cmd->args.flags) {  // clean
                clean_swap_area();
                printf("������������\n");
            } else {  // list
                print_swap_status();
            }
            break;
            
        case CMD_VM_STAT:
            print_vm_stats();
            break;
            
        case CMD_DISK_ALLOC:
            block = storage_allocate(cmd->args.size);
            if (block != (uint32_t)-1) {
                printf("������̿�ɹ���%u\n", block);
            } else {
                printf("���̿ռ����ʧ��\n");
            }
            break;
            
        case CMD_DISK_FREE:
            storage_free(cmd->args.addr);
            printf("���̿� %u ���ͷ�\n", cmd->args.addr);
            break;
            
        case CMD_DISK_STAT:
            print_storage_status();
            break;
            
        case CMD_STATE_SAVE:
            if (dump_system_state(cmd->args.text ? cmd->args.text : "system.dump")) {
                printf("ϵͳ״̬����ɹ�\n");
            } else {
                printf("ϵͳ״̬����ʧ��\n");
            }
            break;
            
        case CMD_STATE_LOAD:
            if (restore_system_state(cmd->args.text ? cmd->args.text : "system.dump")) {
                printf("ϵͳ״̬�ָ��ɹ�\n");
            } else {
                printf("ϵͳ״̬�ָ�ʧ��\n");
            }
            break;
            
        case CMD_STATE_RESET:
            scheduler_shutdown();
            vm_shutdown();
            memory_init();
            vm_init();
            scheduler_init();
            printf("ϵͳ������\n");
            break;
            
        case CMD_APP_CREATE: {
            PCB* process = create_process_with_pid(
                cmd->args.pid,
                cmd->args.priority,
                cmd->args.code_pages ? cmd->args.code_pages : 1,
                cmd->args.data_pages ? cmd->args.data_pages : 1
            );
            
            if (!process) {
                printf("��������ʧ��\n");
                return;
            }
            
            printf("�������̳ɹ���PID=%u\n", process->pid);
            printf("�ڴ沼�֣�\n");
            printf("����Σ�%uҳ\n", process->memory_layout.code.num_pages);
            printf("���ݶΣ�%uҳ\n", process->memory_layout.data.num_pages);
            printf("�ѣ�%uҳ��δ���䣩\n", process->memory_layout.heap.num_pages);
            printf("ջ��%uҳ��δ���䣩\n", process->memory_layout.stack.num_pages);
            break;
        }
            
        case CMD_APP_MONITOR:
            process = get_process_by_pid(cmd->args.pid);
            if (process) {
                monitor_process(process);
            } else {
                printf("�Ҳ������� %u\n", cmd->args.pid);
            }
            break;
            
        case CMD_MEM_PROFILE:
            printf("\n=== ϵͳ�ڴ�ʹ�÷��� ===\n");
            PCB* analyzed[MAX_PROCESSES] = {NULL};  // ��¼�ѷ����Ľ���
            int analyzed_count = 0;
            
            for (int i = 0; i < 3; i++) {
                PCB* current = scheduler.ready_queue[i];
                while (current) {
                    bool already_analyzed = false;
                    for (int j = 0; j < analyzed_count; j++) {
                        if (analyzed[j] == current) {
                            already_analyzed = true;
                            break;
                        }
                    }
                    if (!already_analyzed) {
                        analyze_memory_usage(current);
                        analyzed[analyzed_count++] = current;
                    }
                    current = current->next;
                }
            }
            if (scheduler.running_process && 
                !is_process_analyzed(scheduler.running_process, analyzed, analyzed_count)) {
                analyze_memory_usage(scheduler.running_process);
            }
            break;
            
        case CMD_MEM_OPTIMIZE:
            process = get_process_by_pid(cmd->args.pid);
            if (process) {
                optimize_memory_layout(process);
            } else {
                printf("�Ҳ������� %u\n", cmd->args.pid);
            }
            break;
            
        case CMD_MEM_BALANCE:
            balance_memory_usage();
            break;
            
        case CMD_APP_RUN:
            process = get_process_by_pid(cmd->args.pid);
            if (process) {
                if (process->state == PROCESS_READY) {
                    process->state = PROCESS_RUNNING;
                    set_running_process(process);
                    printf("Ӧ�ó��� PID %u ������\n", cmd->args.pid);
                    printf("ʹ�� 'time tick [n]' ������ģ��ʱ��Ƭ��ת\n");
                } else {
                    printf("Ӧ�ó��� PID %u �޷���������ǰ״̬��%d\n", cmd->args.pid, process->state);
                }
            } else {
                printf("�Ҳ������� %u\n", cmd->args.pid);
            }
            break;
            
        case CMD_TIME_TICK:
            for (uint32_t i = 0; i < cmd->args.size; i++) {
                time_tick();
                printf("\n=== ʱ��Ƭ %u ===\n", i + 1);
                print_scheduler_status();
                Sleep(1000);  // �ӳ�1���Ա�۲�
            }
            break;
            
        case CMD_MEM_WRITE:
            process = get_process_by_pid(cmd->args.pid);
            if (!process) {
                printf("�Ҳ������� %u\n", cmd->args.pid);
                break;
            }
            
            // ����ַ�Ƿ���Ч
            bool address_valid = false;
            if (check_memory_access(process, cmd->args.addr)) {
                address_valid = true;
            }
            
            if (!address_valid) {
                printf("��Ч���ڴ��ַ: 0x%x\n", cmd->args.addr);
                break;
            }
            
            // д���ڴ�
            if (cmd->args.text) {
                size_t len = strlen(cmd->args.text);
                for (size_t i = 0; i < len; i++) {
                    access_memory(process, cmd->args.addr + i, true);  // д�����
                    // ʵ��д������
                    uint32_t frame = process->page_table[cmd->args.addr / PAGE_SIZE].frame_number;
                    uint32_t offset = cmd->args.addr % PAGE_SIZE;
                    uint8_t* phys_addr = (uint8_t*)get_physical_address(frame);
                    if (phys_addr) {
                        phys_addr[offset + i] = cmd->args.text[i];
                    }
                }
                printf("�ɹ�д�� %zu �ֽڵ���ַ 0x%x\n", len, cmd->args.addr);
            }
            break;
            
        case CMD_MEM_READ:
            process = get_process_by_pid(cmd->args.pid);
            if (!process) {
                printf("�Ҳ������� %u\n", cmd->args.pid);
                break;
            }
            
            // ����ַ�Ƿ���Ч
            if (!check_memory_access(process, cmd->args.addr)) {
                printf("��Ч���ڴ��ַ: 0x%x\n", cmd->args.addr);
                break;
            }
            
            // ��ȡ�ڴ�
            access_memory(process, cmd->args.addr, false);  // ��ȡ����
            uint32_t frame = process->page_table[cmd->args.addr / PAGE_SIZE].frame_number;
            uint32_t offset = cmd->args.addr % PAGE_SIZE;
            uint8_t* phys_addr = (uint8_t*)get_physical_address(frame);
            if (phys_addr) {
                printf("��ַ 0x%x ������:\n", cmd->args.addr);
                // ��ʾʮ������
                printf("HEX:  ");
                for (int i = 0; i < 16 && (offset + i) < PAGE_SIZE; i++) {
                    printf("%02x ", phys_addr[offset + i]);
                }
                printf("\n");
                
                // ��ʾASCII
                printf("ASCII: ");
                for (int i = 0; i < 16 && (offset + i) < PAGE_SIZE; i++) {
                    char c = phys_addr[offset + i];
                    // ֻ��ʾ�ɴ�ӡ�ַ���������ʾΪ��
                    printf("%c  ", (c >= 32 && c <= 126) ? c : '.');
                }
                printf("\n");
            }
            break;
            
        case CMD_UNKNOWN:
            printf("δ֪����\n");
            break;
            
        case CMD_PROC_ACCESS:
            simulate_process_memory_access(cmd->args.pid, cmd->args.size);
            break;
            
        case CMD_PROC_ALLOC:
            allocate_process_memory(cmd->args.pid, cmd->args.size, cmd->args.flags);
            break;
            
        case CMD_PROC_TIME:
            process = get_process_by_pid(cmd->args.pid);
            if (process) {
                if (cmd->args.time_slice > 0) {
                    process->time_slice = cmd->args.time_slice;
                    printf("���� %u ��ʱ��Ƭ�Ѹ���Ϊ %u\n", 
                           cmd->args.pid, cmd->args.time_slice);
                } else {
                    printf("����ʱ��Ƭ�������0\n");
                }
            } else {
                printf("�Ҳ������� %u\n", cmd->args.pid);
            }
            break;
            
        case CMD_MEM_STRATEGY:
            switch (cmd->args.flags) {
                case FIRST_FIT:
                    set_allocation_strategy(FIRST_FIT);
                    printf("�ڴ�������������Ϊ�״���Ӧ�㷨\n");
                    break;
                case BEST_FIT:
                    set_allocation_strategy(BEST_FIT);
                    printf("�ڴ�������������Ϊ�����Ӧ�㷨\n");
                    break;
                case WORST_FIT:
                    set_allocation_strategy(WORST_FIT);
                    printf("�ڴ�������������Ϊ�����Ӧ�㷨\n");
                    break;
                default:
                    printf("��Ч���ڴ�������\n");
                    printf("���ò��ԣ�\n");
                    printf("  first - �״���Ӧ�㷨\n");
                    printf("  best  - �����Ӧ�㷨\n");
                    printf("  worst - �����Ӧ�㷨\n");
                    break;
            }
            break;
            
        default:
            printf("����δʵ��\n");
            break;
    }
} 