CC = gcc
CFLAGS = -Wall -g -I./build -I./src
BISON = bison
FLEX = flex

SRC_DIR = src
BUILD_DIR = build

# Object files to generate
OBJS = $(BUILD_DIR)/y.tab.o \
       $(BUILD_DIR)/lex.yy.o \
       $(BUILD_DIR)/ast.o \
       $(BUILD_DIR)/symbol_table.o \
       $(BUILD_DIR)/semantic.o \
       $(BUILD_DIR)/ir.o \
       $(BUILD_DIR)/ir_gen.o \
       $(BUILD_DIR)/ir_opt.o \
       $(BUILD_DIR)/ir_sched.o \
       $(BUILD_DIR)/reg_alloc.o \
       $(BUILD_DIR)/riscv_gen.o
#        $(BUILD_DIR)/ir_opt.o

all: setup parser

# Ensure build directory exists
setup:
	@mkdir -p $(BUILD_DIR)

parser: $(OBJS)
	$(CC) $(CFLAGS) -DYYDEBUG=1 -o $(BUILD_DIR)/parser $(OBJS)

$(BUILD_DIR)/y.tab.c $(BUILD_DIR)/y.tab.h: $(SRC_DIR)/parser.y
	$(BISON) -t -d -o $(BUILD_DIR)/y.tab.c $(SRC_DIR)/parser.y

$(BUILD_DIR)/lex.yy.c: $(SRC_DIR)/lexer.l $(BUILD_DIR)/y.tab.h
	$(FLEX) -o $(BUILD_DIR)/lex.yy.c $(SRC_DIR)/lexer.l

$(BUILD_DIR)/y.tab.o: $(BUILD_DIR)/y.tab.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/lex.yy.o: $(BUILD_DIR)/lex.yy.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) ast.dot ir.txt ir_opt.txt output.s