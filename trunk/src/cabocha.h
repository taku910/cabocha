/* CaboCha -- Yet Another Japanese Dependency Parser
   $Id: cabocha.h 50 2009-05-03 08:25:36Z taku-ku $;
   Copyright(C) 2001-2008 Taku Kudo <taku@chasen.org>
*/
#ifndef CABOCHA_CABOCHA_H_
#define CABOCHA_CABOCHA_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#ifdef _WIN32
#  ifdef DLL_EXPORT
#    define CABOCHA_DLL_EXTERN    __declspec(dllexport)
#  else
#    ifdef  DLL_IMPORT
#      define CABOCHA_DLL_EXTERN  __declspec(dllimport)
#    endif
#  endif
#endif

#ifndef CABOCHA_DLL_EXTERN
#  define CABOCHA_DLL_EXTERN extern
#endif

  enum cabocha_charset_t {
    EUC_JP, CP932, UTF8, ASCII
  };
  enum cabocha_posset_t  {
    IPA, JUMAN, UNIDIC
  };

  enum cabocha_format_t {
    FORMAT_TREE,
    FORMAT_LATTICE,
    FORMAT_TREE_LATTICE,
    FORMAT_XML,
    FORMAT_NONE
  };

  enum cabocha_input_layer_t {
    INPUT_RAW_SENTENCE,
    INPUT_POS,
    INPUT_CHUNK,
    INPUT_SELECTION,
    INPUT_DEP
  };

  enum cabocha_output_layer_t {
    OUTPUT_RAW_SENTENCE,
    OUTPUT_POS,
    OUTPUT_CHUNK,
    OUTPUT_SELECTION,
    OUTPUT_DEP
  };

  enum cabocha_parser_t {
    TRAIN_NE,
    TRAIN_CHUNK,
    TRAIN_DEP
  };

  struct cabocha_t;
  struct cabocha_tree_t;
  struct mecab_node_t;

  struct cabocha_chunk_t {
    int                    link;
    unsigned short int     head_pos;
    unsigned short int     func_pos;
    unsigned short int     token_size;
    size_t                 token_pos;
    float                  score;
    const char             **feature_list;
    unsigned short int     feature_list_size;
  };

  struct cabocha_token_t {
    const char              *surface;
    const char              *normalized_surface;
    const char              *feature;
    const char             **feature_list;
    unsigned short int      feature_list_size;
    const char              *ne;
    struct cabocha_chunk_t  *chunk;
  };

  typedef struct cabocha_t  cabocha_t;
  typedef struct cabocha_tree_t  cabocha_tree_t;
  typedef struct cabocha_chunk_t cabocha_chunk_t;
  typedef struct cabocha_token_t cabocha_token_t;
  typedef struct mecab_node_t mecab_node_t;

  typedef enum cabocha_charset_t cabocha_charset_t;
  typedef enum cabocha_posset_t cabocha_posset_t;
  typedef enum cabocha_format_t cabocha_format_t;
  typedef enum cabocha_input_layer_t cabocha_input_layer_t;
  typedef enum cabocha_output_layer_t cabocha_output_layer_t;
  typedef enum cabocha_parser_t cabocha_parser_t;

