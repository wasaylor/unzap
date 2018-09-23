CC ?= clang
CFLAGS ?= -Wall -std=c11 -march=native -O2

all: clean unzap

unzap: src/main.c src/decompress.c src/zap.c
	$(CC) $^ -o$@ $(CFLAGS)

clean: 
	rm -f unzap

cleandata:
	rm -rf data/
