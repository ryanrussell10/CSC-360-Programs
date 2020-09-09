.PHONY all:
all: diskoperations

diskoperations:
	gcc -Wall -o diskinfo diskinfo.c
	gcc -Wall -o disklist disklist.c
	gcc -Wall -o diskget diskget.c
	gcc -Wall -o diskput diskput.c

.PHONY clean:
clean: 
	-rm -rf *.o *.exe

