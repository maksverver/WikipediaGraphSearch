Wikipedia shortest path search tool.

Download a dump from https://dumps.wikimedia.org/ and extract it.

Generate the index with (e.g.):

% ./index enwiki-20200401-pages-articles.xml

This generates two additional files:

 - enwiki-20200401-pages-articles.graph
 - enwiki-20200401-pages-articles.metadata

The graph file contains the edge data and is the main data structure used to
implement the search. Its structure is described in graph-file-format.txt.

The metadata file contains page and link titles, and is used to map from page
titles to ids and back. It is a sqlite3 database file, with the schema defined
in metadata-writer.cc.

Now it's possible to find a path between two articles, e.g.:

% ./search enwiki-20200401-pages-articles.graph 'Mongolia' 'Cheeseburger'
Searching shortest path from #12481 (Mongolia) to #111090 (Cheeseburger)...
#12481 (Mongolia)
#1933 (Berlin)
#9280 (Ich bin ein Berliner)
#30560 (Hamburger)
#111090 (Cheeseburger)
Vertices reached:  46880
Vertices expanded: 472
Edges expanded:    72884
Time taken:        45 ms

Articles can be referenced by name (e.g. Mongolia) or page id (e.g. #12481),
or a question mark (?) can be used to select a page at random.
