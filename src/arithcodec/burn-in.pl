#!/usr/bin/perl

use strict;
use warnings;

my $MB = 1024 * 1024;
my $i_max = 100;
my @bytes = [];
my $fn = "tmp.dat";
my $fn_enc = $fn.".ac";
my $fn_dec = $fn_enc.".dat";
my $ret = "";

if (-e $fn || -e $fn_enc || -e $fn_dec) {
    print "Error: One of these files already exists: $fn $fn_enc $fn_dec\n";
    exit 0;
}

for (my $i = 1; $i <= $i_max; $i++) {
    my $bytes = int(rand($MB));
    print "Testing codec on $bytes bytes ...";

    # Generate file containing $i random bytes.
    $ret = `dd bs=1 count=$bytes </dev/urandom >$fn 2>/dev/null`;

    # Encode and decode.
    my $enc = `./arith $fn > $fn_enc 2>/dev/null`;
    my $dec = `./arith -d $fn_enc > $fn_dec 2>/dev/null`;

    # Check correctness of decoded file.
    $ret = `diff $fn $fn_dec`;
    if ($ret eq "") {
        print " passed\n";
    } else {
        print " NOT passed: $ret\n";
        cleanup();
        exit -1;
    }
}

cleanup();

exit 0;

sub cleanup {
    unlink $fn or warn "Could not unlink $fn: $!";
    unlink $fn_enc or warn "Could not unlink $fn_enc: $!";
    unlink $fn_dec or warn "Could not unlink $fn_dec: $!";
}

