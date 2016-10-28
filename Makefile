###################### DEFS

CC = gcc
CFLAGS = -Wall
LDFLAGS = -ldl

###################### HELPERS

%.so : %.c
	$(CC) $(CFLAGS) -shared -nostdlib -fPIC $+ -o $@

%.o : %.c
	${CC} ${CFLAGS} -c $? -o $@

####################### TARGETS

# add your libraries to this line, as in 'libtest.so', make sure you have a 'libtest.c' as source
LIBS = alter_speed.so reverse.so stream.so

.PHONY : all clean distclean

all : client server ${LIBS}

client : client.o audio.o packet.o
	${CC} ${CFLAGS} -o $@ $+ ${LDFLAGS}

server : server.o audio.o packet.o
	${CC} ${CFLAGS} -o $@ $+ ${LDFLAGS}

distclean : clean
	rm -f server client *.so
clean:
	rm -f $(OBJECTS) server client *.o *.so *~