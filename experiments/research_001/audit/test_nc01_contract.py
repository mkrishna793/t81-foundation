"""Audit-only examples of NC-01's predicates; not a JCS implementation or verifier.

The fixtures use ASCII keys and small integers, whose JCS bytes are explicit.
No claim of general RFC 8785 conformance follows from these tests.
"""
import hashlib
import json
import unittest


CANONICAL = b'{"a":2,"b":1}'


class NC01Contract(unittest.TestCase):
    def test_positive_canonical_bytes(self):
        self.assertEqual(json.loads(CANONICAL), {"a": 2, "b": 1})
        self.assertEqual(CANONICAL, b'{"a":2,"b":1}')

    def test_valid_reordered_json_has_same_value_but_different_bytes(self):
        reordered = b'{"b":1,"a":2}'
        self.assertEqual(json.loads(reordered), json.loads(CANONICAL))
        self.assertNotEqual(reordered, CANONICAL)
        self.assertNotEqual(hashlib.sha256(reordered).digest(),
                            hashlib.sha256(CANONICAL).digest())

    def test_whitespace_is_valid_but_not_canonical(self):
        for raw in (b'{"a": 2,"b":1}', CANONICAL + b'\n'):
            self.assertEqual(json.loads(raw), json.loads(CANONICAL))
            self.assertNotEqual(raw, CANONICAL)

    def test_malformed_json_is_a_separate_parse_failure(self):
        with self.assertRaises(json.JSONDecodeError):
            json.loads(b'{"a":2,"b":1,}')

    def test_semantic_change_is_canonical_but_has_changed_input_digest(self):
        original_parameters = b'{"quantity":100}'
        tampered_parameters = b'{"quantity":500}'
        self.assertEqual(json.loads(tampered_parameters), {"quantity": 500})
        self.assertNotEqual(hashlib.sha256(original_parameters).digest(),
                            hashlib.sha256(tampered_parameters).digest())

    def test_duplicate_names_need_a_domain_check(self):
        # Ordinary JSON loading silently loses the first member. A real verifier
        # must reject duplicates before this loss; these are not NC-01 fixtures.
        pairs = json.loads(b'{"a":1,"a":2}', object_pairs_hook=list)
        self.assertEqual(pairs, [("a", 1), ("a", 2)])

    def test_single_sided_zero_failure_bounds(self):
        self.assertAlmostEqual(1 - 0.05 ** (1 / 30), 0.09503385, places=7)
        self.assertAlmostEqual(1 - 0.05 ** (1 / 300), 0.00993608, places=7)
        self.assertGreater(1 - 0.025 ** (1 / 30), 0.115)


if __name__ == '__main__':
    unittest.main()
