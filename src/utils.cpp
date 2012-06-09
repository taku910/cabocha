// CaboCha -- Yet Another Japanese Dependency Parser
//
//  $Id: utils.cpp 50 2009-05-03 08:25:36Z taku-ku $;
//
//  Copyright(C) 2001-2008 Taku Kudo <taku@chasen.org>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <cstring>
#include "cabocha.h"
#include "common.h"
#include "scoped_ptr.h"
#include "utils.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

namespace CaboCha {

#if defined(_WIN32) && !defined(__CYGWIN__)
std::wstring Utf8ToWide(const std::string &input) {
  int output_length = ::MultiByteToWideChar(CP_UTF8, 0,
                                            input.c_str(), -1, NULL, 0);
  output_length = output_length <= 0 ? 0 : output_length - 1;
  if (output_length == 0) {
    return L"";
  }
  scoped_array<wchar_t> input_wide(new wchar_t[output_length + 1]);
  const int result = ::MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1,
                                           input_wide.get(), output_length + 1);
  std::wstring output;
  if (result > 0) {
    output.assign(input_wide.get());
  }
  return output;
}

std::string WideToUtf8(const std::wstring &input) {
  const int output_length = ::WideCharToMultiByte(CP_UTF8, 0,
                                                  input.c_str(), -1, NULL, 0,
                                                  NULL, NULL);
  if (output_length == 0) {
    return "";
  }

  scoped_array<char> input_encoded(new char[output_length + 1]);
  const int result = ::WideCharToMultiByte(CP_UTF8, 0, input.c_str(), -1,
                                           input_encoded.get(),
                                           output_length + 1, NULL, NULL);
  std::string output;
  if (result > 0) {
    output.assign(input_encoded.get());
  }
  return output;
}
#endif

PossetType decode_posset(const char *posset) {
  std::string stype(posset);
  to_lower(&stype);
  PossetType type = IPA;
  if (stype == "ipa") {
    type = IPA;
  } else if (stype == "juman") {
    type = JUMAN;
  } else if (stype == "unidic") {
    type = UNIDIC;
  }
  return type;
}

const char *encode_posset(PossetType posset) {
  if (posset == IPA) {
    return "IPA";
  } else if (posset == JUMAN) {
    return "JUMAN";
  } else if (posset == UNIDIC) {
    return "UNIDIC";
  }
  return "IPA";
}

ParserType parser_type(const char *str_type) {
  std::string stype(str_type);
  CaboCha::to_lower(&stype);
  ParserType type = TRAIN_DEP;
  if (stype == "dep") {
    type = TRAIN_DEP;
  } else if (stype == "chunk") {
    type = TRAIN_CHUNK;
  } else if (stype == "ne") {
    type = TRAIN_NE;
  }
  return type;
}

void concat_feature(const Token *token,
                    size_t size,
                    std::string *output) {
  output->clear();
  output->reserve(64);
  const size_t minsize = std::min(static_cast<size_t>(token->feature_list_size),
                                  size);
  for (size_t i = 0; i < minsize; ++i) {
    if (std::strcmp("*", token->feature_list[i]) == 0) {
      break;
    }
    if (i != 0) {
      output->append("-");
    }
    output->append(token->feature_list[i]);
  }
}

std::string create_filename(const std::string &path,
                            const std::string &file) {
  std::string s = path;
#if defined(_WIN32) && !defined(__CYGWIN__)
  if (s.size() && s[s.size()-1] != '\\') {
    s += '\\';
  }
#else
  if (s.size() && s[s.size()-1] != '/') {
    s += '/';
  }
#endif
  s += file;
  return s;
}

void remove_filename(std::string *s) {
  int len = static_cast<int> (s->size() - 1);
  bool ok = false;
  for (; len >= 0; --len) {
#if defined(_WIN32) && !defined(__CYGWIN__)
    if ((*s)[len] == '\\') {
      ok = true;
      break;
    }
#else
    if ((*s)[len] == '/') {
      ok = true;
      break;
    }
#endif
  }
  if (ok) {
    *s = s->substr(0, len);
  } else {
    *s = ".";
  }
}

