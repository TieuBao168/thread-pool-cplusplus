.PHONY: all build directories clean rebuild run help

CXX := g++
CXXFLAGS := -Wall -Wextra -std=c++17 -pthread -O2 -MMD -MP
LDFLAGS := -pthread

SRC_DIR := src
INCLUDE_DIRS := include
BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
DEP_DIR := $(BUILD_DIR)/dep
BIN_DIR := bin
BIN_NAME := thread_pool_binary

CXXFLAGS += $(foreach dir,$(shell find $(INCLUDE_DIRS) -type d),-I$(dir))

SRCS := $(filter-out $(SRC_DIR)/main.cpp, $(shell find $(SRC_DIR) -name "*.cpp"))
MAIN_SRC := $(SRC_DIR)/main.cpp

OBJS := $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(notdir $(SRCS)))
DEPS := $(patsubst %.cpp,$(DEP_DIR)/%.d,$(notdir $(SRCS)))
MAIN_OBJ := $(OBJ_DIR)/main.o
MAIN_DEP := $(DEP_DIR)/main.d

TARGET := $(BIN_DIR)/$(BIN_NAME)

all: directories $(TARGET)

directories:
	mkdir -p $(OBJ_DIR) $(DEP_DIR) $(BIN_DIR)

$(OBJ_DIR)/%.o: $(SRC_DIR)/*/%.cpp | directories
	$(CXX) $(CXXFLAGS) -c $< -o $@
	@mv $(@:.o=.d) $(DEP_DIR)/$(notdir $(@:.o=.d))

$(MAIN_OBJ): $(MAIN_SRC) | directories
	$(CXX) $(CXXFLAGS) -c $< -o $@
	@mv $(@:.o=.d) $(MAIN_DEP)

$(TARGET): $(OBJS) $(MAIN_OBJ)
	@echo "Linking..."
	@$(CXX) $(OBJS) $(MAIN_OBJ) $(LDFLAGS) -o $@ && \
	 (echo ":> Build complete! -> $(TARGET)" && exit 0) || \
	 (echo ":< Build failed!" && exit 1)

-include $(DEPS) $(MAIN_DEP)

clean:
	find $(OBJ_DIR) -type f ! -name '.gitkeep' -delete
	find $(DEP_DIR) -type f ! -name '.gitkeep' -delete
	rm -f $(TARGET)

rebuild: clean all

run: all
	./$(TARGET)

help:
	@echo "Usage:"
	@echo "  make all       - Build the project"
	@echo "  make clean     - Remove build files (preserve .gitkeep)"
	@echo "  make rebuild   - Clean and rebuild"
	@echo "  make run       - Run the executable"
