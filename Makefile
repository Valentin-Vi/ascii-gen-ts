CC   = gcc
EMCC = emcc

CFLAGS   = -O2 -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L
LDFLAGS  = -lm

EMFLAGS  = -O2 -std=c11 \
           -s WASM=1 \
           -s EXPORTED_FUNCTIONS='["_convert_frame"]' \
           -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap"]'

SRC_LIB  = src/converter.c
SRC_MAIN = src/main.c src/video.c

NATIVE_LDFLAGS = $(LDFLAGS) -lavformat -lavcodec -lavutil -lswscale

.PHONY: all native wasm clean

all: native

native: $(SRC_LIB) $(SRC_MAIN)
	$(CC) $(CFLAGS) $^ -o iag $(NATIVE_LDFLAGS)

wasm: $(SRC_LIB)
	$(EMCC) $(EMFLAGS) $^ -o iag.js

clean:
	rm -f iag iag.js iag.wasm
