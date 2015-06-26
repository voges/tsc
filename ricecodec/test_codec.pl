#!/usr/bin/perl

use strict;
use warnings;

my $I_MAX = 4096;
my @bytes = [];
my $fn = "tmp.dat";
my $ret = "";

for (my $i = 1; $i <= $I_MAX; $i++) {
    my $bytes = int(rand 2**16);
    print "Testing codec on $bytes bytes ...";

    # Generate file containing $i random bytes.
    $ret = `dd bs=1 count=$bytes </dev/urandom >$fn 2>/dev/null`;

    # Encode and decode.
    my $enc = `./rice encode $fn $fn.rice 1>/dev/null`;
    my $dec = `./rice decode $fn.rice $fn.rice.dat 1>/dev/null`;

    # Check correctness of decoded file.
    $ret = `diff $fn $fn.rice.dat`;
    if ($ret eq "") {
        print " passed\n";
    } else {
        print " NOT passed: $ret\n";
        exit -1;
    }

    # Print elapsed codec times.
    #print " (user time encoding: $enc)";
}

exit 0;

