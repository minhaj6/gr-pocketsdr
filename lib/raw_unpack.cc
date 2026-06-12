/* -*- c++ -*- */
/*
 * Raw Pocket SDR FE stream unpack / channel demux.
 *
 * Adapted from PocketSDR app/pocket_dump/pocket_dump.c (gen_LUT, write_file),
 * Copyright (c) 2021-2026, T. Takasu, BSD 2-clause license
 * (see LICENSE.PocketSDR).
 * Modifications Copyright (c) 2026 Minhaj Ahmad, GPLv3 (see LICENSE).
 */

#include <gnuradio/pocketsdr/raw_unpack.h>

namespace gr {
namespace pocketsdr {

// lookup tables: LUT_2b[pos][byte] = 2-bit sign-magnitude field at bit pos*2;
// LUT_3b[pos][byte] = 3-bit field for I-only 3-bit mode (pos even only)
static int8_t LUT_2b[4][256];
static int8_t LUT_3b[4][256];

static bool gen_LUT()
{
    static const int8_t val_2b[] = {1, 3, -1, -3}; // sign + magnitude
    static const int8_t val_3b[] = {1, 3, 5, 7, -1, -3, -5, -7};

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 256; j++) {
            LUT_2b[i][j] = val_2b[(j >> (i * 2)) & 3];
            if (i % 2 == 0) {
                int bits = j >> (i * 2);
                LUT_3b[i][j] = val_3b[((bits << 1) & 6) + ((bits >> 3) & 1)];
            }
        }
    }
    return true;
}

static const bool LUT_ready = gen_LUT();

std::vector<int8_t>
unpack_raw(const std::vector<uint8_t>& raw, int nsamp, int ns, int ch, int IQ, int bits)
{
    std::vector<int8_t> data(nsamp * IQ);
    const uint8_t* buff = raw.data();
    int pos = ch % 2 * 2;

    for (int i = 0, j = ch / 2; i < nsamp; i++, j += ns) {
        if (IQ == 1) {
            if (bits == 2) {
                data[i] = LUT_2b[pos][buff[j]];
            } else {
                data[i] = LUT_3b[pos][buff[j]];
            }
        } else {
            data[i * 2] = LUT_2b[pos][buff[j]];
            data[i * 2 + 1] = LUT_2b[pos + 1][buff[j]];
        }
    }
    return data;
}

void unpack_raw_complex(
    const uint8_t* raw, int nsamp, int ns, int ch, int IQ, int bits, gr_complex* out)
{
    int pos = ch % 2 * 2;

    for (int i = 0, j = ch / 2; i < nsamp; i++, j += ns) {
        if (IQ == 1) {
            int8_t I = (bits == 2) ? LUT_2b[pos][raw[j]] : LUT_3b[pos][raw[j]];
            out[i] = gr_complex((float)I, 0.0f);
        } else {
            out[i] = gr_complex((float)LUT_2b[pos][raw[j]],
                                (float)LUT_2b[pos + 1][raw[j]]);
        }
    }
}

} // namespace pocketsdr
} // namespace gr
