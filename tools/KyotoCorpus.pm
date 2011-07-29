package KyotoCorpus;
use IO::File;

sub new ()
{
    my ($self, $file) = @_;  
    $self = {};
    $self->{_file} = _open ($file);
    $self->{_chunk} = [];
    $self->{_morph} = [];
    bless ($self);
}

sub getChunkMorphList ()
{
    my ($self, $cid) = @_;
    return @{$self->{_chunk}[$cid]->{_morph}};
}

sub getChunkSurface ()
{
    my ($self, $cid) = @_;
    my @morph = @{$self->{_chunk}[$cid]->{_morph}};
    my $result;
    for my $i (0..$#morph) {
	$result .= (split /\s+/, $morph[$i])[0];
    }
    return $result;
}

sub getMorph ()
{
    my ($self, $mid) = @_;
    return @{$self->{_morph}[$mid]};
}

sub getMorphSize ()
{
    my ($self) = @_;
    return scalar (@{$self->{_morph}});
}

sub getChunkSize ()
{
    my ($self) = @_;
    return scalar (@{$self->{_chunk}});
}

sub getDep ()
{
    my ($self, $cid) = @_;
    return $self->{_chunk}[$cid]->{_dep};
}

sub getType ()
{
    my ($self, $cid) = @_;
    return $self->{_chunk}[$cid]->{_type};
}

sub getHead ()
{
    my ($self, $cid) = @_;
    return $self->{_chunk}[$cid]->{_head};
}

sub getFunc ()
{
    my ($self, $cid) = @_;
    return $self->{_chunk}[$cid]->{_func};
}

sub getScore ()
{
    my ($self, $cid) = @_;
    return $self->{_chunk}[$cid]->{_score};
}

sub getFeature ()
{
    my ($self, $cid) = @_;
    return $self->{_chunk}[$cid]->{_feature};
}

sub parse ()
{
    my ($self) = @_;

    my @chunk;
    my @morph;
    my @morph2;
    my $id = 0;
    my $fh = $self->{_file};

    while (1) {
        return 0 unless (defined ($_ = <$fh>));
	chomp;
	if (/^EOS$/) {
	    $chunk[$id-1]->{_morph} = [@morph] if ($id);
	    $ok = 0;
	    last;
	} elsif (/^\*/) {
	    my @t = split /\s+/, $_;
	    die "Format Error: $_" if ($t[1] !~ /\d+/ );
	    if ($t[2] =~ /^(-?\d+)([A-Z])?$/) {
		$chunk[$id]->{_dep} = $1;
		$chunk[$id-1]->{_morph} = [@morph] if ($id);
		$chunk[$id]->{_type} = $2;
		$chunk[$id]->{_score} = 0;
		$chunk[$id]->{_head} = 0;
		$chunk[$id]->{_func} = 0;
		$chunk[$id]->{_feature} = [];
	    } else {
		die "Format Error: $_\n";
	    }

	    if ($t[3] =~ /^(\d+)\/(\d+)$/) { # cabocah -O3 && -O4
		$chunk[$id]->{_head} = $1;
		$chunk[$id]->{_func} = $2;
	    }

	    if (scalar (@t) >= 6) { # cabocha -O3
		$chunk[$id]->{_feature} = [splice (@t, 4, $#t)];
		$chunk[$id]->{_score}   = 0;
	    } else { # cabocha -O4
		$chunk[$id]->{_score}   = $t[4];
	    }

	    $id++;
	    @morph = ();
	} else {
	    push @morph, $_;
	    push @morph2, $_;
	}
    }

    $self->{_chunk} = [@chunk];
    $self->{_morph} = [@morph2];

    return 1;
}

sub _open 
{
    my ($file) = @_;
    my $fh;
    $fh = new IO::File;
    $fh->open("nkf -w $file|") || die "FATAL: [$file] cannot open.\n";
    return $fh;
}

1;
