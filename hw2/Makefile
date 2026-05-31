all: shell who name yell tell

shell: object/shell.o
	gcc -Wall -O2 -Iinclude -o shell object/shell.o

object/shell.o: src/shell.c
	gcc -Wall -O2 -Iinclude -c src/shell.c -o object/shell.o

who: src/who.c
	gcc -Wall -O2 -Iinclude -o bin/who src/who.c

name: src/name.c
	gcc -Wall -O2 -Iinclude -o bin/name src/name.c

yell: src/yell.c
	gcc -Wall -O2 -Iinclude -o bin/yell src/yell.c

tell: src/tell.c
	gcc -Wall -O2 -Iinclude -o bin/tell src/tell.c

clean:
	rm -f shell bin/who bin/name bin/yell bin/tell object/*.o
