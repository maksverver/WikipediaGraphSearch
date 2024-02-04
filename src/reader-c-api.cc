#include "wikipath/reader-c-api.h"

#include "wikipath/reader.h"
#include "wikipath/searcher.h"

#include <algorithm>
#include <cassert>
#include <cstring>

namespace {

using wikipath::Reader;
using wikipath::EdgeList;

// Copies a std::string to a new char* buffer, like strdup(s.c_str()), but
// slightly faster because we can the fact that the string's size is known.
char *CopyString(const std::string &s) {
  size_t n = s.size() + 1;
  char *p = (char*) malloc(n + 1);
  if (p != nullptr) {
    memcpy(p, s.data(), n);
    p[n] = '\0';
  }
  return p;
}

char *CopyString(const std::optional<std::string> &s) {
  return s ? CopyString(*s) : nullptr;
}

// Copies the edge list to an array of destination pages.
//
// Returns false if allocation fails.
bool CopyEdgeList(const EdgeList &edges, index_t **pages, size_t *count) {
  if (pages == nullptr && count == nullptr) return true;

  size_t n = 0;
  for (auto it = edges.begin(); it != edges.end(); ++it) ++n;

  if (count != nullptr) *count = n;

  if (pages != nullptr) {
    *pages = (index_t*) calloc(n, sizeof(index_t));
    if (*pages == nullptr) return false;  // allocation failed
    size_t i = 0;
    auto it = edges.begin();
    while (it != edges.end() && i != n) (*pages)[i++] = *it++;
  }

  return true;
}

}  // namespace

void wikipath_string_free(char *p) {
  free(p);
}

void wikipath_ids_free(index_t ids[]) {
  free(ids);
}

int wikipath_Reader_Open(const char *graph_filename, wikipath_Reader **reader) {
  if (auto res = Reader::Open(graph_filename); res) {
    if (reader != nullptr) *reader = res.release();
    return 0;
  } else {
    if (reader != nullptr) *reader = nullptr;
    return 1;
  }
}

void wikipath_Reader_Close(wikipath_Reader **reader) {
  assert(reader != nullptr);
  if (*reader != nullptr) {
    delete *reader;
    *reader = nullptr;
  }
}

index_t wikipath_Reader_RandomPageId(wikipath_Reader *reader) {
  return reader->RandomPageId();
}

index_t wikipath_Reader_VertexCount(wikipath_Reader *reader) {
  return reader->Graph().VertexCount();
}

index_t wikipath_Reader_EdgeCount(wikipath_Reader *reader) {
  return reader->Graph().EdgeCount();
}

int wikipath_Reader_ForwardEdges(
    wikipath_Reader *reader, index_t page,
    index_t **pages, size_t *count) {
  return reader->IsValidPageId(page) &&
      CopyEdgeList(reader->Graph().ForwardEdges(page), pages, count) ? 0 : 1;
}

int wikipath_Reader_BackwardEdges(
    wikipath_Reader *reader, index_t page,
    index_t **pages, size_t *count) {
  return reader->IsValidPageId(page) &&
      CopyEdgeList(reader->Graph().BackwardEdges(page), pages, count) ? 0 : 1;
}

index_t wikipath_Reader_GetPageIdByTitle(wikipath_Reader *reader, const char *title) {
  auto page = reader->Metadata().GetPageByTitle(title);
  return page ? page->id : 0;
}

int wikipath_Reader_GetPage(wikipath_Reader *reader, index_t page_id, char **title) {
  if (auto page = reader->Metadata().GetPageById(page_id); page) {
    if (title != nullptr) *title = CopyString(page->title);
    return 0;
  } else {
    if (title != nullptr) title = nullptr;
    return 1;
  }
}

int wikipath_Reader_GetLink(
    Reader *reader, index_t from_page_id, index_t to_page_id, char **text_out) {
  if (auto text = reader->LinkText(from_page_id, to_page_id); text) {
    if (text_out != nullptr) *text_out = CopyString(*text);
    return 0;
  } else {
    if (text_out != nullptr) *text_out = nullptr;
    return 1;
  }
}

int wikipath_Reader_FindShortestPath(
    wikipath_Reader *reader, index_t from_page_id, index_t to_page_id,
    index_t **pages, size_t *count, wikipath_SearchStats *stats) {
  std::vector<index_t> path = FindShortestPath(reader->Graph(), from_page_id, to_page_id, stats);
  if (!path.empty()) {
    if (count != nullptr) *count = path.size();
    *pages = (index_t*) calloc(path.size(), sizeof(index_t));
    if (*pages == nullptr) return 1;  // allocation failed!
    for (size_t i = 0; i < path.size(); ++i) (*pages)[i] = path[i];
    return 0;
  } else {
    // No path found.
    if (pages != nullptr) *pages = nullptr;
    if (count != nullptr) *count = 0;
    return 1;
  }
}
