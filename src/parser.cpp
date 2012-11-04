// CaboCha -- Yet Another Japanese Dependency Parser
//
//  $Id: parser.cpp 50 2009-05-03 08:25:36Z taku-ku $;
//
//  Copyright(C) 2001-2008 Taku Kudo <taku@chasen.org>
#include <crfpp.h>
#include <string>
#include <vector>
#include "cabocha.h"
#include "chunker.h"
#include "common.h"
#include "dep.h"
#include "freelist.h"
#include "morph.h"
#include "ne.h"
#include "param.h"
#include "scoped_ptr.h"
#include "selector.h"
#include "stream_wrapper.h"
#include "utils.h"

namespace {

const CaboCha::Option long_options[] = {
  {"output-format",   'f', 0, "TYPE",
   "set output format style\n\t\t\t    "
   "0 - tree(default)\n\t\t\t    "
   "1 - lattice\n\t\t\t    2 - tree + lattice\n\t\t\t    3 - XML"},
  {"input-layer",     'I', 0,
   "LAYER", "set input layer\n\t\t\t    "
   "0 - raw sentence layer(default)\n\t\t\t    "
   "1 - POS tagged layer\n\t\t\t    "
   "2 - POS tagger and Chunked layer\n\t\t\t    "
   "3 - POS tagged, Chunked and Feature selected layer"},
  {"output-layer",    'O', 0,
   "LAYER", "set output layer\n\t\t\t    "
   "1 - POS tagged layer\n\t\t\t    "
   "2 - POS tagged and Chunked layer\n\t\t\t    "
   "3 - POS tagged, Chunked and Feature selected layer\n\t\t\t    "
   "4 - Parsed layer(default)"},
  {"ne",              'n', 0, "MODE",
   "output NE tag\n\t\t\t    "
   "0 - without NE(default)\n\t\t\t    "
   "1 - output NE with chunk constraint\n\t\t\t    "
   "2 - output NE without chunk constraint" },
  {"parser-model",    'm', 0, "FILE", "use FILE as parser model file"},
  {"chunker-model",   'M', 0, "FILE", "use FILE as chunker model file"},
  {"ne-model",        'N', 0, "FILE", "use FILE as NE tagger model file"},
  //  {"beam",
  // 'B', "1", "INT", "set beam width for tournament model" },
  { "posset",         'P',  CABOCHA_DEFAULT_POSSET, "STR",
    "make posset of morphological analyzer (default "
    CABOCHA_DEFAULT_POSSET ")" },
  { "charset",        't',  CABOCHA_DEFAULT_CHARSET, "ENC",
    "make charset of binary dictionary ENC (default "
    CABOCHA_DEFAULT_CHARSET ")" },
  { "charset-file",    'T',  0, "FILE", "use charset written in FILE" },
  { "rcfile",          'r', 0, "FILE", "use FILE as resource file" },
  { "mecabrc",         'b', 0, "FILE", "use FILE as mecabrc"},
  { "mecab-dicdir",    'd', 0, "DIR",  "use DIR as mecab dictionary directory"},
  { "output",          'o', 0, "FILE", "use FILE as output file"},
  { "version",         'v', 0, 0, "show the version and exit"},
  { "help",            'h', 0, 0, "show this help and exit"},
  {0, 0, 0, 0}
};

static CaboCha::CharsetType get_charset(const CaboCha::Param &param,
                                        std::string &rcpath) {
  std::string filename = param.get<std::string>("charset-file");
  CaboCha::replace_string(&filename, "$(rcpath)", rcpath);
  if (!filename.empty()) {
    std::ifstream ifs(WPATH(filename.c_str()));
    std::string line;
    std::getline(ifs, line);
    if (!line.empty()) {
      return CaboCha::decode_charset(line.c_str());
    }
  }
  return CaboCha::decode_charset(param.get<std::string>("charset").c_str());
}

static std::string get_default_rc() {
#ifdef HAVE_GETENV
  const char *homedir = getenv("HOME");
  if (homedir) {
    std::string s = CaboCha::create_filename(std::string(homedir),
                                             ".cabocharc");
    std::ifstream ifs(WPATH(s.c_str()));
    if (ifs) {
      return s;
    }
  }

  const char *rcenv = getenv("CABOCHARC");
  if (rcenv) {
    return std::string(rcenv);
  }
#endif

#if defined (HAVE_GETENV) && defined(_WIN32) && !defined(__CYGWIN__)
  CaboCha::scoped_fixed_array<wchar_t, BUF_SIZE> buf;
  const DWORD len = GetEnvironmentVariable(L"CABOCHARC",
                                           buf.get(),
                                           buf.size());
  if (len < buf.size() && len > 0) {
    return CaboCha::WideToUtf8(buf.get());
  }
#endif

#if defined(_WIN32) && !defined(__CYGWIN__)
  const std::string r = CaboCha::get_windows_reg_value("software\\cabocha",
                                                       "cabocharc");
  if (!r.empty()) {
    return r;
  }
#endif

  return std::string(CABOCHA_DEFAULT_RC);
}
}

