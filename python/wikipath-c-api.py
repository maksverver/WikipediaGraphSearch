#!/usr/bin/env python3

import ctypes
from dataclasses import dataclass

lib = ctypes.cdll.LoadLibrary('libwikipath-reader-c-api.so')

index_t = ctypes.c_uint32

class _struct_Reader(ctypes.Structure):
    pass

class _struct_SearchStats(ctypes.Structure):
    _fields_ = [
        ('vertices_reached', ctypes.c_int64),
        ('vertices_expanded', ctypes.c_int64),
        ('edges_expanded', ctypes.c_int64),
        ('time_taken_ms', ctypes.c_int64),
    ]

@dataclass
class SearchStats:
    vertices_reached: int
    vertices_expanded: int
    edges_expanded: int
    time_taken_ms: int

    def from_struct(s):
        return SearchStats(
            vertices_reached=s.vertices_reached,
            vertices_expanded=s.vertices_expanded,
            edges_expanded=s.edges_expanded,
            time_taken_ms=s.time_taken_ms,
        )


reader_ptr_t = ctypes.POINTER(_struct_Reader)

# void wikipath_string_free(char *p);
lib.wikipath_string_free.argtypes = [ctypes.c_char_p]
lib.wikipath_string_free.restype = ctypes.c_void_p

# void wikipath_ids_free(index_t ids[]);
lib.wikipath_ids_free.argtypes = [ctypes.POINTER(index_t)]
lib.wikipath_ids_free.restype = ctypes.c_void_p

# int wikipath_Reader_Open(const char *graph_filename, Reader **reader);
lib.wikipath_Reader_Open.argtypes = [ctypes.c_char_p, ctypes.POINTER(reader_ptr_t)]

# void wikipath_Reader_Close(Reader **reader);
lib.wikipath_Reader_Close.argtypes = [ctypes.POINTER(reader_ptr_t)]

# index_t wikipath_Reader_RandomPageId(Reader *reader);
lib.wikipath_Reader_RandomPageId.argtypes = [reader_ptr_t]
lib.wikipath_Reader_RandomPageId.restype = index_t

# index_t wikipath_Reader_VertexCount(Reader *reader);
lib.wikipath_Reader_VertexCount.argtypes = [reader_ptr_t]
lib.wikipath_Reader_VertexCount.restype = index_t

# index_t wikipath_Reader_EdgeCount(Reader *reader);
lib.wikipath_Reader_EdgeCount.argtypes = [reader_ptr_t]
lib.wikipath_Reader_EdgeCount.restype = index_t

# int wikipath_Reader_ForwardEdges(
#     Reader *reader, index_t page,
#     index_t **pages, size_t *count);
lib.wikipath_Reader_ForwardEdges.argtypes = [reader_ptr_t, index_t,
        ctypes.POINTER(ctypes.POINTER(index_t)), ctypes.POINTER(ctypes.c_size_t)]

# int wikipath_Reader_BackwardEdges(
#     Reader *reader, index_t page,
#     index_t **pages, size_t *count);
lib.wikipath_Reader_BackwardEdges.argtypes = [reader_ptr_t, index_t,
        ctypes.POINTER(ctypes.POINTER(index_t)), ctypes.POINTER(ctypes.c_size_t)]

# index_t wikipath_Reader_GetPageIdByTitle(Reader *reader, const char *title);
lib.wikipath_Reader_GetPageIdByTitle.argtypes = [reader_ptr_t, ctypes.c_char_p]
lib.wikipath_Reader_GetPageIdByTitle.restype = index_t

# int wikipath_Reader_GetPage(Reader *reader, index_t page_id, char **title);
lib.wikipath_Reader_GetPage.argtypes = [reader_ptr_t, index_t, ctypes.POINTER(ctypes.c_char_p)]

# int wikipath_Reader_GetLink(
#     Reader *reader, index_t source, index_t destination, char **text);
lib.wikipath_Reader_GetLink.argtypes = [reader_ptr_t, index_t, index_t, ctypes.POINTER(ctypes.c_char_p)]

# int wikipath_Reader_FindShortestPath(
#     Reader *reader, index_t from_page_id, index_t to_page_id,
#     index_t **pages, size_t *count, SearchStats *stats);
lib.wikipath_Reader_FindShortestPath.argtypes = [reader_ptr_t, index_t, index_t,
        ctypes.POINTER(ctypes.POINTER(index_t)), ctypes.POINTER(ctypes.c_size_t),
        ctypes.POINTER(_struct_SearchStats)]


def _c_char_p_to_str(p):
    if not p:
        return None
    try:
        return str(ctypes.string_at(p), 'utf-8')
    finally:
        lib.wikipath_string_free(p)


def _ids_to_list(ids, count):
    try:
        return ids[0:count.value]
    finally:
        lib.wikipath_ids_free(ids)


def _page_debug_str(id_or_title):
    if isinstance(id_or_title, str): return '"' + id_or_title + '"'
    if isinstance(id_or_title, int): return '#' + str(id_or_title)
    assert False


class ReaderException(Exception):
    def __init__(self, message):
        super().__init__(message)


class PageNotFound(Exception):
    def __init__(self, page):
        self.page = page
        super().__init__('page not found: {}'.format(_page_debug_str(page)))


class LinkNotFound(Exception):
    def __init__(self, from_page, to_page):
        self.from_page = from_page
        self.to_page = to_page
        super().__init__('link not found: from {} to {}'.format(_page_debug_str(from_page), _page_debug_str(to_page)))


