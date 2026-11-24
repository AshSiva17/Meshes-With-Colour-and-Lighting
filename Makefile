BUILD_DIR ?= build
TARGET := $(BUILD_DIR)/assignment03

CPPFLAGS += -Iinclude $(shell pkg-config --cflags glfw3 glm)
CXXFLAGS += -std=c++17 -Wall -Wextra
LDLIBS += $(shell pkg-config --libs glfw3)

$(TARGET): $(BUILD_DIR)/assignment03.o $(BUILD_DIR)/glad.o
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILD_DIR)/assignment03.o: assignment03.cpp include/glad/glad.h include/KHR/khrplatform.h | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/glad.o: src/glad.c include/glad/glad.h include/KHR/khrplatform.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p "$@"
