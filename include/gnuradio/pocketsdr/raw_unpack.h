/* -*- c++ -*- */
/*
 * Raw Pocket SDR FE stream unpack / channel demux.
 *
 * The unpack algorithm and lookup tables follow PocketSDR's
 * app/pocket_dump/pocket_dump.c (Copyright (c) 2021-2026, T. Takasu,
 * BSD 2-clause license, see LICENSE.PocketSDR).
 * Modifications Copyright (c) 2026 Minhaj Ahmad, GPLv3 (see LICENSE).
 *
 * Raw stream layout (FE 4CH, "RAW16"): each sample period is ns=2 bytes;
 * byte (ch/2) of the pair carries channels ch and ch+1 as 2-bit
 * sign-magnitude fields: I at bit (ch%2)*4, Q at bit (ch%2)*4+2.
 * 2-bit code -> level: {0,1,2,3} -> {+1,+3,-1,-3}.
 * FE 2CH ("RAW8") uses ns=1, FE 8CH ("RAW32") uses ns=4, same per-byte
 * layout.
 */

#ifndef INCLUDED_POCKETSDR_RAW_UNPACK_H
#define INCLUDED_POCKETSDR_RAW_UNPACK_H

#include <gnuradio/gr_complex.h>
#include <gnuradio/pocketsdr/api.h>

#include <cstdint>
#include <vector>

namespace gr {
namespace pocketsdr {

/*!
 * \brief Unpack one channel from a raw FE stream to int8 samples.
 *
 * Bit-exact equivalent of PocketSDR pocket_dump's file output:
 * IQ=2 -> interleaved I,Q int8 pairs (INT8X2), IQ=1 -> I int8 (INT8).
 *
 * \param raw   raw stream bytes (nsamp * ns bytes)
 * \param nsamp number of sample periods to unpack
 * \param ns    raw bytes per sample period (1: FE 2CH, 2: FE 4CH, 4: FE 8CH)
 * \param ch    channel index (0-based)
 * \param IQ    sampling type (1: I only, 2: I/Q)
 * \param bits  sample bits (2 or 3; 3 only valid with IQ=1)
 */
POCKETSDR_API std::vector<int8_t> unpack_raw(const std::vector<uint8_t>& raw,
                                             int nsamp,
                                             int ns,
                                             int ch,
                                             int IQ,
                                             int bits);

/*!
 * \brief Unpack one channel from a raw FE stream to gr_complex samples.
 *
 * Same lookup tables as unpack_raw(); IQ=1 channels produce (I + 0j).
 */
POCKETSDR_API void unpack_raw_complex(const uint8_t* raw,
                                      int nsamp,
                                      int ns,
                                      int ch,
                                      int IQ,
                                      int bits,
                                      gr_complex* out);

} // namespace pocketsdr
} // namespace gr

#endif /* INCLUDED_POCKETSDR_RAW_UNPACK_H */
