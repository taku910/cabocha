%module CaboCha
%include exception.i
%{
#include "cabocha.h"
%}

%exception {
  try { $action }
  catch (char *e) { SWIG_exception (SWIG_RuntimeError, e); }
  catch (const char *e) { SWIG_exception (SWIG_RuntimeError, (char*)e); }
}

%rename(Chunk) cabocha_chunk_t;
%rename(Token) cabocha_token_t;
%rename(CharsetType) cabocha_charset_t;
%rename(PossetType) cabocha_posset_t;
%rename(FormatType) cabocha_format_t;
%rename(InputLayerType) cabocha_input_layer_t;
%rename(OutputLayerType) cabocha_output_layer_t;
%rename(ParserType)  cabocha_parser_t;
%nodefault cabocha_chunk_t;
%nodefault cabocha_token_t;

%feature("notabstract") CaboCha::Parser;

%immutable cabocha_chunk_t::link;
%immutable cabocha_chunk_t::head_pos;
%immutable cabocha_chunk_t::func_pos;
%immutable cabocha_chunk_t::token_size;
%immutable cabocha_chunk_t::token_pos;
%immutable cabocha_chunk_t::score;
%immutable cabocha_chunk_t::feature_list;
%immutable cabocha_chunk_t::feature_list_size;
%immutable cabocha_chunk_t::additional_info;

%immutable cabocha_token_t::surface;
%immutable cabocha_token_t::normalized_surface;
%immutable cabocha_token_t::feature;
%immutable cabocha_token_t::feature_list;
%immutable cabocha_token_t::ne;
%immutable cabocha_token_t::feature_list_size;
%immutable cabocha_token_t::chunk;
%immutable cabocha_token_t::additional_info;

%ignore CaboCha::createParser;
%ignore CaboCha::getParserError;

%extend cabocha_token_t {
  const char *feature_list(size_t i) {
    if (self->feature_list_size < i)
     throw "index is out of range";
    return self->feature_list[i];
  }
}

%extend cabocha_chunk_t {
  const char *feature_list(size_t i) {
    if (self->feature_list_size < i)
     throw "index is out of range";
    return self->feature_list[i];
  }
}

%extend CaboCha::Parser {
  Parser(const char *argc);
  Parser();
}

%{
void delete_CaboCha_Parser(CaboCha::Parser *t) {
  delete t;
  t = 0;
}

CaboCha::Parser* new_CaboCha_Parser(const char *arg) {
  CaboCha::Parser *parser = CaboCha::createParser(arg);
  if (!parser) throw CaboCha::getParserError();
  return parser;
}

CaboCha::Parser* new_CaboCha_Parser() {
  CaboCha::Parser *parser = CaboCha::createParser("");
  if (!parser) throw CaboCha::getParserError();
  return parser;
}

%}
%include ../src/cabocha.h
%include version.h
