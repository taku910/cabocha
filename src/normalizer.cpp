// CaboCha -- Yet Another Japanese Dependency Parser
//
//  $Id: normalizer.cpp 50 2009-05-03 08:25:36Z taku-ku $;
//
//  Copyright(C) 2001-2008 Taku Kudo <taku@chasen.org>
#include <string>
#include <fstream>
#include "darts.h"
#include "scoped_ptr.h"
#include "utils.h"
#include "ucs.h"
#include "normalizer.h"

namespace {
struct DoubleArray {
  int base;
  unsigned int check;
};
}

#ifndef NORMALIZER_RULE_COMPILE
#include "normalizer_rule.h"
#endif

namespace {

void escape(const std::string &input, std::string *output) {
  output->clear();
  for (size_t i = 0; i < input.size(); ++i) {
    const int hi = ((static_cast<int>(input[i]) & 0xF0) >> 4);
    const int lo = (static_cast<int>(input[i]) & 0x0F);
    *output += "\\x";
    *output += hi >= 10 ? hi - 10 + 'A' : hi + '0';
    *output += lo >= 10 ? lo - 10 + 'A' : lo + '0';
  }
}

size_t lookup(const DoubleArray *array,
              const char *key, size_t len, int *result) {
  size_t seekto = 0;
  int n = 0;
  int b = array[0].base;
  unsigned int p = 0;
  *result = -1;
  size_t num = 0;

  for (size_t i = 0; i < len; ++i) {
    p = b;
    n = array[p].base;
    if (static_cast<unsigned int>(b) == array[p].check && n < 0) {
      seekto = i;
      *result = - n - 1;
      ++num;
    }
    p = b + static_cast<unsigned char>(key[i]) + 1;
    if (static_cast<unsigned int >(b) == array[p].check) {
      b = array[p].base;
    } else {
      return seekto;
    }
  }
  p = b;
  n = array[p].base;
  if (static_cast<unsigned int>(b) == array[p].check && n < 0) {
    seekto = len;
    *result = -n - 1;
  }

  return seekto;
}

class utf8_mblen_t {
 public:
  static void mblen(const char *begin,
                    const char *end, size_t *mblen) {
    CaboCha::utf8_to_ucs2(begin, end, mblen);
  }
};

#ifndef CABOCHA_USE_UTF8_ONLY
class cp932_mblen_t {
 public:
  static void mblen(const char *begin,
                    const char *end, size_t *mblen) {
    CaboCha::cp932_to_ucs2(begin, end, mblen);
  }
};
class euc_mblen_t {
 public:
  static void mblen(const char *begin,
                    const char *end, size_t *mblen) {
    CaboCha::euc_to_ucs2(begin, end, mblen);
  }
};
#endif

template<class T>
static void normalizeImpl(const DoubleArray *da,
                          const char *ctable,
                          const char *str,
                          size_t len,
                          std::string *output) {
  output->clear();
  const char *begin = str;
  const char *end = str + len;

  while (begin < end) {
    int result = 0;
    size_t mblen = lookup(da, begin,
                          static_cast<int>(end - begin),
                          &result);
    if (mblen > 0) {
      *output += &ctable[result];
      begin += mblen;
    } else {
      T::mblen(begin, end, &mblen);
      output->append(begin, mblen);
      begin += mblen;
    }
  }
}
}

namespace CaboCha {
void Normalizer::normalize(int charset,
                           const std::string &input,
                           std::string *output) {
  Normalizer::normalize(charset,
                        input.c_str(),
                        input.size(), output);
}


void Normalizer::normalize(int charset,
                           const char *str, size_t len,
                           std::string *output) {
#ifndef NORMALIZER_RULE_COMPILE

  switch (charset) {
    case UTF8:
      normalizeImpl<utf8_mblen_t>(utf8_da,
                                  utf8_table,
                                  str, len, output);
      break;
#ifndef CABOCHA_USE_UTF8_ONLY
    case EUC_JP:
      normalizeImpl<euc_mblen_t>(euc_jp_win_da,
                                 euc_jp_win_table,
                                 str, len, output);
      break;
    case CP932:
      normalizeImpl<cp932_mblen_t>(cp932_da,
                                   cp932_table,
                                   str, len, output);
      break;
#endif
    default:
      output->assign(str, len);
      break;
  }
#endif
}

void Normalizer::compile(const char *filename,
                         const char *header_filename) {
  const char* charset[3] = { "utf8", "euc_jp_win", "cp932" };
  std::ofstream ofs(header_filename);
  ofs << "#ifndef CABOCHA_NORMALIZER_RULE_H__" << std::endl;
  ofs << "#define CABOCHA_NORMALIZER_RULE_H__" << std::endl;
  ofs << "namespace {" << std::endl;

  for (size_t i = 0; i < 3; ++i) {
    Iconv iconv;
    std::string tmp(charset[i]);
    replace_string(&tmp, "_", "-");
    CHECK_DIE(iconv.open("utf8", tmp.c_str()));
    std::ifstream ifs(filename);
    CHECK_DIE(ifs) << "no such file or directory: " << filename;
    scoped_array<char> line(new char[BUF_SIZE]);
    char *col[32];
    std::string output;
    std::vector<std::pair<std::string, int> > dic;
    while (ifs.getline(line.get(), BUF_SIZE)) {
      const size_t size = tokenize(line.get(), "\t", col, 2);
      CHECK_DIE(size >= 2) << "format error: " << line.get();
      std::string key = col[0];
      std::string value = col[1];
      iconv.convert(&key);
      iconv.convert(&value);
      dic.push_back(std::make_pair(key, static_cast<int>(output.size())));
      output += value;
      output += '\0';
    }
    std::sort(dic.begin(), dic.end());
    std::vector<char *> ary;
    std::vector<int> values;
    for (size_t k = 0; k < dic.size(); ++k) {
      ary.push_back(const_cast<char *>(dic[k].first.c_str()));
      values.push_back(dic[k].second);
    }

    Darts::DoubleArray da;
    CHECK_DIE(0 == da.build(dic.size(), &ary[0], 0, &values[0]));

    std::string escaped;
    ::escape(output, &escaped);
    ofs << "static const char " << charset[i]
        << "_table[] = \"" << escaped << "\";" << std::endl;

    const DoubleArray *array = const_cast<const DoubleArray *>
        (reinterpret_cast<DoubleArray *>(da.array()));
    const size_t size = da.size();
    ofs << "static const DoubleArray " << charset[i] << "_da[] = {";
    for (size_t k = 0; k < size; ++k) {
      if (k != 0) ofs << ",";
      ofs << "{" << array[k].base << "," << array[k].check << "}";
    }
    ofs << "};" << std::endl;
  }
  ofs << "}" << std::endl;
  ofs << "#endif" << std::endl;
}
}

#ifdef NORMALIZER_RULE_COMPILE
int main(int argc, char **argv) {
  CaboCha::Normalizer::compile("normalizer.rule", "normalizer_rule.h");
}
#endif