class Reader:
    def __init__(self, graph_filename):
        self.graph_filename = bytes(graph_filename, 'utf-8')
        self.open_reader = None

    def __enter__(self):
        assert self.open_reader is None
        reader_ptr = reader_ptr_t()
        if lib.wikipath_Reader_Open(self.graph_filename, ctypes.byref(reader_ptr)) != 0:
                raise ReaderException('open failed')
        self.open_reader = res = OpenReader(reader_ptr)
        return res

    def __exit__(self, *args):
        assert self.open_reader is not None
        lib.wikipath_Reader_Close(ctypes.byref(self.open_reader.reader_ptr))


class OpenReader:
    def __init__(self, reader_ptr):
        self.reader_ptr = reader_ptr

    def get_reader(self):
        p = self.reader_ptr
        if not p:
            raise ReaderException('reader is closed')
        return p

    def random_page_id(self):
        return lib.wikipath_Reader_RandomPageId(self.get_reader())

    def vertex_count(self):
        return lib.wikipath_Reader_VertexCount(self.get_reader())

    def edge_count(self):
        return lib.wikipath_Reader_EdgeCount(self.get_reader())

    def outgoing(self, page_id):
        pages = ctypes.POINTER(index_t)()
        count = ctypes.c_size_t()
        if lib.wikipath_Reader_ForwardEdges(self.get_reader(), page_id, ctypes.byref(pages), ctypes.byref(count)) != 0:
            raise PageNotFound(page_id)
        return _ids_to_list(pages, count)

    def incoming(self, page_id):
        pages = ctypes.POINTER(index_t)()
        count = ctypes.c_size_t()
        if lib.wikipath_Reader_BackwardEdges(self.get_reader(), page_id, ctypes.byref(pages), ctypes.byref(count)) != 0:
            raise PageNotFound(page_id)
        return _ids_to_list(pages, count)

    def page_title(self, page_id):
        title = ctypes.c_char_p()
        if lib.wikipath_Reader_GetPage(self.get_reader(), page_id, ctypes.byref(title)) != 0:
            raise PageNotFound(page_id)
        return _c_char_p_to_str(title)

    def page_id(self, title):
        page_id = lib.wikipath_Reader_GetPageIdByTitle(self.get_reader(), bytes(title, 'utf-8'))
        if page_id == 0: raise PageNotFound(title)
        return page_id

    def link_text(self, from_page_id, to_page_id):
        text = ctypes.c_char_p()
        if lib.wikipath_Reader_GetLink(self.get_reader(), from_page_id, to_page_id, ctypes.byref(text)) != 0:
            raise LinkNotFound(from_page_id, to_page_id)
        return _c_char_p_to_str(text)

    def shortest_path(self, from_page_id, to_page_id):
        '''Searches for a shortest path from `from_page_id` to` `to_page id`,
        and returns a pair (path, stats), where `path` is a list of page ids in
        the path starting `from_page_id` and ending with `to_page_id`, or an
        empty list if no path was found, and `stats` is a SearchStats instance
        that containst statistics collected during the search.'''
        pages = ctypes.POINTER(index_t)()
        count = ctypes.c_size_t()
        stats = _struct_SearchStats()
        err = lib.wikipath_Reader_FindShortestPath(
                self.get_reader(), from_page_id, to_page_id,
                ctypes.byref(pages), ctypes.byref(count),
                ctypes.byref(stats))
        return (
            _ids_to_list(pages, count) if err == 0 else [],
            SearchStats.from_struct(stats))



with Reader('fywiki-20200401-pages-articles.graph') as r:
    print(r.random_page_id())

    print(r.vertex_count())  #  92,651
    print(r.edge_count())    # 907,147
    print(r.outgoing(17190))  # [124, 427, 448, 571, 706, 723, 748, 928, 982, 1310, 1353, 1375, 1377, 1560, 2811, 3059, 3639, 5614, 8818, 9062, 13161, 21563, 21741, 24537, 26474, 26774, 47136, 51235, 67888]
    print(r.incoming(17190))  # [4749, 9062, 14028, 22772, 25006, 27760, 29248, 31221, 32950, 87058]

    print(r.page_title(37097))  # "Georg Pauli"
    print(r.page_title(36086))  # "West-Falklân"
    print(r.page_id("West-Falklân"))  # 36086

    print(r.link_text(135, 364))      # "20"
    print(r.link_text(80603, 982))    # None

    print(r.shortest_path(r.page_id('Georg Pauli'), r.page_id('West-Falklân')))  # [37097, 346, 60106, 13403, 36086]

    print(r.shortest_path(r.page_id('Jitse Sikkema'), r.page_id('Antonie Hubertus Bodaan')))   # None

    try:
        r.link_text(123, 456)
        assert False
    except LinkNotFound as e:
        assert str(e) == 'link not found: from #123 to #456'

    try:
        r.page_id("bestaat niet")
        assert False
    except PageNotFound as e:
        assert str(e) == 'page not found: "bestaat niet"'

    try:
        r.page_title(123456789)
        assert False
    except PageNotFound as e:
        assert str(e) == 'page not found: #123456789'

    try:
        r.incoming(123456789)
        assert False
    except PageNotFound as e:
        assert str(e) == 'page not found: #123456789'

    try:
        r.outgoing(123456789)
        assert False
    except PageNotFound as e:
        assert str(e) == 'page not found: #123456789'