namespace CaboCha {

const char *getGlobalError();
void setGlobalError(const char *str);

class ParserImpl: public Parser {
 public:
  bool        open(Param *);
  bool        open(int, char**);
  bool        open(const char*);
  void        close();
  const Tree *parse(Tree *tree) const;
  const Tree *parse(const char*, size_t);
  const Tree *parse(const char*);
  const char *parseToString(const char*);
  const char *parseToString(const char*, size_t);
  const char *parseToString(const char*, size_t,
                            char*, size_t);
  const char *what() { return what_.str(); }
  const char *version();

  ParserImpl(): tree_(0),
                output_format_(FORMAT_TREE),
                input_layer_(INPUT_RAW_SENTENCE),
                output_layer_(OUTPUT_DEP),
                charset_(EUC_JP), posset_(IPA) {}
  virtual ~ParserImpl() { this->close(); }

 private:
  std::vector<Analyzer *> analyzer_;
  scoped_ptr<Tree>        tree_;
  FormatType              output_format_;
  InputLayerType          input_layer_;
  OutputLayerType         output_layer_;
  CharsetType             charset_;
  PossetType              posset_;
  whatlog                 what_;
};

bool ParserImpl::open(int argc, char **argv) {
  Param param;
  if (!param.open(argc, argv, long_options)) {
    WHAT << param.what();
    close();
    return false;
  }
  return open(&param);
}

bool ParserImpl::open(const char *arg) {
  Param param;
  if (!param.open(arg, long_options)) {
    WHAT << param.what();
    close();
    return false;
  }
  return open(&param);
}

#define REPLACE_PROFILE(p, r, k) do {                           \
    std::string tmp = (p)->get<std::string>(k);                 \
    replace_string(&tmp, "$(rcpath)", r);                       \
    (p)->set<std::string>(k, tmp.c_str(), true); } while (0)

#define PUSH_ANALYZER(T) do {                           \
    T *analyzer = new T;                                \
    CHECK_FALSE(analyzer) << "allocation failed";       \
    analyzer->set_charset(charset_);                    \
    analyzer->set_posset(posset_);                      \
    if (!analyzer->open(*param)) {                      \
      WHAT << analyzer->what();                         \
      delete analyzer;                                  \
      return false;                                     \
    }                                                   \
    analyzer_.push_back(analyzer);                      \
  } while (0)

