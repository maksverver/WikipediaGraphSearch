// Python bindings for the C++ classes.
//
// The philosophy is to mostly match the C++ class and method definitions, but
// rename method names to match python convention (e.g., FooBar() -> foo_bar()).
// However, there are a few small changes:
//
//  - value types (e.g., Page, Link and SearchStats) are mutable in C++, but
//    immutable in Python. These types are likely used as readonly anyway, and
//    making them immutable has the benefit that they can used as dict keys and
//    in sets.
//
//  - GraphReader.forward_edges() and GraphReader.backward_edges() currently
//    return list copies instead of immutable views.
//
//  - shortest_path() is defined as a method on GraphReader, instead of a
//    standalone method, where it logically belongs.
//
//  - shortest_path_with_stats() is a separate method that returns a pair of
//    (path, stats) because Python does not support output arguments.
//
//  - similarly, shortest_path_annotated_dag() and
//    shortest_path_annotated_dag_with_stats() are
//    defined as methods of Reader.
//
// I intentionally omitted docstrings, because I don't want to duplicate
// documentation with the existing comments on the C++ classes. To understand
// the Python API, users should read the comments in the C++ header files.

#include "wikipath/graph-reader.h"
#include "wikipath/metadata-reader.h"
#include "wikipath/reader.h"
#include "wikipath/searcher.h"
#include "wikipath/annotated-dag.h"

#include <pybind11/operators.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <string>
#include <sstream>
#include <utility>
#include <vector>

using namespace wikipath;
namespace py = pybind11;

namespace {

void ValidatePageIndex(index_t vertex_count, index_t page_id) {
  if (page_id < 1 || page_id >= vertex_count) [[unlikely]] {
    throw std::out_of_range("page index out of range");
  }
}

struct QuotedString { std::optional<std::string_view> sv; };

// Prints a string surrounding by quotes, with backslash, double quote and
// ASCII control characters escaped. This can be used to implement repr() so
// that the resulting representation can be evaluated as a string in Python.
std::ostream &operator<<(std::ostream &os, const QuotedString &qs) {
  if (!qs.sv) return os << "None";
  os << '"';
  for (char ch : *qs.sv) {
    if (ch == '\\' || ch == '"') {
      os << '\\' << ch;
    } else if ((unsigned) ch < 32) {
      // Escape ASCII control characters.
      os << "\\x" << std::hex << std::setfill('0') << std::setw(2) << (unsigned) ch;
    } else {
      os << ch;  // Assumes this is valid UTF-8!
    }
  }
  os << '"';
  return os;
}

std::ostream &operator<<(std::ostream &os, const SearchStats &stats) {
  return os
      << "wikipath.SearchStats(vertices_reached=" << stats.vertices_reached
      << ", vertices_expanded=" << stats.vertices_expanded
      << ", edges_expanded=" << stats.edges_expanded
      << ", time_taken_ms=" << stats.time_taken_ms << ")";
}

std::ostream &operator<<(std::ostream &os, const MetadataReader::Page &page) {
  return os
      << "wikipath.MetadataReader.Page(id=" << page.id
      << ", title=" << QuotedString{page.title} << ")";
}

std::ostream &operator<<(std::ostream &os, const MetadataReader::Link &link) {
  return os
      << "wikipath.MetadataReader.Link(from_page_id=" << link.from_page_id
      << ", to_page_id=" << link.to_page_id
      << ", title=" << QuotedString{link.title}
      << ")";
}

std::ostream &operator<<(std::ostream &os, const AnnotatedPage &page) {
  return os
      << "wikipath.AnnotatedPage(id=" << page.Id()
      << ", title=" << QuotedString(page.Title())
      << ")";
}

std::ostream &operator<<(std::ostream &os, const AnnotatedLink &link) {
  return os
      << "wikipath.AnnotatedLink(src=" << *link.Src()
      << ", dst=" << *link.Dst()
      << ", text=" << QuotedString{link.Text()}
      << ")";
}

template<class T> std::string ToString(const T &value) {
  std::ostringstream oss;
  oss << value;
  return oss.str();
}

template<class T> std::vector<T> ToVector(std::span<const T> span) {
  return std::vector<T>(span.begin(), span.end());
}

// I've created Wrappers for AnnotatedDag, AnnotatedLink, and AnnotatedPage,
// which keep a std::shared_ptr to the Reader and AnnotatedDag objects they
// depend on, to prevent the Reader or DAG from being garbage collected when
// it is still referenced by a dependant.
//
// The alternative would have been to using pybind11's py::keep_alive() call
// policy, which does something similar on the Python side, but it is not strong
// enough to handle cases like returning a pair or a vector that contains an
// object that needs to keep another object alive, so instead I opted to keep
// the logic on the C++ side, even though it introduces a bit of overhead to
// each object derived from the AnnotatedDag.

class AnnotatedDagWrapper;
class AnnotatedLinkWrapper;

class AnnotatedPageWrapper {
public:
  AnnotatedPageWrapper(
      const AnnotatedPage *ptr,
      std::shared_ptr<AnnotatedDagWrapper> dag)
    : ptr(ptr), dag(std::move(dag))  {};

