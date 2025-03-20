.PHONY: all clean rebuild run directories

# Compiler and flags
CXX := g++
CXXFLAGS := -Wall -Wextra -std=c++11 -pthread -O2
LDFLAGS := -pthread

# Folders
INCLUDE_DIR := include
SRC_DIR := src
OBJ_DIR := obj
BUILD_DIR := build
BIN_NAME := thread_pool_binary

# Source files
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRCS))

# Target executable
TARGET := $(BUILD_DIR)/thread_pool_binary

# Default target
all: directories $(TARGET)

# Create directories
directories:
	mkdir -p $(BUILD_DIR) $(OBJ_DIR)

# Compilation to object
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -I$(INCLUDE_DIR) -c $< -o $@

# Link to target
$(TARGET): $(OBJS)
	$(CXX) $(OBJS) $(LDFLAGS) -o $@

# Clean
clean:
	rm -rf $(BUILD_DIR)/thread_pool_binary
	rm -rf $(OBJ_DIR)/*.o

# Clean and rebuild
rebuild: clean all

# Run program
run: 
	cd $(BUILD_DIR) && ./$(BIN_NAME)