bool ParserImpl::open(Param *param) {
  close();

  std::string rcfile = param->get<std::string>("rcfile");
  if (rcfile.empty()) {
    rcfile = get_default_rc();
  }

  if (!param->load(rcfile.c_str())) {
    WHAT << param->what();
    close();
    return false;
  }

  std::string rcpath = rcfile;
  remove_filename(&rcpath);

  REPLACE_PROFILE(param, rcpath, "parser-model");
  REPLACE_PROFILE(param, rcpath, "chunker-model");
  REPLACE_PROFILE(param, rcpath, "ne-model");
  REPLACE_PROFILE(param, rcpath, "chasenrc");

  close();

  input_layer_   =
      static_cast<InputLayerType>(param->get<int>("input-layer"));
  output_layer_  =
      static_cast<OutputLayerType>(param->get<int>("output-layer"));
  output_format_ =
      static_cast<FormatType>(param->get<int>("output-format"));

  const int action = param->get<int>("action-mode");
  const int ne     = param->get<int>("ne");
  if (action == TRAINING_MODE) {
    output_format_ = FORMAT_NONE;
  }

  if (output_layer_ != OUTPUT_DEP) {
    output_format_ = FORMAT_LATTICE;
  }

  charset_ = get_charset(*param, rcpath);
  posset_ = decode_posset(param->get<std::string>("posset").c_str());

  CHECK_FALSE(charset_ != -1);
  CHECK_FALSE(posset_ != -1);

  switch (input_layer_) {
    case INPUT_RAW_SENTENCE:  // case 1
      {
        switch (output_layer_) {
          case OUTPUT_POS:
            PUSH_ANALYZER(MorphAnalyzer);
            if (ne) PUSH_ANALYZER(NE);
            break;
          case OUTPUT_CHUNK:
            PUSH_ANALYZER(MorphAnalyzer);
            if (ne) PUSH_ANALYZER(NE);
            PUSH_ANALYZER(Chunker);
            break;
          case OUTPUT_SELECTION:
            PUSH_ANALYZER(MorphAnalyzer);
            if (ne) PUSH_ANALYZER(NE);
            PUSH_ANALYZER(Chunker);
            PUSH_ANALYZER(Selector);
            break;
          case OUTPUT_DEP:
            PUSH_ANALYZER(MorphAnalyzer);
            if (ne) PUSH_ANALYZER(NE);
            PUSH_ANALYZER(Chunker);
            PUSH_ANALYZER(Selector);
            PUSH_ANALYZER(DependencyParser);
            break;
          default:
            break;
        }
        break;
      }

    case INPUT_POS:  // case 2
      {
        if (ne) PUSH_ANALYZER(NE);
        switch (output_layer_) {
          case OUTPUT_POS:
            break;
          case OUTPUT_CHUNK:
            PUSH_ANALYZER(Chunker);
            break;
          case OUTPUT_SELECTION:
            PUSH_ANALYZER(Chunker);
            PUSH_ANALYZER(Selector);
            break;
          case OUTPUT_DEP:
            PUSH_ANALYZER(Chunker);
            PUSH_ANALYZER(Selector);
            PUSH_ANALYZER(DependencyParser);
            break;
          default:
            break;
        }
        break;
      }

    case INPUT_CHUNK:  // case 3
      {
        switch (output_layer_) {
          case OUTPUT_POS:
          case OUTPUT_CHUNK:
            break;
          case OUTPUT_SELECTION:
            PUSH_ANALYZER(Selector);
            break;
          case OUTPUT_DEP:
            PUSH_ANALYZER(Selector);
            PUSH_ANALYZER(DependencyParser);
            break;
          default:
            break;
        }
        break;
      }

    case  INPUT_SELECTION:  // case 4
      {
        switch (output_layer_) {
          case OUTPUT_POS:
          case OUTPUT_CHUNK:
          case OUTPUT_SELECTION:
            break;
          case OUTPUT_DEP:
            PUSH_ANALYZER(DependencyParser);
            break;
          default:
            break;
        }
        break;
      }

    default:
      break;
  }

  return true;
}

void ParserImpl::close() {
  for (size_t i = 0; i < analyzer_.size(); ++i) {
    delete analyzer_[i];
  }
  analyzer_.clear();
  output_format_ = FORMAT_TREE;
  input_layer_ = INPUT_RAW_SENTENCE;
  output_layer_ = OUTPUT_DEP;
}

const Tree *ParserImpl::parse(Tree *tree) const {
  if (!tree) {
    return 0;
  }
  tree->set_charset(charset_);
  tree->set_posset(posset_);
  tree->set_output_layer(output_layer_);
  for (size_t i = 0; i < analyzer_.size(); ++i) {
    if (!analyzer_[i]->parse(tree)) {
      return 0;
    }
  }
  return const_cast<const Tree *> (tree);
}

