/* -*- c++ -*- */
/*
 * Copyright 2026 Minhaj Ahmad.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_POCKETSDR_POCKETSDR_SOURCE_IMPL_H
#define INCLUDED_POCKETSDR_POCKETSDR_SOURCE_IMPL_H

#include <gnuradio/pocketsdr/pocketsdr_source.h>

#include <memory>
#include <vector>

#include "pocketsdr_dev.h"

namespace gr {
  namespace pocketsdr {

    class pocketsdr_source_impl : public pocketsdr_source
    {
     private:
      std::unique_ptr<fe_device> d_dev;
      fe_device::info_t d_info;
      std::vector<int> d_channels;     // 0-based selected channels
      std::vector<uint8_t> d_raw;      // raw stream read buffer

     public:
      pocketsdr_source_impl(const std::string& conf_file,
                            const std::vector<int>& channels);
      ~pocketsdr_source_impl();

      bool start() override;
      bool stop() override;

      double get_sample_rate() const override { return d_info.fs; }
      double get_center_freq(int i) const override;
      uint64_t overruns() const override { return d_dev->overruns(); }

      int work(int noutput_items,
               gr_vector_const_void_star& input_items,
               gr_vector_void_star& output_items) override;
    };

  } // namespace pocketsdr
} // namespace gr

#endif /* INCLUDED_POCKETSDR_POCKETSDR_SOURCE_IMPL_H */
