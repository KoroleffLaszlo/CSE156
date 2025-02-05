CC = g++
CFLAG = -Wall -Werror -g -lstdc++fs

BDIR = bin
ODIR = build
IDIR = include
SDIR = src

TARGET_S = $(BDIR)/myserver
# TARGET_C = $(BDIR)/myclient

# universally used dependents 
_OBJ_U = datagram.o # TODO: add respective file_t file read/write wrapper
OBJ_U = $(patsubst %, $(ODIR)/common/%, $(_OBJ_U))
_DEPS_U = datagram.h
DEPS_U = $(patsubst %, $(ODIR)/common/%, $(_DEPS_U))

_OBJ_S = server.o s_main.o conn.o
OBJ_S = $(patsubst %, $(ODIR)/server/%, $(_OBJ_S)) $(OBJ_U)
_DEPS_S = server.h conn.h
DEPS_S = $(patsubst %, $(IDIR)/server/%, $(_DEPS_S))  $(DEPS_U)

_OBJ_C = client.o c_main.o
OBJ_C = $(patsubst %, $(ODIR)/client/%, $(_OBJ_C)) $(DEPS_U)
_DEPS_C = client.h
DEPS_C = $(patsubst %, $(IDIR)/client%, $(_DEPS_C)) $(DEPS_U)

.DEFAULT_GOAL := all

$(ODIR)/common/%.o: $(SDIR)/common/%.cpp | dir
	$(CC) -c -o $@ $< $(CFLAG)

$(ODIR)/server/%.o: $(SDIR)/server/%.cpp | dir
	$(CC) -c -o $@ $< $(CFLAG)

$(ODIR)/client/%.o: $(SDIR)/client/%.cpp | dir
	$(CC) -c -o $@ $< $(CFLAG)

$(TARGET_S): $(OBJ_S) | dir
	$(CC) -o $@ $^ $(CFLAG)

$(TARGET_C): $(OBJ_C) | dir
	$(CC) -o $@ $^ $(CFLAG)

all:  $(TARGET_S) $(TARGET_C)

.PHONY: all dir clean

dir: # TODO: add client obj directory
	@mkdir -p $(BDIR) $(ODIR)/common $(ODIR)/server $(ODIR)/client

clean:
	rm -rf $(ODIR)/* $(BDIR)/* core