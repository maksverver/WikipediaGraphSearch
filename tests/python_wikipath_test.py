#!/usr/bin/env python3

import unittest
import wikipath

class TestGraphReader(unittest.TestCase):

    def setUp(self):
        self.reader = wikipath.GraphReader('testdata/example.graph')

    def tearDown(self):
        del self.reader

    def test__vertex_count(self):
        # Quirkily enough, vertex_count includes the invalid 0 vertex.
        self.assertEqual(self.reader.vertex_count, 7)

    def test__edge_count(self):
        self.assertEqual(self.reader.edge_count, 10)

    def test__forward_edges(self):
        self.assertEqual(self.reader.forward_edges(1), [2, 3])

    def test__backward_edges(self):
        self.assertEqual(self.reader.backward_edges(1), [2, 3, 4])

    def test__shortest_path(self):
        self.assertEqual(self.reader.shortest_path(5, 2), [5, 6, 3, 2])

    def test__shortest_path__single_vertex(self):
        self.assertEqual(self.reader.shortest_path(4, 4), [4])

    def test__shortest_path__direct_connection(self):
        self.assertEqual(self.reader.shortest_path(4, 1), [4, 1])

    def test__shortest_path__not_found(self):
        self.assertEqual(self.reader.shortest_path(1, 4), [])

    def test__shortest_path_with_stats(self):
        path, stats = self.reader.shortest_path_with_stats(4, 2)
        self.assertEqual(path, [4, 1, 2])
        self.assertEqual(stats.vertices_reached, 4)
        self.assertEqual(stats.vertices_expanded, 2)
        self.assertEqual(stats.edges_expanded, 3)
        self.assertTrue(stats.time_taken_ms >= 0)

    def test__shortest_path_with_stats__not_found(self):
        path, stats = self.reader.shortest_path_with_stats(1, 4)
        self.assertEqual(path, [])
        self.assertEqual(stats.vertices_reached, 4)
        self.assertEqual(stats.vertices_expanded, 2)
        self.assertEqual(stats.edges_expanded, 2)
        self.assertTrue(stats.time_taken_ms >= 0)


class TestMetadataReader(unittest.TestCase):

    def setUp(self):
        self.reader = wikipath.MetadataReader('testdata/example.metadata')

    def tearDown(self):
        del self.reader

    def test__get_page_by_id(self):
        self.assertEqual(
            self.reader.get_page_by_id(4),
            wikipath.MetadataReader.Page(id=4, title='Rose'))

    def test__get_page_by_id__not_found(self):
        self.assertEqual(self.reader.get_page_by_id(999999999), None)

    def test__get_page_by_title(self):
        self.assertEqual(self.reader.get_page_by_title('Rose'),
        wikipath.MetadataReader.Page(id=4, title='Rose'))

    def test__get_page_by_title__not_found(self):
        self.assertEqual(self.reader.get_page_by_title('xyzzy'), None)

    def test__get_link(self):
        self.assertEqual(
            self.reader.get_link(4, 1),
            wikipath.MetadataReader.Link(from_page_id=4, to_page_id=1, title=None))

    def test__get_link__with_title(self):
        self.assertEqual(
            self.reader.get_link(4, 5),
            wikipath.MetadataReader.Link(from_page_id=4, to_page_id=5, title='violets'))

    def test__get_link__with_pipe_trick(self):
        self.assertEqual(
            self.reader.get_link(5, 6),
            wikipath.MetadataReader.Link(from_page_id=5, to_page_id=6, title=''))

    def test__get_link__link_not_found(self):
        self.assertEqual(self.reader.get_link(4, 6), None)

    def test__get_link__page_not_found(self):
        self.assertEqual(self.reader.get_link(1, 999999999), None)


