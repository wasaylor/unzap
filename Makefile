CC ?= clang
CFLAGS ?= -Wall -std=c11 -march=native -O2

all: clean unzap

unzap: src/main.c src/decompress.c src/zap.c
	$(CC) $^ -o$@ $(CFLAGS)

# Stand-alone decompress program
# "Un-Bounty-Hunter-LZ"
unbhlz: src/decompress.c
	$(CC) $^ -o$@ -DUNBHLZ $(CFLAGS)

clean: 
	rm -f unzap unbhlz

cleandata:
	rm -rf data/
