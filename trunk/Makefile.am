# Process this file with automake to produce Makefile.in
#
# $Id: Makefile.am 1460 2005-07-21 13:31:24Z taku-ku $;
#
@SET_MAKE@
AUTOMAKE_OPTIONS = 1.4 no-dependencies
SUBDIRS = src model man
confdir = $(sysconfdir)
conf_DATA = cabocharc
EXTRA_DIR = perl ruby python java swig tools example doc
bin_SCRIPTS = cabocha-config

dist-hook:
	for subdir in $(EXTRA_DIR); do \
	  cp -rp $$subdir $(distdir); \
	  rm -f $(distdir)/$$subdir/*~; \
	  rm -f $(distdir)/$$subdir/*.{bak,orig}; \
	  rm -rf $(distdir)/$$subdir/CVS; \
	  rm -rf $(distdir)/$$subdir/*/CVS; \
	  rm -rf $(distdir)/$$subdir/.svn; \
	  rm -rf $(distdir)/.svn; \
	  rm -rf $(distdir)/*/.svn; \
	  rm -rf $(distdir)/*/*/.svn; \
	  rm -rf $(distdir)/*/*/*/.svn; \
	  rm -rf $(distdir)/*/*/*/*/.svn; \
	  rm -rf $(distdir)/$$subdir/*/CVS; \
	done

win-dist:
	rm -fr @PACKAGE@-@VERSION@.exe wintmp
	mkdir wintmp
	cp -f example/*.c example/*.cpp src/cabocha.h wintmp
	mkdir wintmp/model
	for txt in model/*.ipa.txt; do \
	  nkf -s $$txt > wintmp/$$txt; \
	done
	perl -pe 's#\@prefix\@/lib/cabocha/model/#\$$(rcpath)\\..\\model\\#g; \
                  s#\@CHARSET\@#SHIFT-JIS#g; \
                  s/# charset-file/charset-file/g;' \
		< cabocharc.in | nkf -sc > wintmp/cabocharc
	(cd wintmp; qkc -s -m *.cpp *.c *.h cabocharc)
	ISCC.exe cabocha.iss
	rm -fr wintmp

script-clean:
	(cd perl; $(MAKE) clean; rm -f Makefile.old Makefile)
	(cd ruby; $(MAKE) clean; rm -f Makefile *.log)
	(cd python; rm -fr build *.pyc)
	(cd java; $(MAKE) clean)

dist-all-package:
	(test -f Makefile) && $(MAKE) distclean
	./configure
	$(MAKE) dist
	$(MAKE) clean
	$(MAKE) win
	$(MAKE) win-dist

export-package:
	./upload.pl -p cabocha -n cabocha        -r @VERSION@ -f cabocha-@VERSION@.tar.bz2
	./upload.pl -p cabocha -n cabocha-win32  -r @VERSION@ -f cabocha-@VERSION@.exe
