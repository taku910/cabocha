#!/bin/sh

TARGET=normalizer_rule_compiler
c++ -O3 -DHAVE_ICONV -DNORMALIZER_RULE_COMPILE normalizer.cpp utils.cpp ucs.cpp -o $TARGET
./$TARGET normalizer.rule normalizer_rule.h
rm -f $TARGET
