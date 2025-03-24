#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/memory.h"
#include "../include/process.h"
#include "../include/vm.h"
#include "../include/dump.h"
#include "../include/storage.h"
#include "../include/ui.h"

int main(void) {
    // 正常运行模式
    memory_init();
    vm_init();
    scheduler_init();
    storage_init();
    ui_init();
    
    printf("系统初始化完成\n");
    ui_run();
    
    ui_shutdown();
    storage_shutdown();
    vm_shutdown();
    
    return 0;
} 