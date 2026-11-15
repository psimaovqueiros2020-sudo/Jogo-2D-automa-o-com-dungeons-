# Makefile da Clareira da Luna
#   make        compila o jogo (precisa da raylib)
#   make test   compila e corre os testes da logica (NAO precisa da raylib)
#   make clean  apaga os executaveis

CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra
SRC     = main.c world.c logic.c render.c
HDR     = world.h logic.h render.h

ifeq ($(OS),Windows_NT)
    TARGET = clareira.exe
    LIBS   = -lraylib -lopengl32 -lgdi32 -lwinmm
else
    TARGET = clareira
    ifeq ($(shell uname -s),Darwin)
        LIBS = -lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
    else
        LIBS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
    endif
endif

$(TARGET): $(SRC) $(HDR)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LIBS)

# Os testes so usam world.c e logic.c e a raylib FALSA (tests/fake).
# O -Itests/fake so existe aqui: nunca no comando do jogo.
test: tests/test_logica.c world.c logic.c $(HDR)
	$(CC) $(CFLAGS) -Wno-unused-function -Itests/fake tests/test_logica.c world.c logic.c -o test_logica -lm
	./test_logica

clean:
	rm -f clareira clareira.exe test_logica test_logica.exe

.PHONY: test clean
