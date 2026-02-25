#!/usr/bin/env python3

# A Python script that implements the same functionality as the C++ inspect app.
#
# This was renamed from inspect.py to graph-inspect.py to avoid clashing with
# the Python standard libary module called "inspect".

import sys
import wikipath


def Main():
	if len(sys.argv) != 3:
		print(f'Usage: {sys.argv[0]} <wiki.graph> <PageTitle|#PageId|?>')
		sys.exit(1)

	_, graph_filename, page_arg = sys.argv

	reader = wikipath.Reader(graph_filename)
	page_id = reader.parse_page_argument(page_arg)
	if not page_id:
		print('Page not found!')
		sys.exit(1)

	print(reader.page_ref(page_id))

	print("Outgoing links:")
	for destination in reader.graph.forward_edges(page_id):
		print(' -> ' + reader.forward_link_ref(page_id, destination))

	print("Incoming links:")
	for source in reader.graph.backward_edges(page_id):
		print(' <- ' + reader.backward_link_ref(source, page_id))


if __name__ == '__main__':
	Main()
