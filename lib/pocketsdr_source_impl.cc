/* -*- c++ -*- */
/*
 * Copyright 2026 Minhaj Ahmad.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "pocketsdr_source_impl.h"

#include <gnuradio/io_signature.h>
#include <gnuradio/pocketsdr/raw_unpack.h>

#include <stdexcept>

namespace gr {
  namespace pocketsdr {

    // read raw data in chunks of this many sample periods
    static constexpr int CHUNK_SAMPLES = 65536;
    static constexpr int READ_TIMEOUT_MS = 1000;

    pocketsdr_source::sptr
    pocketsdr_source::make(const std::string& conf_file,
                           const std::vector<int>& channels)
    {
      return gnuradio::make_block_sptr<pocketsdr_source_impl>(conf_file,
                                                              channels);
    }

    pocketsdr_source_impl::pocketsdr_source_impl(
        const std::string& conf_file, const std::vector<int>& channels)
      : gr::sync_block(
            "pocketsdr_source",
            gr::io_signature::make(0, 0, 0),
            gr::io_signature::make(channels.size(), channels.size(),
                                   sizeof(gr_complex)))
    {
      if (channels.empty()) {
        throw std::invalid_argument("pocketsdr_source: no channels selected");
      }
      d_dev = std::make_unique<fe_device>();

      if (!conf_file.empty() && !d_dev->write_conf(conf_file)) {
        throw std::runtime_error("pocketsdr_source: configuration write failed: " +
                                 conf_file);
      }
      if (!d_dev->get_info(d_info)) {
        throw std::runtime_error("pocketsdr_source: device info read failed");
      }
      for (int ch : channels) {
        if (ch < 1 || ch > d_info.nch) {
          throw std::invalid_argument(
              "pocketsdr_source: invalid channel " + std::to_string(ch) +
              " (device has " + std::to_string(d_info.nch) + " channels)");
        }
        d_channels.push_back(ch - 1);
      }
      d_raw.resize((size_t)CHUNK_SAMPLES * d_info.ns);

      d_logger->info("Pocket SDR FE {}CH: fs={:.6f} MHz", d_info.nch,
                     d_info.fs * 1e-6);
      for (size_t i = 0; i < d_channels.size(); i++) {
        int ch = d_channels[i];
        d_logger->info("  out{} <- CH{}: LO={:.6f} MHz {} {} bits",
                       i, ch + 1, d_info.fo[ch] * 1e-6,
                       d_info.IQ[ch] == 2 ? "I/Q" : "I", d_info.bits[ch]);
      }
    }

    pocketsdr_source_impl::~pocketsdr_source_impl() {}

    bool pocketsdr_source_impl::start()
    {
      return d_dev->start();
    }

    bool pocketsdr_source_impl::stop()
    {
      d_dev->stop();
      if (d_dev->overruns() > 0) {
        d_logger->warn("{} ring-buffer overruns (samples lost)",
                       d_dev->overruns());
      }
      return true;
    }

    double pocketsdr_source_impl::get_center_freq(int i) const
    {
      if (i < 0 || i >= (int)d_channels.size()) return 0.0;
      return d_info.fo[d_channels[i]];
    }

    int pocketsdr_source_impl::work(int noutput_items,
                                    gr_vector_const_void_star& input_items,
                                    gr_vector_void_star& output_items)
    {
      int nsamp = std::min(noutput_items, CHUNK_SAMPLES);
      int ret = d_dev->read(d_raw.data(), nsamp * d_info.ns, READ_TIMEOUT_MS);

      if (ret < 0) {
        d_logger->error("USB streaming error - stopping");
        return WORK_DONE;
      }
      if (ret == 0) {
        return 0; // timeout: let the scheduler call us again
      }
      for (size_t i = 0; i < d_channels.size(); i++) {
        int ch = d_channels[i];
        unpack_raw_complex(d_raw.data(), nsamp, d_info.ns, ch, d_info.IQ[ch],
                           d_info.bits[ch],
                           static_cast<gr_complex*>(output_items[i]));
      }
      return nsamp;
    }

  } /* namespace pocketsdr */
} /* namespace gr */
