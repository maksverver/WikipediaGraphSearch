#!/usr/bin/env python3

import http_server
import json
import os
import socket
import unittest
from urllib.request import Request, quote, urlopen
from urllib.error import HTTPError

WIKI_BASE_URL = 'http://example.com/wiki/'
GRAPH_DIRECTORY = 'testdata'
GRAPH_FILENAME = 'example.graph'
GRAPH_FILEPATH = os.path.join(GRAPH_DIRECTORY, GRAPH_FILENAME)
HOST = 'localhost'
DOCROOT = 'htdocs/'

VERTEX_COUNT = 7  # (6 + 1 for the reserved 0 vertex)
EDGE_COUNT = 10

PAGE_RED_ID = 1
PAGE_RED_TITLE = 'Red'

PAGE_BLUE_ID = 3
PAGE_BLUE_TITLE = 'Blue'

PAGE_ROSE_ID = 4
PAGE_ROSE_TITLE = 'Rose'

MakePage = lambda id, title: {'page': {'id': id, 'title': title}}

PAGE_RED = MakePage(PAGE_RED_ID, PAGE_RED_TITLE)
PAGE_BLUE = MakePage(PAGE_BLUE_ID, PAGE_BLUE_TITLE)
PAGE_ROSE = MakePage(PAGE_ROSE_ID, PAGE_ROSE_TITLE)

ERROR_PAGE_NOT_FOUND = {'error': {'message': 'Page not found.'}}


def FindFreePort():
    with socket.socket() as s:
        s.bind((HOST, 0))
        _, port = s.getsockname()
        return port


class TestHttpServer(unittest.TestCase):
    @classmethod
    def setUpClass(self):
        # There is a race condition between calling GetFreePort() and the server
        # binding the socket, but it works well enough in practice.
        port = FindFreePort()
        self.url_prefix = f'http://{HOST}:{port}'
        self.server_thread = http_server.Serve(
            wiki_base_url=WIKI_BASE_URL,
            graph_filename=GRAPH_FILEPATH,
            host=HOST, port=port,
            docroot='htdocs/',
            thread_daemon=True)
        # Verify that the server is running and serving requests.
        with urlopen(Request(self.url_prefix, method='HEAD')) as response:
            assert response.status == 200

    def responseToJson(self, response):
        self.assertEqual(response.status, 200)
        self.assertEqual(response.headers['Content-Type'], 'application/json')
        return json.load(response)

    def assertNotFound(self, url):
        try:
            with urlopen(url) as response:
                assert False
        except HTTPError as e:
            self.assertEqual(e.status, 404)

    def test__app_js(self):
        with urlopen(Request(self.url_prefix + '/app.js')) as response:
            self.assertEqual(response.status, 200)
            self.assertEqual(response.headers['Content-Type'], 'text/javascript')

    def test__api_corpus(self):
        with urlopen(Request(self.url_prefix + '/api/corpus')) as response:
            json = self.responseToJson(response)
            self.assertEqual(json, {
                'corpus': {
                    'base_url': WIKI_BASE_URL,
                    'graph_filename': GRAPH_FILENAME,
                    'vertex_count': VERTEX_COUNT,
                    'edge_count': EDGE_COUNT,
                }})

    def test__api_page__by_title(self):
        with urlopen(f'{self.url_prefix}/api/page?page={quote(PAGE_ROSE_TITLE)}') as response:
            json = self.responseToJson(response)
            self.assertEqual(json, PAGE_ROSE)

    def test__api_page__by_id(self):
        with urlopen(f'{self.url_prefix}/api/page?page={quote(f"#{PAGE_ROSE_ID}")}') as response:
            json = self.responseToJson(response)
            self.assertEqual(json, PAGE_ROSE)

    def test__api_page__random(self):
        with urlopen(f'{self.url_prefix}/api/page?page={quote("?")}') as response:
            json = self.responseToJson(response)
            self.assertTrue('page' in json)

    def test__api_page__page_not_found(self):
        self.assertNotFound(f'{self.url_prefix}/api/page?page=nonexistent')

    def test__api_shortest_path__path_found(self):
        url = f'{self.url_prefix}/api/shortest-path?start={quote(PAGE_ROSE_TITLE)}&finish={quote(PAGE_BLUE_TITLE)}'
        with urlopen(url) as response:
            json = self.responseToJson(response)
            self.assertEqual(json['start'], PAGE_ROSE)
            self.assertEqual(json['finish'], PAGE_BLUE)
            self.assertEqual(json['path'], [PAGE_ROSE, PAGE_RED, PAGE_BLUE])
            self.assertEqual(json['stats']['vertices_reached'], 4)
            self.assertEqual(json['stats']['vertices_expanded'], 2)
            self.assertEqual(json['stats']['edges_expanded'], 3)
            self.assertTrue(json['stats']['time_taken_ms'] >= 0)

    def test__api_shortest_path__path_not_found(self):
        url = f'{self.url_prefix}/api/shortest-path?start={quote(PAGE_BLUE_TITLE)}&finish={quote(PAGE_ROSE_TITLE)}'
        with urlopen(url) as response:
            json = self.responseToJson(response)
            self.assertEqual(json['start'], PAGE_BLUE)
            self.assertEqual(json['finish'], PAGE_ROSE)
            self.assertEqual(json['path'], [])
            self.assertNotIn('error', json)

    def test__api_shortest_path__start_not_found(self):
        url = f'{self.url_prefix}/api/shortest-path?start=nonexistent&finish={quote(PAGE_ROSE_TITLE)}'
        try:
            with urlopen(url) as response:
                assert False
        except HTTPError as e:
            self.assertEqual(e.status, 404)
            responseJson = json.load(e)
            self.assertEqual(responseJson['start'], ERROR_PAGE_NOT_FOUND)
            self.assertEqual(responseJson['finish'], PAGE_ROSE)
            self.assertNotIn('path', responseJson)
            self.assertEqual(responseJson['error']['message'], 'Page not found.')

    def test__api_shortest_path__finish_not_found(self):
        url = f'{self.url_prefix}/api/shortest-path?start={quote(PAGE_ROSE_TITLE)}&finish=nonexistent'
        try:
            with urlopen(url) as response:
                assert False
        except HTTPError as e:
            self.assertEqual(e.status, 404)
            responseJson = json.load(e)
            self.assertEqual(responseJson['start'], PAGE_ROSE)
            self.assertEqual(responseJson['finish'], ERROR_PAGE_NOT_FOUND)
            self.assertNotIn('path', responseJson)
            self.assertEqual(responseJson['error']['message'], 'Page not found.')

    def test__api_shortest_path__start_and_finish_not_found(self):
        url = f'{self.url_prefix}/api/shortest-path?start=argh&finish=blargh'
        try:
            with urlopen(url) as response:
                assert False
        except HTTPError as e:
            self.assertEqual(e.status, 404)
            responseJson = json.load(e)
            self.assertEqual(responseJson['start'], ERROR_PAGE_NOT_FOUND)
            self.assertEqual(responseJson['finish'], ERROR_PAGE_NOT_FOUND)
            self.assertNotIn('path', responseJson)
            self.assertEqual(responseJson['error']['message'], 'Page not found.')


if __name__ == '__main__':
    unittest.main()