const Tree* ParserImpl::parse(const char *str, size_t len) {
  if (!str) {
    WHAT << "NULL pointer is given";
    return 0;
  }
  // Set charset/posset, bacause Tree::read() may depend on
  // these parameters.
  if (!tree_.get()) {
    tree_.reset(new Tree);
  }

  tree_->set_charset(charset_);
  tree_->set_posset(posset_);

  if (!tree_->read(str, len, input_layer_)) {
    WHAT << "format error: [" << str << "] ";
    return 0;
  }

  if (!parse(tree_.get())) {
    WHAT << tree_->what();
    return 0;
  }

  return tree_.get();
}

const Tree* ParserImpl::parse(const char *str) {
  return parse(str, std::strlen(str));
}

const char *ParserImpl::parseToString(const char *str, size_t len,
                                      char *out, size_t len2) {
  if (!str) {
    WHAT << "NULL pointer is given";
    return 0;
  }
  if (!parse(str, len)) {
    return 0;
  }
  return tree_->toString(output_format_, out, len2);
}

const char *ParserImpl::parseToString(const char* str, size_t len) {
  if (!str) {
    WHAT << "NULL pointer is given";
    return 0;
  }
  if (!parse(str, len)) {
    return 0;
  }
  return tree_->toString(output_format_);
}

const char *ParserImpl::parseToString(const char* str) {
  return parseToString(str, std::strlen(str));
}

Parser *Parser::create(int argc, char **argv) {
  return createParser(argc, argv);
}

Parser *Parser::create(const char *arg) {
  return createParser(arg);
}

const char *Parser::version() {
  return VERSION;
}

Parser *createParser(int argc, char **argv) {
  ParserImpl *parser = new ParserImpl();
  if (!parser->open(argc, argv)) {
    setGlobalError(parser->what());
    delete parser;
    return 0;
  }
  return parser;
}

Parser *createParser(const char *argv) {
  ParserImpl *parser = new ParserImpl();
  if (!parser->open(argv)) {
    setGlobalError(parser->what());
    delete parser;
    return 0;
  }
  return parser;
}

const char *getLastError() {
  return getGlobalError();
}

const char *getParserError() {
  return getGlobalError();
}
}

#define WHAT_ERROR(msg) {                       \
    std::cout << msg << std::endl;              \
    return EXIT_FAILURE; }

int cabocha_do(int argc, char **argv) {
  CaboCha::ParserImpl parser;
  CaboCha::Param param;

  param.open(argc, argv, long_options);

  if (!param.help_version()) {
    return EXIT_SUCCESS;
  }

  std::string ofilename = param.get<std::string>("output");
  if (ofilename.empty()) {
    ofilename = "-";
  }

  CaboCha::ostream_wrapper ofs(ofilename.c_str());
  if (!*ofs) {
    WHAT_ERROR("no such file or directory: " << ofilename);
  }

  if (!parser.open(&param)) {
    std::cout << parser.what() << std::endl;
    std::exit(EXIT_FAILURE);
  }

  const std::vector <std::string>& rest_ = param.rest_args();
  std::vector<std::string> rest = rest_;

  if (rest.empty()) {
    rest.push_back("-");
  }

  int input_layer = param.get<int>("input-layer");
  std::string input;

  for (size_t i = 0; i < rest.size(); ++i) {
    CaboCha::istream_wrapper ifs(rest[i].c_str());

    if (!*ifs) {
      WHAT_ERROR("no such file or directory: " << rest[i]);
    }

    while (true) {
      if (!CaboCha::read_sentence(ifs.get(), &input, input_layer)) {
        std::cerr << "too long line #line must be <= "
                  << CABOCHA_MAX_LINE_SIZE;
        return false;
      }

      if (ifs->eof() && input.empty()) {
        return false;
      }

      if (ifs->fail()) {
        std::cerr << "input-beffer overflow. "
                  << "The line is splitted. use -b #SIZE option."
                  << std::endl;
        ifs->clear();
      }

      const char *r = parser.parseToString(input.c_str(), input.size());
      if (!r) {
        WHAT_ERROR(parser.what());
      }
      *ofs << r << std::flush;
    }
  }

  return EXIT_SUCCESS;

#undef WHAT_ERROR
}
