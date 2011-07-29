#!/usr/bin/ruby

require 'CaboCha'
sentence = "太郎はこの本を二郎を見た女性に渡した。"

# c = CaboCha::Parser.new("");
c = CaboCha::Parser.new;
puts c.parseToString(sentence)
tree =  c.parse(sentence)

puts tree.toString(CaboCha::FORMAT_TREE);
puts tree.toString(CaboCha::FORMAT_LATTICE);

