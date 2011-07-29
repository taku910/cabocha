#include "token_rule.h"

namespace CaboCha {

  // Rule that matches a single feature
  class TokenRule::FeatureRule {
  private:
    bool not_rule_ : 1;
    bool or_rule_  : 1;
    bool ast_rule_ : 1;
    union {
      std::string *atom_pattern_;
      std::map<std::string, char> *or_pattern_;
    } pattern_;

  public:
    explicit FeatureRule(): not_rule_(false),
                            or_rule_(false), ast_rule_(false) {
      pattern_.or_pattern_ = 0;
      pattern_.atom_pattern_ = 0;
    }

    virtual ~FeatureRule() { clear(); }

    bool ast_rule() const { return ast_rule_; }
    bool or_rule() const { return or_rule_; }
    bool not_rule() const { return not_rule_; }

    void set_ast_rule(bool flg) { ast_rule_ = flg; }
    void set_or_rule(bool flg)  { or_rule_ = flg; }
    void set_not_rule(bool flg) { not_rule_ = flg; }

    bool match(const char *key) const {
      bool match = false;
      if (ast_rule_) {
        match = true;   // always match
      } else if (or_rule_) {
        std::map<std::string, char>::const_iterator it
          = pattern_.or_pattern_->find(key);
        match = (it != pattern_.or_pattern_->end());
        match = (it->second < 0 ? !match : match);
      } else {
        match = (*(pattern_.atom_pattern_) == key);
      }
      return (not_rule_ ? !match : match);
    }

    bool build(const Sexp::Cell *cell) {
      if (cell->is_atom()) {  // terminal rule
        or_rule_ = false;
        ast_rule_ = false;
        const char *atom = cell->atom();
        if (std::strcmp(atom, "*") == 0) {
          ast_rule_ = true;
          std::cout << "INI: * " << not_rule_ << " " << ast_rule_ << std::endl;
        } else {
          CHECK_DIE(std::strlen(atom) >= 1);
          if (atom[0] == '^') {
            not_rule_ = true;
            ++atom;
          }
          std::cout << "INI: " << atom << " "
                    << not_rule_ << " " << ast_rule_ << std::endl;
          pattern_.atom_pattern_ = new std::string(atom);
        }
      } else {
        or_rule_ = true;
        ast_rule_ = false;
        pattern_.or_pattern_ = new std::map<std::string, char>;
        for (const Sexp::Cell *cell2 = cell; cell2; cell2 = cell2->cdr()) {
          CHECK_DIE(cell2->car()->is_atom()) << "ERROR: must be ATOM";
          const char *atom = cell2->car()->atom();
          CHECK_DIE(atom && atom[0] != '\0') << "empty ATOM";
          char value = +1;
          if (atom[0] == '^') {
            value = -1;
            ++atom;
          }
          std::cout << "PAT: " << atom << " "
                    << static_cast<int>(value) << std::endl;
          pattern_.or_pattern_->insert(std::make_pair<std::string, char>
                                       (atom, value));
        }
      }
      return true;
    }

    void clear() {
      if (or_rule_) {
        delete pattern_.or_pattern_;
        pattern_.or_pattern_ = 0;
      } else {
        delete pattern_.atom_pattern_;
        pattern_.atom_pattern_ = 0;
      }
      or_rule_ = not_rule_ = false;
    }
  };

  bool TokenRule::match(const Token *token) const {
    if (qst_rule_) return !not_rule_;
    size_t size = std::min(feature_rules_.size(),
                           static_cast<size_t>(token->feature_list_size));
    for (size_t i = 0; i < size; ++i) {
      if (!feature_rules_[i]->match(token->feature_list[i]))
        return not_rule_;
    }
    return !not_rule_;
  }

  bool TokenRule::build(const Sexp::Cell *cell) {
    clear();
    CHECK_DIE(!cell->is_atom());
    bool prev_negate = false;
    for (const Sexp::Cell *cell2 = cell; cell2; cell2 = cell2->cdr()) {
      if (cell2->car()->is_atom() &&
          std::strcmp("^", cell2->car()->atom()) == 0) {
        prev_negate = true;
      } else {
        FeatureRule *rule = new FeatureRule;
        CHECK_DIE(rule->build(cell2->car()));
        rule->set_not_rule(prev_negate);
        feature_rules_.push_back(rule);
        prev_negate = false;
      }
    }
    return true;
  }

  void TokenRule::clear() {
    DELETE_STL_CONTAINERS(feature_rules_);
    not_rule_ = qst_rule_ = ast_rule_ = false;
  }
}
