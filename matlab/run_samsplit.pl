#!/usr/bin/perl

#
# This script runs 'samsplit' on the filenames read via STDIN.
#

use strict;
use warnings;

# Hash containing the column numbers for the SAM fields
my %sam_fields = ("qname" => 1, 
                  "flag"  => 2, 
                  "rname" => 3,
                  "pos"   => 4,
                  "mapq"  => 5,
                  "cigar" => 6,
                  "rnext" => 7,
                  "pnext" => 8,
                  "tlen"  => 9,
                  "seq"   => 10,
                  "qual"  => 11,
                  "flag"  => 12,);

# Fields to extract from SAM files
my @fields = qw(pos cigar seq qual);

while (<>) {
    chomp;
    print "Processing: $_\n";
    
    my @samsplit_files = ();
    foreach my $field (@fields) {
        push(@samsplit_files, "$file.$field"); #< generate name for output file
        my $samsplit_command = "samsplit --field=$sam_fields{$field} $file > $samsplit_files[-1]";
        print "Executing: $samsplit_command\n";
        system($samsplit_command) == 0 or die "'system' failed: $?";
        print "Created: $samsplit_files[-1]\n";
    }
}

exit 0;
