// CaboCha -- Yet Another Japanese Dependency Parser
//
//  $Id: learner.cpp 50 2009-05-03 08:25:36Z taku-ku $;
//
//  Copyright(C) 2001-2008 Taku Kudo <taku@chasen.org>
#include <crfpp.h>
#include <string>
#include <vector>
#include <fstream>
#include "cabocha.h"
#include "param.h"
#include "common.h"
#include "utils.h"
#include "ucs.h"
#include "svm.h"

namespace {
using namespace CaboCha;

ParserType guess_text_model_type(const char *filename) {
  std::ifstream ifs(WPATH(filename));
  CHECK_DIE(ifs);
  std::string line;
  int line_num = 0;
  while (std::getline(ifs, line)) {
    if (line.empty()) {
      break;
    }
    if (line == "type: dep") {
      return TRAIN_DEP;
    }
    if (++line_num >= 20) {
      break;
    }
  }

  return TRAIN_NE;
}

const std::string convert_character_encoding(const char *filename,
                                             Iconv *iconv) {
  if (iconv->from() == iconv->to()) {
    return std::string(filename);
  }

  const std::string new_filename = std::string(filename) + ".tmp";
  std::string line;
  std::ifstream ifs(WPATH(filename));
  CHECK_DIE(ifs) << "no such file or directory: " << filename;
  std::ofstream ofs(WPATH(new_filename.c_str()));
  CHECK_DIE(ifs) << "permission denied: " << new_filename;

  while (std::getline(ifs, line)) {
    std::string original = line;
    if (!iconv->convert(&line)) {
      std::cerr << "iconv conversion failed. skip this entry:"
                << original << std::endl;
      line = original;
      continue;
    }
    ofs << line << "\n";
  }

  return new_filename;
}
}

using namespace CaboCha;

int cabocha_model_index(int argc, char **argv) {
  static const CaboCha::Option long_options[] = {
    {"sigma",    's', "0.0001",  "FLOAT",
     "set minimum feature weight for PKE approximation (default 0.0001)" },
    {"minsup",   'n', "2",  "INT",
     "set minimum frequency support for PKE approximation (default 1)" },
    { "charset",   't',  CABOCHA_DEFAULT_CHARSET, "ENC",
      "make charset of binary dictionary ENC (default "
      CABOCHA_DEFAULT_CHARSET ")" },
    { "model-charset",  'f',  CABOCHA_DEFAULT_CHARSET,
      "ENC", "assume charset of input text-model as ENC (default "
      CABOCHA_DEFAULT_CHARSET ")" },
    {"version",  'v', 0,        0,       "show the version and exit" },
    {"help",     'h', 0,        0,       "show this help and exit" },
    {0, 0, 0, 0, 0}
  };

  CaboCha::Param param;
  param.open(argc, argv, long_options);

  if (!param.help_version()) {
    return 0;
  }

  const std::vector<std::string> &rest = param.rest_args();
  if (rest.size() != 2) {
    std::cout << param.help();
    return 0;
  }

  const std::string from = param.get<std::string>("model-charset");
  const std::string to =   param.get<std::string>("charset");
  const std::string input = rest[0];
  const ParserType type = guess_text_model_type(input.c_str());

  Iconv iconv;
  CHECK_DIE(iconv.open(from.c_str(), to.c_str()))
      << "iconv_open() failed with from=" << from << " to=" << to;

  if (type == TRAIN_DEP) {
    const double sigma = param.get<double>("sigma");
    const size_t minsup = param.get<size_t>("minsup");
    CHECK_DIE(CaboCha::FastSVMModel::compile(input.c_str(),
                                             rest[1].c_str(),
                                             sigma,
                                             minsup,
                                             &iconv));
  } else if (type == TRAIN_NE) {
    std::string tmp_input = convert_character_encoding(input.c_str(),
                                                       &iconv);
    std::vector<const char*> argv;
    argv.push_back("CRF++");
    argv.push_back("--convert");
    argv.push_back(tmp_input.c_str());
    argv.push_back(rest[1].c_str());
    CHECK_DIE(0 == crfpp_learn(argv.size(),
                               const_cast<char **>(&argv[0])))
        << "crfpp_learn execution error";
    if (input != tmp_input) {
      Unlink(tmp_input.c_str());
    }
  }

  return 0;
}

