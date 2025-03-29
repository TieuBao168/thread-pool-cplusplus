.PHONY: all build directories clean rebuild run

# Compiler and flags
CXX := g++
CXXFLAGS := -Wall -Wextra -std=c++11 -pthread -O2 -MMD -MP
LDFLAGS := -pthread

# Folders
INCLUDE_DIR := include
SRC_DIR := src
OBJ_DIR := obj
DEP_DIR := dep
BUILD_DIR := build
BIN_NAME := thread_pool_binary

# Detect source files
SRCS := $(filter-out $(SRC_DIR)/main.cpp, $(wildcard $(SRC_DIR)/*.cpp))
MAIN_SRC := $(SRC_DIR)/main.cpp

# Object and dependency files
OBJS := $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))
MAIN_OBJ := $(OBJ_DIR)/main.o
DEPS := $(patsubst $(OBJ_DIR)/%.o, $(DEP_DIR)/%.d, $(OBJS) $(MAIN_OBJ))

# Target executable
TARGET := $(BUILD_DIR)/$(BIN_NAME)

# Default target
all: directories $(TARGET)

# Create necessary directories
directories:
	mkdir -p $(BUILD_DIR) $(OBJ_DIR) $(DEP_DIR)

# Compile object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | directories
	$(CXX) $(CXXFLAGS) -I$(INCLUDE_DIR) -c $< -o $@
	mv $(OBJ_DIR)/$*.d $(DEP_DIR)/$*.d

$(MAIN_OBJ): $(MAIN_SRC) | directories
	$(CXX) $(CXXFLAGS) -I$(INCLUDE_DIR) -c $< -o $@
	mv $(OBJ_DIR)/main.d $(DEP_DIR)/main.d

# Link the final executable
$(TARGET): $(OBJS) $(MAIN_OBJ)
	@echo "Building $(BIN_NAME)..."
	@$(CXX) $(OBJS) $(MAIN_OBJ) $(LDFLAGS) -o $@ && \
	 (echo "Build complete! -> File located at $(BUILD_DIR)/$(BIN_NAME)" && exit 0) || \
	 (echo "Build failed!" && exit 1)

# Include dependencies
-include $(DEPS)

# Clean
clean:
	rm -rf $(BUILD_DIR)/*binary* $(OBJ_DIR)/*.o $(DEP_DIR)/*.d

# Clean and rebuild
rebuild: clean all

# Run program
run: all
	cd $(BUILD_DIR) && ./$(BIN_NAME)

# Help command
help:
	@echo "Usage:"
	@echo "  make all       - Build the project"
	@echo "  make clean     - Remove build files"
	@echo "  make rebuild   - Clean and rebuild"
	@echo "  make run       - Run the executable"