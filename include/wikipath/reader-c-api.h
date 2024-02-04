#ifndef WIKIPATH_READER_C_API_H
#define WIKIPATH_READER_C_API_H

// A C-only wrapper library around the Reader interface.
//
// This is intended to be used from other languages (like Python) that can
// integrate more easily with C libraries than C++ libraries.
//
// If not specified otherwise, functions that return an int return 0 on success
// and a nonzero value on error.

#include <stdlib.h>
#include <stdint.h>

#ifdef __cplusplus

namespace wikipath {
	struct Reader;
	struct SearchStats;
}

typedef wikipath::Reader wikipath_Reader;
typedef wikipath::SearchStats wikipath_SearchStats;

#endif

#pragma GCC visibility push(default)

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t index_t;  // from common.h

// From searcher.h
struct SearchStats {
    int64_t vertices_reached = 0;
    int64_t vertices_expanded = 0;
    int64_t edges_expanded = 0;
    int64_t time_taken_ms = 0;
};

// Frees a string. It is safe but unnecessary to free NULL pointers.
void wikipath_string_free(char *p);

// Frees a list of indices.
void wikipath_ids_free(index_t ids[]);


// Opens the reader.
//
// If reader is not NULL, it is assigned the reader handler (which must be freed
// with wikipath_Reader_Close()) on success, or NULL on failure.
int wikipath_Reader_Open(
	const char *graph_filename,
	wikipath_Reader **reader);

// Closes the reader, releases its resources, and assigns it NULL.
//
// While `reader` must not be NULL, *reader may be NULL, in which case this
// function does nothing.
//
// The reader handle may not be used afterwards.
void wikipath_Reader_Close(wikipath_Reader **reader);

// Returns a random page id.
index_t wikipath_Reader_RandomPageId(wikipath_Reader *reader);

//
// Graph methods
//

// Returns the number of vertices in the graph, i.e., the number of pages.
index_t wikipath_Reader_VertexCount(wikipath_Reader *reader);

// Returns the number of edges in the graph, i.e., the number of links between
// pages. This does not double-count incoming and outgoing links.
index_t wikipath_Reader_EdgeCount(wikipath_Reader *reader);

// Retrieves the outgoing links for the given page.
//
// If pages != NULL, it will point to an array of page ids (which must be
// freed with wikipath_ids_free()).
// If count != NULL, it will point to the length of the array.
int wikipath_Reader_ForwardEdges(
    wikipath_Reader *reader, index_t page,
    index_t **pages, size_t *count);

// Retrieves the incoming links for the given page.
//
//
// If pages != NULL, it will point to an array of page ids (which must be
// freed with wikipath_ids_free()).
// If count != NULL, it will point to the length of the array.
int wikipath_Reader_BackwardEdges(
    wikipath_Reader *reader, index_t page,
    index_t **pages, size_t *count);

//
// Metadata methods
//

// Returns the page id for a given title (which is case sensitive!) or 0 if the
// page is not found.
index_t wikipath_Reader_GetPageIdByTitle(
    wikipath_Reader *reader, const char *title);

// Retrieves a page by id.
//
// Returns 0 if the page exists. If `title` is not NULL, it is used to store
// the title of the page, which must be freed with wikipath_string_free().
//
// Returns 1 if the page is not found.
int wikipath_Reader_GetPage(
    wikipath_Reader *reader, index_t page_id, char **title);

// Retrieves a link by the ids of the source and destination page.
//
// Returns 0 if the link exists. If `text` is not NULL, it is used to store
// the link text (which must be freed with wikipath_string_free()), or NULL if
// there is no link text (which implies the link text is the same as the title
// of the destination page).
//
// Returns 1 if the link does not exist.
int wikipath_Reader_GetLink(
	  wikipath_Reader *reader, index_t from_page_id, index_t to_page_id,
  	char **text);

//
// Search methods
//

// Searches for a shortest path and returns 0 if a path is found.
//
// If there is a path, it will include both the start and finish page, so
// it has length 1 if from_page_id == to_pageId, or at least 2 otherwise.
//
// If pages != NULL, it will point to an array of page ids (which must be
// freed with wikipath_ids_free()).
//
// If count != NULL, it will point to the length of the pages array.
//
// If stats != NULL, search statistics will be written to *stats.
int wikipath_Reader_FindShortestPath(
	wikipath_Reader *reader, index_t from_page_id, index_t to_page_id,
	index_t **pages, size_t *count, wikipath_SearchStats *stats);

#ifdef __cplusplus
}  // extern "C"
#endif

#pragma GCC visibility pop

#endif  // ndef WIKIPATH_READER_C_API_H
