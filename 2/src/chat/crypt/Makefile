CC=gcc
CFLAGS=-Wall
.PHONY: clean distclean

default: chat

nothing:

chat: chat.h chat.c client.c server.c io.h io.c
	$(CC) $(CFLAGS) -o chat chat.c client.c server.c io.c

clean:
	$(RM) *~

distclean: clean
	$(RM) chat
