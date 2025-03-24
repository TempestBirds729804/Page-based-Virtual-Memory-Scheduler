# 页式虚拟内存调度器 | Page-based Virtual Memory Scheduler 🚀

一个用C语言实现的基于页面的虚拟内存调度系统，提供了内存管理、进程调度和虚拟内存管理的功能。

A C language implementation of a page-based virtual memory scheduling system that provides memory management, process scheduling, and virtual memory management capabilities.

## ✨ 功能特性 | Features

- 🔄 虚拟内存管理 | Virtual Memory Management
- 📊 页面置换算法 | Page Replacement Algorithm
- ⚙️ 进程调度 | Process Scheduling
- 💾 内存分配与回收 | Memory Allocation and Reclamation
- 📈 系统状态监控 | System State Monitoring
- 🖥️ 用户友好的命令行界面 | User-friendly Command Line Interface

## 📁 项目结构 | Project Structure

```
.
├── src/           # 源代码文件 | Source Code Files
│   ├── main.c     # 主程序入口 | Main Program Entry
│   ├── memory.c   # 内存管理模块 | Memory Management Module
│   ├── vm.c       # 虚拟内存管理 | Virtual Memory Management
│   ├── process.c  # 进程管理 | Process Management
│   ├── ui.c       # 用户界面 | User Interface
│   ├── storage.c  # 存储管理 | Storage Management
│   └── dump.c     # 系统状态导出 | System State Export
├── include/       # 头文件 | Header Files
├── obj/          # 编译目标文件 | Compiled Object Files
├── bin/          # 可执行文件 | Executable Files
└── Makefile      # 编译配置文件 | Build Configuration File
```

## 🛠️ 编译要求 | Build Requirements

- GCC 编译器 | GCC Compiler
- Make 工具 | Make Tool
- C99 标准支持 | C99 Standard Support

## 🔧 编译和运行 | Build and Run

1. 克隆仓库 | Clone the repository:
```bash
git clone https://github.com/TempestBirds729804/Page-based-Virtual-Memory-Scheduler.git
cd Page-based-Virtual-Memory-Scheduler
```

2. 编译项目 | Build the project:
```bash
make
```

3. 运行程序 | Run the program:
```bash
./bin/vm_scheduler
```

## 📖 使用说明 | Usage Guide

程序启动后，您可以通过命令行界面进行以下操作 | After starting the program, you can perform the following operations through the command line interface:

- ➕ 创建新进程 | Create New Process
- 📊 查看内存使用情况 | View Memory Usage
- 🔄 监控页面置换 | Monitor Page Replacement
- 📈 查看系统状态 | View System Status
- 💾 导出系统状态信息 | Export System State Information


## 👨‍💻 作者 | Author

- TempestBird (HFUT)
