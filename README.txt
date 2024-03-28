Wikipedia shortest path search tool.


DEPENDENCIES

Tools:

  - CMake
  - A modern C++ compiler
  - optional: Python

Libraries:

  - sqlite3 (https://www.sqlite.org/index.html) (all tools)
  - libxml2 (https://gitlab.gnome.org/GNOME/libxml2) (index, xml-stats)
  - optional: wt (https://www.webtoolkit.eu/wt) (websearch)
  - optional: boost (https://www.boost.org/) (websearch)
  - optional: pybind11 (https://github.com/pybind/pybind11) (Python module)


BUILDING

% cmake -B build
% make -C build

Binaries are written to build/apps/

If the Python module is included in the build, it is written to
build/src/wikipath.cpython-311-x86_64-linux-gnu.so


DEVELOPMENT

For local development, generate the build directory with:

% cmake -B build -D CMAKE_EXPORT_COMPILE_COMMANDS=1 -D CMAKE_BUILD_TYPE=Debug

This generates build/compile_commands.json, which can be used by the clangd
language server to determine how to build source files.


RUNNING: index

The `index` tool converts a Wikipedia database dump in XML format to an
efficient graph datastructure which is needed by the other tools.

Download a Wikipedia database dump from https://dumps.wikimedia.org/ (e.g.,
"enwiki-20240120-pages-articles.xml.bz2") and extract it (e.g., "bunzip2 -k").

Tip: the English wikipedia is large (>20 GB compressed, >90 GB uncompressed).
For testing, download a smaller wiki like the Simple English wiki (simplewiki,
~250 MB compressed) or the Frisian wiki (fywiki, ~65 MB compressed).

Generate the page index with:

% ./index enwiki-20240120-pages-articles.xml

This generates two additional files:

 - enwiki-20240120-pages-articles.graph
 - enwiki-20240120-pages-articles.metadata

The graph file contains the edge data and is the main data structure used to
implement the search. Its structure is described in docs/graph-file-format.txt.

The metadata file contains page and link titles, and is used to map from page
titles to ids and back. It is a sqlite3 database file with a fairly
straightforward schema which is defined in src/metadata-writer.cc.

After generating these two files, the xml file can be deleted.


RUNNING: search

The search tool finds a path betweeen two pages, e.g.:

% ./search enwiki-20200401-pages-articles.graph 'Mongolia' 'Cheeseburger'
Searching shortest path from #12481 (Mongolia) to #111090 (Cheeseburger)...
Vertices reached:  46880
Vertices expanded: 472
Edges expanded:    72884
Search time:       45 ms
#12481 (Mongolia)
#1933 (Berlin)
#9280 (Ich bin ein Berliner)
#30560 (Hamburger)
#111090 (Cheeseburger)


It's also possible to list all possible paths, one per line:

./search enwiki-20240220-pages-articles.graph "Potthastia" "Love Don't Cost a Thing (song)" paths
...
#2619974 (Potthastia) -> #31862 (Fly; displayed as: Diptera) -> #4307 (European Union) -> #11218 (Portugal) -> #290683 (MTV Europe Music Awards) -> #668382 (Love Don't Cost a Thing (song); displayed as: Love Don't Cost a Thing)
#2619974 (Potthastia) -> #31862 (Fly; displayed as: Diptera) -> #13256 (Steven Spielberg) -> #9118 (MTV) -> #477810 (List of Super Bowl halftime shows; displayed as: halftime show) -> #668382 (Love Don't Cost a Thing (song); displayed as: Love Don't Cost a Thing)
#2619974 (Potthastia) -> #31862 (Fly; displayed as: Diptera) -> #13256 (Steven Spielberg) -> #23693 (Fox Broadcasting Company; displayed as: Fox) -> #7731 (Jennifer Lopez) -> #668382 (Love Don't Cost a Thing (song); displayed as: Love Don't Cost a Thing)
...


Paths can be ordered lexicographically by page id (default), page title, or link
text (the text after "displayed as:"):

./search enwiki-20240220-pages-articles.graph "Potthastia" "Love Don't Cost a Thing (song)" paths
...
#2619974 (Potthastia) -> #1566599 (Animal) -> #90958 (Barnacle) -> #11218 (Portugal) -> #290683 (MTV Europe Music Awards) -> #668382 (Love Don't Cost a Thing (song); displayed as: Love Don't Cost a Thing)
#2619974 (Potthastia) -> #1566599 (Animal) -> #94962 (Falconry; displayed as: birds of prey) -> #11218 (Portugal) -> #290683 (MTV Europe Music Awards) -> #668382 (Love Don't Cost a Thing (song); displayed as: Love Don't Cost a Thing)
#2619974 (Potthastia) -> #1566599 (Animal) -> #94962 (Falconry; displayed as: birds of prey) -> #270029 (Saturday Night Live) -> #7731 (Jennifer Lopez) -> #668382 (Love Don't Cost a Thing (song); displayed as: Love Don't Cost a Thing)
#2619974 (Potthastia) -> #1566599 (Animal) -> #987198 (Canada) -> #42163 (Music video) -> #664908 (VH1 Top 20 Video Countdown) -> #668382 (Love Don't Cost a Thing (song); displayed as: Love Don't Cost a Thing)
...

When sorting by page title or link text, strings are compared according to the
current locale (LC_COLLATE).

The `search` tool can also count the total number of paths, more quickly than
enumerating them all:

$ ./search enwiki-20240220-pages-articles.graph "Potthastia" "Love Don't Cost a Thing (song)" count
...
113

There are a few more options. Run `search` without arguments for a list of all
output types and associated options.


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


RUNNING: inspect

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


WEB FRONTENDS


OLD WEB FRONTEND: websearch

The `websearch` binary is a standalone implementation in C++ using the Wt
library. The UI is very simple and I do not plan to update it anymore.

It can be run like this:

% ./websearch enwiki-20240120-pages-articles.graph --docroot /usr/share/Wt \
    --http-address localhost --http-port 8080
[2024-Jan-29 23:18:54.468] 667118 - [info] "config: reading Wt config file: /etc/wt/wt_config.xml (location = './websearch')"
[2024-Jan-29 23:18:54.471] 667118 - [info] "WServer/wthttp: initializing built-in wthttpd"
[2024-Jan-29 23:18:54.498] 667118 - [info] "wthttp: started server: http://127.0.0.1:8080 (localhost)"
[2024-Jan-29 23:18:54.499] 667118 - [info] "wthttp: started server: http://[::1]:8080 (localhost)"
...

Open http://localhost:8080/ in a web browser to use the tool.


NEW WEB FRONTEND: python/http_server.py

The new web frontend is based on an HTTP server implemented in Python, which
uses the wikipath Python module to expose graph search functionality over the
web. The UI looks prettier and the architecture is more robust.

The implementation is separated into two parts:

  - python/http_server.py, which provides a REST API to search the graph.
    See docs/http-server-api.txt for a summary of supported methods.

  - htdocs/, the subdirectory with the static files that are used by the client.
    Most importantly, htdocs/app.js implements the client-side logic in plain
    JavaScript.

The Python server can be run locally as follows:

PYTHONPATH=build/src/ python/http_server.py --host=localhost --port=8080 \
    fywiki-20240201-pages-articles.graph \
    --wiki_base_url=https://fy.wikipedia.org/wiki/

As before, open http://localhost:8080/ in a web browser to use the tool.

In the above command line, build/src/ is the subdirectory that contains the
compiled Python module (e.g. build/src/wikipath.cpython-311-x86_64-linux-gnu.so).

http_server.py also serves static content from htdocs/ (by default). In a
production environment, it's more efficient to have a dedicated webserver like
nginx serve these files statically.


NOTES ON THE --mlock OPTION

By default, GraphReader maps the entire graph data file into memory, which does
not cause the file to be loaded. Instead, pages will be loaded on-demand by the
kernel when they are first accessed. This is ideal for the command-line tools
which do only a single search and then exit, since they will only cause the
small subset of pages that are needed for the search to be loaded.

However, for the HTTP server, which is expected to serve many requests over
its lifetime, it is more efficient to load the entire graph file into memory up
front, in order to reduce the latency of each query. This is especially true
for the Docker image running on e.g. Google Cloud Run where random disk I/O is
slow.

To improve performance, python/http_server.py supports a few options which allow
the graph data to be preloaded on startup:

  --mlock=NONE        Load pages on demand (default).
  --mlock=FOREGROUND  Call mlock() in the foreground.
  --mlock=BACKGROUND  Call mlock() in the background (ignoring failure).
  --mlock=POPULATE    Call mmap() with the MAP_POPULATE flag.

See include/wikipath/graph-reader.h for detailed descriptions of these options.

The FOREGROUND and BACKGROUND options use mlock() to lock the entire graph data
file into memory, which requires the CAP_IPC_LOCK capability and is subject to
the RLIMIT_MEMLOCK resource limit (which can be raised by root, and changed per
user via /etc/security/limits.conf).

THE POPULATE option does not use mlock() and provides relatively weak guarantees
(i.e., it may fail silently, and even if it succeeds, there is no guarantee that
loaded pages remain resident indefinitely), but it doesn't require special
permissions.

On Google Cloud Run, I believe --mlock=BACKGROUND works best, because it keeps
startup latency low, while eventually locking the entire file into memory. See
Dockerfile for details how to enable this feature in a Docker container.


EOF
