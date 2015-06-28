#!/usr/bin/perl

use strict;
use warnings;

my $I_MAX = 64;
my @bytes = [];
my $fn = "tmp.dat";
my $ret = "";

for (my $i = 1; $i <= $I_MAX; $i++) {
    my $bytes = int(rand 2**16);
    print "Testing codec on $bytes bytes ...";

    # Generate file containing $i random bytes.
    $ret = `dd bs=1 count=$bytes </dev/urandom >$fn 2>/dev/null`;

    # Encode and decode.
    my $enc = `./arith $fn > $fn.ac 2>/dev/null`;
    my $dec = `./arith -d $fn.ac > $fn.ac.dat 2>/dev/null`;

    # Check correctness of decoded file.
    $ret = `diff $fn $fn.ac.dat`;
    if ($ret eq "") {
        print " passed\n";
    } else {
        print " NOT passed: $ret\n";
        exit -1;
    }
}

print "Files created: $fn $fn.ac $fn.ac.dat\n";
exit 0;

