BIN=${HOME}/bin
detected_OS := $(shell sh -c 'uname -s 2>/dev/null || echo not')

CFLAGS+=-Wall -g -I/opt/local/include

ifeq ($(detected_OS),FreeBSD)
endif

ifeq ($(detected_OS),Darwin)  # Mac OS X
CFLAGS += -I/opt/local/include/openssl
LDFLAGS +=/opt/local/lib/libcrypto.a
endif

tag:	tag.c
	${CC} ${CFLAGS} tag.c ${LDFLAGS} -o tag

install::	${BIN}/tag ${BIN}/tagi

${BIN}/tag:	tag
	cp tag ${BIN}/tag

${BIN}/tagi:	tagi
	cp tagi ${BIN}/tagi


clean::
	rm -f *.o tag *.core tests/*.new

test::	tag
	./dotests

looptest::	tag
	./tag -T -d "(100)^4" 3 00 1101
