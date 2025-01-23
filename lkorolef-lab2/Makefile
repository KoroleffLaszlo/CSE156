CC = g++
CFLAG = -Wall -Werror -g -lstdc++fs

BDIR = bin
ODIR = build
IDIR = include
SDIR = src

# Targets
TARGET_S = $(BDIR)/myserver
TARGET_C = $(BDIR)/myclient

# Object files for server and client
_OBJ_S = server.o myserver.o
OBJ_S = $(patsubst %, $(ODIR)/%, $(_OBJ_S))

_OBJ_C = client.o file_t.o packet_t.o myclient.o
OBJ_C = $(patsubst %, $(ODIR)/%, $(_OBJ_C))

# Dependencies for server and client
_DEPS_S = server.h
DEPS_S = $(patsubst %, $(IDIR)/%, $(_DEPS_S))

_DEPS_C = client.h file_t.h packet_t.h
DEPS_C = $(patsubst %, $(IDIR)/%, $(_DEPS_C))

# Rule to compile object files
$(ODIR)/%.o: $(SDIR)/%.cpp $(DEPS_S) $(DEPS_C) | dir
	$(CC) -c -o $@ $< $(CFLAG)

# Rules to build executables
$(TARGET_S): $(OBJ_S) | dir
	$(CC) -o $@ $^ $(CFLAG)

$(TARGET_C): $(OBJ_C) | dir
	$(CC) -o $@ $^ $(CFLAG)

# Phony targets
.PHONY: all dir clean

# Build all targets
all: $(TARGET_S) $(TARGET_C)

# Create necessary directories
dir:
	@mkdir -p $(BDIR) $(ODIR)

# Clean generated files
clean:
	rm -f $(ODIR)/*.o $(TARGET_S) $(TARGET_C) core