  index_t Id() const { return ptr->Id(); }
  std::string_view Title() const { return ptr->Title(); }
  std::string Ref() const { return ptr->Ref(); }
  std::string String() const { return ToString(*ptr); }

  std::vector<AnnotatedLinkWrapper> Links(LinkOrder order = DEFAULT_LINK_ORDER) const;

  bool operator==(const AnnotatedPageWrapper &other) const noexcept {
    return ptr == other.ptr;
  }

  size_t Hash() const noexcept {
    return reinterpret_cast<size_t>(ptr) * 31337;
  }

private:
  const AnnotatedPage *ptr;
  std::shared_ptr<AnnotatedDagWrapper> dag;  // keep-alive
};

class AnnotatedLinkWrapper {
public:
  AnnotatedLinkWrapper(
      const AnnotatedLink *ptr,
      std::shared_ptr<AnnotatedDagWrapper> dag)
    : ptr(ptr), dag(std::move(dag))  {};

  AnnotatedPageWrapper Src() const { return AnnotatedPageWrapper(ptr->Src(), dag); }
  AnnotatedPageWrapper Dst() const { return AnnotatedPageWrapper(ptr->Dst(), dag); }
  std::string_view Text() const { return ptr->Text(); }
  std::string ForwardRef() const { return ptr->ForwardRef(); }
  std::string BackwardRef() const { return ptr->BackwardRef(); }
  std::string String() const { return ToString(*ptr); }

  bool operator==(const AnnotatedLinkWrapper &other) const noexcept {
    return ptr == other.ptr;
  }

  size_t Hash() const noexcept {
    return reinterpret_cast<size_t>(ptr) * 31337;
  }

private:
  const AnnotatedLink *ptr;
  std::shared_ptr<AnnotatedDagWrapper> dag;  // keep-alive
};

std::vector<AnnotatedLinkWrapper>
AnnotatedPageWrapper::Links(LinkOrder order) const {
  std::span<const AnnotatedLink> links = ptr->Links(order);
  std::vector<AnnotatedLinkWrapper> res;
  res.reserve(links.size());
  for (const AnnotatedLink &link : links) res.push_back(AnnotatedLinkWrapper(&link, dag));
  return res;
}

class AnnotatedDagWrapper {
public:
  AnnotatedDagWrapper(
      const std::shared_ptr<Reader> &reader, index_t start, index_t finish,
      std::vector<std::pair<index_t, index_t>> edges)
    : dag(reader.get(), start, finish, std::move(edges)), reader(reader) {}

  ~AnnotatedDagWrapper() {}

  AnnotatedDag &operator*() { return dag; }
  AnnotatedDag *operator->() { return &dag; }

  static AnnotatedPageWrapper Start(const std::shared_ptr<AnnotatedDagWrapper> &dag) {
    return AnnotatedPageWrapper((*dag)->Start(), dag);
  }

  static AnnotatedPageWrapper Finish(const std::shared_ptr<AnnotatedDagWrapper> &dag) {
    return AnnotatedPageWrapper((*dag)->Finish(), dag);
  }

  static int64_t CountPaths(const std::shared_ptr<AnnotatedDagWrapper> &dag) {
    return (*dag)->CountPaths();
  };

