// CaboCha -- Yet Another Japanese Dependency Parser
//
//  $Id: ucs.h 50 2009-05-03 08:25:36Z taku-ku $;
//
//  Copyright(C) 2001-2008 Taku Kudo <taku@chasen.org>
#ifndef CABOCHA_UCS_H__
#define CABOCHA_UCS_H__

#if defined HAVE_ICONV
#include <iconv.h>
#endif

#if defined(_WIN32) && !defined(__CYGWIN__)
#include "windows.h"
#endif

namespace CaboCha {

CharsetType decode_charset(const char *charset);
const char *encode_charset(CharsetType charset);

unsigned short utf8_to_ucs2(const char *begin,
                            const char *end,
                            size_t *mblen);

unsigned short ascii_to_ucs2(const char *begin,
                             const char *end,
                             size_t *mblen);

#ifndef CABOCHA_USE_UTF8_ONLY
unsigned short euc_to_ucs2(const char *begin,
                           const char *end,
                           size_t *mblen);

unsigned short cp932_to_ucs2(const char *begin,
                             const char *end,
                             size_t *mblen);
#endif

class Iconv {
 private:
#ifdef HAVE_ICONV
  iconv_t ic_;
#else
  int ic_;
#endif

#if defined(_WIN32) && !defined(__CYGWIN__)
  DWORD from_cp_;
  DWORD to_cp_;
#endif

 public:
  explicit Iconv();
  virtual ~Iconv();
  bool open(const char *from, const char *to);
  bool open(CharsetType from, CharsetType to);
  bool convert(std::string *);
};
}
#endif
