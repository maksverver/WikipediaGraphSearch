#!/usr/bin/env python3

# Python implementation of a REST API to support Wikipedia graph search.
# See docs/http-server-api.txt for the supported methods.

import argparse
import http.server
import json
import os
import os.path
import sys
import socketserver
from urllib.parse import urlsplit, parse_qs
import wikipath

# Directory from which static content is served.
#
# Careful! Do not put symlinks in this directory, since Python's
# SimpleHTTPRequestHandler does not restrict access to the docroot directory.
docroot = None

# The wikipath.Reader instance which is used to access the graph and metadata.
reader = None

# A static byte array that describes the serving corpus, returned by /api/corpus
get_corpus_response_bytes = None


class ClientError(Exception):
    '''Represents an HTTP client error. The message will be sent back to the
       client, so it should not contain private information.'''
    def __init__(self, message, status=400):
        self.message = message
        self.status = status


def SendStatusMessage(req, status, message, header_only):
    '''Sends a HTTP response with the given status and message (which must
       be ASCII-only, so don't include client-supplied data!)'''
    req.send_response(status, message)
    if message:
        message_data = message.encode('ascii')
        req.send_header('Content-Type', 'text/plain')
        req.send_header('Content-Length', len(message_data))
    req.end_headers()
    if message and not header_only: req.wfile.write(message_data)


def SendJsonResponse(req, result, header_only, status=(200, 'OK')):
    data = json.dumps(result).encode('utf-8')
    req.send_response(*status)
    req.send_header('Content-Type', 'application/json')
    req.send_header('Content-Length', len(data))
    req.end_headers()
    if not header_only: req.wfile.write(data)


def GetQueryArg(qs, key, missing_allowed=False, empty_allowed=False):
    '''Extracts a unique query argument from a parsed query string.'''
    values = qs.get(key, [])
    if len(values) < 1:
        if not missing_allowed:
            raise ClientError(f'Missing {key} argument')
        return None
    if len(values) > 1:
        raise ClientError(f'Multiple {key} arguments')
    value = values[0]
    if not value and not empty_allowed:
        raise ClientError(f'Empty {key} argument')
    return value


def GetPageById(id):
    '''Returns a page by id, which may be an integer or the string
       representation of an integer.'''
    try:
        page_id = int(id)
    except ValueError as e:
        raise ClientError("Invalid page id") from e
    if page_id < 1 or page_id >= reader.graph.vertex_count:
        raise ClientError("Page id out of range")
    return reader.metadata.get_page_by_id(page_id)


def GetRandomPage():
    return reader.metadata.get_page_by_id(reader.random_page_id())


def GetPage(arg):
    '''Returns the page matching the string argument, which can be:
         - "#123", to return the page with id 123
         - "Foo", to return the page with title "Foo"
         - "?", to return a page at random
       If the given page id is invalid, this method raises a ClientException.
       However, if there is no page with the given title, this method returns
       None instead.'''
    if arg.startswith('#'):
        return GetPageById(arg[1:])
    if arg == '?':
        return GetRandomPage()
    return reader.metadata.get_page_by_title(arg)


def ErrorDict(message):
    return {"error": {"message": message}}


def PageDict(page, prev_page=None):
    if not page: return ErrorDict("Page not found.")
    result = {"page": {"id": page.id, "title": page.title}}
    if prev_page is not None:
        text = reader.link_text(prev_page.id, page.id)
        if text is not None and text != page.title:
            result['displayed_as'] = text
    return result


def GET_api_corpus(req, query, header_only):
    req.send_response(200, 'OK')
    req.send_header('Content-Type', 'application/json')
    req.send_header('Content-Length', len(get_corpus_response_bytes))
    req.end_headers()
    if not header_only: req.wfile.write(get_corpus_response_bytes)


