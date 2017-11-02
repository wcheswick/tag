#define _WITH_GETLINE
#include <stdio.h>

#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/time.h>
#include <string.h>
#include <assert.h>

#include <sha.h>

#include "arg.h"
#define USED(x) ((void)(x))

#define HEADBITS	1

#define	MINBITS		1	// else sequence terminates

#define TAGI	"tagi"		// our program for expanding input patterns

int dflag = 0;
int Tflag = 0;

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

#define MILLION	((u_int64_t)1000000)
#define BILLION	(MILLION*1000)
#define TRILION (BILLION*1000)
#define QUADRILLION	(TRILION*1000)

#define STATUS_FREQ	(500*MILLION)
#define LONGEST_RUN_TOLERANCE	(1*BILLION)

typedef unsigned long long uulong;

typedef struct pattern {
	int 	size;	// <=8
	u_char	bits;	// right justified
} pattern;

typedef struct tagsum {		// for finding loops
	uint64_t length;
	uint64_t digest;
} tagsum_t;

typedef struct full_bits_t {
	int length;
	u_char *bits;
} full_bits_t;

char *initial_string;	// what we were called with
char *pure_string;	// possibly expanded, just bits

pattern start_pattern;
pattern zero_add_bits, one_add_bits;
int remove_head;

long headptr = 0;	// the bit address of the head in the ring buffer 
long tailptr = 0;	// the bit address of the head in the ring buffer 

uint64_t taglen = 0;	// length of our tag stream, in bits
uint64_t loopcount = 0;

uint64_t longesttag = 0;
uint64_t sincelongest = 0;

#define SUMSIZE	MILLION	// way more than we are using

tagsum_t summary[SUMSIZE];
uint64_t first_sum;
size_t sum_count = 0;
int sums_to_collect;
int late_cycle_detection;

typedef unsigned long long longtime_t;
longtime_t start_time;

/*
 * real time, milliseconds
 */
longtime_t
rtime_ms(void) {
        struct timeval tp;
        struct timezone tz;

        if (gettimeofday(&tp, &tz) < 0)
                perror("gettimeofday");
        return (tp.tv_sec*1000000 + tp.tv_usec)/1000;
}

void
show_status(char *status) {
	printf("%-10s @ %lld: len %lld  longest: %lld  since: %lld\n", 
		initial_string, loopcount,
		taglen, longesttag, sincelongest);
}

void
terminate(int n, char *status) {
	longtime_t finish_time = rtime_ms();
	double elapsed = (finish_time - start_time)/1000.0;

	printf("%-10s %10.3f %10d  %s\n", initial_string, elapsed, n, status);
	exit(0);
}

// get the full bit string into an array of u_char for the current
// string. The memory in full->bits is only defined until the next call
// to this routine.

u_char *bitmem = 0;
size_t bitmemsize = 0;

void
extract_full_bits(full_bits_t *full) {
	u_long i;
	u_char *bp;

	full->length = taglen;
	if (full->length+1 > bitmemsize) {
		bitmemsize = taglen * 1.25;
		bitmem = (u_char*)realloc(bitmem, bitmemsize);
		if (!bitmem)
			terminate(loopcount, "out of memory (extract)");
	}
	full->bits = bitmem;
	bp = full->bits;

	for (i=headptr; i!=tailptr;  ) {
		u_char mask = 1<<(7 - (i % 8));
		u_char b = ringbuffer[i/8] & mask;
		*bp++ = b ? '1' : '0';
		i++;
		if (i/8 == ringbufsize)
			i = 0;
	}
	*bp = '\0';
}