  static std::vector<std::vector<AnnotatedLinkWrapper>> Paths(
      const std::shared_ptr<AnnotatedDagWrapper> &dag,
      size_t maxlen, size_t skip, LinkOrder order) {
    std::vector<std::vector<AnnotatedLinkWrapper>> paths;
    if (maxlen > 0) {
      (*dag)->EnumeratePaths(
        [&](std::span<const AnnotatedLink*> links) {
          std::vector<AnnotatedLinkWrapper> &path = paths.emplace_back();
          path.reserve(links.size());
          for (const AnnotatedLink *link : links) {
            path.push_back(AnnotatedLinkWrapper(link, dag));
          }
          return paths.size() < maxlen;
        }, skip, order);
    }
    return paths;
  }

private:
  AnnotatedDag dag;
  std::shared_ptr<Reader> reader;  // keep-alive
};

std::shared_ptr<AnnotatedDagWrapper>
ShortestPathAnnotatedDag(
    const std::shared_ptr<Reader> &reader, index_t start, index_t finish, SearchStats *stats) {
  if (reader->IsValidPageId(start) && reader->IsValidPageId(finish)) {
    if (auto dag = FindShortestPathDag(reader->Graph(), start, finish, stats)) {
      return std::make_shared<AnnotatedDagWrapper>(reader, start, finish, std::move(*dag));
    }
  }
  return {};
}

}  // namespace

template<> struct std::hash<AnnotatedLinkWrapper> {
  size_t operator()(const AnnotatedLinkWrapper &obj) const noexcept {
    return obj.Hash();
  }
};

template<> struct std::hash<AnnotatedPageWrapper> {
  size_t operator()(const AnnotatedPageWrapper &obj) const noexcept {
    return obj.Hash();
  }
};

