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

namespace CaboCha {
bool ChunkingTrainingWithCRFPP(ParserType type,
                               CharsetType charset,
                               PossetType posset,
                               int freq,
                               const char *train_file,
                               const char *model_file,
                               const char *crfpp_param);

bool DependencyTrainingWithSVM(const char *train_file,
                               const char *model_file,
                               CharsetType charset,
                               PossetType posset,
                               int degree,
                               double cost,
                               double cache_size);
}

namespace {
using namespace CaboCha;
ParserType guess_text_model_type(const char *filename) {
  std::ifstream ifs(WPATH(filename));
  CHECK_DIE(ifs);
  std::string line;
  std::getline(ifs, line);
  if (line.find("version: ") != std::string::npos) {
    return TRAIN_NE;
  }
  return TRAIN_DEP;
}
}

using namespace CaboCha;

int cabocha_model_index(int argc, char **argv) {
  static const CaboCha::Option long_options[] = {
    {"sigma",    's', "0.001",  "FLOAT",
     "set minimum feature weight for PKE approximation (default 0.001)" },
    {"minsup",   'M', "2",  "INT",
     "set minimum frequency support for PKE approximation (default 2)" },
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

  if (!param.help_version()) return 0;

  const std::vector<std::string> &rest = param.rest_args();
  if (rest.size() != 2) {
    std::cout << param.help();
    return 0;
  }

  const std::string from = param.get<std::string>("model-charset");
  const std::string to =   param.get<std::string>("charset");
  const int from_id = decode_charset(from.c_str());
  const int to_id = decode_charset(to.c_str());
  std::string input = rest[0];

  if (from_id != to_id) {
    const std::string input2 = rest[1] + ".tmp";
    std::string line;
    std::ifstream ifs(WPATH(input.c_str()));
    CHECK_DIE(ifs) << "no such file or directory: " << input;
    std::ofstream ofs(WPATH(input2.c_str()));
    CHECK_DIE(ifs) << "permission denied: " << input2;

    Iconv iconv;
    CHECK_DIE(iconv.open(from.c_str(), to.c_str()))
        << "iconv_open() failed with from=" << from << " to=" << to;

    while (std::getline(ifs, line)) {
      std::string original = line;
      if (!iconv.convert(&line)) {
        std::cerr << "iconv conversion failed. skip this entry:"
                  << original << std::endl;
        line = original;
        continue;
      }
      ofs << line << std::endl;
    }

    input = input2;
  }

  const ParserType type = guess_text_model_type(input.c_str());

  if (type == TRAIN_DEP) {
    const float sigma = param.get<float>("sigma");
    const size_t minsup = param.get<size_t>("minsup");
    CHECK_DIE(CaboCha::SVM::compile(input.c_str(),
                                    rest[1].c_str(),
                                    sigma,
                                    minsup));
  } else if (type == TRAIN_NE) {
    std::vector<const char*> argv;
    argv.push_back("CRF++");
    argv.push_back("--convert");
    argv.push_back(input.c_str());
    argv.push_back(rest[1].c_str());
    CHECK_DIE(0 == crfpp_learn(argv.size(),
                               const_cast<char **>(&argv[0])))
        << "crfpp_learn execution error";
  }

  if (from_id != to_id) {
    Unlink(input.c_str());
  }

  return 0;
}

int cabocha_learn(int argc, char **argv) {
  static const CaboCha::Option long_options[] = {
    {"parser-type", 'e', "dep",   "STRING",
     "choose from ne/chunk/dep (default dep)" },
    {"crfpp-param", 'p', "-f 2",  "STRING",
     "parameter for CRF++ (default -f 2)" },
    {"freq",     'f', "1",      "INT",
     "use features that occuer no less than INT (default 1)" },
    {"cost",     'c', "0.001",    "FLOAT",
     "set FLOAT for cost parameter (default 0.001)" },
    {"sigma",    's', "0.001",  "FLOAT",
     "set minimum feature weight for PKE approximation (default 0.001)" },
    {"minsup",   'M', "2",  "INT",
     "set minimum frequency support for PKE approximation (default 2)" },
    {"degree",   'd', "2",  "INT",
     "set degree of polynomial kernel (default 2)"},
    {"cache-size",  'b',  "1000.0", "FLOAT",
     "set FLOAT for cache memory size (MB)" },
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

  if (!param.help_version()) return 0;

  const std::vector<std::string> &rest = param.rest_args();
  if (rest.size() != 2) {
    std::cout << param.help();
    return 0;
  }

  const std::string stype = param.get<std::string>("parser-type");
  const ParserType type = parser_type(stype.c_str());
  CHECK_DIE(type != -1) <<  "unknown parser type: " << stype;

  const PossetType posset =
      decode_posset(param.get<std::string>("posset").c_str());
  const CharsetType charset =
      decode_charset(param.get<std::string>("charset").c_str());

  if (type == TRAIN_DEP) {
    const float cost       = param.get<float>("cost");
    const float cache_size = param.get<float>("cache-size");
    const int   degree     = param.get<int>("degree");
    const std::string text_model_file = rest[1] + ".txt";
    CHECK_DIE(CaboCha::DependencyTrainingWithSVM(rest[0].c_str(),
                                                 text_model_file.c_str(),
                                                 charset,
                                                 posset,
                                                 degree,
                                                 cost,
                                                 cache_size));
    const float  sigma  = param.get<float>("sigma");
    const size_t minsup = param.get<size_t>("minsup");
    CHECK_DIE(CaboCha::SVM::compile(text_model_file.c_str(),
                                    rest[1].c_str(),
                                    sigma,
                                    minsup));
  } else if (type == TRAIN_CHUNK || type == TRAIN_NE) {
    const std::string crfpp_param = param.get<std::string>("crfpp-param");
    const int freq = param.get<int>("freq");
    CHECK_DIE(CaboCha::ChunkingTrainingWithCRFPP(type,
                                                 charset,
                                                 posset,
                                                 freq,
                                                 rest[0].c_str(),
                                                 rest[1].c_str(),
                                                 crfpp_param.c_str()));
  }

  return 0;
}
