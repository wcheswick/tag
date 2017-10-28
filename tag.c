#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <string.h>
#include <assert.h>

#include "arg.h"

#define HEADBITS	1

#define	MINBITS		1	// else sequence terminates


int dflag = 0;

typedef u_char entry_t;

u_long ringbufsize = 0;
entry_t *ringbuffer = 0;


//#define BWK
#ifdef BWK
#define RBUFSIZE	(10)
#define RBUFSIZEINCR	(10)
#else
#define RBUFSIZE	(100*1000000)
#define RBUFSIZEINCR	(100*1000000)
#endif

#define STATUS_FREQ	1000000000
#define LONGEST_RUN_TOLERANCE	1000000000

typedef struct pattern {
	int 	size;	// <=8
	u_char	bits;	// right justified
} pattern;


char *initial_string;	// what we were called with

pattern start_pattern;
pattern zero_add_pattern, one_add_pattern;
int remove_head;

long headptr = 0;	// the bit address of the head in the ring buffer 
long tailptr = 0;	// the bit address of the head in the ring buffer 

u_long taglen = 0;	// length of our tag stream, in bits
u_long loopcount = 0;

u_long longesttag = 0;
u_long sincelongest = 0;

#ifdef oldhash
u_long hash = 0;	// a fairly unique hash of a result
int hashtail = sizeof(hash)*8 - 1;
#endif

void
show_status(char *status) {
	fprintf(stderr, "%s: %ld longest=%ld sincelongest=%ld length: %lu, %.1e \n", 
		status, loopcount, longesttag, sincelongest, taglen, (double)taglen);
}

void
terminate(char *status) {
	printf("%s	%s	%lu\n", initial_string, status, loopcount);
	exit(0);
}

void
dump(void) {
	u_long i;
	int n = 4;

	printf("%-4lu %3lu  \"", loopcount, taglen);

	for (i=headptr; i!=tailptr;  ) {
		u_char mask = 1<<(7 - (i % 8));
		u_char b = ringbuffer[i/8] & mask;
		if (dflag > 1) {
			if (n-- == 0 ) {
				putc(' ', stdout);
				n = 4-1;
			}
		}
		putc(b ? '1' : '0', stdout);
		i++;
		if (i/8 == ringbufsize)
			i = 0;
	}
	printf("\"");
	if (dflag > 1) {
		printf("  %ld/%ld %ld/%ld  ",
			headptr/8, headptr % 8,
			tailptr/8, tailptr % 8);
		for (i=0; i<ringbufsize; i++)
			printf("%.02x ", ringbuffer[i]);
	}
	printf("\n");
}

int
head(void) {
	u_char mask = 1<<(7 - (headptr % 8));
	u_char b = ringbuffer[headptr/8] & mask;	// get the first bit

	if (taglen < remove_head) {
		terminate("Died");
	}
	headptr += remove_head;
	if (headptr/8 == ringbufsize)	// new address is just the bits
		headptr = headptr % 8;
	taglen -= remove_head;

#ifdef oldhash
	hash = hash << remove_head;
	hashtail += remove_head;
#endif

	return b;
}

void
dotailend(void) {
if (dflag > 1)	printf("** end of buffer\n");
	u_long bytes_used = taglen/8;
	if (bytes_used > 0.95*(tailptr/8) ||
		ringbufsize - 100 < bytes_used) {	// bump up the memory
		ringbufsize += RBUFSIZEINCR;
		printf("** more memory: %lu MB\n", ringbufsize/1000000);
		ringbuffer = (entry_t *)realloc(ringbuffer, ringbufsize);
		if (ringbuffer == 0) 
			terminate("out of memory");
	} else {
		tailptr = tailptr % 8;	// back to beginning of the buffer
		ringbuffer[tailptr/8] = 0;
if (dflag > 1) printf("buffer reset: %ld %ld\n", headptr, tailptr);
	}
}

// suffix n (7 >= n >= 1) bits to the tail.  If we are at the end of the
// ringbuffer, and running out of room, extend the buffer.

void
append(int n, int bits) {
	int ba = tailptr % 8;
	u_char mask = (1<<n) - 1;
	int shift = 7 - ba - (n-1);

#ifdef oldhash
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
#endif

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
//printf("Tailptr=%3ld  shift = %2d n=%d bits=%02x mask=%02x  %02x\n",
//		tailptr, shift, n, bits, mask, ringbuffer[tailptr / 8]);

		ringbuffer[tailptr / 8] |= (ringbuffer[tailptr / 8] &
			(mask << shift)) | (bits <<shift);

//printf("tailptr=%3ld  shift = %2d bits=%02x mask=%02x  %02x\n",
//		tailptr, shift, bits, mask, ringbuffer[tailptr / 8]);
		tailptr += n;
		if (tailptr/8 == ringbufsize)
			dotailend();
		return;
	}

	// partly in the next byte.  Advance one step through the ringbuffer

