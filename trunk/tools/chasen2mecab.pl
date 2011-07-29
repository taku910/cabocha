#!/usr/bin/perl

# と      ト      と      助詞-格助詞-引用

while (<>) {
    chomp;
    if (/EOS/) {
	print "EOS\n";
    } elsif (/^\*/) {
	print "$_\n";
    } else {
	my @a = split /\t/, $_;
	my @b = split /-/, $a[3];
	while (@b < 4) {
	    push @b, "*";
	}
	$a[4] = $a[4] eq "" ? "*" : $a[4];
	$a[5] = $a[5] eq "" ? "*" : $a[5];
	print "$a[0]\t$b[0],$b[1],$b[2],$b[3],$a[4],$a[5],$a[2],$a[1]\t$a[6]\n";
    }
}
