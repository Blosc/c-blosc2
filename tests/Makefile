CC=gcc
CFLAGS=-O3 -Wall -pthread
LDFLAGS=-pthread
BLOSC_LIB= $(wildcard ../blosc/*.c)

# The list of executables
# Generated PNG (intermediate) files
SOURCES := $(wildcard *.c)
EXECUTABLES := $(patsubst %.c, %.exe, $(SOURCES))

# Support for internal LZ4 and LZ4HC
LZ4_DIR = ../internal-complibs/lz4-1.7.0
CFLAGS += -I$(LZ4_DIR)
BLOSC_LIB += $(wildcard $(LZ4_DIR)/*.c)

# Support for external LZ4 and LZ4HC
#LDFLAGS += -llz4

# Support for external Zlib
LDFLAGS += -DHAVE_ZLIB -lz

# Support for internal Zlib
#ZLIB_DIR = ../internal-complibs/zlib-1.2.8
#CFLAGS += -DHAVE_ZLIB -I$(ZLIB_DIR)
#BLOSC_LIB += $(wildcard $(ZLIB_DIR)/*.c)


# Support for internal Zstd
ZSTD_DIR = ../internal-complibs/zstd-1.2.0
CFLAGS += -DHAVE_ZSTD -I$(ZSTD_DIR) -I$(ZSTD_DIR)/common
BLOSC_LIB += $(wildcard $(ZSTD_DIR)/*/*.c)


.PHONY: all
all: $(EXECUTABLES)

test: $(EXECUTABLES)
	sh test_all.sh

%.exe: %.c $(BLOSC_LIB)
	$(CC) $(CFLAGS) $(LDFLAGS) "$<" $(BLOSC_LIB) -o "$@"

clean:
	rm -rf $(EXECUTABLES)
