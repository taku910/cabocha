# $Id: test.pl 1076 2003-05-11 16:56:56Z taku-ku $;

use lib "../src/.libs";
use lib $ENV{PWD} . "/blib/lib";
use lib $ENV{PWD} . "/blib/arch";

my $sentence = "太郎はこの本を二郎を見た女性に渡した。";

use CaboCha;
# my $c = new CaboCha::Parser("-f2");
my $c = new CaboCha::Parser;
print $c->parseToString($sentence);
my $tree = $c->parse($sentence);

print $tree->toString($CaboCha::FORMAT_TREE);
print $tree->toString($CaboCha::FORMAT_LATTICE);

