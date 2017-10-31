OS!=uname -s

BIN=${HOME}/bin
CFLAGS+=-Wall -O3 -g -I/opt/local/include

include Makefile-${OS}.inc

tag:	tag.c
	${CC} ${CFLAGS} tag.c -o tag

install::	${BIN}/tag ${BIN}/tagi ${BIN}/findcycle

${BIN}/tag:	tag
	cp tag ${BIN}/tag

${BIN}/tagi:	tagi
	cp tagi ${BIN}/tagi

${BIN}/findcycle:	findcycle
	cp findcycle ${BIN}/findcycle


clean::
	rm -f *.o tag *.core tests/*.new

test::	tag
	./dotests

looptest::	tag
	./tag -T -d "(100)^4" 3 00 1101
