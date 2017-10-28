CFLAGS+=-Wall -Wno-unused-variable -g -O0

tag:	tag.c
	${CC} ${CFLAGS} tag.c -lm -o tag

clean::
	rm -f *.o tag *.core

tests::	tag
	for i in 5 13 14 22 25 46 47 54 63 65 70 74 78 80 91 93 106; \
	do \
		echo $$i; \
		tag -d $$i >tests/$$i; \
	done
