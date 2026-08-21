CC = $(CROSS_COMPILE)gcc

CFLAGS = -Wall -Werror -g -pthread -I $(INC_DIR)
LFLAGS = -pthread

INC_DIR = ./api
INC_FILES = $(wildcard $(INC_DIR)/*.h)

SRC_DIR = ./src
SRC_FILES = $(wildcard $(SRC_DIR)/*.c)

OBJ_DIR = ./obj
OBJ_FILES = $(addprefix $(OBJ_DIR)/,$(notdir $(SRC_FILES:.c=.o)))

OUTPUT_DIR = ./out
EXECUTABLE = aesdsocket

.PHONY: all clean directories

all: directories $(EXECUTABLE)

directories:
	mkdir -p $(OBJ_DIR) $(OUTPUT_DIR)

$(EXECUTABLE): $(OBJ_FILES)
	$(CC) $(LFLAGS) $^ -o $(OUTPUT_DIR)/$@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(INC_FILES) | directories
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_FILES) $(OUTPUT_DIR)/$(EXECUTABLE)
