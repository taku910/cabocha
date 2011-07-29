modeldir   = @prefix@/lib/cabocha/model
model_DATA = @CABOCHA_MODEL_LIST@
EXTRA_DIST = @CABOCHA_TXTMODEL_LIST@
CLEANFILES = *.model *.model.tmp
SUFFIXES = .txt .model
CHARSET = @CHARSET@

@CABOCHA_MODEL_LIST@:
	../src/cabocha-model-index -f UTF8 -t $(CHARSET) $*.txt $@
