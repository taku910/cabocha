# Process this file with automake to produce Makefile.in

html_HTMLS = cabocha.html
EXTRA_DIST = $(html_HTMLS) index.html cabocha.css libcabocha.html training.html \
             1st.png feature.png layer.png 1-in.txt 1-out-2-in.txt 2-out-3-in.txt \
             3-out-4-in.txt 4-out.txt IPA-sample.txt JUMAN-sample.txt cabocharc.txt \
             selector.pl

update-html:
	@for html in $(html_HTMLS); do \
		cmd=`basename $$html .html`.1 && \
		man2html ../man/$$cmd > $$html; \
	done
