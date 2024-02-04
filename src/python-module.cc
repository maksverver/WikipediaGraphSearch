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
//  - GraphReader.forward_edges() and GraphReader.backward_edges() return lists
//    of page ids, instead of iterable EdgeLists, because typically it's more
//    efficient to retrieve the entire edge list at once, instead of iterating
//    over it page-by-page on the Python side.
//
//  - shortest_path() is defined as a method on GraphReader, instead of a
//    standalone method, since it logically belongs there.
//
//  - shortest_path_with_stats() is a separate method that returns a pair of
//    (path, stats) since Python does not support output arguments.
//
// I intentionally omitted docstrings, because I don't want to duplicate
// documentation with the existing comments on the C++ classes. To understand
// the Python API, users should read the comments in the C++ header files.

#include "wikipath/graph-reader.h"
#include "wikipath/metadata-reader.h"
#include "wikipath/reader.h"
#include "wikipath/searcher.h"

#include <pybind11/operators.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <iomanip>
#include <iostream>
#include <optional>
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

struct QuotedString { const std::string &s; };

// Prints a string surrounding by quotes, with backslash, double quote and
// ASCII control characters escaped. This can be used to implement repr() so
// that the resulting representation can be evaluated as a string in Python.
std::ostream &operator<<(std::ostream &os, const QuotedString &s) {
  os << '"';
  for (char ch : s.s) {
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

}  // namespace

PYBIND11_MODULE(wikipath, module) {
  py::class_<GraphReader>(module, "GraphReader")
      .def(py::init(&GraphReader::Open), py::arg("filename"))
      .def_property_readonly("vertex_count", &GraphReader::VertexCount)
      .def_property_readonly("edge_count", &GraphReader::EdgeCount)
      .def("forward_edges",
          [](GraphReader &gr, index_t page_id) {
            ValidatePageIndex(gr.VertexCount(), page_id);
            EdgeList el = gr.ForwardEdges(page_id);
            return std::vector<index_t>(el.begin(), el.end());
          },
          py::arg("page_id"))
      .def("backward_edges",
          [](GraphReader &gr, index_t page_id) {
            ValidatePageIndex(gr.VertexCount(), page_id);
            EdgeList el = gr.BackwardEdges(page_id);
            return std::vector<index_t>(el.begin(), el.end());
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
            return std::make_pair(
                FindShortestPath(reader, start, finish, &stats),
                std::move(stats));
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
      .def("__repr__", [](const SearchStats &stats) {
        std::ostringstream oss;
        oss << "wikipath.SearchStats(vertices_reached=" << stats.vertices_reached
            << ", vertices_expanded=" << stats.vertices_expanded
            << ", edges_expanded=" << stats.edges_expanded
            << ", time_taken_ms=" << stats.time_taken_ms << ")";
        return oss.str();
      });
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
      .def("__repr__", [](const MetadataReader::Page &page) {
        std::ostringstream oss;
        oss << "wikipath.MetadataReader.Page(id=" << page.id
            << ", title=" << QuotedString{page.title} << ")";
        return oss.str();
      });
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
      .def("__repr__", [](const MetadataReader::Link &link) {
        std::ostringstream oss;
        oss << "wikipath.MetadataReader.Link(from_page_id=" << link.from_page_id
            << ", to_page_id=" << link.to_page_id
            << ", title=";
        if (!link.title) oss << "None"; else oss << QuotedString(*link.title);
        oss << ")";
        return oss.str();
      });
  ;

  py::class_<Reader>(module, "Reader")
      .def(py::init(&Reader::Open), py::arg("graph_filename"))
      .def_property_readonly("graph", &Reader::Graph)
      .def_property_readonly("metadata", &Reader::Metadata)
      .def("is_valid_page_id", &Reader::IsValidPageId, py::arg("page_id"))
      .def("random_page_id", &Reader::RandomPageId)
      .def("parse_parge_argument", &Reader::ParsePageArgument, py::arg("arg"))
      .def("page_title", &Reader::PageTitle, py::arg("page_id"))
      .def("page_ref", &Reader::PageRef, py::arg("page_id"))
      .def("link_text", &Reader::LinkText, py::arg("from_page_id"), py::arg("to_page_id"))
      .def("forward_link_ref", &Reader::ForwardLinkRef, py::arg("from_page_id"), py::arg("to_page_id"))
      .def("backward_link_ref", &Reader::BackwardLinkRef, py::arg("from_page_id"), py::arg("to_page_id"))
  ;
}
