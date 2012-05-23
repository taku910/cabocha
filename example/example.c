#include <cabocha.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(eval) if (! eval) { \
    fprintf (stderr, "Exception:%s\n", cabocha_strerror(cabocha)); \
    cabocha_destroy(cabocha); \
    return -1; }

int main(int argc, char **argv) {
  const char p[] = "太郎は花子が持っている本を次郎に渡した。";
  cabocha_t *cabocha = cabocha_new(argc, argv);
  unsigned int i;
  unsigned int size;
  unsigned int cid;
  const char *result = 0;
  cabocha_tree_t *tree = 0;

  CHECK(cabocha);
  result = cabocha_sparse_tostr(cabocha, p);
  CHECK(result);
  printf("%s", result);
  tree = cabocha_sparse_totree(cabocha, p);
  CHECK(tree);
  printf("%s", cabocha_tree_tostr(tree, CABOCHA_FORMAT_TREE));

  size = cabocha_tree_token_size(tree);
  cid = 0;
  for (i = 0; i < size; ++i) {
    const cabocha_token_t *token = cabocha_tree_token(tree, i);
    if (token->chunk != NULL)
      printf ("* %d %dD %d/%d %f\n",
              cid++,
              token->chunk->link,
              token->chunk->head_pos,
              token->chunk->func_pos,
              token->chunk->score);
    printf ("%s\t%s\t%s\n",
            token->surface,
            token->feature,
            token->ne ? token->ne : "O");
  }
  printf ("EOS\n");

  cabocha_destroy(cabocha);

  return 0;
}
 
