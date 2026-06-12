#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright 2026 Minhaj Ahmad.
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

import os

from gnuradio import gr, gr_unittest
try:
    from gnuradio.pocketsdr import unpack_raw
except ImportError:
    import sys
    dirname, filename = os.path.split(os.path.abspath(__file__))
    sys.path.append(os.path.join(dirname, "bindings"))
    from gnuradio.pocketsdr import unpack_raw

DATA_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                        os.pardir, os.pardir, "tests", "data")


class qa_pocketsdr_source(gr_unittest.TestCase):
    """Bit-exact unpack regression test.

    Feeds a captured raw Pocket SDR FE 4CH stream (tests/data/raw_slice.bin,
    captured with pocket_dump -r) through the gr-pocketsdr unpack path and
    compares against reference output generated from PocketSDR's own
    pocket_dump unpack code (see tests/gen_reference.c). A bit-exact match
    is required.
    """

    def _unpack_and_compare(self, ch):
        with open(os.path.join(DATA_DIR, "raw_slice.bin"), "rb") as f:
            raw = f.read()
        with open(os.path.join(DATA_DIR, "ref_ch%d_IQ.bin" % ch), "rb") as f:
            ref = f.read()
        ns = 2  # FE 4CH raw stream: 2 bytes per sample period
        nsamp = len(raw) // ns
        out = bytes(b & 0xFF for b in unpack_raw(list(raw), nsamp, ns,
                                                 ch - 1, 2, 2))
        self.assertEqual(len(out), len(ref))
        self.assertEqual(out, ref)

    def test_001_unpack_ch1(self):
        self._unpack_and_compare(1)

    def test_002_unpack_ch2(self):
        self._unpack_and_compare(2)

    def test_003_unpack_ch3(self):
        self._unpack_and_compare(3)

    def test_004_unpack_ch4(self):
        self._unpack_and_compare(4)


if __name__ == '__main__':
    gr_unittest.run(qa_pocketsdr_source)
