OPTIMIZATION ?= 0

SRC_DIR = ../../src
INTERFACE_DIR = $(SRC_DIR)/interface
IMPLEMENTATION_DIR = $(SRC_DIR)/implementation/linux

ifeq ($(OPTIMIZATION), 0)
PREPROCESSOR_DEFINITIONS = -D _DEBUG
OUTPUT_DIR = ./debug
else
OUTPUT_DIR = ./release
endif

COMPILER = clang
COMPILER_FLAGS=-c -g -I $(INTERFACE_DIR) -O$(OPTIMIZATION)
COMPILE = $(COMPILER) $(COMPILER_FLAGS) $(PREPROCESSOR_DEFINITIONS)

LINKER = $(COMPILER)
LINKER_FLAGS = -L $(LOCAL_LIB) -L $(VULKAN_LIBRARY) \
	-lglfw3 -lvulkan -ldl -lpthread -lm -lGL \
	-lX11 -lXxf86vm -lXrandr -lXinerama -lXcursor

default: all

all: make_output_dir compile_shaders $(OUTPUT_DIR)/zGame

make_output_dir: $(OUTPUT_DIR)
	mkdir -p $(OUTPUT_DIR)

compile_shaders: $(OUTPUT_DIR)/vertex.spv $(OUTPUT_DIR)/compute.spv $(OUTPUT_DIR)/fragment.spv

$(OUTPUT_DIR)/zGame: \
	$(OUTPUT_DIR)/math3d.o \
	$(OUTPUT_DIR)/system_bridge.o \
	$(OUTPUT_DIR)/main.o
	$(LINKER) \
		$(OUTPUT_DIR)/math3d.o \
		$(OUTPUT_DIR)/system_bridge.o \
		$(OUTPUT_DIR)/main.o \
		$(LINKER_FLAGS) -o $@

$(OUTPUT_DIR)/vertex.spv: $(SRC_DIR)/shaders/shader.vert
	glslangValidator -V $< -o $@

$(OUTPUT_DIR)/compute.spv: $(SRC_DIR)/shaders/shader.comp
	glslangValidator -V $< -o $@

$(OUTPUT_DIR)/fragment.spv: $(SRC_DIR)/shaders/shader.frag
	glslangValidator -V $< -o $@

$(OUTPUT_DIR)/math3d.o: $(IMPLEMENTATION_DIR)/math3d.c $(INTERFACE_DIR)/math3d.h
	$(COMPILE) $< -o $@

$(INTERFACE_DIR)/system_bridge.h: $(INTERFACE_DIR)/math3d.h
	touch $@

$(OUTPUT_DIR)/system_bridge.o: $(IMPLEMENTATION_DIR)/system_bridge.c $(INTERFACE_DIR)/system_bridge.h
	$(COMPILE) $< -o $@

$(OUTPUT_DIR)/main.o: $(SRC_DIR)/main.c $(INTERFACE_DIR)/system_bridge.h
	$(COMPILE) $< -o $@