if (dflag > 1) {
	printf("tailptr=%3ld  shift = %2d bits=%02x  %02x\n",
		tailptr, shift, bits, ringbuffer[tailptr / 8]);
}

	// splits between this byte and the next
	ringbuffer[tailptr / 8] |=
		(ringbuffer[tailptr / 8] & (mask >> -shift)) | (bits >> -shift);
if (dflag > 1) {
	printf("        %3ld  shift = %2d bits=%02x  %02x\n",
		tailptr, shift, bits, ringbuffer[tailptr / 8]);
}
	tailptr += n;
	assert(tailptr/8 != headptr/8);	// ptr collision is a subtle disaster
	assert(tailptr/8 <= ringbufsize);	// should not go past
	if (tailptr/8 == ringbufsize)	// end of the ring buffer
		dotailend();
	n = -shift;
	mask = (1<<n) - 1;
	shift = 7 - (n-1);
if (dflag > 1) {
	printf("        %3ld  shift = %2d bits=%02x mask=%02x  %02x  %02x\n",
		tailptr, shift, bits, mask, ringbuffer[tailptr / 8],
		((bits & mask) << shift));
}
	ringbuffer[tailptr / 8] = ((bits & mask) << shift);
if (dflag > 1) {
	printf("        %3ld  shift = %2d bits=%02x  %02x\n",
			tailptr, shift, bits, ringbuffer[tailptr / 8]);
}
}

// called with our string of bits.  

void
tag(void) {
	while (taglen > 0) {
//if (loopcount == 73) dflag = 2;
		int b = head();
		if (!b) 
			append(zero_add_pattern.size, zero_add_pattern.bits);
		else
			append(one_add_pattern.size, one_add_pattern.bits);
		loopcount++;
		if (dflag)
			dump();
		if ((loopcount % STATUS_FREQ) == 0) {
			show_status("status");
		}
	}
}

void
append_bit(char c) {
	switch (c) {
	case '0':
		append(1, 0);
		break;
	case '1':
		append(1, 1);
		break;
	default:
		fprintf(stderr, "pattern char not '0' or '1': %c\n", c);
		exit(10);
	}
}

// This is a very crude parser.  It handles:
// [bits][({bits})^count]
//
//	Anthing fancier should be done somewhere else.

void
init_with_pattern(char *start) {
	char *cp = start;
	char bits[1000];
	int repeat;
	int i, n;

	// initial bits first.  Maybe all the bits.

	while (*cp && isdigit(*cp))
		append_bit(*cp++);
	if (*cp == '\0')
		return;
fprintf(stderr, "start=%s *cp=%c\n", start, *cp);

	n = sscanf(cp, "(%s)^%d", bits, &repeat);
fprintf(stderr, "start=%s n=%d bits=%s repeat=%d\n", start, n, bits, repeat);
	if (n != 2 || repeat < 1) {
		fprintf(stderr, "Bad input pattern: %s\n", cp);
		exit(11);
	}

	n = strlen(bits);
	for (i=0; i<repeat; i++) {
		int j;
		for (j=0; j<n; j++)
			append_bit(bits[j]);
	}		
}

// must be a string of ones and zeros, 0 - 8 bits long

pattern
parse_pattern(char *s) {
	pattern p;
	int i;

	p.size = strlen(s);
	p.bits = 0;

	if (p.size > 8) {
		fprintf(stderr, "pattern > 8 bits unsupported: %s\n", s);
		exit(12);
	}

	for (i=0; i<p.size; i++) {
		char ch = s[i];

		p.bits = p.bits << 1;
		switch (ch) {
		case '0':
			break;
		case '1':
			p.bits |= 1;
			break;
		default:
			fprintf(stderr, "pattern not '0' or '1': %s\n", s);
			exit(13);
		}
	}
	
	return p;
}

int
usage(void) {
	fprintf(stderr, "usage: tag [-d] bitpattern remove-count add-if-zero add-if-one\n");
	return 1;
}

int
main(int argc, char *argv[]) {
	char *raw_pattern;

	ringbufsize = RBUFSIZE;
	ringbuffer = (entry_t *)malloc(ringbufsize);

	ARGBEGIN {
	case 'd':	dflag++;	break;
	default:
		return usage();
	} ARGEND;

	if (argc != 4)
		return usage();

	initial_string = strdup(argv[0]);
	init_with_pattern(argv[0]);
dump();

	remove_head = atoi(argv[1]);
	if (remove_head < 1) {
		fprintf(stderr, "must remove at least one entry\n");
		return 3;
	}

	zero_add_pattern = parse_pattern(argv[2]);
	one_add_pattern = parse_pattern(argv[3]);

	if (dflag)
		dump();
exit(13);
	tag();
	terminate("died");
	return 0;
}