void remove_pathname(std::string *s) {
  int len = static_cast<int> (s->size() - 1);
  bool ok = false;
  for (; len >= 0; --len) {
#if defined(_WIN32) && !defined(__CYGWIN__)
    if ((*s)[len] == '\\') {
      ok = true;
      break;
    }
#else
    if ((*s)[len] == '/') {
      ok = true;
      break;
    }
#endif
  }
  if (ok) {
    *s = s->substr(len + 1, s->size() - len);
  } else {
    *s = ".";
  }
}

void replace_string(std::string *s,
                    const std::string &src,
                    const std::string &dst) {
  const size_t pos = s->find(src);
  if (pos != std::string::npos) {
    s->replace(pos, src.size(), dst);
  }
}

bool to_lower(std::string *s) {
  for (size_t i = 0; i < s->size(); ++i) {
    char c = (*s)[i];
    if ((c >= 'A') && (c <= 'Z')) {
      c += 'a' - 'A';
      (*s)[i] = c;
    }
  }
  return true;
}

bool read_sentence(std::istream *is, std::string *str,
                   int inputLayer) {
  str->clear();
  if (inputLayer == INPUT_RAW_SENTENCE) {
    std::getline(*is, *str);
  } else {
    scoped_fixed_array<char, BUF_SIZE * 16> line;
    size_t line_num = 0;
    while (is->getline(line.get(), line.size())) {
      str->append(line.get());
      *str += '\n';
      if (++line_num > CABOCHA_MAX_LINE_SIZE) {
        return false;
      }
      if (std::strlen(line.get()) == 0 || strcmp(line.get(), "EOS") == 0) {
        break;
      }
    }
  }
  return true;
}

bool escape_csv_element(std::string *w) {
  if (w->find(',') != std::string::npos ||
      w->find('"') != std::string::npos) {
    std::string tmp = "\"";
    for (size_t j = 0; j < w->size(); j++) {
      if ((*w)[j] == '"') {
        tmp += '"';
      }
      tmp += (*w)[j];
    }
    tmp += '"';
    *w = tmp;
  }
  return true;
}

#if defined(_WIN32) && !defined(__CYGWIN__)
std::string get_windows_reg_value(const char *key,
                                  const char *name) {
  HKEY hKey;
  scoped_fixed_array<wchar_t, 1024> v;
  DWORD vt;
  DWORD size = v.size() * sizeof(v[0]);

  ::RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                  CaboCha::Utf8ToWide(key).c_str(),
                  0, KEY_READ, &hKey);
  ::RegQueryValueExW(hKey,
                     CaboCha::Utf8ToWide(name).c_str(),
                     0, &vt, reinterpret_cast<BYTE *>(v.get()), &size);
  ::RegCloseKey(hKey);
  if (vt == REG_SZ) {
    return CaboCha::WideToUtf8(v.get());
  }

  ::RegOpenKeyExW(HKEY_CURRENT_USER,
                  CaboCha::Utf8ToWide(key).c_str(),
                  0, KEY_READ, &hKey);
  ::RegQueryValueExW(hKey,
                   CaboCha::Utf8ToWide(name).c_str(),
                   0, &vt, reinterpret_cast<BYTE *>(v.get()), &size);
  ::RegCloseKey(hKey);
  if (vt == REG_SZ) {
    return CaboCha::WideToUtf8(v.get());

  }

  return std::string("");
}
#endif

int progress_bar(const char* message, size_t current, size_t total) {
  static char bar[] = "###########################################";
  static int scale = sizeof(bar) - 1;
  static int prev = 0;

  int cur_percentage  = static_cast<int> (100.0 * current/total);
  int bar_len = static_cast<int> (1.0 * current*scale/total);

  if (prev != cur_percentage) {
    printf("%s: %3d%% |%.*s%*s| ",
           message, cur_percentage, bar_len, bar, scale - bar_len, "");
    if (cur_percentage == 100)
      printf("\n");
    else
      printf("\r");
    fflush(stdout);
  }

  prev = cur_percentage;

  return 1;
}

void Unlink(const char *filename) {
#if defined(_WIN32) && !defined(__CYGWIN__)
  ::DeleteFileW(WPATH(filename));
#else
  ::unlink(filename);
#endif
}
}
