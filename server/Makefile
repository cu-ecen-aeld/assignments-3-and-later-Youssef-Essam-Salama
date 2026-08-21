CC = $(CROSS_COMPILE)gcc

CFLAGS = -Wall -Werror -g -I $(INC_DIR)
LFLAGS = 

INC_DIR = ./api
INC_FILES = $(wildcard $(INC_DIR)/*.h)

SRC_DIR = ./src
SRC_FILES = $(wildcard $(SRC_DIR)/*.c)

OBJ_DIR = ./obj
OBJ_FILES = $(addprefix $(OBJ_DIR)/,$(notdir $(SRC_FILES:.c=.o)))

OUTPUT_DIR = ./out
EXECUTABLE = aesdsocket

.PHONY: clean

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJ_FILES)
	$(CC) $(LFLAGS) $^ -o $(OUTPUT_DIR)/$@

$(OBJ_FILES): $(SRC_FILES) $(INC_FILES)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(EXECUTABLE) $(OBJ_FILES)
