CC = gcc
CFLAGS = -pedantic -O2
INCLUDE = include
SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)
BIN = bin/clog

all: $(BIN)

# Link object files to build the binary
$(BIN): $(OBJ)
	@mkdir -p ./bin
	$(CC) $(OBJ) -o $@

# Compile each .c to .o
%.o: %.c
	$(CC) $(CFLAGS) -I$(INCLUDE) -c $< -o $@

clean:
	rm -f $(OBJ) $(BIN)