class TestReader(unittest.TestCase):

    def setUp(self):
        self.reader = wikipath.Reader('testdata/example.graph')

    def tearDown(self):
        del self.reader

    def test__is_valid_page_id(self):
        self.assertEqual(self.reader.is_valid_page_id(0), False)
        self.assertEqual(self.reader.is_valid_page_id(1), True)
        self.assertEqual(self.reader.is_valid_page_id(6), True)
        self.assertEqual(self.reader.is_valid_page_id(7), False)

    def test__graph(self):
        self.assertEqual(self.reader.graph.vertex_count, 7)
        self.assertEqual(self.reader.graph.edge_count, 10)

    def test__metadata(self):
        self.assertEqual(self.reader.metadata.get_page_by_id(4).title, 'Rose')

    def test__random_page_id(self):
        ids = [self.reader.random_page_id() for _ in range(10)]
        self.assertTrue(len(set(ids)) > 0)
        self.assertTrue(min(ids) > 0)
        self.assertTrue(max(ids) < 7)

    def test__parse_page_argument__title(self):
        self.assertEqual(self.reader.parse_page_argument('Rose'), 4)

    def test__parse_page_argument__title_not_found(self):
        self.assertEqual(self.reader.parse_page_argument('xyzzy'), 0)

    def test__parse_page_argument__id(self):
        self.assertEqual(self.reader.parse_page_argument('#4'), 4)

    def test__parse_page_argument__id_out_of_range(self):
        self.assertEqual(self.reader.parse_page_argument('#0'), 0)
        self.assertEqual(self.reader.parse_page_argument('#999999999'), 0)

    def test__parse_page_argument__random(self):
        ids = [self.reader.parse_page_argument('?') for _ in range(10)]
        self.assertTrue(len(set(ids)) > 0)
        self.assertTrue(min(ids) > 0)
        self.assertTrue(max(ids) < 7)

    def test__page_title(self):
        self.assertEqual(self.reader.page_title(4), 'Rose')

    def test__page_title__not_found(self):
        self.assertEqual(self.reader.page_title(0), 'untitled')
        self.assertEqual(self.reader.page_title(999999999), 'untitled')

    def test__page_ref(self):
        self.assertEqual(self.reader.page_ref(4), '#4 (Rose)')

    def test__page_ref_not_found(self):
        self.assertEqual(self.reader.page_ref(0), '#0 (untitled)')
        self.assertEqual(self.reader.page_ref(999999999), '#999999999 (untitled)')

    def test__link_text(self):
        self.assertEqual(self.reader.link_text(4, 1), 'Red')

    def test__link_text__with_title(self):
        self.assertEqual(self.reader.link_text(4, 5), 'violets')

    def test__link_text__with_pipe_trick(self):
        self.assertEqual(self.reader.link_text(5, 6), 'Violet')

    def test__link_text__not_found(self):
        self.assertEqual(self.reader.link_text(1, 4), 'unknown')

    def test__forward_link_ref(self):
        self.assertEqual(self.reader.forward_link_ref(4, 1), '#1 (Red)')

    def test__forward_link_ref__with_title(self):
        self.assertEqual(
                self.reader.forward_link_ref(4, 5),
                '#5 (Violet (flower); displayed as: violets)')

    def test__forward_link_ref__with_pipe_trick(self):
        self.assertEqual(
                self.reader.forward_link_ref(5, 6),
                '#6 (Violet (color); displayed as: Violet)')

    def test__forward_link_ref__not_found(self):
        self.assertEqual(
                self.reader.forward_link_ref(1, 4),
                '#4 (Rose; displayed as: unknown)')

    def test__backward_link_ref(self):
        self.assertEqual(self.reader.backward_link_ref(4, 1), '#4 (Rose)')

    def test__backward_link_ref__with_title(self):
        self.assertEqual(
                self.reader.backward_link_ref(4, 5),
                '#4 (Rose; displayed as: violets)')

    def test__backward_link_ref__with_pipe_trick(self):
        self.assertEqual(
                self.reader.backward_link_ref(5, 6),
                '#5 (Violet (flower); displayed as: Violet)')

    def test__backward_link_ref__not_found(self):
        self.assertEqual(
                self.reader.backward_link_ref(1, 4),
                '#1 (Red; displayed as: unknown)')


if __name__ == '__main__':
    unittest.main()
