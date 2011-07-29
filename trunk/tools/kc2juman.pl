#!/usr/bin/perl

my $tag = "";
while(<>) {
    chomp;
    next if (/^\#/);
    if (/^EOS$/) {
       print "EOS\n";
       next;
    } 
    if (/^\*/) {
	print "$_\n";
	next;
    }
    my @a = split();
#    日 にち * 接尾辞 名詞性名詞助数辞 * *
    $a[2] = $a[0] if ($a[2] eq "*");
    print "$a[0]\t$a[3],$a[4],$a[5],$a[6],$a[2],$a[1],*\n";
}
