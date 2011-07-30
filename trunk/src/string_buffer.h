// CaboCha -- Yet Another Japanese Dependency Parser
//
//  $Id: string_buffer.h 41 2008-01-20 09:31:34Z taku-ku $;
//
//  Copyright(C) 2001-2008 Taku Kudo <taku@chasen.org>
#ifndef CABOCHA_STRINGBUFFER_H_
#define CABOCHA_STRINGBUFFER_H_

#include <string>
#include "common.h"
#include "utils.h"

namespace CaboCha {

#define _ITOA(n)  char fbuf[64]; itoa(n, fbuf);  return this->write(fbuf);
#define _UITOA(n) char fbuf[64]; uitoa(n, fbuf);  return this->write(fbuf);
#define _DTOA(n)  char fbuf[64]; dtoa(n, fbuf);  return this->write(fbuf);

class StringBuffer {
 private:
  size_t size_;
  size_t alloc_size_;
  char *ptr_;
  bool is_delete_;
  bool error_;
  bool reserve(size_t size);

 public:
  explicit StringBuffer(): size_(0), alloc_size_(0),
                           ptr_(0), is_delete_(true), error_(false) {}
  explicit StringBuffer(char *_s, size_t _l):
      size_(0), alloc_size_(_l), ptr_(_s), is_delete_(false), error_(false) {}

  virtual ~StringBuffer();

  StringBuffer& write(char);
  StringBuffer& write(const char*, size_t);
  StringBuffer& write(const char* );
  StringBuffer& operator<< (double n)             { _DTOA(n); }
  StringBuffer& operator<< (short int n)          { _ITOA(n); }
  StringBuffer& operator<< (int n)                { _ITOA(n); }
  StringBuffer& operator<< (long int n)           { _ITOA(n); }
  StringBuffer& operator<< (unsigned short int n) { _UITOA(n); }
  StringBuffer& operator<< (unsigned int n)       { _UITOA(n); }
  StringBuffer& operator<< (unsigned long int n)  { _UITOA(n); }
  StringBuffer& operator<< (char n)               { return this->write(n); }
  StringBuffer& operator<< (unsigned char n)      { return this->write(n); }
  StringBuffer& operator<< (const char* n)        { return this->write(n); }
  StringBuffer& operator<< (const std::string& n) { return this->write(n.c_str()); }

  void clear() { size_ = 0; }
  const char *str() { return error_ ?  0 : const_cast<const char*> (ptr_); }
};
}
#endif
