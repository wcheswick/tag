#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <assert.h>

#include "arg.h"

#define HEADBITS	1
#define HEADSIZE	3

#define	MINBITS		1	// else sequence terminates


int dflag = 0;

typedef u_char entry_t;

u_long ringbufsize = 0;
entry_t *ringbuffer = 0;

#define RBUFSIZE	(100*1000000)
#define RBUFSIZEINCR	(100*1000000)
#define BPE		(sizeof(entry_t)*8)

#define STATUS_FREQ	1000000000
#define LONGEST_RUN_TOLERANCE	1000000000

char *pattern = "100";
int  repeat = 5;

long headptr = 0;	// the bit address of the head in the ring buffer 
long tailptr = 0;	// the bit address of the head in the ring buffer 

u_long taglen = 0;	// length of our tag stream, in bits
u_long loopcount = 0;

u_long longesttag = 0;
u_long sincelongest = 0;

u_long hash = 0;	// a fairly unique hash of a result
int hashtail = sizeof(hash)*8 - 1;

void
show_status(char *status) {
	fprintf(stderr, "%s: %ld length: %lu, %.1e \n", 
		status, loopcount, taglen, (double)taglen);
}

void
terminate(char *status) {
	printf("(%s)^%d	%s	%lu\n", pattern, repeat, status, loopcount);
	exit(0);
}

void
dump(void) {
	u_long i;
	printf("%-4lu %3lu  %.16lx %d  \"", loopcount, taglen, hash, hashtail);

	for (i=headptr; i!=tailptr; i = (i + 1) % ringbufsize ) {
		u_char mask = 1<<(7 - (i % 8));
		u_char b = ringbuffer[i/8] & mask;
//		if ((i % 8) == 0 )
//			putc(' ', stdout);
		putc(b ? '1' : '0', stdout);
	}
	printf("\"\n");
}

int
head(void) {
	u_char mask = 1<<(7 - (headptr % 8));
	u_char b = ringbuffer[headptr/8] & mask;	// get the first bit

	if (taglen < HEADSIZE) {
		terminate("Died");
	}
	headptr += HEADSIZE;
	if (headptr/8 == ringbufsize)	// new address is just the bits
		headptr = headptr % 8;
	taglen -= HEADSIZE;

	hash = hash << HEADSIZE;
	hashtail += HEADSIZE;

	return b;
}

void
dotailend(void) {
//	printf("** end of buffer\n");
	if (taglen/8 > 0.95*(tailptr/8)) {	// bump up the memory
		ringbufsize += RBUFSIZEINCR;
		printf("** more memory: %lu MB\n", ringbufsize/1000000);
		ringbuffer = (entry_t *)realloc(ringbuffer, ringbufsize);
                       if (ringbuffer == 0) 
                               terminate("out of memory");
	} else {
		tailptr = tailptr % 8;	// back to beginning of the buffer
	}
}

// suffix n (7 >= n >= 1) bits to the tail.  If we are at the end of the ringbuffer, and
// running out of room, extend the buffer.

void
append(int n, int bits) {
	int ba = tailptr % 8;
	u_char mask = (1<<n) - 1;
	int shift = 7 - ba - (n-1);
	int hashshift = hashtail - (n-1);

	if (hashshift >= 0) {
		hash = hash ^ ((u_long)bits << hashshift);
		hashtail = hashshift - 1;
		if (hashtail < 0)
			hashtail = sizeof(hash)*8 - 1;
	} else {
		hashshift = -hashshift;
		hash = hash ^ ((u_long)bits >> hashshift);
		hashtail = sizeof(hash)*8 - 1;
		hash = hash ^ (((u_long)bits % 
			(1<<-hashshift)) << (hashtail - (hashshift + 1)));
		hashtail -= hashshift;
	}

	taglen += n;
	if (taglen > longesttag) {
		sincelongest = 0;
		longesttag = taglen;
	} else {
		sincelongest++;
		if ((sincelongest % LONGEST_RUN_TOLERANCE) == 0) {
			char buf[100];
			snprintf(buf, sizeof(buf), "possible cycle (%.1e)", 
				(double)sincelongest);
			show_status(buf);
		}
	}

	if (shift >= 0) {	// all in the current byte
		ringbuffer[tailptr / 8] |= (ringbuffer[tailptr / 8] &
			(mask << shift)) | (bits <<shift);
		tailptr += n;
		if (tailptr/8 == ringbufsize)
			dotailend();
		return;
	}

	// splits between this byte and the next
	ringbuffer[tailptr / 8] |=
		(ringbuffer[tailptr / 8] & (mask >> -shift)) | (bits >> -shift);
	tailptr += n;

	assert(tailptr/8 <= ringbufsize);	// should not go past
	if (tailptr/8 == ringbufsize)	// end of the ring buffer
		dotailend();
	ba = tailptr % 8;
	n = -shift;
	mask = (1<<n) - 1;
	shift = 7 - (n-1);
	ringbuffer[tailptr / 8] = (ringbuffer[tailptr / 8] & (mask << shift)) |
			(bits <<shift);
}

// called with our string of bits.  

void
tag(void) {
	while (taglen > 0) {
		int b = head();
		if (!b) 
			append(2, 0x0);
		else
			append(4, 0xd);
		loopcount++;
		if (dflag)
			dump();
		if ((loopcount % STATUS_FREQ) == 0) {
			show_status("status");
		}
	}
}

void
setup(char *pattern, int repeat) {
	int i;
	int bits = 0;
	int len = strlen(pattern);

	for (i=0; i<strlen(pattern); i++)
		bits = (bits << 1) | (pattern[i] == '1' ? 1 : 0);

	for (i=0; i<repeat; i++)
		append(len, bits);
}

int
usage(void) {
	fprintf(stderr, "usage: tag [-d] [[pattern] repeat]\n");
	return 1;
}

int
main(int argc, char *argv[]) {
	ARGBEGIN {
	case 'd':	dflag++;	break;
	default:
		return usage();
	} ARGEND;

	switch (argc) {
	case 1: 
		repeat = atoi(argv[0]);
		break;
	case 2:
		repeat = atoi(argv[1]);
		pattern = argv[1];
		break;
	default:
		return usage();
	}

	ringbufsize = RBUFSIZE;
	ringbuffer = (entry_t *)malloc(ringbufsize);

	printf("(%s)^%d\n", pattern, repeat);

	setup(pattern, repeat);

	if (dflag)
		dump();

	tag();
	terminate("died");
	return 0;
}

