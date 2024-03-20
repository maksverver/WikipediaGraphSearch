#!/usr/bin/env python3

import unittest
import wikipath

class Test_GraphReader(unittest.TestCase):

    def setUp(self):
        self.reader = wikipath.GraphReader('testdata/example-1.graph')

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


class Test_MetadataReader(unittest.TestCase):

    def setUp(self):
        self.reader = wikipath.MetadataReader('testdata/example-1.metadata')

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


class Test_Reader(unittest.TestCase):

    def setUp(self):
        self.reader = wikipath.Reader('testdata/example-1.graph')

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


class Test_Reader_OpenOptions(unittest.TestCase):

    def test__lock_into_memory(self):
        reader = wikipath.Reader('testdata/example-1.graph',
                wikipath.GraphReader.OpenOptions(lock_into_memory=True))
        dag = reader.shortest_path_annotated_dag('Rose', 'Red')
        self.assertEqual(len(dag), 1)


class Test_GraphReader_shortest_path_dag(unittest.TestCase):

    def setUp(self):
        reader = wikipath.Reader('testdata/example-2.graph')
        self.start = reader.parse_page_argument('A2')
        self.finish = reader.parse_page_argument('F2')
        self.graph = reader.graph

    def test__shortest_path_dag(self):
        self.assertEqual(self.graph.shortest_path_dag(self.start, self.finish), [
                (2, 4), (2, 5), (4, 6), (5, 7), (5, 8), (6, 9), (7, 10), (8, 10),
                (9, 11), (9, 12), (9, 13), (10, 12), (10, 13), (11, 15), (12, 15), (13, 15)])

    def test__shortest_path_dag__no_edges(self):
        self.assertEqual(self.graph.shortest_path_dag(self.start, self.start), [])
        self.assertEqual(self.graph.shortest_path_dag(self.finish, self.finish), [])

    def test__shortest_path_dag__not_found(self):
        self.assertEqual(self.graph.shortest_path_dag(self.finish, self.start), None)

    def test__shortest_path_dag_with_stats(self):
        edges, stats = self.graph.shortest_path_dag_with_stats(self.start, self.finish)
        self.assertEqual(edges, [
                (2, 4), (2, 5), (4, 6), (5, 7), (5, 8), (6, 9), (7, 10), (8, 10),
                (9, 11), (9, 12), (9, 13), (10, 12), (10, 13), (11, 15), (12, 15), (13, 15)])
        self.assertEqual(stats.vertices_reached,  15)
        self.assertEqual(stats.vertices_expanded, 11)
        self.assertEqual(stats.edges_expanded,    26)
        self.assertTrue(stats.time_taken_ms >= 0)

    def test__shortest_path_dag_with_stats__not_found(self):
        edges, stats = self.graph.shortest_path_dag_with_stats(self.finish, self.start)
        self.assertEqual(edges, None)
        self.assertEqual(stats.vertices_reached,  3)
        self.assertEqual(stats.vertices_expanded, 2)
        self.assertEqual(stats.edges_expanded,    2)
        self.assertTrue(stats.time_taken_ms >= 0)



