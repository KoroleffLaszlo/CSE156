CC = g++
CFLAG = -Wall -Werror -g -lstdc++fs

BDIR = bin
ODIR = build
IDIR = include
SDIR = src

# TARGET_S = $(BDIR)/myserver
TARGET_C = $(BDIR)/myclient

# _OBJ_S = server.o myserver.o
# OBJ_S = $(patsubst %, $(ODIR)/%, $(_OBJ_S))

_OBJ_C = client.o c_main.o
OBJ_C = $(patsubst %, $(ODIR)/%, $(_OBJ_C))

# _DEPS_S = server.h
# DEPS_S = $(patsubst %, $(IDIR)/%, $(_DEPS_S))

_DEPS_C = client.h
DEPS_C = $(patsubst %, $(IDIR)/%, $(_DEPS_C))

.DEFAULT_GOAL := all

$(ODIR)/%.o: $(SDIR)/%.cpp | dir
	$(CC) -c -o $@ $< $(CFLAG)

# $(TARGET_S): $(OBJ_S) | dir
# 	$(CC) -o $@ $^ $(CFLAG)

$(TARGET_C): $(OBJ_C) | dir
	$(CC) -o $@ $^ $(CFLAG)

all:  $(TARGET_C) #$(TARGET_S)

.PHONY: all dir clean

dir:
	@mkdir -p $(BDIR) $(ODIR)

clean:
	rm -f $(ODIR)/*.o $(TARGET_C) core