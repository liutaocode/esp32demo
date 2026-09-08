#!/usr/bin/env python3
"""Reject missing, corrupt, misplaced or oversized embedded voice resources."""
import unittest
from test_verify_firmware import VERIFY

class ResourceTest(unittest.TestCase):
    def test_complete(self):
        VERIFY.verify_tts_resource(bytes(0x10000) + b'voice', b'voice')
    def test_missing(self):
        with self.assertRaises(ValueError):
            VERIFY.verify_tts_resource(bytes(0x10000), b'voice')
    def test_corrupt(self):
        with self.assertRaises(ValueError):
            VERIFY.verify_tts_resource(bytes(0x10000) + b'wrong', b'voice')
    def test_empty(self):
        with self.assertRaises(ValueError):
            VERIFY.verify_tts_resource(bytes(0x10000), b'')
    def test_outside_app(self):
        with self.assertRaises(ValueError):
            VERIFY.verify_tts_resource(bytes(0x360000) + b'voice', b'voice')
    def test_oversize(self):
        with self.assertRaises(ValueError):
            VERIFY.verify_tts_resource(bytes(0x10000) + b'x' * 1000000, b'x' * 1000000)

if __name__ == '__main__':
    unittest.main()
