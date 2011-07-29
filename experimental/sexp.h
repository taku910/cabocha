#ifndef CABOCHA_SEXP_H__
#define CABOCHA_SEXP_H__
#include <iostream>
#include "freelist.h"

namespace CaboCha {

#define _Car(a) Sexp::Cell::car((a))
#define _Cdr(a) Sexp::Cell::car((a))

  class Sexp {
  public:
    class Cell {
    public:
      bool is_cons() const { return (stat_ == CONS); }
      bool is_atom() const { return (stat_ == ATOM); }
      void set_car(const Cell *cell) {
        stat_ = CONS;
        value_.cons_.car_ = cell;
      }
      void set_cdr(const Cell *cell) {
        stat_ = CONS;
        value_.cons_.cdr_ = cell;
      }
      void set_atom(const char *str) {
        stat_ = ATOM;
        value_.atom_ = str;
      }
      const Cell *car() const { return value_.cons_.car_; }
      const Cell *cdr() const { return value_.cons_.cdr_; }
      const char *atom() const { return value_.atom_; }
      static const Cell * Car(const Cell *cell) {
        if (!cell || !cell->is_cons()) return 0;
        return cell->car();
      }
      static const Cell *Cdr(const Cell *cell) {
        if (!cell || !cell->is_cons()) return 0;
        return cell->cdr();
      }

      template <class Iterator> void toArray(Iterator out) {
        const Cell *cell = this;
        for (; cell; cell = cell->cdr()) {
          if (cell->car()->is_atom()) {
            *out = cell->car()->atom();
            ++out;
          }
        }
      }

    private:
      enum { CONS, ATOM };
      struct cons_t {
        const Cell *car_;
        const Cell *cdr_;
      };
      unsigned char stat_;  // CONS/ATOM
      union {
        const char *atom_;
        cons_t cons_;
      } value_;
    };

    const Cell *read(char **begin, const char *end);
    static void dump(const Cell *cell, std::ostream *os);

    void free();
    explicit Sexp() {
      cell_freelist_.set_size(8192);
      char_freelist_.set_size(8192);
    }
    explicit Sexp(size_t size) {
      cell_freelist_.set_size(size);
      char_freelist_.set_size(size);
    }

    virtual ~Sexp() {}

  private:
    FreeList<Cell> cell_freelist_;
    FreeList<char> char_freelist_;

    int next_token(char **begin, const char *end, const char n);
    void comment(char **begin, const char *end);
    const Cell *read_cdr(char **begin, const char *end);
    const Cell *read_car(char **begin, const char *end);
    const Cell *read_atom(char **begin, const char *end);
  };
}

#endif