int cabocha_learn(int argc, char **argv) {
  static const CaboCha::Option long_options[] = {
    {"parser-type", 'e', "dep",   "STRING",
     "choose from ne/chunk/dep (default dep)" },
    {"freq",     'f', "1",      "INT",
     "use features that occuer no less than INT (default 1)" },
    {"cost",     'c', "0.0015",    "FLOAT",
     "set FLOAT for cost parameter (default 0.001)" },
    {"sigma",    's', "0.001",  "FLOAT",
     "set minimum feature weight for PKE approximation (default 0.001)" },
    {"minsup",   'n', "2",  "INT",
     "set minimum frequency support for PKE approximation (default 2)" },
    {"old-model", 'M', 0, "FILE",
     "set FILE as old SVM model file" },
    {"parsing-algorithm", 'a', "0", "INT",
     "set dependency parsing algorithm (0:shift-reduce, 1:tournament)"},
    { "charset",   't',  CABOCHA_DEFAULT_CHARSET, "ENC",
      "set parser charset to ENC (default "
      CABOCHA_DEFAULT_CHARSET ")" },
    { "posset",         'P',  CABOCHA_DEFAULT_POSSET, "STR",
      "set parser posset to STR (default "
      CABOCHA_DEFAULT_POSSET ")" },
    {"version",  'v', 0,        0,       "show the version and exit" },
    {"help",     'h', 0,        0,       "show this help and exit" },
    {0, 0, 0, 0, 0}
  };

  CaboCha::Param param;
  param.open(argc, argv, long_options);

  if (!param.help_version()) {
    return 0;
  }

  const std::vector<std::string> &rest = param.rest_args();
  if (rest.size() != 2) {
    std::cout << param.help();
    return 0;
  }

  const std::string stype = param.get<std::string>("parser-type");
  const std::string old_model_file = param.get<std::string>("old-model");
  const ParserType type = parser_type(stype.c_str());
  CHECK_DIE(type != -1) <<  "unknown parser type: " << stype;

  const PossetType posset =
      decode_posset(param.get<std::string>("posset").c_str());
  const CharsetType charset =
      decode_charset(param.get<std::string>("charset").c_str());

  const double cost   = param.get<double>("cost");
  const int    freq   = param.get<int>("freq");
  const float  sigma  = param.get<float>("sigma");
  const size_t minsup = param.get<size_t>("minsup");

  if (type == TRAIN_DEP) {
    const int    parsing_algorithm = param.get<int>("parsing-algorithm");
    const std::string text_model_file = rest[1] + ".txt";
    CHECK_DIE(CaboCha::runDependencyTraining(
                  rest[0].c_str(),
                  text_model_file.c_str(),
                  old_model_file.empty() ? 0 : old_model_file.c_str(),
                  charset,
                  posset,
                  parsing_algorithm,
                  cost, freq, 0));
    CaboCha::Iconv iconv;
    iconv.open(charset, charset);
    CHECK_DIE(CaboCha::FastSVMModel::compile(text_model_file.c_str(),
                                             rest[1].c_str(),
                                             sigma,
                                             minsup, &iconv));
  } else if (type == TRAIN_CHUNK || type == TRAIN_NE) {
    CHECK_DIE(old_model_file.empty())
        << "old-model is not supported in CHUNK|NE mode";
    if (type == TRAIN_CHUNK) {
      CHECK_DIE(CaboCha::runChunkingTraining(
                    rest[0].c_str(),
                    rest[1].c_str(),
                    old_model_file.empty() ? 0 : old_model_file.c_str(),
                    charset,
                    posset,
                    cost, freq, 0));
    } else if (type == TRAIN_NE) {
      CHECK_DIE(CaboCha::runNETraining(
                    rest[0].c_str(),
                    rest[1].c_str(),
                    old_model_file.empty() ? 0 : old_model_file.c_str(),
                    charset,
                    posset,
                    cost, freq, 0));
    }
  }

  return 0;
}
