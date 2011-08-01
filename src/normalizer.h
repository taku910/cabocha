// CaboCha -- Yet Another Japanese Dependency Parser
//
//  $Id: normalizer.h 41 2008-01-20 09:31:34Z taku-ku $;
//
//  Copyright(C) 2001-2008 Taku Kudo <taku@chasen.org>
#ifndef CABOCHA_NORMALIZER_H_
#define CABOCHA_NORMALIZER_H_

#include <string>

namespace CaboCha {
class Normalizer {
 public:
  static void normalize(int chartype,
                        const std::string &input,
                        std::string *output);

  static void normalize(int chartype,
                        const char *str, size_t len,
                        std::string *output);

  static void compile(const char *filename,
                      const char *header_filename);
};
}
#endif
