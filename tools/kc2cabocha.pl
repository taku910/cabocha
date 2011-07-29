#!/usr/bin/perl

# $Id: kc2cabocha.pl.in,v 1.1 2003/03/28 15:37:27 taku-ku Exp $;

use MeCab;
use KyotoCorpus;
  
my $mecab = new MeCab::Tagger("");

for my $file (@ARGV) {

    my $kc = new KyotoCorpus ($file);

    while ($kc->parse ()) {
        my $size = $kc->getChunkSize ();
	my @chunk;
	my $sentence;
	for (my $i = 0; $i < $size; ++$i) {
	    $sentence .= $kc->getChunkSurface ($i);
	    push @chunk, [ ( $kc->getChunkSurface ($i), 
			     $i,
			     $kc->getDep ($i),
			     $kc->getType ($i) ) ];
	}

	my @morph;
	for (split "\n", $mecab->parse($sentence)) {
	    my @t = split /\t/, $_, 2;
	    push @morph, [ ($t[0], $_) ];
	}

	my $cc = "";
	my $cm = "";
	my @result;
	my $cid = 0;
	my @mstack;
	my @cstack;

	while (@morph) {
	    if ($cc eq "" && $cm eq "") {
		my @c = @{ shift @chunk }; 
		my @m = @{ shift @morph };
		$cc = $c[0];
		$cm = $m[0];
		push @mstack, $m[1];
		push @cstack, [ @c ];
	    } elsif (length ($cc) == length ($cm)) {
		my @head = @{ $cstack [ $#cstack] };
		$result[$cid]->{_morph} =  [ @mstack ];
		$result[$cid]->{_src}  = $head[1];
		$result[$cid]->{_dep}  = $head[2];
		$result[$cid]->{_type} = $head[3];
		
		$cid++;
		$cc = "";
		$cm = "";
		@mstack = ();
		@cstack = ();

	    } elsif (length ($cc) > length ($cm)) {
		my @m = @{ shift @morph };
		$cm .= $m[0];
		push @mstack, $m[1];
	    } elsif (length ($cc) < length ($cm)) {
		my @c = @{ shift @chunk };
		push @cstack, [ @c ];
		$cc .= $c[0];
	    }
	} 

	die "FATAL: Cannot align sentence\n" if ($cm ne "EOS");

	for my $i (0.. $#result) {
	    my $d = $result[$i]->{_dep};
	    my $to = -1;
	    if ($d != -1) {
		for (my $j = 0; $j <= $#result; $j++) {
		    if ($result[$j]->{_src} >= $d) {
			$to = $j;
			last;
		    }
		}
	    }
	    printf "* %d %d%s\n", $i, $to, $result[$i]->{_type};
	    my @morph = @ { $result[$i]->{_morph} };
	    for (@morph) { print "$_\n"; }
	}

	print "EOS\n";
    }
}
