CC = g++
CFLAG = -Wall -Werror -g -lstdc++fs

BDIR = bin
ODIR = build
IDIR = include
SDIR = src

EXEC = $(BDIR)/myweb

_OBJ = client.o main.o parse.o log.o
OBJ = $(patsubst %, $(ODIR)/%, $(_OBJ))

_DEPS = client.h parse.h log.h
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