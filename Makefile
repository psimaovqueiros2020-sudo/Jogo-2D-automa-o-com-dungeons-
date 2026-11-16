# Makefile da Clareira da Luna
#   make        compila o jogo (precisa da raylib)
#   make test   compila e corre os testes da logica (NAO precisa da raylib)
#   make clean  apaga os executaveis

CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra
SRC     = main.c world.c logic.c render.c items.c menu.c
HDR     = world.h logic.h render.h items.h menu.h

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

# Os testes: tests/test_logica.c so usa world.c, logic.c e items.c; tests/test_menu.c
# usa tambem menu.c e render.c (com cliques FALSOS). Ambos usam a raylib FALSA
# (tests/fake). O -Itests/fake e o -DCLAREIRA_TEST so existem aqui: NUNCA no comando do jogo.
TFLAGS = $(CFLAGS) -Wno-unused-function -DCLAREIRA_TEST -Itests/fake

test: tests/test_logica.c tests/test_menu.c tests/fake/raylib.h world.c logic.c items.c menu.c render.c $(HDR)
	$(CC) $(TFLAGS) tests/test_logica.c world.c logic.c items.c -o test_logica -lm
	./test_logica
	$(CC) $(TFLAGS) tests/test_menu.c menu.c render.c world.c logic.c items.c -o test_menu -lm
	./test_menu

clean:
	rm -f clareira clareira.exe test_logica test_logica.exe test_menu test_menu.exe

.PHONY: test clean
