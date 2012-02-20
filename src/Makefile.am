AUTOMAKE_OPTIONS = no-dependencies
lib_LTLIBRARIES = libcabocha.la
EXTRA_DIST = Makefile.msvc.in make.bat normalizer.rule normalizer_rule.sh
pkglibexecdir = ${libexecdir}/cabocha
INCLUDES = -DCABOCHA_DEFAULT_POSSET="\"$(POSSET)"\" -DCABOCHA_DEFAULT_CHARSET="\"$(CHARSET)"\" -DMODEL_VERSION=$(MODEL_VERSION) $(CABOCHA_USE_UTF8_ONLY) -DCABOCHA_DEFAULT_RC="\"$(CABOCHA_DEFAULT_RC)\""
libcabocha_la_LDFLAGS = -no-undefined -version-info $(LTVERSION)
libcabocha_la_SOURCES = analyzer.h cabocha.h char_category.h chunk_learner.cpp chunker.cpp chunker.h \
	common.h darts.h dep.cpp dep.h dep_learner.cpp eval.cpp freelist.h \
	learner.cpp libcabocha.cpp mmap.h morph.cpp morph.h ne.cpp ne.h normalizer.cpp \
	normalizer.h normalizer_rule.h param.cpp param.h parser.cpp scoped_ptr.h \
	selector.cpp tree_allocator.cpp \
	selector.h selector_pat.h stream_wrapper.h string_buffer.cpp string_buffer.h svm.cpp \
	svm.h svm_learn.cpp svm_learn.h  timer.h  tree.cpp  tree_allocator.h ucs.cpp ucs.h \
	ucstable.h utils.cpp utils.h

include_HEADERS = cabocha.h
bin_PROGRAMS    = cabocha
pkglibexec_PROGRAMS = cabocha-model-index cabocha-learn cabocha-system-eval

cabocha_model_index_SOURCES = cabocha-model-index.cpp
cabocha_model_index_LDADD = libcabocha.la

cabocha_system_eval_SOURCES = cabocha-system-eval.cpp
cabocha_system_eval_LDADD = libcabocha.la

cabocha_learn_SOURCES = cabocha-learn.cpp
cabocha_learn_LDADD = libcabocha.la

cabocha_SOURCES = cabocha.cpp
cabocha_LDADD = libcabocha.la
