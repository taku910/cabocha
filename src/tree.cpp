// CaboCha -- Yet Another Japanese Dependency Parser
//
//  $Id: tree.cpp 50 2009-05-03 08:25:36Z taku-ku $;
//
//  Copyright(C) 2001-2008 Taku Kudo <taku@chasen.org>
#include <algorithm>
#include <vector>
#include <string>
#include <sstream>
#include "cabocha.h"
#include "common.h"
#include "normalizer.h"
#include "mecab.h"
#include "string_buffer.h"
#include "freelist.h"
#include "ucs.h"
#include "tree_allocator.h"

namespace CaboCha {
namespace {

size_t get_string_length(const std::string &str, int charset) {
  const char *begin = str.c_str();
  const char *end = str.c_str() + str.size();
  size_t mblen = 0;
  size_t result = 0;
  while (begin < end) {
    switch (charset) {
      case UTF8:
        utf8_to_ucs2(begin, end, &mblen);
        break;
#ifndef CABOCHA_USE_UTF8_ONLY
      case EUC_JP:
        euc_to_ucs2(begin, end, &mblen);
        break;
      case CP932:
        cp932_to_ucs2(begin, end, &mblen);
        break;
#endif
      default:
        mblen = 1;
    }
    if (mblen == 1) {
      ++result;
    } else {
      result += 2;
    }
    begin += mblen;
  }
  return result;
}

char *getline(char **begin, int *length) {
  char *end = *begin + *length;
  char *n = std::find(*begin, end, '\n');
  if (n != end) {
    *n = '\0';
  }
  *length -= static_cast<int>(n - *begin);
  char *result = *begin;
  *begin = n + 1;
  return result;
}

void make_csv_feature(const char **feature, size_t size,
                      std::string *str) {
  std::string tmp;
  for (size_t i = 0; i < size; ++i) {
    tmp = feature[i] ? std::string(feature[i]) : "*";
    escape_csv_element(&tmp);
    if (i != 0) {
      *str += ',';
    }
    *str += tmp;
  }
}

void write_lattice_token(const Token &token, StringBuffer *os) {
  *os << token.surface << '\t' << token.feature;
  if (token.ne) {
    *os << '\t' << token.ne;
  }
  *os << '\n';
}

void write_lattice(const Tree &tree, StringBuffer *os,
                   int output_layer, int charset) {
  const size_t size = tree.token_size();
  if (output_layer == OUTPUT_RAW_SENTENCE) {
    if (tree.empty()) {
      *os << tree.sentence() << '\n';
    } else {
      for (size_t i = 0; i < size; ++i) {
        *os << tree.token(i)->surface;
      }
      *os << '\n';
    }
  } else {
    size_t ci = 0;
    for (size_t i = 0; i < size; i++) {
      const Token *token = tree.token(i);
      if (token->chunk && output_layer != OUTPUT_POS) {
        const Chunk *chunk = token->chunk;
        switch (output_layer) {
          case OUTPUT_CHUNK:
            *os << "* " << ci++ << ' ' << chunk->link << "D ";
            break;
          case OUTPUT_SELECTION:
            *os << "* " << ci++ << ' ' << chunk->link << "D "
                << chunk->head_pos << "/" << chunk->func_pos
                << " " << chunk->score;
            if (chunk->feature_list) {
              std::string str;
              make_csv_feature(chunk->feature_list,
                               chunk->feature_list_size, &str);
              *os << ' ' << str;
            }
            break;
          case OUTPUT_DEP:
            *os << "* " << ci++ << ' ' << chunk->link << "D "
                << chunk->head_pos << "/" << chunk->func_pos
                << " " << chunk->score;
            break;
        }
        *os << '\n';
      }
      write_lattice_token(*token, os);
    }
    *os << "EOS\n";
  }
}

void write_xml_token(const Token &token, StringBuffer *os, int i) {
  *os << "  <tok id=\"" << i << "\""
      << " feature=\"" << token.feature << "\"";
  if (token.ne) {
    *os << " ne=\""     << token.ne << "\"";
  }
  *os << ">" << token.surface << "</tok>\n";
}

void write_xml(const Tree &tree, StringBuffer *os,
               int output_layer, int charset) {
  size_t ci = 0;
  size_t size = tree.token_size();

  *os << "<sentence>\n";
  for (size_t i = 0; i < size; ++i) {
    const Token *token = tree.token(i);
    const Chunk *chunk = token->chunk;
    if (chunk && output_layer != OUTPUT_POS) {
      if (ci) {
        *os << " </chunk>\n";
      }
      *os << " <chunk id=\"" << ci++ << "\" link=\"" << chunk->link
          << "\" rel=\"D"
          << "\" score=\"" << chunk->score
          << "\" head=\"" << chunk->head_pos + i
          << "\" func=\"" << chunk->func_pos + i << "\"";
      if (output_layer == OUTPUT_SELECTION && chunk->feature_list) {
        std::string str;
        make_csv_feature(chunk->feature_list, chunk->feature_list_size, &str);
        *os << " feature=\"" << str << "\"";
      }
      *os << ">\n";
    }
    write_xml_token(*token, os, i);
  }

  if (ci) {
    *os << " </chunk>\n";
  }
  *os << "</sentence>\n";
}

void write_tree(const Tree &tree, StringBuffer *os,
                int output_layer, int charset) {
  const size_t size = tree.token_size();
  bool in = false;
  size_t max_len = 0;
  std::string ne;
  std::vector<std::pair<size_t, std::string> > chunks;

  for (size_t i = 0; i < size;) {
    size_t cid = i;
    chunks.resize(chunks.size() + 1);
    std::string surface;
    for (; i < size; ++i) {
      const Token *token = tree.token(i);
      if (in && token->ne &&
          (token->ne[0] == 'B' || token->ne[0] == 'O')) {
        surface += "</";
        surface += ne;
        surface += ">";
        in = false;
      }

      if (i != cid && token->chunk) {
        break;
      }

      if (token->ne && token->ne[0] == 'B') {
        ne = std::string(token->ne + 2);
        surface += "<";
        surface += ne;
        surface += ">";
        in = true;
      }

      surface += std::string(token->surface);

      if (in && i + 1 == size) {
        surface += "</";
        surface += ne;
        surface += ">";
      }
    }

    max_len = std::max(static_cast<size_t>(max_len),
                       get_string_length(surface, charset));
    chunks.back() = std::make_pair(cid, surface);
  }

  std::vector<int> e(chunks.size(), 0);

  for (size_t i = 0; i < chunks.size(); ++i) {
    bool isdep = false;
    const int  link = tree.token(chunks[i].first)->chunk->link;
    const std::string &surface = chunks[i].second;
    const size_t rem = max_len - get_string_length(surface, charset) + i * 2;
    for (size_t j = 0; j < rem; ++j) {
      *os << ' ';
    }
    *os << surface.c_str();

    for (size_t j = i+1; j < chunks.size(); j++) {
      if (link == static_cast<int>(j)) {
        *os << "-" << "D";
        isdep = 1;
        e[j] = 1;
      } else if (e[j]) {
        *os <<  " |";
      } else if (isdep) {
        *os << "  ";
      } else {
        *os << "--";
      }
    }
    *os << "\n";
  }

  *os << "EOS\n";
}

bool write_tree(const Tree &tree, StringBuffer *os,
                int output_layer, int output_format, int charset) {
  os->clear();
  switch (output_format) {
    case FORMAT_LATTICE:
      write_lattice(tree, os, output_layer, charset);
      break;
    case FORMAT_TREE_LATTICE:
      write_tree(tree, os, output_layer, charset);
      write_lattice(tree, os, output_layer, charset);
      break;
    case FORMAT_TREE:
      write_tree(tree, os, output_layer, charset);
      break;
    case FORMAT_XML:
      write_xml(tree, os, output_layer, charset);
      break;
    case FORMAT_NONE:
      break;
    default:
      *os << "unknown format: " << output_format << '\n';
      return true;
  }

  return true;
}
} // end of anonymous namespace

Tree::Tree()
    : tree_allocator_(new TreeAllocator),
      charset_(EUC_JP), posset_(IPA),
      output_layer_(OUTPUT_DEP) {}

Tree::~Tree() {
  delete tree_allocator_;
  tree_allocator_ = 0;
}

void Tree::clear() {
  tree_allocator_->free();
  tree_allocator_->token.clear();
  tree_allocator_->chunk.clear();
  tree_allocator_->sentence.clear();
}

void Tree::set_sentence(const char *sentence) {
  tree_allocator_->sentence = sentence;
}

const char *Tree::sentence() const {
  return tree_allocator_->sentence.c_str();
}

size_t Tree::sentence_size() const {
  return tree_allocator_->sentence.size();
}

void Tree::set_sentence(const char *sentence, size_t length) {
  tree_allocator_->sentence.assign(sentence, length);
}

const Chunk *Tree::chunk(size_t i) const {
  return tree_allocator_->chunk[i];
}

const Token *Tree::token(size_t i) const {
  return tree_allocator_->token[i];
}

Chunk *Tree::mutable_chunk(size_t i) {
  return const_cast<Chunk *>(tree_allocator_->chunk[i]);
}

Token *Tree::mutable_token(size_t i) {
  return const_cast<Token *>(tree_allocator_->token[i]);
}

bool   Tree::empty() const { return tree_allocator_->token.empty(); }
void   Tree::clear_chunk() { return tree_allocator_->chunk.clear(); }
size_t Tree::chunk_size() const { return tree_allocator_->chunk.size(); }
size_t Tree::token_size() const { return tree_allocator_->token.size(); }
size_t Tree::size() const { return tree_allocator_->token.size(); }

Chunk *Tree::add_chunk() {
  Chunk *chunk =tree_allocator_->allocChunk();
  tree_allocator_->chunk.push_back(chunk);
  return chunk;
}

Token *Tree::add_token() {
  Token *token = tree_allocator_->allocToken();
  tree_allocator_->token.push_back(token);
  return token;
}

char *Tree::strdup(const char *str) {
  return tree_allocator_->alloc(str);
}

char *Tree::alloc(size_t size) {
  return tree_allocator_->alloc(size);
}

char **Tree::alloc_char_array(size_t size) {
  return tree_allocator_->alloc_char_array(size);
}

TreeAllocator *Tree::allocator() const {
  return tree_allocator_;
}

bool Tree::read(const mecab_node_t *node) {
  clear();
  if (!node) {
    return false;
  }
  std::string normalized;
  char *cols[256];
  for (; node; node = node->next) {
    if (node->stat == MECAB_BOS_NODE || node->stat == MECAB_EOS_NODE) {
      continue;
    }
    Token *token  = add_token();
    char *surface = this->alloc(node->length);
    std::copy(node->surface, node->surface + node->length, surface);
    surface[node->length] = '\0';
    token->surface = surface;
    normalized.clear();
    Normalizer::normalize(charset_,
                          node->surface, node->length,
                          &normalized);
    token->normalized_surface = this->strdup(normalized.c_str());
    token->feature = this->strdup(node->feature);
    token->chunk = 0;
    token->ne = 0;
    tree_allocator_->sentence.append(node->surface, node->length);
    char *tmp = this->strdup(token->feature);
    const size_t s = tokenizeCSV(tmp, cols, sizeof(cols));
    char **feature = alloc_char_array(s);
    std::copy(cols, cols + s, feature);
    token->feature_list = const_cast<const char **> (feature);
    token->feature_list_size = s;
  }
  return true;
}

bool Tree::read(const char *input,
                InputLayerType input_layer) {
  return this->read(input, std::strlen(input), input_layer);
}

bool Tree::read(const char *input, size_t length,
                InputLayerType input_layer) {
  clear();

  if (!input) {
    return false;
  }

  int chunk_id = 0;

  switch (input_layer) {
    case INPUT_RAW_SENTENCE:
      set_sentence(input, length);
      break;

    case INPUT_POS:
    case INPUT_CHUNK:
    case INPUT_SELECTION:
    case INPUT_DEP:
      Chunk *chunk = 0;
      Chunk *old_chunk = 0;
      char *column[8];
      char *cols[256];
      std::string normalized;

      char *buf = this->alloc(length + 1);
      std::strncpy(buf, input, length);
      int len = static_cast<int>(length);

      while (true) {
        char *line = getline(&buf, &len);
        if (len <= 0) break;
        if (std::strlen(line) >= 3 && line[0] == '*' && line[1] == ' ') {
          const size_t size = tokenize(line, " ", column, sizeof(column));
          if (size >= 3 && (column[1][0] == '-' || isdigit(column[1][0]))) {
            if (input_layer == INPUT_POS) continue;
            chunk = add_chunk();
            chunk->link = std::atoi(column[2]);

            if (chunk_id != std::atoi(column[1])) {
              return false;
            }
            ++chunk_id;

            if (size >= 4) {
              size_t i = 0;
              const size_t len = std::strlen(column[3]) - 1;
              for (i = 0; i < len ; i++)
                if (column[3][i] == '/') break;
              chunk->head_pos = std::atoi(column[3]);
              chunk->func_pos = std::atoi(column[3] +i + 1);
            }

            if (size >= 5) {
              chunk->score = std::atof(column[4]);
            }

            if (size >= 6) {
              const size_t s = tokenizeCSV(column[5], cols, sizeof(cols));
              char **feature = alloc_char_array(s);
              std::copy(cols, cols + s, feature);
              chunk->feature_list = const_cast<const char **> (feature);
              chunk->feature_list_size = s;
            }

            chunk->token_pos = token_size();

            if (old_chunk && old_chunk->token_size == 0) {
              return false;
            }

            old_chunk = chunk;
          } else {
            return false;
          }
        } else {
          const size_t size = tokenize(line, "\t", column, sizeof(column));
          if (size >= 2 &&
              std::strlen(column[0]) > 0 && std::strlen(column[1]) > 0) {
            Token *token   = add_token();
            token->chunk   = chunk;
            token->surface = column[0];
            Normalizer::normalize(charset_,
                                  column[0], std::strlen(column[0]),
                                  &normalized);
            token->normalized_surface = this->strdup(normalized.c_str());
            token->feature = column[1];
            token->ne      = size >= 3 ? column[2] : 0;
            tree_allocator_->sentence.append(column[0]);

            char *tmp = this->strdup(token->feature);
            const size_t s = tokenizeCSV(tmp, cols, sizeof(cols));
            char **feature = alloc_char_array(s);
            std::copy(cols, cols + s, feature);
            token->feature_list = const_cast<const char **> (feature);
            token->feature_list_size = s;
            chunk = 0;
            if (old_chunk) {
              old_chunk->token_size++;
            }
          } else {
            break;
          }
        }
      }

      if (old_chunk && old_chunk->token_size == 0) {
        return false;
      }
  }

  // verfy chunk link
  for (size_t i = 0; i < chunk_size(); ++i) {
    if (chunk(i)->link !=  -1 &&
        (chunk(i)->link >= static_cast<int>(chunk_size()) ||
         chunk(i)->link < -1)) {
      return false;
    }
  }


  return true;
}

const char *Tree::toString(FormatType output_format) {
  StringBuffer *os = tree_allocator_->mutable_string_buffer();
  os->clear();
  if (!write_tree(*this, os, output_layer_, output_format, charset_)) {
    return 0;
  }
  *os << '\0';
  return os->str();
}

const char *Tree::toString(FormatType output_format,
                           char *out, size_t len) const {
  StringBuffer os(out, len);
  if (!write_tree(*this, &os, output_layer_, output_format, charset_)) {
    return 0;
  }
  os << '\0';
  return os.str();
}

const char *Tree::what() {
  return tree_allocator_->what();
}
}
