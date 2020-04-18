CXXFLAGS=-std=c++17 -Wall -O3 $(shell pkg-config libxml-2.0 --cflags)
LDFLAGS=-pthread
LDLIBS=$(shell pkg-config libxml-2.0 --libs) -lsqlite3

COMMON_HDRS=common.h

READER_SRCS=graph-reader.cc metadata-reader.cc reader.cc
READER_HDRS=graph-reader.h metadata-reader.h reader.h

INDEX_SRCS=graph-writer.cc indexer.cc metadata-writer.cc parser.cc
INDEX_HDRS=$(COMMON_HDRS) graph-writer.h metadata-writer.h parser.h

SEARCH_SRCS=$(READER_SRCS) searcher.cc search.cc
SEARCH_HDRS=$(READER_HDRS) searcher.h

INSPECT_SRCS=$(READER_SRCS) inspect.cc
INSPECT_HDRS=$(READER_HDRS)

BINARIES=index inspect search xml-stats

all: $(BINARIES)

xml-stats: xml-stats.cc
	$(CXX) $(CXXFLAGS) -o $@ xml-stats.cc $(LDFLAGS) $(LDLIBS)

index: $(INDEX_SRCS) $(INDEX_HDRS)
	$(CXX) $(CXXFLAGS) -o $@ $(INDEX_SRCS) $(LDFLAGS) $(LDLIBS)

inspect: $(INSPECT_SRCS) $(INSPECT_HDRS)
	$(CXX) $(CXXFLAGS) -o $@ $(INSPECT_SRCS) $(LDFLAGS) $(LDLIBS)

search: $(SEARCH_SRCS) $(SEARCH_HDRS)
	$(CXX) $(CXXFLAGS) -o $@ $(SEARCH_SRCS) $(LDFLAGS) $(LDLIBS)

clean:
	rm -f $(BINARIES)

.PHONY: all clean
