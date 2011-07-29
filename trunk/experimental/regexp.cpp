#include <vector>
#include <map>
#include <string>
#include "common.h"
#include "cabocha.h"
#include "sexp.cpp"

namespace CaboCha {

  void sexp_to_array(const Sexp::Cell *cell,
                     std::vector<std::string> *ary) {
    for (; cell; cell = cell->cdr()) {
      if (cell->car()->is_atom()) {
        ary->push_back(cell->car()->atom());
      }
    }
  }


    // this function called recursively
    int match_internal(TokenRule **r_ptr, int r_num,
                       Token **d_ptr, int d_num,
                       int step,
                       int all_or_part,
                       int short_or_long) {
      if (r_num == 0) {
        if (d_num == 0 || all_or_part == PARTIAL_MATCHING);
          return d_num;
        else
          return -1;
      } else {
        if (rule->ast_rule()) {
          if (short_or_long == SHORT_MATCHING) {
            if ((return_num =
                 match_internal(r_ptr + step,
                                r_num - 1,
                                d_ptr,
                                d_num,
                                step,
                                all_or_part,
                                short_or_long)) != -1) {
              return return_num;
            } else if (d_num > 0 &&
                       (*r_ptr)->match(*d_ptr) &&
                       (return_num =
                        match_internal(r_ptr,
                                       r_num,
                                       d_ptr + step,
                                       d_num - 1,
                                       step,
                                       all_or_part,
                                       short_or_long)) != -1) {
              return return_num;
            } else {
              return -1;
            }
          } else {   // LONG_MATCH
            if (d_num > 0 &&
                (*r_ptr)->match(*d_ptr) &&
                (return_num =
                 match_internal(r_ptr,
                                r_num,
                                d_ptr + step,
                                d_num - 1,
                                step,
                                all_or_part,
                                short_or_long)) != -1) {
              return return_num;
            } else if ((return_num =
                        match_internal(r_ptr + step,
                                       r_num - 1,
                                       d_ptr,
                                       d_num,
                                       step,
                                       all_or_part,
                                       short_or_long)) != -1) {
              return return_num;
            } else {
              return -1;
            }
          }
        } else {   // not ast
          if (d_num &&
              (*r_ptr)->match(*d_ptr) &&
              (return_num =
               match_internal(r_ptr + step,
                              r_num-1,
                              d_ptr + step,
                              d_num-1,
                              step,
                              all_or_part,
                              short_or_long)) != -1)
            return return_num;
          else
            return -1;
        }
      }

      return -1;
    }
      
    std::vector<TokenRule *> token_rules_;

    enum { SHORT_MATCHING, LONG_MATCIHNG };
    enum { PARTIAL_MATCHING, ALL_MATGHING };
      
  public:

    explicit TokenRules() {}
    virtual ~TokenRules() { clear(); }
    void clear() { DELETE_STL_CONTAINERS(token_rules_); }

    bool build(const Sexp::Cell *cell) {
      clear();
      bool prev_negation = false;
      TokenRule *prev = 0;
      for (const Sexp::Cell *cell2 = cell; cell2; cell2 = cell2->cdr()) {
        if (cell2->car()->is_atom()) {
          const char *atom = cell2->car()->atom();
          CHECK_DIE(atom);
          if (std::strcmp(atom, "^") == 0) {
            prev_negation = true;
          } else if (std::strcmp(atom, "?") == 0) {
            TokenRule *rule = new TokenRule;
            rule->set_qst_rule(true);
            rule->set_not_rule(prev_negation);
            token_rules_.push_back(rule);
            prev = rule;
            prev_negation = false;
          } else if (std::strcmp(atom, "*") == 0) {
            CHECK_DIE(prev);
            prev->set_ast_rule(true);
            prev->set_not_rule(prev_negation);
            prev_negation = false;
          } else if (std::strcmp(atom, "?*") == 0) {
            TokenRule *rule = new TokenRule;
            rule->set_qst_rule(true);
            rule->set_ast_rule(true);
            rule->set_not_rule(prev_negation);
            token_rules_.push_back(rule);
            prev = rule;
            prev_negation = false;
          } else {
            return false;
          }
        } else {
          TokenRule *rule = new TokenRule;
          rule->build(cell2->car());
          rule->set_not_rule(prev_negation);
          token_rules_.push_back(rule);
          prev_negation = false;
          prev = rule;
        }
      }
      return true;
    }
  };

  class MorphRule {
  private:
    TokenRules * pre_pattern_;
    TokenRules * self_pattern_;
    TokenRules * post_pattern_;

  public:
    int match(Token **d_ptr,
              int bw_length,
              int fw_length) const {
      if (!pre_pattern && bw_length != 0)
        return -1;

      if (pre_pattern &&
          pre_pattern->match(d_ptr - 1,
                             bw_length,
                             -1,
                             ALL_MATGHING,
                             SHORT_MATCHING) == -1)
        return -1;

      int match_length = fw_length;
      while (match_length > 0) {
        if (!self_pattern_) {
          match_length = 1;
        } else if ((match_result = 
                    self_pattern->match(d_ptr,
                                        match_length,
                                        +1,
                                        PARTIAL_MATCHING,
                                        LONG_MATCIHNG)) != -1) {
          match_length -= match_result;
        } else {
          return -1;
        }

        // post
        if (!post_pattern_ ||
            post_pattern_->match(d_ptr + match_length,
                                 fw_length - match_length,
                                 +1,
                                 ALL_MATGHING,
                                 SHORT_MATCHING) != -1) {
          return match_length;
        }
        --match_length;
      }
      return -1;
    }
  }
}

// bool open(const char *filename) {
//     Mmap<char> mmap;
//     CHECK_FALSE(mmap.open(filename)) << "no such file or directory: " << filename;
//     const char *begin = mmap.begin();
//     const char *end = mmap.end();
//     while (begin < end) {
//       if (!pre_cell || !self_cell || !post_cell || !action) {
//         std::cerr << "format error";
//         continue;
//       }
//     }
//   }
// }

using namespace CaboCha;

int main() {
  CaboCha::Sexp sexp;
  CaboCha::TokenRules tokenrules;

  std::string buf, line;
  while (std::getline(std::cin, line)) {
    buf += line;
    buf += "\n";
  }

#define _Cdr(a) CaboCha::Sexp::Cell::Cdr(a)
#define _Car(a) CaboCha::Sexp::Cell::Car(a)

  char *begin = (char *)buf.c_str();
  char *end = begin + strlen(begin);
  while (begin < end) {
    const CaboCha::Sexp::Cell *body_cell = sexp.read(&begin, end);
    if (!body_cell) continue;
    const Sexp::Cell *pre_cell = _Car(body_cell);
    const Sexp::Cell *self_cell = _Car(_Cdr(body_cell));
    const Sexp::Cell *post_cell = _Car(_Cdr(_Cdr(body_cell)));
    const Sexp::Cell *action = _Cdr(_Cdr(_Cdr(body_cell)));
    std::cout << "\n\n\n";
    std::cout << "all: ";
    Sexp::dump(body_cell);
    std::cout << "pre: ";
    Sexp::dump(pre_cell);
    std::cout << "sel: ";
    Sexp::dump(self_cell);
    std::cout << "pos: ";
    Sexp::dump(post_cell);
    std::cout << "action: ";
    Sexp::dump(action);
    tokenrules.build(pre_cell);
    tokenrules.build(self_cell);
    tokenrules.build(post_cell);
  }
  return 0;
}

