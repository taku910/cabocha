#!/usr/local/bin/perl

my $eucpre  = qr{(?<!\x8F)};
my $eucpost = qr{
    (?=
     (?:[\xA1-\xFE][\xA1-\xFE])* # JIS X 0208 ¤¬ 0Ê¸»ú°Ê¾åÂ³¤¤¤Æ
     (?:[\x00-\x7F\x8E\x8F]|\z)  # ASCII, SS2, SS3 ¤Þ¤¿¤Ï½ªÃ¼
     )
    }x;

my $KUTOUTEN      = "$eucpre(¡£|¡¢)$eucpost";
my $OPEN_BRACKET  = "$eucpre(¡Ê|¡Æ|¡È|¡Ô|¡Ö|¡Ø|¡Î|¡Ò|¡Ð)$eucpost";
my $CLOSE_BRACKET = "$eucpre(¡Ë|¡Ç|¡É|¡Õ|¡×|¡Ù|¡Ï|¡Ó|¡Ñ)$eucpost";
$| = 1;

my @seg;
my $seg_id;
while(<>) {
    chomp;
    if (/^#/) {
        next; 
    } elsif (/^\* (\d+) (-?\d+[ADPI])/) { # ID DEP TYPE
	$seg[$1]->{SEG}  = "";
	$seg[$1]->{DEP}  = $2;
	$seg_id = $1;
    } elsif (/^EOS$/ || /^\s*$/) {
	for my $i (0..$#seg) {
	    my @result = ();
	    my $str = $seg[$i]->{SEG};
	    my $dep = $seg[$i]->{DEP};
	    my ($a, $b) = &detect_head_func_pos(@{$seg[$i]->{MORPH}});
	    push @result, &detect_head_func($a,$b,@{$seg[$i]->{MORPH}});
	    push @result, &detect_misc($seg[$i]->{SEG});
	    push @result, "F_BOS:1" if ($i == 0);
	    push @result, "F_EOS:1" if ($i == $#seg);
	    print "* $i $dep $a/$b @result\n"; # featurs
	    print $seg[$i]->{ALL};          # others
	}
	print "EOS\n";
	@seg = ();
    } else {
	$seg[$seg_id]->{SEG} .= (split(/\s+/,$_))[0];
	$seg[$seg_id]->{ALL} .= "$_\n";
	push(@{$seg[$seg_id]->{MORPH}}, $_);
    }
}

sub detect_misc
{
    # misc
    my $chunk = $_[0];
    my @result;
    push @result, ("G_PUNC:$1", "F_PUNC:$1") if ($chunk =~ /$KUTOUTEN/);
    push @result, ("G_OB:$1"  , "F_OB:$1")   if ($chunk =~ /$OPEN_BRACKET/);
    push @result, ("G_CB:$1"  , "F_CB:$1")   if ($chunk =~ /$CLOSE_BRACKET/);
    return @result;
}

sub detect_head_func_pos
{
    my @morph  = @_;
    my $func_id = 0;
    my $head_id = 0;

    # search from end of chunk
    for my $i (0 .. $#morph) {
	my @tmp = split(/\s+/,$morph[$i]);
	if ($tmp[3] !~ /^ÆÃ¼ì$/) {
	    $func   = $morph[$i];
	    $func_id = $i;
	}
	
#	if ($tmp[3] !~ /^(ÆÃ¼ì|½õ»ì|ÀÜÈø¼­)$/) {
	if ($tmp[3] !~ /^(ÆÃ¼ì|½õ»ì)$/) {
	    $head   = $morph[$i];
	    $head_id = $i;
	}
    }
    
    return ($head_id, $func_id);
}
    
sub detect_head_func
{
    my $head_id = shift @_;
    my $func_id = shift @_;
    my @morph  = @_;
    my @result;

    # decied head and func
    my $head = $morph[$head_id];
    my $func = $morph[$func_id];
    die "FATAL: Cannot define head and func [$morph[0]] [$head][$func]\n" 
	if ($head eq "" || $func eq "");

    # head
    my @head_list = split(/\s+/,$head);
    my @func_list = split(/\s+/,$func);

    push @result, "F_H0:$head_list[0]" if ($head_list[0] ne "*");
    push @result, "F_H1:$head_list[3]" if ($head_list[3] ne "*");
    push @result, "F_H2:$head_list[4]" if ($head_list[4] ne "*");
    push @result, "F_H3:$head_list[5]" if ($head_list[5] ne "*");
    push @result, "F_H4:$head_list[6]" if ($head_list[6] ne "*");

    push @result, "F_F0:$func_list[0]" if ($func_list[0] ne "*");
    push @result, "F_F1:$func_list[3]" if ($func_list[3] ne "*");
    push @result, "F_F2:$func_list[4]" if ($func_list[4] ne "*");
    push @result, "F_F3:$func_list[5]" if ($func_list[5] ne "*");
    push @result, "F_F4:$func_list[6]" if ($func_list[6] ne "*");

    # case
    push @result, "G_CASE:$func_list[0]" if ($func_list[3] eq "½õ»ì");

    # single head
    if ($head_list[4] ne "*") {
	push @result, "B:$head_list[3]/$head_list[4]";
    } else {
	push @result, "B:$head_list[3]";
    }

    # single func
    if ($func_list[3]  =~ /^(½õ»ì|Éû»ì|Ï¢ÂÎ»ì|ÀÜÂ³»ì)$/) {
	push @result, "A:$func_list[0]";
    } elsif ($func_list[6] ne "*") {
        push @result, "A:$func_list[6]";
    } elsif ($func_list[3] ne "*" && $func_list[4] ne "*") {
	push @result, "A:$func_list[3]/$func_list[4]";
    } else {
        push @result, "A:$func_list[3]";
    }
    return @result;
}
