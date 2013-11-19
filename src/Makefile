CC 		= gcc
CFLAGS		= -g -Wall -Werror
OBJS = proxy.o http_parser.o


proxy: ${OBJS}
	@${CC} ${OBJS} -o $@ ${CFLAGS}; cp -f $@ ../$@

.c.o:
	@$(CC) -c $(CFLAGS) $<

clean:
	@rm proxy *.o