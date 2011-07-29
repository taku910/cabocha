use ExtUtils::MakeMaker;
WriteMakefile(
    'NAME'		=> 'CaboCha',
    'CC'                => `cabocha-config --cxx`,
    'LD'                => `cabocha-config --cxx`,
    'INC'               => `cabocha-config --cflags`,
    'LIBS'              => `cabocha-config --libs`,
    'VERSION'	        => `cabocha-config --version`,
    'OBJECT'            => 'CaboCha_wrap.o' 
);
