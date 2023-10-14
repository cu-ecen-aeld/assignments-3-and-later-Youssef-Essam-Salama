CC = $(CROSS-COMPILE)gcc

CFLAGS = -Wall -g -I $(INC_DIR)
LFLAGS = 

INC_DIR = .
INC_FILES = $(wildcard $(INC_DIR)/*.h)

SRC_DIR = .
SRC_FILES = $(wildcard $(SRC_DIR)/*.c)

OBJ_DIR = .
OBJ_FILES = $(addprefix $(OBJ_DIR)/,$(notdir $(SRC_FILES:.c=.o)))

OUTPUT_DIR = .
EXECUTABLE = writer


all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJ_FILES)
	$(CC) $(LFLAGS) $^ -o $(OUTPUT_DIR)/$@

$(OBJ_FILES): $(SRC_FILES) $(INC_FILES)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm $(EXECUTABLE) $(OBJ_FILES)
