/* -*- c++ -*- */
/*
 * Pocket SDR FE USB device driver (Linux / libusb-1.0).
 *
 * Adapted for gr-pocketsdr from the PocketSDR C library
 * (https://github.com/tomojitakasu/PocketSDR: src/sdr_usb.c, src/sdr_dev.c,
 * src/sdr_conf.c), Copyright (c) 2021-2026, T. Takasu, All rights reserved,
 * distributed under the BSD 2-clause license (see LICENSE.PocketSDR).
 * Modifications Copyright (c) 2026 Minhaj Ahmad, GPLv3 (see LICENSE).
 *
 * Supports Pocket SDR FE 2CH/4CH/8CH (MAX2771). Spider SDR (MAX2769B) is
 * intentionally not supported.
 */

#ifndef INCLUDED_POCKETSDR_DEV_H
#define INCLUDED_POCKETSDR_DEV_H

#include <libusb-1.0/libusb.h>

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

namespace gr {
namespace pocketsdr {

class fe_device
{
public:
    static constexpr int MAX_RFCH = 8;     // max number of RF channels
    static constexpr int MAX_REG = 11;     // MAX2771 registers 0x0-0xA
    static constexpr int MAX_UBUFF = 6;    // number of USB bulk buffers
    static constexpr int SIZE_UBUFF = 1 << 20; // USB bulk buffer size (bytes)
    static constexpr int BUFF_SIZE = SIZE_UBUFF * MAX_UBUFF; // ring size

    // device info read from hardware
    struct info_t {
        int nch;               // number of RF channels (2, 4 or 8)
        int ns;                // raw stream bytes per sample period (1, 2 or 4)
        double fs;             // sampling rate (Hz)
        double fo[MAX_RFCH];   // LO frequency per channel (Hz)
        int IQ[MAX_RFCH];      // sampling type per channel (1:I, 2:IQ)
        int bits[MAX_RFCH];    // sample bits per channel (2 or 3)
    };

    // open device (throws std::runtime_error on failure)
    fe_device(int bus = -1, int port = -1);
    ~fe_device();

    fe_device(const fe_device&) = delete;
    fe_device& operator=(const fe_device&) = delete;

    // write a PocketSDR-format configuration file to the device registers
    bool write_conf(const std::string& file);

    // read device info/status from the device registers
    bool get_info(info_t& info);

    bool start();
    bool stop();

    // read raw stream bytes; blocks up to timeout_ms until size bytes are
    // available. returns size, or 0 on timeout, or -1 after a USB error.
    int read(uint8_t* buff, int size, int timeout_ms);

    // ring-buffer overrun counter (reader fell behind the USB stream)
    uint64_t overruns() const { return d_overruns; }

private:
    bool usb_req(int mode, uint8_t req, uint16_t val, uint8_t* data, int size);
    int read_dev_type(double* fx);
    void transfer_done(struct libusb_transfer* transfer);
    static void transfer_cb(struct libusb_transfer* transfer);
    void event_handler();

    libusb_context* d_ctx = nullptr;
    libusb_device_handle* d_h = nullptr;
    struct libusb_transfer* d_transfer[MAX_UBUFF] = {};
    uint8_t* d_buff = nullptr;     // raw stream ring buffer
    volatile bool d_state = false; // streaming state
    bool d_usb_error = false;
    int64_t d_rp = 0, d_wp = 0;    // ring read/write pointers
    uint64_t d_overruns = 0;
    std::thread d_thread;
    std::mutex d_mtx;
    std::condition_variable d_cv;
};

} // namespace pocketsdr
} // namespace gr

#endif /* INCLUDED_POCKETSDR_DEV_H */