void
dump(void) {
	full_bits_t full;

	printf("%-4llu %3llu  \"", loopcount, taglen);
	extract_full_bits(&full);
	fwrite(full.bits, 1, full.length, stdout);
	printf("\"");
	if (dflag > 1) {
		int i;

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
		terminate(loopcount, "Died");
	}
	headptr += remove_head;
	if (headptr/8 == ringbufsize)	// new address is just the bits
		headptr = headptr % 8;
	taglen -= remove_head;
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
			terminate(loopcount, "out of memory");
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

	taglen += n;
	if (taglen > longesttag) {
		sincelongest = 0;
		longesttag = taglen;
	} else {
		sincelongest++;
		if ((sincelongest % LONGEST_RUN_TOLERANCE) == 0) {
			char buf[100];
			snprintf(buf, sizeof(buf), "possible cycle (%llu) %llu", 
				sincelongest, LONGEST_RUN_TOLERANCE);
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

void
init_with_pattern(char *start) {
	char *cp = start;

	while (*cp && isdigit(*cp))
		append_bit(*cp++);
	if (*cp != '\n' && *cp != '\0') {
		fprintf(stderr, "non-bits found in initial pattern: %s '%.02x\n",
			start, *cp);
		exit(13);
	}
	return;
}

// must be a string of ones and zeros, 0 - 8 bits long

pattern
parse_bits(char *s) {
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

char *
process_initial(char *input) {
	char command[100000];
	char *nonbit = strpbrk(input, "10");
	char *linep = NULL;
	size_t linecapp;
	int n;
	FILE *pfn;

	if (nonbit== NULL)
		return input;	// he gave us only bits: that's fine

	// We have to process inputs like (100)^110
	// This job is given to an external script, which can be
	// arbitrarily complicated.

	n = snprintf(command, sizeof(command), "%s '%s'", TAGI, input);
	assert(n < sizeof(command));	// input string much too long

	pfn = popen(command, "r");
	n = getline(&linep, &linecapp, pfn);
	if (n < 0) {
		ferror(pfn);
		exit(20);
	}
	n = fclose(pfn);
	if (n) {
		exit(21);
	}
	return linep;
}

// digest the answer.  This is probably way overkill.

uint64_t
make_digest(void) {
	full_bits_t full;
	u_char sha1_digest[20];
	SHA_CTX context;
	uint64_t digest;
	int i;

	extract_full_bits(&full);
//fprintf(stderr, "%-25s", full.bits);
	SHA1_Init(&context);
	SHA1_Update(&context, full.bits, full.length);
	SHA1_Final(sha1_digest, &context);

	// digest it to 64 bits

	digest = 0;
	for (i=0; i<sizeof(sha1_digest); i++) {
		digest ^= (uint64_t)sha1_digest[i] <<
			(sizeof(digest) - (i % sizeof(digest)) - 1)*8;
	}
	return digest;
}

void
add_to_summary(void) {
	int i = loopcount - first_sum;
	tagsum_t sum;

	if (i >= SUMSIZE)
		terminate(loopcount, "out of memory (summary too long)");

	sum.length = taglen;
	sum.digest = make_digest();
	summary[sum_count++] = sum;
}

void
check_cycles(void) {	// Using Brent's algorithm
	int x0 = 0;
	int turtle = x0;
	int rabbit = x0+1;
	int lam = 1;
	int power = 1;
	int mu;
	char buf[100];

	while (summary[rabbit].digest != summary[turtle].digest) {
		if (lam == power ) {
			lam = 0;
			power *= 2;
			turtle = rabbit;
		}
		lam++;
		rabbit++;
		if (rabbit >= sum_count)
			return;
	}

	turtle = rabbit = x0;
	rabbit = x0 + lam;

	for (mu=0; summary[rabbit+mu].digest != summary[turtle+mu].digest;)
		mu++;

	snprintf(buf, sizeof(buf), "%speriod %d", 
		 late_cycle_detection ? "or before, ": "", lam);
	terminate(first_sum+mu+1, buf);	// adjust from index to ordinal
}

#define INIT_CYCLE_CHECK_LEN	100000
#define CYCLE_CHECK_FREQ	(10*MILLION)
#define CYCLE_CHECK_LEN		100

void
start_sum_collection(int count) {
	sums_to_collect = count;
	first_sum = loopcount;
	sum_count = 0;
	late_cycle_detection = (loopcount != 0);
}

void
tag(void) {
	// do aggressive cycle collection for the beginning of the sequence
	start_sum_collection(INIT_CYCLE_CHECK_LEN);

	while (taglen > 0) {
		int b = head();
		if (!b) 
			append(zero_add_bits.size, zero_add_bits.bits);
		else
			append(one_add_bits.size, one_add_bits.bits);
		loopcount++;
		if (dflag)
			dump();

		if ((loopcount % STATUS_FREQ) == 0) {
			show_status("status");
		}

		if (sums_to_collect) {
			add_to_summary();
			sums_to_collect--;
			if (sums_to_collect == 0)
				check_cycles();
		} else if (loopcount % CYCLE_CHECK_FREQ == 0) {
			// every once in a while, check for a short loop
			start_sum_collection(CYCLE_CHECK_LEN);
		}
	}
}

int
usage(void) {
	fprintf(stderr, "usage: tag [-d] bitpattern remove-count add-if-zero add-if-one\n");
	return 1;
}

int
main(int argc, char *argv[]) {
	start_time = rtime_ms();

	ringbufsize = RBUFSIZE;
	ringbuffer = (entry_t *)malloc(ringbufsize);

	ARGBEGIN {
	case 'd':	dflag++;	break;
	case 'T':	Tflag++;	break;
	default:
		return usage();
	} ARGEND;

	if (argc != 4)
		return usage();

	initial_string = strdup(argv[0]);
	init_with_pattern(process_initial(initial_string));

	remove_head = atoi(argv[1]);
	if (remove_head < 1) {
		fprintf(stderr, "must remove at least one entry\n");
		return 3;
	}

	zero_add_bits = parse_bits(argv[2]);
	one_add_bits = parse_bits(argv[3]);

	if (dflag)
		dump();

	tag();
	terminate(loopcount, "died");
	return 0;
}