class Test_Reader_shortest_path_annotated_dag(unittest.TestCase):

    def setUp(self):
        self.reader = wikipath.Reader('testdata/example-2.graph')
        self.start = self.reader.parse_page_argument('A2')
        self.finish = self.reader.parse_page_argument('F2')

    def test__shortest_path_annotated_dag(self):
        self.assertEqual(len(self.reader.shortest_path_annotated_dag(self.start, self.finish)), 7)

    def test__shortest_path_annotated_dag__no_edges(self):
        self.assertEqual(len(self.reader.shortest_path_annotated_dag(self.start, self.start)), 1)

    def test__shortest_path_annotated_dag__not_found(self):
        self.assertEqual(self.reader.shortest_path_annotated_dag(self.finish, self.start), None)

    def test__shortest_path_annotated_dag__invalid_indices(self):
        self.assertEqual(self.reader.shortest_path_annotated_dag(self.start,   0), None)
        self.assertEqual(self.reader.shortest_path_annotated_dag(self.start, 999), None)
        self.assertEqual(self.reader.shortest_path_annotated_dag(0,   self.finish), None)
        self.assertEqual(self.reader.shortest_path_annotated_dag(999, self.finish), None)

    def test__shortest_path_annotated_dag__by_name(self):
        self.assertEqual(len(self.reader.shortest_path_annotated_dag('A2', 'F2')), 7)
        self.assertEqual(self.reader.shortest_path_annotated_dag('A2', 'Nonexistent'), None)
        self.assertEqual(self.reader.shortest_path_annotated_dag('Nonexistent', 'F2'), None)

    def test__shortest_path_annotated_dag_with_stats(self):
        dag, stats = self.reader.shortest_path_annotated_dag_with_stats(self.start, self.finish)
        self.assertEqual(len(dag), 7)
        self.assertEqual(stats.vertices_reached,  15)
        self.assertEqual(stats.vertices_expanded, 11)
        self.assertEqual(stats.edges_expanded,    26)
        self.assertTrue(stats.time_taken_ms >= 0)

    def test__shortest_path_annotated_dag_with_stats__by_name(self):
        dag, stats = self.reader.shortest_path_annotated_dag_with_stats('A2', 'F2')
        self.assertEqual(len(dag), 7)
        self.assertEqual(type(stats), wikipath.SearchStats)

        dag, stats = self.reader.shortest_path_annotated_dag_with_stats('A2', 'Nonexistent')
        self.assertEqual(dag, None)
        self.assertEqual(type(stats), wikipath.SearchStats)

    def test__shortest_path_annotated_dag_with_stats__not_found(self):
        dag, stats = self.reader.shortest_path_annotated_dag_with_stats(self.finish, self.start)
        self.assertEqual(dag, None)
        self.assertEqual(stats.vertices_reached,  3)
        self.assertEqual(stats.vertices_expanded, 2)
        self.assertEqual(stats.edges_expanded,    2)
        self.assertTrue(stats.time_taken_ms >= 0)


class Test_AnnotatedPage(unittest.TestCase):

    def setUp(self):
        self.reader = wikipath.Reader('testdata/example-3.graph')
        self.dag = self.reader.shortest_path_annotated_dag(
            self.reader.parse_page_argument('Start'),
            self.reader.parse_page_argument('Finish'))

    def test__AnnotatedPage__id(self):
        self.assertEqual(self.dag.start.id, 1)
        self.assertEqual(self.dag.finish.id, 7)

    def test__AnnotatedPage__title(self):
        self.assertEqual(self.dag.start.title, 'Start')
        self.assertEqual(self.dag.finish.title, 'Finish')

    def test__AnnotatedPage__ref(self):
        self.assertEqual(self.dag.start.ref, '#1 (Start)')
        self.assertEqual(self.dag.finish.ref, '#7 (Finish)')

    def test__AnnotatedPage__str(self):
        self.assertEqual(str(self.dag.start), '#1 (Start)')
        self.assertEqual(str(self.dag.finish), '#7 (Finish)')

    def test__AnnotatedPage__repr(self):
        self.assertEqual(repr(self.dag.start), 'wikipath.AnnotatedPage(id=1, title="Start")')
        self.assertEqual(repr(self.dag.finish), 'wikipath.AnnotatedPage(id=7, title="Finish")')

    def test__AnnotatedPage__equality(self):
        self.assertEqual(self.dag.start, self.dag.start)
        self.assertEqual(self.dag.finish, self.dag.finish)
        self.assertNotEqual(self.dag.start, self.dag.finish)

    def test__AnnotatedPage__links(self):
        self.assertEqual(
            [(link.src.title, link.dst.title) for link in self.dag.start.links()],
            [('Start', 'C'), ('Start', 'A'), ('Start', 'B')])
        self.assertEqual(self.dag.finish.links(), [])


