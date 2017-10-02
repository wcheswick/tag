#include <stdio.h>
#include <stdlib.h>
#include <gmp.h>
#include <sys/types.h>
#include <string.h>

int
usage(void) {
	fprintf(stderr, "usage: tag [pow [base]]\n");
	return 1;
}

int main(int argc, char *argv[]) {
	char *bstr = "100";
	u_long p = 110;
	mpz_t b;
	mpz_t start;
	int base = 10;	// decimal strings
	char *bits;

	switch (argc) {
	case 1: 
		break;
	case 2:
		p = atoi(argv[1]);
		break;
	case 3:
		p = atoi(argv[1]);
		bstr = argv[2];
		break;
	default:
		return usage();
	}

fprintf(stderr, "%s^%ld\n", bstr, p);

	mpz_init_set_str (b, bstr, base);
	mpz_pow_ui(start, b, p);

	bits = mpz_get_str(NULL, 2, start);
	printf("%ld bits: %s\n", strlen(bits), bits);
	return 0;
}

