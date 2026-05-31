CC = gcc
CFLAGS = -Wall -O2 -Iinclude `mysql_config --cflags`
LDFLAGS = `mysql_config --libs`

all: shell bin/who bin/name bin/yell bin/tell bin/listMail bin/mailto bin/delMail bin/createGroup bin/listGroup bin/delGroup bin/leaveGroup bin/addTo bin/remove bin/gyell

shell: object/shell.o
	$(CC) $(CFLAGS) -o shell object/shell.o $(LDFLAGS)

object/shell.o: src/shell.c
	$(CC) $(CFLAGS) -c src/shell.c -o object/shell.o

bin/who: src/who.c
	$(CC) $(CFLAGS) -o bin/who src/who.c

bin/name: src/name.c
	$(CC) $(CFLAGS) -o bin/name src/name.c

bin/yell: src/yell.c
	$(CC) $(CFLAGS) -o bin/yell src/yell.c

bin/tell: src/tell.c
	$(CC) $(CFLAGS) -o bin/tell src/tell.c

bin/listMail: src/listMail.c
	$(CC) $(CFLAGS) -o bin/listMail src/listMail.c $(LDFLAGS)

bin/mailto: src/mailto.c
	$(CC) $(CFLAGS) -o bin/mailto src/mailto.c $(LDFLAGS)

bin/delMail: src/delMail.c
	$(CC) $(CFLAGS) -o bin/delMail src/delMail.c $(LDFLAGS)

bin/createGroup: src/createGroup.c
	$(CC) $(CFLAGS) -o bin/createGroup src/createGroup.c $(LDFLAGS)

bin/listGroup: src/listGroup.c
	$(CC) $(CFLAGS) -o bin/listGroup src/listGroup.c $(LDFLAGS)

bin/delGroup: src/delGroup.c
	$(CC) $(CFLAGS) -o bin/delGroup src/delGroup.c $(LDFLAGS)

bin/leaveGroup: src/leaveGroup.c
	$(CC) $(CFLAGS) -o bin/leaveGroup src/leaveGroup.c $(LDFLAGS)
	
bin/addTo: src/addTo.c
	$(CC) $(CFLAGS) -o bin/addTo src/addTo.c $(LDFLAGS)

bin/remove: src/remove.c
	$(CC) $(CFLAGS) -o bin/remove src/remove.c $(LDFLAGS)
	
bin/gyell: src/gyell.c
	$(CC) $(CFLAGS) -o bin/gyell src/gyell.c $(LDFLAGS)

clean:
	rm -f shell object/*.o bin/who bin/name bin/yell bin/tell bin/listMail bin/mailto bin/delMail bin/createGroup bin/listGroup bin/delGroup bin/leaveGroup bin/addTo bin/remove bin/gyell