class Test_AnnotatedLink(unittest.TestCase):

    def setUp(self):
        self.reader = wikipath.Reader('testdata/example-3.graph')
        self.dag = self.reader.shortest_path_annotated_dag(
            self.reader.parse_page_argument('Start'),
            self.reader.parse_page_argument('Finish'))

    def test__AnnotatedLink(self):
        start_c, start_a, start_b = self.dag.start.links()
        c_g, c_h = start_c.dst.links()

        self.assertEqual(start_a.src, self.dag.start)
        self.assertEqual(start_b.src, self.dag.start)
        self.assertEqual(start_c.src, self.dag.start)
        self.assertEqual(start_c.dst, c_g.src)
        self.assertEqual(start_c.dst, c_h.src)

        self.assertEqual(start_a.dst.title, 'A')
        self.assertEqual(start_b.dst.title, 'B')
        self.assertEqual(start_c.dst.title, 'C')
        self.assertEqual(start_a.text, 'A')
        self.assertEqual(start_b.text, 'B')
        self.assertEqual(start_c.text, 'C')

        self.assertEqual(c_g.dst.title, 'G')
        self.assertEqual(c_h.dst.title, 'H')
        self.assertEqual(c_g.text, 'x')
        self.assertEqual(c_h.text, 'y')

        self.assertEqual(start_c.forward_ref, '#2 (C)')
        self.assertEqual(c_g.forward_ref, '#5 (G; displayed as: x)')
        self.assertEqual(start_c.backward_ref, '#1 (Start)')
        self.assertEqual(c_g.backward_ref, '#2 (C; displayed as: x)')
        self.assertEqual(str(start_c), '#2 (C)')
        self.assertEqual(str(c_g), '#5 (G; displayed as: x)')
        self.assertEqual(repr(start_c), 'wikipath.AnnotatedLink(src=wikipath.AnnotatedPage(id=1, title="Start"), dst=wikipath.AnnotatedPage(id=2, title="C"), text="C")')
        self.assertEqual(repr(c_g), 'wikipath.AnnotatedLink(src=wikipath.AnnotatedPage(id=2, title="C"), dst=wikipath.AnnotatedPage(id=5, title="G"), text="x")')

    def test__AnnotatedLink_order(self):
        start = self.dag.start
        self.assertEqual(
            [link.forward_ref for link in start.links(wikipath.LinkOrder.ID)],
            ['#2 (C)', '#3 (A)', '#4 (B)'])
        self.assertEqual(
            [link.forward_ref for link in start.links(wikipath.LinkOrder.TITLE)],
            ['#3 (A)', '#4 (B)', '#2 (C)'])
        self.assertEqual(
            [link.forward_ref for link in start.links(wikipath.LinkOrder.TEXT)],
            ['#3 (A)', '#4 (B)', '#2 (C)'])

        a = start.links()[1].dst
        self.assertEqual(a.title, 'A')

        self.assertEqual(
            [link.forward_ref for link in a.links(wikipath.LinkOrder.ID)],
            ['#5 (G; displayed as: y)', '#6 (H; displayed as: x)'])
        self.assertEqual(
            [link.forward_ref for link in a.links(wikipath.LinkOrder.TITLE)],
            ['#5 (G; displayed as: y)', '#6 (H; displayed as: x)'])
        self.assertEqual(
            [link.forward_ref for link in a.links(wikipath.LinkOrder.TEXT)],
            ['#6 (H; displayed as: x)', '#5 (G; displayed as: y)'])


# Converts a path (a list of AnnotatedLink objects) to page titles (a list of
# strings).
def path_titles(dag, path):
    last_page = dag.start
    titles = [last_page.title]
    for link in path:
        assert link.src == last_page
        last_page = link.dst
        titles.append(last_page.title)
    assert last_page == dag.finish
    return titles

# Converts a list of paths to a list of page title lists.
def paths_titles(dag, paths):
    return [path_titles(dag, path) for path in paths]


