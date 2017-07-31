# Makefile for sludge

CC=gcc

.PHONY: clean

default: src/sludge.c
	$(CC) -g -o sludge src/sludge.c

clean: 
	@echo "Cleaning up"
	rm sludge
