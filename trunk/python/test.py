#!/usr/bin/python
# -*- coding: utf-8 -*-

import CaboCha

# c = CaboCha.Parser("");
c = CaboCha.Parser()

sentence = "太郎はこの本を二郎を見た女性に渡した。"

print c.parseToString(sentence)

tree =  c.parse(sentence)

print tree.toString(CaboCha.FORMAT_TREE)
print tree.toString(CaboCha.FORMAT_LATTICE)
