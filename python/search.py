#!/usr/bin/env python3

# A Python script that implements the same functionality as the C++ search app.

import sys
import wikipath

if len(sys.argv) != 4:
  print(f'Usage: {sys.argv[0]} <wiki.graph> <Start|#id|?> <Finish|#id|?>')
  sys.exit(1)

_, graph_filename, start_arg, finish_arg = sys.argv

reader = wikipath.Reader(graph_filename)

start = reader.parse_page_argument(start_arg)
if not start:
  print('Start not found!')
  sys.exit(1)

finish = reader.parse_page_argument(finish_arg)
if not finish:
  print('Finish not found!')
  sys.exit(1)

print(f'Searching shortest path from {reader.page_ref(start)}  to {reader.page_ref(finish)}...', file=sys.stderr)

path, stats = reader.graph.shortest_path_with_stats(start, finish)
if not path:
  print(f'No path found!', file=sys.stderr)
else:
  print(reader.page_ref(path[0]))
  for i in range(1, len(path)):
    print(reader.forward_link_ref(path[i - 1], path[i]))

print(f'Vertices reached:  {stats.vertices_reached}', file=sys.stderr)
print(f'Vertices expanded: {stats.vertices_expanded}', file=sys.stderr)
print(f'Edges expanded:    {stats.edges_expanded}', file=sys.stderr)
print(f'Time taken:        {stats.time_taken_ms} ms', file=sys.stderr)