#ifndef SWIG
  CABOCHA_DLL_EXTERN int                    cabocha_do(int argc, char **argv);

  /* parser */
  CABOCHA_DLL_EXTERN cabocha_t             *cabocha_new(int argc, char **argv);
  CABOCHA_DLL_EXTERN cabocha_t             *cabocha_new2(const char *arg);
  CABOCHA_DLL_EXTERN const char            *cabocha_strerror(cabocha_t* cabocha);
  CABOCHA_DLL_EXTERN const char            *cabocha_sparse_tostr(cabocha_t* cabocha,
                                                                 const char* str);
  CABOCHA_DLL_EXTERN const char            *cabocha_sparse_tostr2(cabocha_t* cabocha,
                                                                  const char* str, size_t lenght);
  CABOCHA_DLL_EXTERN const char            *cabocha_sparse_tostr3(cabocha_t* cabocha, const char* str, size_t length,
                                                                  char *output_str, size_t output_length);
  CABOCHA_DLL_EXTERN void                  cabocha_destroy(cabocha_t* cabocha);
  CABOCHA_DLL_EXTERN const cabocha_tree_t  *cabocha_sparse_totree(cabocha_t* cabocha, const char* str);
  CABOCHA_DLL_EXTERN const cabocha_tree_t  *cabocha_sparse_totree2(cabocha_t* cabocha, const char* str, size_t length);

  /* tree */
  CABOCHA_DLL_EXTERN cabocha_tree_t        *cabocha_tree_new();
  CABOCHA_DLL_EXTERN void                   cabocha_tree_destroy(cabocha_tree_t* tree);
  CABOCHA_DLL_EXTERN int                    cabocha_tree_empty(cabocha_tree_t* tree);
  CABOCHA_DLL_EXTERN void                   cabocha_tree_clear(cabocha_tree_t* tree);
  CABOCHA_DLL_EXTERN void                   cabocha_tree_clear_chunk(cabocha_tree_t* tree);
  CABOCHA_DLL_EXTERN size_t                 cabocha_tree_size(cabocha_tree_t* tree);
  CABOCHA_DLL_EXTERN size_t                 cabocha_tree_chunk_size(cabocha_tree_t* tree);
  CABOCHA_DLL_EXTERN size_t                 cabocha_tree_token_size(cabocha_tree_t* tree);
  CABOCHA_DLL_EXTERN const char            *cabocha_tree_sentence(cabocha_tree_t* tree);
  CABOCHA_DLL_EXTERN size_t                 cabocha_tree_sentence_size(cabocha_tree_t* tree);
  CABOCHA_DLL_EXTERN void                   cabocha_tree_set_sentence(cabocha_tree_t* tree,
                                                                      const char *sentence,
                                                                      size_t length);
  CABOCHA_DLL_EXTERN int                   cabocha_tree_read(cabocha_tree_t* tree,
                                                             const char *input,
                                                             size_t length,
                                                             cabocha_input_layer_t input_layer);
  CABOCHA_DLL_EXTERN int                   cabocha_tree_read_from_mecab_node(cabocha_tree_t* tree,
                                                                             const mecab_node_t *node);

  CABOCHA_DLL_EXTERN const cabocha_token_t *cabocha_tree_token(cabocha_tree_t* tree, size_t i);
  CABOCHA_DLL_EXTERN const cabocha_chunk_t *cabocha_tree_chunk(cabocha_tree_t* tree, size_t i);

  CABOCHA_DLL_EXTERN cabocha_token_t       *cabocha_tree_add_token(cabocha_tree_t* tree);
  CABOCHA_DLL_EXTERN cabocha_chunk_t       *cabocha_tree_add_chunk(cabocha_tree_t* tree);

  CABOCHA_DLL_EXTERN char                  *cabocha_tree_strdup(cabocha_tree_t* tree, const char *str);
  CABOCHA_DLL_EXTERN char                  *cabocha_tree_alloc(cabocha_tree_t* tree, size_t size);

  CABOCHA_DLL_EXTERN const char            *cabocha_tree_tostr(cabocha_tree_t* tree, cabocha_format_t format);
  CABOCHA_DLL_EXTERN const char            *cabocha_tree_tostr2(cabocha_tree_t* tree, cabocha_format_t format,
                                                                char *str, size_t length);

  CABOCHA_DLL_EXTERN void                   cabocha_tree_set_charset(cabocha_tree_t* tree,
                                                                     cabocha_charset_t charset);
  CABOCHA_DLL_EXTERN cabocha_charset_t      cabocha_tree_charset(cabocha_tree_t* tree);
  CABOCHA_DLL_EXTERN void                   cabocha_tree_set_posset(cabocha_tree_t* tree,
                                                                    cabocha_posset_t posset);
  CABOCHA_DLL_EXTERN cabocha_posset_t       cabocha_tree_posset(cabocha_tree_t* tree);
  CABOCHA_DLL_EXTERN void                   cabocha_tree_set_output_layer(cabocha_tree_t* tree,
                                                                          cabocha_output_layer_t output_layer);
  CABOCHA_DLL_EXTERN cabocha_output_layer_t cabocha_tree_output_layer(cabocha_tree_t* tree);

  CABOCHA_DLL_EXTERN int                    cabocha_learn(int argc, char **argv);
  CABOCHA_DLL_EXTERN int                    cabocha_system_eval(int argc, char **argv);
  CABOCHA_DLL_EXTERN int                    cabocha_model_index(int argc, char **argv);
