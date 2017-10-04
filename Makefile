CFLAGS+=-Wall -Wno-unused-variable -g

tag:	tag.c
	${CC} ${CFLAGS} tag.c -lm -o tag

clean::
	rm -f *.o tag *.core

