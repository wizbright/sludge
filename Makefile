.PHONY: files

default: all

all: sludge 

sludge: sludge.o crc32.o
	gcc -o sludge sludge.o crc32.o
	mv sludge ./bin
	rm *.o

sludge.o: src/sludge.c src/crc32.h
	gcc -c src/sludge.c

crc32.o: src/crc32.c src/crc32.h
	gcc -c src/crc32.c

file_gen: src/file_gen.c
	gcc -o src/file_gen.c -o file_gen
	mv file_gen execs/

files: file_gen
	./execs/file_gen

clean:
	rm *.o -rf
	rm ./bin/sludge
