#include <iostream>
#include <cabocha.h>

#define CHECK(eval) if (! eval) { \
    std::cerr << "Exception: " << parser->what() << std::endl ; \
    delete parser; \
    return -1; }

int main (int argc, char **argv) {
  CaboCha::Parser *parser = CaboCha::createParser(argc, argv);
  char p[] = "太郎は次郎が持っている本を花子に渡した。";

  if (!parser) {
    std::cerr << CaboCha::getParserError() << std::endl;
    return -1;
  }

  std::cout << parser->parseToString(p);
  const CaboCha::Tree *tree = parser->parse (p);
  CHECK(tree);


  size_t cid = 0;
  for (size_t i = 0; i < tree->token_size(); ++i) {
    const CaboCha::Token *token = tree->token(i);
    if (token->chunk)
      std::cout << "* "
                << cid++ << ' '
                << token->chunk->link << "D "
                << token->chunk->head_pos  << '/'
                << token->chunk->func_pos << std::endl;
    std::cout << token->surface << '\t'
              << token->feature;
    if (token->ne) {
      std::cout << '\t' << token->ne;
    }
    std::cout << std::endl;
  }

  std::cout << "EOS\n";

  return 0;
}
