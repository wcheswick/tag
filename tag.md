NAME
	tag - Compute tag sequences

SYNOPSIS
	tag [-d] bit-pattern remove-count add-if-zero add-if-one

	tag 'tagi pattern' remove-count add-if-zero add-if-one
	
	tag `tagi 'pattern'` remove-count add-if-zero add-if-one

DESCRIPTION

Computes tag systems, see https://oeis.org/A291792 for examples. The
result is written to standout output.

A tag system starts with an initial stream of (in our case) bits.  The
first bit in the stream is examined, and bits are suffixed to the stream,
one pattern if set, another if clear.  The leading 'remove-count' bits are
removed, and the process continues.

For example,
	tag -d '(100)^5' 3 00 1101
starts out:
	0     15  "100100100100100"
	1     16  "1001001001001101"
	2     17  "10010010011011101"
	3     18  "100100110111011101"
	4     19  "1001101110111011101"
	...
	(100)^5 Died    409

All known systems end either looping or in a null string.  This program
can explore variations for a very long time.  It is written to conserve
memory, which it will use to exhaustion if the sequence gets that long.

The first parameter can just be a string of bits, or have the form demonstrated
here and described in *tagi(1)*, with an optional prefix, a
bit pattern and a repeat count. This syntax requires shell quoting.
'remove-count' must be greater than zero, and the adding strings may
not be longer than eight bits.

OPTIONS
	-d	shows each step of the computation

EXAMPLES.

	tagi  1101
	1101

	tagi '(100)^5'
	100100100100100

	tagi '1001(100)^15'
	1001100100100100100100100100100100100100100100100

SEE ALSO
	*tag(1)*
