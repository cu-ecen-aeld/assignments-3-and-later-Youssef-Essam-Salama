CC ?= $(CROSS_COMPILE)gcc

INC_DIRS = ./api ./cfg
SRC_DIRS = ./src
OBJ_DIR = ./obj
OUTPUT_DIR = .
TARGET = aesdsocket

CFLAGS ?= -Wall -Werror -g -pthread $(addprefix -I,$(INC_DIRS))
LDFLAGS ?= -pthread

INC_FILES = $(wildcard $(INC_DIRS)/*.h)
SRC_FILES = $(wildcard $(SRC_DIRS)/*.c)
OBJ_FILES = $(addprefix $(OBJ_DIR)/,$(notdir $(SRC_FILES:.c=.o)))

.PHONY: all clean directories

all: directories $(TARGET)

directories:
	mkdir -p $(OBJ_DIR) $(OUTPUT_DIR)

$(TARGET): $(OBJ_FILES)
	$(CC) $(LDFLAGS) $^ -o $(OUTPUT_DIR)/$@

$(OBJ_DIR)/%.o: $(SRC_DIRS)/%.c $(INC_FILES) | directories
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_FILES) $(OUTPUT_DIR)/$(TARGET)
