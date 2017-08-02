

all: sludge 

sludge: sludge.o crc32.o
	gcc -o sludge sludge.o crc32.o
	mv sludge ./bin
	rm *.o

sludge.o: src/sludge.c src/crc32.h
	gcc -c src/sludge.c

crc32.o: src/crc32.c src/crc32.h
	gcc -c src/crc32.c

clean:
	rm *.o -rf
	rm ./bin/sludge
