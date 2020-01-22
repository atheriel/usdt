
CC=gcc
CFLAGS= -std=gnu11
LDFLAGS=-lelf -ldl -Wl,-z,noexecstack
VERSION=0.1.0

PREFIX=/usr

OBJ = build/lib/dynamic-symbols.o \
  build/lib/errors.o \
  build/lib/hash-table.o \
  build/lib/libstapsdt-x86_64.o \
  build/lib/libstapsdt.o \
  build/lib/sdtnote.o \
  build/lib/section.o \
  build/lib/shared-lib.o \
  build/lib/string-table.o \
  build/lib/util.o
HEADERS = src/dynamic-symbols.h \
  src/errors.h \
  src/hash-table.h \
  src/libstapsdt.h \
  src/sdtnote.h \
  src/section.h \
  src/shared-lib.h \
  src/string-table.h \
  src/util.h

SOLINK = libstapsdt.so
SONAME = libstapsdt.so.0

all: out/libstapsdt.a out/$(SONAME)

build/lib/libstapsdt-x86_64.o: src/asm/libstapsdt-x86_64.s
	mkdir -p build
	$(CC) $(CFLAGS) -fPIC -c $^ -o $@

build/lib/%.o: src/%.c $(HEADERS)
	mkdir -p build/lib/
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

out/libstapsdt.a: $(OBJ) build/lib/libstapsdt-x86_64.o
	mkdir -p out
	ar rcs $@ $^

out/$(SONAME): $(OBJ) build/lib/libstapsdt-x86_64.o
	mkdir -p out
	$(CC) $(CFLAGS) -shared -Wl,-soname=$(SONAME) -o $@ $^ $(LDFLAGS)

clean:
	rm -rf build/*
	rm -rf out/*

.PHONY: all clean
