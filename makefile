.DEFAULT_GOAL := raycaster_sdl
CC = g++
COMPILER_FLAGS = -std=c++11
LINKER_FLAGS = -lSDL2 -lSDL2_image -lSDL2_ttf
SRC=src
OBJ=obj

OBJ_FILES = \
    $(OBJ)/main.o \

$(OBJ)/main.o : $(SRC)/main.cpp
	${CC} $< $(COMPILER_FLAGS) -c $(LINKER_FLAGS) -o $@
	
$(OBJ)/Area.o : $(SRC)/Area.cpp
	${CC} $< $(COMPILER_FLAGS) -c $(LINKER_FLAGS) -o $@

raycaster_sdl : $(OBJ_FILES)
	${CC} ${COMPILER_FLAGS} $^ ${LDFLAGS} $(LINKER_FLAGS) -o raycaster