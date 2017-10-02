CFLAGS+=-I/opt/local/include -L/opt/local/lib

tag:	tag.c
	${CC} ${CFLAGS} tag.c -lgmp -lm -o tag
