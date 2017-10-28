NAME
	tagi - Expand binary input patterns for *tag(1)*

SYNOPSIS
	tagi bits
	tagi '[prefix](bits)^repeat'

DESCRIPTION

*tagi* translates a bit description shorthand into a string of bits,
which is written to standard output.  This can be used as the first
parameter for *tag(1)*.

EXAMPLES.

	tagi  1101
	1101

	tagi '(100)^5'
	100100100100100

	tagi '1001(100)^15'
	1001100100100100100100100100100100100100100100100

SEE ALSO
	*tag(1)*
