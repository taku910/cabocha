// CaboCha -- Yet Another Japanese Dependency Parser
//
//  $Id: ucs.cpp 50 2009-05-03 08:25:36Z taku-ku $;
//
//  Copyright(C) 2001-2008 Taku Kudo <taku@chasen.org>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include "common.h"
#include "scoped_ptr.h"
#include "utils.h"

#if defined(_WIN32) && !defined(__CYGWIN__)
#include "windows.h"
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef CABOCHA_USE_UTF8_ONLY
#include "ucstable.h"
#endif

#include "ucs.h"

namespace CaboCha {
namespace {

#if defined(_WIN32) && !defined(__CYGWIN__)
DWORD decode_charset_win32(const char *str) {
  const CharsetType charset = CaboCha::decode_charset(str);
  switch (charset) {
    case UTF8:
      return 65001;
    case EUC_JP:
      return 20932;
    case CP932:
      return 932;
    default:
      std::cerr << "charset " << str
                << " is not defined, use 'autodetect'";
      return 50932;  // auto detect
  }
  return 0;
}
#endif

const char *decode_charset_iconv(const char *str) {
  return CaboCha::encode_charset(CaboCha::decode_charset(str));
}
}

// All internal codes are represented in UCS2,
// if you want to use specific local codes, e.g, big5/euc-kr,
// make a function which maps the local code to the UCS code.
unsigned short utf8_to_ucs2(const char *begin,
                            const char *end,
                            size_t *mblen) {
  const size_t len = end - begin;

  if (static_cast<unsigned char> (begin[0]) < 0x80) {
    *mblen = 1;
    return static_cast<unsigned char> (begin[0]);

  } else if (len >= 2 && (begin[0] & 0xe0) == 0xc0) {
    *mblen = 2;
    return((begin[0] & 0x1f) << 6) | (begin[1] & 0x3f);

  } else if (len >= 3 && (begin[0] & 0xf0) == 0xe0) {
    *mblen = 3;
    return((begin[0] & 0x0f) << 12) |
        ((begin[1] & 0x3f) << 6) | (begin[2] & 0x3f);

    /* belows are out of UCS2 */
  } else if (len >= 4 && (begin[0] & 0xf8) == 0xf0) {
    *mblen = 4;
    return 0;

  } else if (len >= 5 && (begin[0] & 0xfc) == 0xf8) {
    *mblen = 5;
    return 0;

  } else if (len >= 6 && (begin[0] & 0xfe) == 0xfc) {
    *mblen = 6;
    return 0;

  } else {
    *mblen = 1;
    return 0;
  }
}

unsigned short ascii_to_ucs2(const char *begin, const char *end,
                             size_t *mblen) {
  *mblen = 1;
  return static_cast<unsigned char> (begin[0]);
}

#ifndef CABOCHA_USE_UTF8_ONLY
unsigned short euc_to_ucs2(const char *begin, const char *end,
                           size_t *mblen) {
  const size_t len = end - begin;

  // JISX 0212, 0213
  if (static_cast<unsigned char> (begin[0]) == 0x8f && len >= 3) {
    unsigned short key = (static_cast<unsigned char> (begin[1]) << 8) +
        static_cast<unsigned char> (begin[2]);
    if (key < 0xA0A0) {  // offset  violation
      *mblen = 1;
      return static_cast<unsigned char> (begin[0]);
    }
    *mblen = 3;
    return euc_hojo_tbl[key - 0xA0A0];
    // JISX 0208 + 0201
  } else if ((static_cast<unsigned char> (begin[0]) & 0x80) && len >= 2) {
    *mblen = 2;
    return euc_tbl[(static_cast<unsigned char> (begin[0]) << 8) +
                   static_cast<unsigned char> (begin[1])];
  } else {
    *mblen = 1;
    return static_cast<unsigned char> (begin[0]);
  }
}

unsigned short cp932_to_ucs2(const char *begin, const char *end,
                             size_t *mblen) {
  const size_t len = end - begin;
  if ((static_cast<unsigned char> (begin[0]) & 0x80) && len >= 2) {
    *mblen = 2;
    return cp932_tbl[(static_cast<unsigned char>(begin[0]) << 8) +
                     static_cast<unsigned char>(begin[1])];
  } else {
    *mblen = 1;
    return static_cast<unsigned char> (begin[0]);
  }
}
#endif

CharsetType decode_charset(const char *charset) {
  std::string tmp = charset;
  to_lower(&tmp);
  if (tmp == "sjis"  || tmp == "shift-jis" ||
      tmp == "shift_jis" || tmp == "cp932")
    return CP932;
  else if (tmp == "euc"   || tmp == "euc_jp" ||
           tmp == "euc-jp" || tmp == "euc-jp-win" ||
           tmp == "euc-jp-ms" || tmp == "eucjp-ms")
    return EUC_JP;
  else if (tmp == "utf8" || tmp == "utf_8" ||
           tmp == "utf-8" || tmp == "utf")
    return UTF8;
  else if (tmp == "ascii")
    return ASCII;
  std::cerr << "Cannot parse " << charset << std::endl;
  return ASCII;
}

const char *encode_charset(CharsetType charset) {
  switch (charset) {
    case UTF8:
      return "UTF8";
    case EUC_JP:
#ifdef HAVE_ICONV_EUC_JP_MS
      return "EUC-JP-MS";
#else
      return "EUC-JP";
#endif
    case  CP932:
#ifdef HAVE_ICONV_CP932
      return "CP932";
#else
      return "SHIFT_JIS";
#endif
    default:
      std::cerr << "charset " << charset
                << " is not defined, use "  CABOCHA_DEFAULT_CHARSET;
      return CABOCHA_DEFAULT_CHARSET;
  }
  return CABOCHA_DEFAULT_CHARSET;
}

bool Iconv::open(CharsetType from, CharsetType to) {
  const char *from2 = encode_charset(from);
  const char *to2 = encode_charset(to);
  return open(from2, to2);
}

bool Iconv::open(const char* from, const char* to) {
  ic_ = 0;
  from_ = decode_charset(from);
  to_ = decode_charset(to);
#if defined HAVE_ICONV
  const char *from2 = decode_charset_iconv(from);
  const char *to2 = decode_charset_iconv(to);
  if (std::strcmp(from2, to2) == 0) {
    return true;
  }
  ic_ = 0;
  ic_ = iconv_open(to2, from2);
  if (ic_ == (iconv_t)(-1)) {
    ic_ = 0;
    return false;
  }
#else
#if defined(_WIN32) && !defined(__CYGWIN__)
  from_cp_ = decode_charset_win32(from);
  to_cp_ = decode_charset_win32(to);
  if (from_cp_ == to_cp_) {
    return true;
  }
  ic_ = from_cp_;
#else
  std::cerr << "iconv_open is not supported" << std::endl;
#endif
#endif

  return true;
}

bool Iconv::convert(std::string *str) {
  if (str->empty()) {
    return true;
  }
  if (ic_ == 0) {
    return true;
  }

#if defined HAVE_ICONV
  size_t ilen = 0;
  size_t olen = 0;
  ilen = str->size();
  olen = ilen * 4;
  std::string tmp;
  tmp.reserve(olen);
  char *ibuf = const_cast<char *>(str->data());
  char *obuf_org = const_cast<char *>(tmp.data());
  char *obuf = obuf_org;
  std::fill(obuf, obuf + olen, 0);
  size_t olen_org = olen;
  iconv(ic_, 0, &ilen, 0, &olen);  // reset iconv state
  while (ilen != 0) {
    if (iconv(ic_, (ICONV_CONST char **)&ibuf, &ilen, &obuf, &olen)
        == (size_t) -1) {
      return false;
    }
  }
  str->assign(obuf_org, olen_org - olen);
#else
#if defined(_WIN32) && !defined(__CYGWIN__)
  // covert it to wide character first
  const size_t wide_len = ::MultiByteToWideChar(from_cp_, 0,
                                                str->c_str(),
                                                -1, NULL, 0);
  if (wide_len == 0) {
    return false;
  }

  scoped_array<wchar_t> wide_str(new wchar_t[wide_len + 1]);

  if (!wide_str.get()) {
    return false;
  };

  if (::MultiByteToWideChar(from_cp_, 0, str->c_str(), -1,
                            wide_str.get(), wide_len + 1) == 0) {
    return false;
  }

  if (to_cp_ == 1200 || to_cp_ == 1201) {
    str->resize(2 * wide_len);
    std::memcpy(const_cast<char *>(str->data()),
                reinterpret_cast<char *>(wide_str.get()), wide_len * 2);
    if (to_cp_ == 1201) {
      char *buf = const_cast<char *>(str->data());
      for (size_t i = 0; i < 2 * wide_len; i += 2) {
        std::swap(buf[i], buf[i+1]);
      }
    }
    return true;
  }

  const size_t output_len = ::WideCharToMultiByte(to_cp_, 0,
                                                  wide_str.get(),
                                                  -1,
                                                  NULL, 0, NULL, NULL);

  if (output_len == 0) {
    return false;
  }

  scoped_array<char> encoded(new char[output_len + 1]);
  if (::WideCharToMultiByte(to_cp_, 0, wide_str.get(), wide_len,
                            encoded.get(), output_len + 1,
                            NULL, NULL) == 0) {
    return false;
  }

  str->assign(encoded.get());

#endif
#endif

  return true;
}

Iconv::Iconv(): ic_(0) {}

Iconv::~Iconv() {
#if defined HAVE_ICONV
  if (ic_ != 0) {
    iconv_close(ic_);
  }
#endif
}
}
