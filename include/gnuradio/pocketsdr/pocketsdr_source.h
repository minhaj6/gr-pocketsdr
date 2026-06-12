/* -*- c++ -*- */
/*
 * Copyright 2026 Minhaj Ahmad.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_POCKETSDR_POCKETSDR_SOURCE_H
#define INCLUDED_POCKETSDR_POCKETSDR_SOURCE_H

#include <gnuradio/pocketsdr/api.h>
#include <gnuradio/sync_block.h>

#include <string>
#include <vector>

namespace gr {
  namespace pocketsdr {

    /*!
     * \brief Signal source for the Pocket SDR FE 2CH/4CH/8CH GNSS RF
     * frontend (https://github.com/tomojitakasu/PocketSDR).
     * \ingroup pocketsdr
     *
     * Streams 2-bit (or 3-bit I-only) IF samples from the FE over USB and
     * outputs one gr_complex stream per selected RF channel at the
     * quantization levels {±1, ±3}. I-only channels output (I + 0j).
     * All output streams are demuxed from the same raw USB stream, so
     * inter-channel sample alignment is preserved.
     *
     * The device is configured with a standard PocketSDR configuration
     * file (the same format used by pocket_conf); pass an empty string to
     * keep the device's current settings.
     */
    class POCKETSDR_API pocketsdr_source : virtual public gr::sync_block
    {
     public:
      typedef std::shared_ptr<pocketsdr_source> sptr;

      /*!
       * \brief Create a Pocket SDR FE source.
       *
       * \param conf_file PocketSDR configuration file written to the device
       *                  on start ("" keeps current device settings)
       * \param channels  1-based RF channel numbers, one output per entry
       *                  (e.g. {1} or {1, 2})
       */
      static sptr make(const std::string& conf_file,
                       const std::vector<int>& channels);

      //! Sampling rate read back from the device (Hz)
      virtual double get_sample_rate() const = 0;

      //! LO (center) frequency of the i-th selected channel (Hz)
      virtual double get_center_freq(int i) const = 0;

      //! Ring-buffer overrun count since start (0 = no samples lost)
      virtual uint64_t overruns() const = 0;
    };

  } // namespace pocketsdr
} // namespace gr

#endif /* INCLUDED_POCKETSDR_POCKETSDR_SOURCE_H */