class Test_AnnotatedDag(unittest.TestCase):

    def setUp(self):
        self.reader = wikipath.Reader('testdata/example-2.graph')
        self.dag = self.reader.shortest_path_annotated_dag('A2', 'F2')
        self.expected_titles = [
            ['A2', 'B1', 'C1', 'D1', 'E1', 'F2'],
            ['A2', 'B1', 'C1', 'D1', 'E2', 'F2'],
            ['A2', 'B1', 'C1', 'D1', 'E3', 'F2'],
            ['A2', 'B2', 'C2', 'D2', 'E2', 'F2'],
            ['A2', 'B2', 'C2', 'D2', 'E3', 'F2'],
            ['A2', 'B2', 'C3', 'D2', 'E2', 'F2'],
            ['A2', 'B2', 'C3', 'D2', 'E3', 'F2'],
        ]

    def test__count_paths(self):
        self.assertEqual(self.dag.count_paths(), 7)
        self.assertEqual(len(self.dag), 7)

    def test__paths(self):
        dag = self.dag
        expected_titles = self.expected_titles

        self.assertEqual(paths_titles(dag, dag.paths()), expected_titles)
        self.assertEqual(paths_titles(dag, dag.paths(100)), expected_titles)
        self.assertEqual(paths_titles(dag, dag.paths(maxlen=100)), expected_titles)
        self.assertEqual(paths_titles(dag, dag.paths(2, 3)), expected_titles[3:5])
        self.assertEqual(paths_titles(dag, dag.paths(maxlen=2, skip=3)), expected_titles[3:5])

        for i in range(len(dag)):
            for j in range(i, len(dag)):
                self.assertEqual(
                        paths_titles(dag, dag.paths(maxlen=j - i, skip=i)),
                        expected_titles[i:j])

    def test__paths__start_is_finish(self):
        dag = self.reader.shortest_path_annotated_dag('B1', 'B1')
        self.assertEqual(len(dag), 1)
        self.assertEqual(dag.paths(), [[]])
        self.assertEqual(dag.paths(skip=1), [])

    def test__PathEnumerator__enumerate_all(self):
        self.assertEqual(
                paths_titles(self.dag, self.dag.path_enumerator()),
                self.expected_titles)

    def test__PathEnumerator__copy(self):
        e = self.dag.path_enumerator(1)
        self.assertEqual(path_titles(self.dag, e.path), self.expected_titles[1])
        e.advance(2)
        f = wikipath.PathEnumerator(e)
        self.assertEqual(paths_titles(self.dag, e), self.expected_titles[4:])
        self.assertEqual(paths_titles(self.dag, f), self.expected_titles[4:])

    def test__PathEnumerator__skip(self):
        all_paths = list(self.dag.path_enumerator())
        for i in range(len(self.dag)):
            for j in range(i + 1, len(self.dag)):
                e = self.dag.path_enumerator(skip=i)
                self.assertTrue(e)
                self.assertEqual(e.has_path, True)
                self.assertEqual(e.path, all_paths[i])
                e.advance(skip=j - i - 1)
                self.assertTrue(e)
                self.assertEqual(path_titles(self.dag, e.path), path_titles(self.dag, all_paths[j]))
                self.assertEqual(e.path, all_paths[j])
                e.advance(skip=len(self.dag) - j - 1)
                self.assertTrue(not e)
                self.assertEqual(e.has_path, False)
                self.assertEqual(e.path, None)

    def test__PathEnumerator__skip_all(self):
        self.assertEqual(self.dag.path_enumerator(skip=999).has_path, False)

        e = self.dag.path_enumerator()
        e.advance(999)
        self.assertEqual(e.has_path, False)


    def test__PathEnumerator__start_is_finish(self):
        dag = self.reader.shortest_path_annotated_dag(7, 7)
        e = dag.path_enumerator()
        self.assertEqual(e.path, [])
        e.advance()
        self.assertEqual(e.path, None)


class Test_AnnotatedDag_paths_order(unittest.TestCase):

    def setUp(self):
        self.reader = wikipath.Reader('testdata/example-3.graph')
        self.dag = self.reader.shortest_path_annotated_dag('Start', 'Finish')

    def test__paths_order(self):
        dag = self.dag

        for order, expected_titles in [
            (wikipath.LinkOrder.ID, [
                    ['Start', 'C', 'G', 'Finish'],
                    ['Start', 'C', 'H', 'Finish'],
                    ['Start', 'A', 'G', 'Finish'],
                    ['Start', 'A', 'H', 'Finish'],
                    ['Start', 'B', 'G', 'Finish'],
                    ['Start', 'B', 'H', 'Finish'],
                ]),
            (wikipath.LinkOrder.TITLE, [
                    ['Start', 'A', 'G', 'Finish'],
                    ['Start', 'A', 'H', 'Finish'],
                    ['Start', 'B', 'G', 'Finish'],
                    ['Start', 'B', 'H', 'Finish'],
                    ['Start', 'C', 'G', 'Finish'],
                    ['Start', 'C', 'H', 'Finish'],
                ]),
            (wikipath.LinkOrder.TEXT, [
                    ['Start', 'A', 'H', 'Finish'],
                    ['Start', 'A', 'G', 'Finish'],
                    ['Start', 'B', 'G', 'Finish'],
                    ['Start', 'B', 'H', 'Finish'],
                    ['Start', 'C', 'G', 'Finish'],
                    ['Start', 'C', 'H', 'Finish'],
                ]),
        ]:
            self.assertEqual(paths_titles(dag, dag.paths(order=order)), expected_titles)
            self.assertEqual(paths_titles(dag, dag.paths(2, 3, order)), expected_titles[3:5])

            for i in range(len(dag)):
                for j in range(i, len(dag)):
                    self.assertEqual(
                            paths_titles(dag, dag.paths(maxlen=j - i, skip=i, order=order)),
                            expected_titles[i:j])

            self.assertEqual(paths_titles(dag, dag.path_enumerator(order=order)), expected_titles)


if __name__ == '__main__':
    unittest.main()
