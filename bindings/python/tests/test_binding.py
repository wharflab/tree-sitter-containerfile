from unittest import TestCase

from tree_sitter import Language, Parser
import tree_sitter_containerfile


class TestLanguage(TestCase):
    def test_can_load_grammar(self):
        try:
            Parser(Language(tree_sitter_containerfile.language()))
        except Exception:
            self.fail("Error loading Containerfile grammar")

    def test_can_load_queries(self):
        self.assertTrue(tree_sitter_containerfile.HIGHLIGHTS_QUERY)
        self.assertTrue(tree_sitter_containerfile.INJECTIONS_QUERY)
