# SPDX-License-Identifier: MIT
NAME    = mdma
CC      = g++
PROF    = -O3
C_FLAGS = -std=c++20 -Wall -Wextra -pedantic-errors -Wconversion -fmax-errors=5\
          -Wno-unused-parameter $(PROF)
L_FLAGS = -lm -lstdc++ -lmd4c-html -ltidy -ltinyxml2 -luriparser -lImlib2\
          -lb64 -lcurl $(PROF)
OBJ_DIR = obj
DEFINES = -DMDMA_FRAMEWORK="$(shell xxd -i ../framework.html | \
          xargs printf '%s' | cut -d '{' -f2- | cut -d '}' -f1)"

SRC_FILES := $(wildcard *.cpp)
O_FILES   := $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(SRC_FILES))

OUT = ../$(NAME)

all:
	@$(MAKE) make_dynamic -s

debug:
	@$(MAKE) make_debug -s

make_dynamic: $(O_FILES)
	@printf "\033[1;33mMaking \033[37m   ...."
	$(CC) -o $(OUT) $(O_FILES) $(L_FLAGS)
	@printf "\033[1;32m %s DONE!\033[0m\n" $(NAME)

make_debug: PROF = -O0 -g -rdynamic
make_debug: DEFINES := $(DEFINES) -DMDMA_DEBUG
make_debug: $(O_FILES)
	@printf "\033[1;33mMaking \033[37m   ...."
	$(CC) -o $(OUT) $(O_FILES) $(L_FLAGS)
	@printf "\033[1;32m DEBUG %s DONE!\033[0m\n" $(NAME)

PRINT_FMT1 = "\033[1m\033[31mCompiling \033[37m....\033[34m %-20s"
PRINT_FMT2 = "\t\033[33m%6s\033[31m lines\033[0m \n"
PRINT_FMT  = $(PRINT_FMT1)$(PRINT_FMT2)

$(OBJ_DIR)/%.o: %.cpp
	@printf $(PRINT_FMT) $*.cpp "`wc -l $*.cpp | cut -f1 -d' '`"
	@$(CC) $< $(C_FLAGS) $(DEFINES) -c -o $@

clean:
	@printf "\033[1;36mCleaning \033[37m ...."
	@rm -f $(O_FILES) $(OUT)
	@printf "\033[1;37m $(NAME) cleaned!\033[0m\n"
