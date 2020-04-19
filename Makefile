CXXFLAGS=-std=c++17 -Wall -O3
LDFLAGS=-pthread
LDLIBS=-lsqlite3

XMLFLAGS=$(shell pkg-config libxml-2.0 --cflags)
XMLLIBS=$(shell pkg-config libxml-2.0 --libs)

COMMON_HDRS=common.h

READER_SRCS=graph-reader.cc metadata-reader.cc reader.cc pipe-trick.cc
READER_HDRS=graph-reader.h metadata-reader.h reader.h pipe-trick.h

INDEX_SRCS=graph-writer.cc indexer.cc metadata-writer.cc parser.cc
INDEX_HDRS=$(COMMON_HDRS) graph-writer.h metadata-writer.h parser.h

INSPECT_SRCS=$(READER_SRCS) inspect.cc
INSPECT_HDRS=$(READER_HDRS)

SEARCH_SRCS=$(READER_SRCS) searcher.cc search.cc
SEARCH_HDRS=$(READER_HDRS) searcher.h

WEBSEARCH_SRCS=$(READER_SRCS) searcher.cc websearch.cc
WEBSEARCH_HDRS=$(READER_HDRS) searcher.h

BINARIES=index inspect pipe-trick_test search websearch xml-stats

all: $(BINARIES)

xml-stats: xml-stats.cc
	$(CXX) $(CXXFLAGS) $(XMLFLAGS) -o $@ xml-stats.cc $(LDFLAGS) $(LDLIBS) $(XMLLIBS)

index: $(INDEX_SRCS) $(INDEX_HDRS)
	$(CXX) $(CXXFLAGS) $(XMLFLAGS) -o $@ $(INDEX_SRCS) $(LDFLAGS) $(LDLIBS) $(XMLLIBS)

inspect: $(INSPECT_SRCS) $(INSPECT_HDRS)
	$(CXX) $(CXXFLAGS) -o $@ $(INSPECT_SRCS) $(LDFLAGS) $(LDLIBS)

search: $(SEARCH_SRCS) $(SEARCH_HDRS)
	$(CXX) $(CXXFLAGS) -o $@ $(SEARCH_SRCS) $(LDFLAGS) $(LDLIBS)

pipe-trick_test: pipe-trick.h pipe-trick.cc pipe-trick_test.cc
	$(CXX) $(CXXFLAGS) -o $@ pipe-trick.cc pipe-trick_test.cc $(LDFLAGS) $(LDLIBS)

websearch: $(WEBSEARCH_SRCS) $(WEBSEARCH_HDRS)
	$(CXX) $(CXXFLAGS) -o $@ $(WEBSEARCH_SRCS) $(LDFLAGS) $(LDLIBS) -lwthttp -lwt

test: pipe-trick_test
	./pipe-trick_test

clean:
	rm -f $(BINARIES)

.PHONY: all test clean
