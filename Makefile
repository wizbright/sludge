.PHONY: files
CC = gcc -g
default: all

all: sludge files

sludge: sludge.o crc32.o
	$(CC) -o sludge sludge.o crc32.o
	mv sludge bin/sludge
	rm *.o

sludge.o: src/sludge.c src/crc32.h
	$(CC) -c src/sludge.c

crc32.o: src/crc32.c src/crc32.h
	$(CC) -c src/crc32.c

file_gen: src/file_gen.c
	$(CC) src/file_gen.c -o file_gen
	mv file_gen bin/file_gen

files: file_gen
	./bin/file_gen
	mv test_file*.txt bin/

clean:
	rm *.o -rf
	rm ./bin/*