PYBIND11_MODULE(wikipath, module) {
  py::class_<GraphReader>(module, "GraphReader")
      .def(py::init(&GraphReader::Open), py::arg("filename"))
      .def_property_readonly("vertex_count", &GraphReader::VertexCount)
      .def_property_readonly("edge_count", &GraphReader::EdgeCount)
      .def("forward_edges",
          [](GraphReader &gr, index_t page_id) {
            ValidatePageIndex(gr.VertexCount(), page_id);
            return ToVector(gr.ForwardEdges(page_id));
          },
          py::arg("page_id"))
      .def("backward_edges",
          [](GraphReader &gr, index_t page_id) {
            ValidatePageIndex(gr.VertexCount(), page_id);
            return ToVector(gr.BackwardEdges(page_id));
          },
          py::arg("page_id"))
      .def("shortest_path",
          [](GraphReader &reader, index_t start, index_t finish) {
            return FindShortestPath(reader, start, finish, nullptr);
          },
          py::arg("start"), py::arg("finish"))
      .def("shortest_path_with_stats",
          [](GraphReader &reader, index_t start, index_t finish) {
            SearchStats stats = {};
            auto path = FindShortestPath(reader, start, finish, &stats);
            return std::make_pair(std::move(path), std::move(stats));
          },
          py::arg("start"), py::arg("finish"))
      .def("shortest_path_dag",
          [](GraphReader &reader, index_t start, index_t finish) {
            return FindShortestPathDag(reader, start, finish, nullptr);
          },
          py::arg("start"), py::arg("finish"))
      .def("shortest_path_dag_with_stats",
          [](GraphReader &reader, index_t start, index_t finish) {
            SearchStats stats = {};
            auto dag = FindShortestPathDag(reader, start, finish, &stats);
            return std::make_pair(std::move(dag), std::move(stats));
          },
          py::arg("start"), py::arg("finish"))
  ;

  py::class_<SearchStats>(module, "SearchStats")
      .def(
          py::init([](
                int64_t vertices_reached,
                int64_t vertices_expanded,
                int64_t edges_expanded,
                int64_t time_taken_ms) {
              return SearchStats{
                .vertices_reached = vertices_reached,
                .vertices_expanded = vertices_expanded,
                .edges_expanded = edges_expanded,
                .time_taken_ms = time_taken_ms,
              };
            }),
          "Constructs a SearchStats object.",
          py::arg("vertices_reached") = 0,
          py::arg("vertices_expanded") = 0,
          py::arg("edges_expanded") = 0,
          py::arg("time_taken_ms") = 0)
      .def_readonly("vertices_reached", &SearchStats::vertices_reached)
      .def_readonly("vertices_expanded", &SearchStats::vertices_expanded)
      .def_readonly("edges_expanded", &SearchStats::edges_expanded)
      .def_readonly("time_taken_ms", &SearchStats::time_taken_ms)
      .def(pybind11::self == pybind11::self)
      .def(hash(py::self))
      .def("__repr__", &ToString<SearchStats>)
  ;

  py::class_<MetadataReader> metadata_reader(module, "MetadataReader");
  metadata_reader
      .def(py::init(&MetadataReader::Open), py::arg("filename"))
      .def("get_page_by_id", &MetadataReader::GetPageById, py::arg("page_id"))
      .def("get_page_by_title", &MetadataReader::GetPageByTitle, py::arg("title"))
      .def("get_link", &MetadataReader::GetLink, py::arg("from_page_id"), py::arg("to_page_id"))
  ;

  py::class_<MetadataReader::Page>(metadata_reader, "Page")
      .def(
          py::init([](index_t id, std::string title) {
            return MetadataReader::Page{
                .id = id,
                .title = std::move(title)
            };
          }),
          "Constructs a Page object.",
          py::arg("id"),
          py::arg("title"))
      .def_readonly("id", &MetadataReader::Page::id)
      .def_readonly("title", &MetadataReader::Page::title)
      .def(pybind11::self == pybind11::self)
      .def(hash(py::self))
      .def("__repr__", &ToString<MetadataReader::Page>)
  ;

  py::class_<MetadataReader::Link>(metadata_reader, "Link")
      .def(
          py::init([](
              index_t from_page_id,
              index_t to_page_id,
              std::optional<std::string> title) {
            return MetadataReader::Link{
                .from_page_id = from_page_id,
                .to_page_id = to_page_id,
                .title = std::move(title)
            };
          }),
          "Constructs a Link object.",
          py::arg("from_page_id"),
          py::arg("to_page_id"),
          py::arg("title") = std::nullopt)
      .def_readonly("from_page_id", &MetadataReader::Link::from_page_id)
      .def_readonly("to_page_id", &MetadataReader::Link::to_page_id)
      .def_readonly("title", &MetadataReader::Link::title)
      .def(pybind11::self == pybind11::self)
      .def(hash(py::self))
      .def("__repr__", &ToString<MetadataReader::Link>)
  ;

  py::enum_<LinkOrder>(module, "LinkOrder")
      .value("ID", LinkOrder::ID)
      .value("TITLE", LinkOrder::TITLE)
      .value("TEXT", LinkOrder::TEXT)
  ;

  py::class_<AnnotatedPageWrapper>(module, "AnnotatedPage")
    .def_property_readonly("id", &AnnotatedPageWrapper::Id)
    .def_property_readonly("title", &AnnotatedPageWrapper::Title)
    .def_property_readonly("ref", &AnnotatedPageWrapper::Ref)
    .def("links", &AnnotatedPageWrapper::Links,
        py::arg("order") = DEFAULT_LINK_ORDER)
    .def("__str__", &AnnotatedPageWrapper::Ref)
    .def("__repr__", &AnnotatedPageWrapper::String)
    .def(pybind11::self == pybind11::self)
    .def(hash(py::self))
  ;

  py::class_<AnnotatedLinkWrapper>(module, "AnnotatedLink")
    .def_property_readonly("src", &AnnotatedLinkWrapper::Src)
    .def_property_readonly("dst", &AnnotatedLinkWrapper::Dst)
    .def_property_readonly("text", &AnnotatedLinkWrapper::Text)
    .def_property_readonly("forward_ref", &AnnotatedLinkWrapper::ForwardRef)
    .def_property_readonly("backward_ref", &AnnotatedLinkWrapper::BackwardRef)
    .def("__str__", &AnnotatedLinkWrapper::ForwardRef)
    .def("__repr__", &AnnotatedLinkWrapper::String)
    .def(pybind11::self == pybind11::self)
    .def(hash(py::self))
  ;

  py::class_<AnnotatedDagWrapper, std::shared_ptr<AnnotatedDagWrapper>>(module, "AnnotatedDag")
    .def_property_readonly("start", &AnnotatedDagWrapper::Start)
    .def_property_readonly("finish", &AnnotatedDagWrapper::Finish)
    .def("count_paths", &AnnotatedDagWrapper::CountPaths,
        "Returns the total number of shortest paths from start to finish.")
    .def("__len__", &AnnotatedDagWrapper::CountPaths,
        "Returns the total number of shortest paths from start to finish.")
    .def("paths", &AnnotatedDagWrapper::Paths,
        "Returns a list of shortest paths from `start` to `finish`.",
        py::arg_v("maxlen", std::numeric_limits<size_t>::max(), "Maximum number of paths to generate"),
        py::arg_v("skip", size_t{0}, "Number of paths to skip. Useful to implement pagination."),
        py::arg("order") = DEFAULT_LINK_ORDER)
  ;

  py::class_<Reader, std::shared_ptr<Reader>>(module, "Reader")
      .def(py::init(&Reader::Open), py::arg("graph_filename"))
      .def_property_readonly("graph", &Reader::Graph)
      .def_property_readonly("metadata", &Reader::Metadata)
      .def("is_valid_page_id", &Reader::IsValidPageId, py::arg("page_id"))
      .def("random_page_id", &Reader::RandomPageId)
      .def("parse_page_argument", &Reader::ParsePageArgument, py::arg("arg"))
      .def("page_title", &Reader::PageTitle, py::arg("page_id"))
      .def("page_ref", &Reader::PageRef, py::arg("page_id"))
      .def("link_text", &Reader::LinkText, py::arg("from_page_id"), py::arg("to_page_id"))
      .def("forward_link_ref", &Reader::ForwardLinkRef, py::arg("from_page_id"), py::arg("to_page_id"))
      .def("backward_link_ref", &Reader::BackwardLinkRef, py::arg("from_page_id"), py::arg("to_page_id"))
      .def("shortest_path_annotated_dag",
          [](const std::shared_ptr<Reader> &reader, index_t start, index_t finish) {
            return ShortestPathAnnotatedDag(reader, start, finish, nullptr);
          },
          "Returns an AnnotatedDag representing all shortest paths from `start` to `finish`,\n"
          "or None if `finish` is not reachable from `start`.",
          py::arg("start"), py::arg("finish"))
      .def("shortest_path_annotated_dag",
          [](const std::shared_ptr<Reader> &reader, const char *start_arg, const char *finish_arg) {
            index_t start = reader->ParsePageArgument(start_arg);
            index_t finish = reader->ParsePageArgument(finish_arg);
            return ShortestPathAnnotatedDag(reader, start, finish, nullptr);
          },
          "Variant of shortest_path_annotated_dag() that takes string arguments,\n"
          "which are interpreted by parse_page_argument().",
          py::arg("start"), py::arg("finish"))
      .def("shortest_path_annotated_dag_with_stats",
          [](const std::shared_ptr<Reader> &reader, index_t start, index_t finish) {
            SearchStats stats = {};
            auto dag = ShortestPathAnnotatedDag(reader, start, finish, &stats);
            return std::make_pair(std::move(dag), std::move(stats));
          },
          "Returns a pair of:\n"
          "  1. the AnnotatedDag representing all shortest paths from `start` to `finish`,\n"
          "     or None if `finish` is not reachable from `start`.\n"
          "  2. a SearchStats object with statistics about the search.\n",
          py::arg("start"), py::arg("finish"))
      .def("shortest_path_annotated_dag_with_stats",
          [](const std::shared_ptr<Reader> &reader, const char *start_arg, const char *finish_arg) {
            index_t start = reader->ParsePageArgument(start_arg);
            index_t finish = reader->ParsePageArgument(finish_arg);
            SearchStats stats = {};
            auto dag = ShortestPathAnnotatedDag(reader, start, finish, &stats);
            return std::make_pair(std::move(dag), std::move(stats));
          },
          "Variant of shortest_path_annotated_dag_with_stats() that takes string arguments,\n"
          "which are interpreted by parse_page_argument().",
          py::arg("start"), py::arg("finish"))
  ;
}
