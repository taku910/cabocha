#include <vector>
#include <string>
#include <iterator>
#include "param.h"
#include "common.h"
#include "token_rule.h"

namespace CaboCha {

  class Selector {
  private:
    class SelectorTokenRule {
    private:
      TokenRule rule_;
      std::vector<std::string> action_;
      bool not_rule_;

      void rewrite(const Token *token, std::string *output) const {
        return;
      }

    public:
      bool apply(const Token *token, std::string *output) const {
        if (rule_.match(token)) {
          rewrite(token, output);
        }
        return false;
      }

      bool build(const Sexp::Cell *cell) {
        not_rule_ = false;
        action_.clear();
        rule_.clear();
        if (cell->is_atom()) return false;
        if (cell->car()->is_atom()) {
          if (std::strcmp("^", cell->car()->atom()) != 0)
            return false;
          not_rule_ = true;
        } else {
          const Sexp::Cell *pattern = cell->car();
          cell = cell->cdr();
          if (!cell) return false;
          //          const Sexp::Cell *output = cell->car();
          cell = cell->cdr();
          if (!rule_.build(pattern)) return false;
          //          output->toArray(std::back_inserter(action_));
        }
      }
    };

    class SelectorTokenRules {
    private:
      std::vector<SelectorTokenRule *> token_rules_;
    public:
      bool match(const Token *token, std::string *output) {
        for (size_t i = 0; i < token_rules_.size(); ++i) {
          if (token_rules_[i]->apply(token, output))
            return true;
        }
        return false;
      }
    };

    // 2 patterns
    // (DIRECTION  ( ( (* * *) (foo bar) )  ( (* * *) (bar buz) )))
    // (DIRECTION  ( (^ (* * *) (foo bar) ) ( (* * *) (bar buz) )))
    bool parse_rule(const Sexp *sexp) {
      if (!cell) return false;
      // DIRECTION
      
      std::vector<TokenRule *> *direction = 0;
      {
        if (!cell->is_atom()) return false;
        const Sexp::Cell *atom = cell->car();
        if (atom->is_cons()) return false;
        std::string tmp direction->atom();
        if (tmp == "LR_FIRST") {
          direction = &lr_first_rule_;
        } else if (tmp == "LR_LAST") {
          direction = &lr_last_rule_;
        } else if (tmp == "RL_FIRST") {
          direction = &rl_fist_rule_;
        } else if (tmp == "RL_LAST") {
          direction = &rl_last_rule_;
        } else if (tmp == "ANY") {
          direction = &any_last_rule_;
          return false;
        }
      }

      bool negate_next = false;
      while (cell) {
        if (cell->car()->is_atom()) {
          if (std::strcmp("^", cell->car()->atom()) != 0)
            return false;
          negate_next = true;
        } else {
          const Sexp::Cell *pattern = cell->car();
          cell = cell->cdr();
          if (!cell) return false;
          const Sexp::Cell *output = cell->car();
          cell = cell->cdr();
          SelectorTokenRule *rule = new TokenRule;
          if (!rule->build(pattern, output)) {
            delete rule;
            return false;
          }
          rule->set_not_rule(negate_next);
          direction->push_back(rule);
          negate_next = false;
        }
      }

      return true;
    }
  }



    enum {LR_FIRST, LR_LAST, RL_FIRST, RL_LAST, ANY};

    std::vector<SelectorTokenRules *> lr_fisrt_rules_;
    std::vector<SelectorTokenRules *> lr_last_rules_;
    std::vector<SelectorTokenRules *> rl_fisrt_rules_;
    std::vector<SelectorTokenRules *> rl_last_rules_;
    std::vector<SelectorTokenRules *> any_rules_;

  public:
    bool parse_chunk(Chunk *chunk) const {
      // lr_fisrt
      {
        std::string output;
        for (size_t k = 0; k < lr_first_rules_.size(); ++k) {
          for (int i = 0; i < n; ++i) {
            if (lr_first_rules_[k]->apply(token, &tmp)) {
              output = tmp;
              break;
            }
          }
        }
      }

      // lr_last
      {
        std::string output;
        for (size_t k = 0; k < lr_first_rules_.size(); ++k) {
          for (int i = 0; i < n; ++i) {
            std::string tmp;
            if (lr_last_rules_[k]->apply(token, &tmp)) {
              output = tmp;
            }
          }
        }
      }

      // lr_fisrt
      {
        std::string output;
        for (size_t k = 0; k < lr_first_rules_.size(); ++k) {
          for (int i = n - 1; i >= 0; ++i) {
            if (lr_first_rules_[k]->apply(token, &tmp)) {
              output = tmp;
              break;
            }
          }
        }
      }

      // rl_last
      {
        std::string output;
        for (size_t k = 0; k < rl_first_rules_.size(); ++k) {
          for (int i = n - 1; i >= 0; ++i) {
            std::string tmp;
            if (lr_last_rules_[k]->apply(token, &tmp)) {
              output = tmp;
            }
          }
        }
      }

      // any
      {
        for (size_t k = 0; k < rl_first_rules_.size(); ++k) {
          for (int i = 0; i < n; ++i) {
            std::string tmp;
            if (lr_last_rules_[k]->apply(token, &tmp)) {
              append(tmp);
            }
          }
        }
      }

      return true;
    }

    bool open(const Param &param) {
      std::string head_rule = param.get<std::string>("head-rule");
      std::string func_rule = param.get<std::string>("func-rule");
      Sexp::sexp;
      CHECK_DIE(parse_rule(sexp, head_rule,
                           &head_token_rule, &head_direction_)) <<
        "parse error: head_rule " << head_rule;

      CHECK_DIE(parse_rule(sexp, func_rule,
                           &func_token_rule, &func_direction_)) <<
        "parse error: func_rule " << head_rule;
    }
  }
}