def GET_api_page(req, query, header_only):
    page = GetPage(GetQueryArg(parse_qs(query), 'page'))
    SendJsonResponse(req, PageDict(page), header_only=header_only,
            status=(200, 'OK') if page else (404, 'Not found'))


def GET_api_shortest_path(req, query, header_only):
    qs = parse_qs(query)
    start  = GetPage(GetQueryArg(qs, 'start'))
    finish = GetPage(GetQueryArg(qs, 'finish'))
    result = {
        "start": PageDict(start),
        "finish": PageDict(finish),
    }
    if not (start and finish):
        result["error"] = {"message": "Page not found."}
        SendJsonResponse(req, result, header_only=header_only, status=(404, 'Not found'))
        return
    path, stats = reader.graph.shortest_path_with_stats(start.id, finish.id)
    result_path = []
    prev_page = None
    for page_id in path:
        page = GetPageById(page_id)
        result_path.append(PageDict(page, prev_page))
        prev_page = page
    result["path"] = result_path
    result["stats"] = {
        "vertices_reached": stats.vertices_reached,
        "vertices_expanded": stats.vertices_expanded,
        "edges_expanded": stats.edges_expanded,
        "time_taken_ms": stats.time_taken_ms,
    }
    SendJsonResponse(req, result, header_only=header_only)


request_handlers = {
    '/api/corpus': ('GET', GET_api_corpus),
    '/api/page': ('GET', GET_api_page),
    '/api/shortest-path': ('GET', GET_api_shortest_path),
}


def HandleRequest(req, requested_method):
    url = urlsplit(req.path)
    method, handler = request_handlers.get(url.path, (None, None))
    if not handler:
        return False
    header_only = requested_method == 'HEAD'
    try:
        if method == 'GET' and requested_method in ('GET', 'HEAD'):
            handler(req, query=url.query, header_only=header_only)
        # Currently, we only have GET handlers.
        # elif method == requested_method:
        #     handler(req, query=url.query)
        else:
            SendStatusMessage(req, 405, 'Method not allowed', header_only=header_only)
    except ClientError as e:
        SendStatusMessage(req, e.status, e.message, header_only=header_only)
    return True


class RequestHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        assert docroot is not None
        super().__init__(*args, directory=docroot, **kwargs)

    def do_HEAD(self):
        HandleRequest(self, 'HEAD') or super().do_HEAD()

    def do_GET(self):
        HandleRequest(self, 'GET') or super().do_GET()

    # Currently, we only have GET handlers.
    # def do_POST(self):
    #     if not HandleRequest(self, 'POST'):
    #         return SendStatusMessage(self, 404, 'Not found', header_only=False)


class Server(socketserver.TCPServer):
    allow_reuse_address = True


def Main():
    global docroot, reader, get_corpus_response_bytes

    parser = argparse.ArgumentParser(
        description='Wikipedia graph search server',
        add_help=False,
        formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument("--help", action='help')
    parser.add_argument('-d', '--docroot', default="htdocs/", help='Directory with static files to serve')
    parser.add_argument('-h', '--host', default="localhost", help='Host to bind to')
    parser.add_argument('-p', '--port', default="8001", help='Port to bind to')
    parser.add_argument('--wiki_base_url', default='https://en.wikipedia.org/wiki/')
    parser.add_argument('filename.graph')
    args = parser.parse_args()

    graph_filename = vars(args)['filename.graph']
    host = args.host
    port = int(args.port)
    docroot = args.docroot

    reader = wikipath.Reader(graph_filename)

    get_corpus_response_bytes = json.dumps({
        'corpus': {
            'base_url': args.wiki_base_url,
            'graph_filename': os.path.basename(graph_filename),
            'vertex_count': reader.graph.vertex_count,
            'edge_count': reader.graph.edge_count,
        }
    }).encode('utf-8')

    print(f'Starting server at {host}:{port}', file=sys.stderr)
    server = Server((host, port), RequestHandler)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        server.shutdown()


if __name__ == '__main__':
    Main()
