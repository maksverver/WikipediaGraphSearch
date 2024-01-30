Wikipedia shortest path search tool.

To use the tool, first download a Wikipedia database dump from
https://dumps.wikimedia.org/ (e.g. "enwiki-20240120-pages-articles.xml.bz2") and
extract it (e.g. with "bunzip2 -k").

Tip: the English wikipedia is large (>20 GB compressed). For testing, download a
smaller wiki like the Simple English wiki (simplewiki, ~250 MB) or the Frisian
wiki (fywiki, ~65 MB).

Generate the page index with:

% ./index enwiki-20240120-pages-articles.xml

This generates two additional files:

 - enwiki-20240120-pages-articles.graph
 - enwiki-20240120-pages-articles.metadata

The graph file contains the edge data and is the main data structure used to
implement the search. Its structure is described in graph-file-format.txt.

The metadata file contains page and link titles, and is used to map from page
titles to ids and back. It is a sqlite3 database file, with the schema defined
in metadata-writer.cc.

After generating these two files, the xml file can be deleted.

Now it's possible to find a path between two pages, e.g.:

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

The inspect tool can be used to identify pages and their links:

% ./inspect enwiki-20231101-pages-articles.graph 'Edsger W. Dijkstra'
#4694 (Edsger W. Dijkstra)
Outgoing links:
 -> #134 (Amsterdam)
 -> #761 (Austin, Texas)
 -> #1241 (Association for Computing Machinery; displayed as: ACM)
..
Incoming links:
 <- #221 (August 6)
 <- #428 (ALGOL)
 <- #1702 (BASIC; displayed as: Dijkstra)
..

For both the `search` and `inspect` tools, articles can be referenced by name
(e.g., "Mongolia"), by page id (e.g., "#12481"), or selected at random ("?").

To find a shortest path between two random articles:

% ./search enwiki-20231101-pages-articles.graph '?' '?'
Searching shortest path from #5149995 (Bobby, the Petrol Boy) to #5180352 (Jacky Bovay)...
#5149995 (Bobby, the Petrol Boy)
#1420 (Berlin)
#11200 (Paris)
#15047 (Tour de France)
#1095984 (1955 Tour de France; displayed as: 1955)
#5180352 (Jacky Bovay)

The websearch binary (which requires Wt, the web toolkit) runs the web frontend:

% ./websearch enwiki-20240120-pages-articles.graph --docroot /usr/share/Wt \
    --http-address localhost --http-port 8080
[2024-Jan-29 23:18:54.468] 667118 - [info] "config: reading Wt config file: /etc/wt/wt_config.xml (location = './websearch')"
[2024-Jan-29 23:18:54.471] 667118 - [info] "WServer/wthttp: initializing built-in wthttpd"
[2024-Jan-29 23:18:54.498] 667118 - [info] "wthttp: started server: http://127.0.0.1:8080 (localhost)"
[2024-Jan-29 23:18:54.499] 667118 - [info] "wthttp: started server: http://[::1]:8080 (localhost)"
...

Open http://localhost:8080/ in a web browser to use the tool.
