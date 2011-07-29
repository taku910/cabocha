require 'mkmf'

cabocha_config = with_config('cabocha-config', 'cabocha-config')
use_cabocha_config = enable_config('cabocha-config')

`cabocha-config --libs-only-l`.chomp.split.each { | lib |
  have_library(lib)
}

$CFLAGS += ' ' + `#{cabocha_config} --cflags`.chomp

have_header('cabocha.h') && create_makefile('CaboCha')
