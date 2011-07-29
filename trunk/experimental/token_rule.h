#ifndef CABOCHA_TOKENRULE_H__
#define CABOCHA_TOKENRULE_H__

#include <vector>
#include <map>
#include <string>
#include "sexp.h"
#include "cabocha.h"
#include "common.h"

namespace CaboCha {

  //  class Sexp;
  //  class Sexp::Cell;

  // Rule that matches a single token
  class TokenRule {
  private:
    bool not_rule_ : 1;
    bool ast_rule_ : 1;
    bool qst_rule_ : 1;
    class FeatureRule;
    std::vector<FeatureRule *> feature_rules_;

  public:
    explicit TokenRule(): not_rule_(false),
                          ast_rule_(false), qst_rule_(false) {}
    virtual ~TokenRule() { clear(); }

    bool ast_rule() const { return ast_rule_; }
    bool qst_rule() const { return qst_rule_; }
    bool not_rule() const { return not_rule_; }
    void set_ast_rule(bool flg) { ast_rule_ = flg; }
    void set_qst_rule(bool flg) { qst_rule_ = flg; }
    void set_not_rule(bool flg) { not_rule_ = flg; }

    bool match(const Token *token) const;
    bool build(const Sexp::Cell *cell);
    void clear();
  };
}

#endif
