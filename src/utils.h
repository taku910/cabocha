// CaboCha -- Yet Another Japanese Dependency Parser
//
//  $Id: utils.h 50 2009-05-03 08:25:36Z taku-ku $;
//
//  Copyright(C) 2001-2008 Taku Kudo <taku@chasen.org>
#ifndef CABOCHA_UTILS_H_
#define CABOCHA_UTILS_H_

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <algorithm>
#include <iostream>
#include <vector>
#include "cabocha.h"
#include "common.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

namespace CaboCha {

template <class T> inline T _min(T x, T y) { return(x < y) ? x : y; }
template <class T> inline T _max(T x, T y) { return(x > y) ? x : y; }

PossetType decode_posset(const char *posset);
const char *encode_posset(PossetType posset);
ParserType parser_type(const char *str_type);

void concat_feature(const Token *token,
                    size_t size,
                    std::string *output);

int progress_bar(const char* message,
                 size_t current, size_t total);

#if defined(_WIN32) && !defined(__CYGWIN__)
std::string get_windows_reg_value(const char *key,
                                  const char *name);
#endif

void inline dtoa(double val, char *s) {
  std::sprintf(s, "%-16f", val);
  char *p = s;
  for (; *p != ' '; ++p) {}
  *p = '\0';
  return;
}

template <class T>
inline void itoa(T val, char *s) {
  char *t;
  T mod;

  if (val < 0) {
    *s++ = '-';
    val = -val;
  }
  t = s;

  while (val) {
    mod = val % 10;
    *t++ = static_cast<char>(mod) + '0';
    val /= 10;
  }

  if (s == t) *t++ = '0';
  *t = '\0';
  std::reverse(s, t);

  return;
}

template <class T>
inline void uitoa(T val, char *s) {
  char *t;
  T mod;
  t = s;
  while (val) {
    mod = val % 10;
    *t++ = static_cast<char>(mod) + '0';
    val /= 10;
  }

  if (s == t) *t++ = '0';
  *t = '\0';
  std::reverse(s, t);
  return;
}

inline const char *read_ptr(const char **ptr, size_t size) {
  const char *r = *ptr;
  *ptr += size;
  return r;
}

template <class T>
inline void read_static(const char **ptr, T& value) {
  const char *r = read_ptr(ptr, sizeof(T));
  memcpy(&value, r, sizeof(T));
}

bool to_lower(std::string *str);
bool escape_csv_element(std::string *w);

bool read_sentence(std::istream *is, std::string *str,
                   int inputLayer);


std::string create_filename(const std::string &path,
                            const std::string &file);
void remove_filename(std::string *s);
void remove_pathname(std::string *s);
void replace_string(std::string *s,
                    const std::string &src,
                    const std::string &dst);

template <class Iterator>
inline size_t tokenizeCSV(char *str,
                          Iterator out, size_t max) {
  char *eos = str + std::strlen(str);
  char *start = 0;
  char *end = 0;
  size_t n = 0;

  for (; str < eos; ++str) {
    while (*str == ' ' || *str == '\t') ++str;  // skip white spaces
    bool inquote = false;
    if (*str == '"') {
      start = ++str;
      end = start;
      for (; str < eos; ++str) {
        if (*str == '"') {
          str++;
          if (*str != '"')
            break;
        }
        *end++ = *str;
      }
      inquote = true;
      str = std::find(str, eos, ',');
    } else {
      start = str;
      str = std::find(str, eos, ',');
      end = str;
    }
    if (max-- > 1) *end = '\0';
    *out++ = start;
    ++n;
    if (max == 0) break;
  }

  return n;
}

template <class Iterator>
inline size_t tokenize(char *str, const char *del,
                       Iterator out, size_t max) {
  char *stre = str + std::strlen(str);
  const char *dele = del + std::strlen(del);
  size_t size = 0;

  while (size < max) {
    char *n = std::find_first_of(str, stre, del, dele);
    *n = '\0';
    *out++ = str;
    ++size;
    if (n == stre) break;
    str = n + 1;
  }

  return size;
}

// continus run of space is regarded as one space
template <class Iterator>
inline size_t tokenize2(char *str, const char *del,
                        Iterator out, size_t max) {
  char *stre = str + std::strlen(str);
  const char *dele = del + std::strlen(del);
  size_t size = 0;

  while (size < max) {
    char *n = std::find_first_of(str, stre, del, dele);
    *n = '\0';
    if (*str != '\0') {
      *out++ = str;
      ++size;
    }
    if (n == stre) break;
    str = n + 1;
  }

  return size;
}

inline char getEscapedChar(const char p) {
  switch (p) {
    case '0':  return '\0';
    case 'a':  return '\a';
    case 'b':  return '\b';
    case 't':  return '\t';
    case 'n':  return '\n';
    case 'v':  return '\v';
    case 'f':  return '\f';
    case 'r':  return '\r';
    case 's':  return ' ';
    case '\\': return '\\';
    default: break;
  }
  return '\0';  // never be here
}
}
#endif
