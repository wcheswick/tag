CFLAGS+=-Wall -Wno-unused-variable -g -O0
BIN=${HOME}/bin

tag:	tag.c
	${CC} ${CFLAGS} tag.c -lm -o tag


install::	${BIN}/tag ${BIN}/tagi

${BIN}/tag:	tag
	cp tag ${BIN}/tag

${BIN}/tagi:	tagi
	cp tagi ${BIN}/tagi


clean::
	rm -f *.o tag *.core

test::
	./dotests
