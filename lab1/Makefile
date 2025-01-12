CC = g++
CFLAG = -Wall -Werror -g

BDIR = bin
ODIR = build
IDIR = include
SDIR = src

EXEC = $(BDIR)/main

_OBJ = client.o main.o
OBJ = $(patsubst %, $(ODIR)/%, $(_OBJ))

_DEPS = client.h
DEPS = $(patsubst %, $(IDIR)/%, $(_DEPS))

$(ODIR)/%.o: $(SDIR)/%.cpp $(DEPS) | dir
	$(CC) -c -o $@ $< $(CFLAG)

$(EXEC): $(OBJ) | dir
	$(CC) -o $@ $^ $(CFLAG)

.PHONY: dir
dir:
	@mkdir -p $(BDIR) $(ODIR)

.PHONY: clean
clean:
	rm -f $(ODIR)/*.o $(EXEC) core