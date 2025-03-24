# 编译器和编译选项
CC = gcc
CFLAGS = -Wall -Wextra -I.\include

# 目录设置
OBJ_DIR = obj
BIN_DIR = bin
SRC_DIR = src

# 源文件列表
SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SOURCES))
TARGET = $(BIN_DIR)/vm_system.exe

# 默认目标
all: directories $(TARGET)

# 创建必要的目录
directories:
	@if not exist $(OBJ_DIR) mkdir $(OBJ_DIR)
	@if not exist $(BIN_DIR) mkdir $(BIN_DIR)

# 链接目标文件生成可执行文件
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET)

# 编译源文件生成目标文件
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# 清理生成的文件
clean:
	@if exist $(OBJ_DIR) rmdir /s /q $(OBJ_DIR)
	@if exist $(BIN_DIR) rmdir /s /q $(BIN_DIR)

# 运行程序
run: all
	$(TARGET)

# 运行测试
test: all
	$(TARGET) --test

# 伪目标声明
.PHONY: all clean run test directories