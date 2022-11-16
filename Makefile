# Makefile example
all: main.c 
	gcc -Wall -o ring main.c

clean: 
	$(RM) ring

