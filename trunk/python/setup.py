#!/usr/bin/env python

from distutils.core import setup,Extension,os
import string

def cmd1(str):
    return os.popen(str).readlines()[0][:-1]

def cmd2(str):
    return string.split (cmd1(str))

setup(name = "cabocha-python",
	version = cmd1("cabocha-config --version"),
	py_modules=["CaboCha"],
	ext_modules = [
		Extension("_CaboCha",
			["CaboCha_wrap.cxx",],
			include_dirs=cmd2("cabocha-config --inc-dir"),
			library_dirs=cmd2("cabocha-config --libs-only-L"),
			libraries=cmd2("cabocha-config --libs-only-l"))
			])
