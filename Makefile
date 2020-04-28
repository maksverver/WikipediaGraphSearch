COMPILE_FLAGS=$(CXXFLAGS) -std=c++17 -Wall -O3
LINK_FLAGS=$(LDFLAGS) -lpthread $(LDLIBS) -lsqlite3

XMLFLAGS=$(shell pkg-config libxml-2.0 --cflags)
XMLLIBS=$(shell pkg-config libxml-2.0 --libs)

COMMON_HDRS=common.h

READER_SRCS=graph-reader.cc metadata-reader.cc reader.cc pipe-trick.cc
READER_HDRS=graph-header.h graph-reader.h metadata-reader.h reader.h pipe-trick.h

INDEX_SRCS=graph-writer.cc indexer.cc metadata-writer.cc parser.cc
INDEX_HDRS=$(COMMON_HDRS) graph-header.h graph-writer.h metadata-writer.h parser.h

INSPECT_SRCS=$(READER_SRCS) inspect.cc
INSPECT_HDRS=$(READER_HDRS)

SEARCH_SRCS=$(READER_SRCS) searcher.cc search.cc
SEARCH_HDRS=$(READER_HDRS) searcher.h

WEBSEARCH_SRCS=$(READER_SRCS) searcher.cc websearch.cc
WEBSEARCH_HDRS=$(READER_HDRS) searcher.h

BINARIES=index inspect pipe-trick_test search websearch xml-stats

all: $(BINARIES)

xml-stats: xml-stats.cc
	$(CXX) $(COMPILE_FLAGS) $(XMLFLAGS) -o $@ xml-stats.cc $(LINK_FLAGS) $(XMLLIBS)

index: $(INDEX_SRCS) $(INDEX_HDRS)
	$(CXX) $(COMPILE_FLAGS) $(XMLFLAGS) -o $@ $(INDEX_SRCS) $(LINK_FLAGS) $(XMLLIBS)

inspect: $(INSPECT_SRCS) $(INSPECT_HDRS)
	$(CXX) $(COMPILE_FLAGS) -o $@ $(INSPECT_SRCS) $(LINK_FLAGS)

search: $(SEARCH_SRCS) $(SEARCH_HDRS)
	$(CXX) $(COMPILE_FLAGS) -o $@ $(SEARCH_SRCS) $(LINK_FLAGS)

pipe-trick_test: pipe-trick.h pipe-trick.cc pipe-trick_test.cc
	$(CXX) $(COMPILE_FLAGS) -o $@ pipe-trick.cc pipe-trick_test.cc $(LINK_FLAGS)

websearch: $(WEBSEARCH_SRCS) $(WEBSEARCH_HDRS)
	$(CXX) $(COMPILE_FLAGS) -o $@ $(WEBSEARCH_SRCS) $(LINK_FLAGS) -lwthttp -lwt

test: pipe-trick_test
	./pipe-trick_test

clean:
	rm -f $(BINARIES)

.PHONY: all test clean
