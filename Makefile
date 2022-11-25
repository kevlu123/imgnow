CXX=g++
CXX_FLAGS=-O3 -std=c++20
INC=-Iclip -ISDL/include
LIBS=$(CLIP_DIR)/$(CLIP).a -lpthread -lSDL2 -lxcb
TARGET=imgnow
BUILD_DIR=build
SRC=                    \
	tinyfiledialogs.cpp \
	app.cpp             \
	image.cpp           \
	main.cpp            \
	window.cpp
	
CLIP=libclip
CLIP_DIR=$(BUILD_DIR)/$(CLIP)

all: imgnow

imgnow: $(CLIP)
	@echo Building imgnow
	@mkdir -p build/target
	@$(CXX) $(CXX_FLAGS) $(INC) -o $(BUILD_DIR)/target/$(TARGET) $(SRC:%=imgnow/%) $(LIBS)

$(CLIP):
	@echo Building libclip
	@mkdir -p $(CLIP_DIR)
	@cmake -Hclip -B$(CLIP_DIR)
	@cmake --build $(CLIP_DIR)

.PHONY: clean
clean:
	@-rm -rf $(BUILD_DIR)/*
	@-rm -d $(BUILD_DIR)