#endif

#ifdef __cplusplus
}
#endif

/* for C++ */
#ifdef __cplusplus

namespace CaboCha {

class Tree;
typedef struct cabocha_chunk_t Chunk;
typedef struct cabocha_token_t Token;

typedef enum cabocha_charset_t CharsetType;
typedef enum cabocha_posset_t PossetType;
typedef enum cabocha_format_t FormatType;
typedef enum cabocha_input_layer_t InputLayerType;
typedef enum cabocha_output_layer_t OutputLayerType;
typedef enum cabocha_parser_t ParserType;

class TreeAllocator;

class Tree {
 public:
  void set_sentence(const char *sentence);
  const char *sentence() const;
  size_t sentence_size() const;

#ifndef SWIG
  void set_sentence(const char *sentence, size_t length);
#endif

  const Chunk *chunk(size_t i) const;
  const Token *token(size_t i) const;

#ifndef SWIG
  Chunk *mutable_chunk(size_t i);
  Token *mutable_token(size_t i);

  Token *add_token();
  Chunk *add_chunk();

  char *strdup(const char *str);
  char *alloc(size_t size);
  char **alloc_char_array(size_t size);

  TreeAllocator *allocator() const;
#endif

  bool   read(const char *input,
              InputLayerType input_layer);

#ifndef SWIG
  bool   read(const char *input, size_t length,
              InputLayerType input_layer);
  bool   read(const mecab_node_t *node);
#endif

  bool   empty() const;
  void   clear();
  void   clear_chunk();

  size_t chunk_size() const;
  size_t token_size() const;
  size_t size() const;

  const char *toString(FormatType output_format);

#ifndef SWIG
  const char *toString(FormatType output_format,
                       char *output, size_t length) const;
#endif

  CharsetType charset() const { return charset_; }
  void set_charset(CharsetType charset) { charset_ = charset; }
  PossetType posset() const { return posset_; }
  void set_posset(PossetType posset) { posset_ = posset; }
  OutputLayerType output_layer() const { return output_layer_; }
  void set_output_layer(OutputLayerType output_layer) { output_layer_ = output_layer; }

  const char *what();

  explicit Tree();
  virtual ~Tree();

 private:
  TreeAllocator              *tree_allocator_;
  CharsetType                 charset_;
  PossetType                  posset_;
  OutputLayerType             output_layer_;
};

class Parser {
 public:
  virtual const Tree *parse(const char *input)                          = 0;
  virtual const char *parseToString(const char *input)                  = 0;
  virtual const Tree *parse(Tree *tree) const                           = 0;

#ifndef SWIG
  virtual const Tree *parse(const char *input, size_t length)           = 0;
  virtual const char *parseToString(const char *input, size_t length)   = 0;
  virtual const char *parseToString(const char *input, size_t length,
                                    char       *output, size_t output_length) = 0;
#endif

  virtual const char *what() = 0;
  static const char *version();

  virtual ~Parser() {}

#ifndef SWIG
  static Parser *create(int argc, char **argv);
  static Parser *create(const char *arg);
#endif
};

CABOCHA_DLL_EXTERN Parser *createParser(int argc, char **argv);
CABOCHA_DLL_EXTERN Parser *createParser(const char *arg);
CABOCHA_DLL_EXTERN const char *getParserError();
}
#endif
#endif
