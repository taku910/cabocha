#!/usr/bin/perl -w

use strict;
use MeCab;

my $mecab;

sub parseXML {
    my $str = shift @_;
    my $sentence;
    my %ne;
    my $len = 0;
    while ($str =~ s/^(.*?)\<(ORGANIZATION|PERSON|LOCATION|ARTIFACT|DATE|TIME|MONEY|PERCENT|OPTIONAL)[^\>]*\>(.+?)\<\/\2\>//) {
        $len += length($1);
        $ne{$len} = [ ($len + length ($3), $2) ] if ($2 ne "OPTIONAL"); # len, NE, type
        $len += length($3);
        $sentence .= $1;
        $sentence .= " $3 "; # force insert  space
    }
    $sentence  .= $str;

    if (!$mecab) {
        $mecab = new MeCab::Tagger("-d/usr/local/lib/mecab/dic/jumandic");
        die "MeCab load error\n" if (!$mecab);
    }

    my $in = 0;
    my $end = -1;
    my $begin = 0;
    my $type = "O";
    for my $line (split "\n", $mecab->parse($sentence)) {
        last if ($line =~ /EOS/);
        my ($suf, $fet) = split /\t/, $line;
        my $n = $ne{$begin};
        if (defined $n) {
            my ($e, $t) = @{$n};
            print "$suf\t$fet\tB-$t\n";
            $type = "I-$t";
            $end = $e;
        } else {
            if ($end <= $begin) {
                $type = "O";
            }
            print "$suf\t$fet\t$type\n";
        }
        $begin += length($suf);
    }
    print "EOS\n";
}

while (<>) {
    chomp;
    if (/^<DOCID>(\d+)<\/DOCID>/) {
#        print "# ID: $1\n";
    }
    if (/^<TEXT>/) {
        while (<>) {
            chomp;
            last if (/^<\/TEXT>/);
            &parseXML($_);
        }
    }
}
