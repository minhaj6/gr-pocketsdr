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
 * Notable changes from the original C sources:
 *  - C++ class, RAII, std::thread/mutex/condition_variable
 *  - bulk-transfer events handled on the device's own libusb context
 *    (the original handled events on the default context)
 *  - blocking read() with timeout and ring-overrun detection
 *  - conf write supports MAX2771 (Pocket SDR FE) only
 */

#include "pocketsdr_dev.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace gr {
namespace pocketsdr {

// USB IDs and vendor requests (PocketSDR src/pocket_sdr.h)
#define SDR_DEV_VID 0x04B4     // Cypress
#define SDR_DEV_PID1 0x1004    // EZ-USB FX2LP (FE 2CH)
#define SDR_DEV_PID2 0x00F1    // EZ-USB FX3 (FE 4CH/8CH)
#define SDR_DEV_IF 0           // USB interface number
#define SDR_DEV_EP 0x86        // bulk transfer endpoint
#define SDR_VR_STAT 0x40       // vendor request: get status
#define SDR_VR_REG_READ 0x41   // vendor request: read register
#define SDR_VR_REG_WRITE 0x42  // vendor request: write register
#define SDR_VR_START 0x44      // vendor request: start bulk transfer
#define SDR_VR_STOP 0x45       // vendor request: stop bulk transfer

#define USB_VR (LIBUSB_RECIPIENT_DEVICE | LIBUSB_REQUEST_TYPE_VENDOR)
#define USB_VR_IN (USB_VR | LIBUSB_ENDPOINT_IN)
#define USB_VR_OUT (USB_VR | LIBUSB_ENDPOINT_OUT)
#define TO_REQUEST 15000  // control transfer timeout (ms)
#define TO_TRANSFER 3000  // bulk transfer timeout (ms)

// device types
#define TYPE_POCKET_2CH 0
#define TYPE_POCKET_4CH 1
#define TYPE_POCKET_8CH 3

// MAX2771 register field definition (PocketSDR src/sdr_conf.c)
struct reg_field_t {
    const char* field; // field name ("" terminates the table)
    uint8_t addr;      // register address
    uint8_t nbit;      // number of bits
    uint8_t pos;       // bit position (0:LSB)
    uint8_t fix[2];    // fixed setting (0:free,1:fixed) [CH1, CH2..n]
    uint32_t val[2];   // value for fixed setting       [CH1, CH2..n]
};

static const reg_field_t MAX2771_field[] = {
    {"CHIPEN",          0x0,  1, 31, {1, 1}, {1, 1}},
    {"IDLE",            0x0,  1, 30, {1, 1}, {0, 0}},
    {"MIXPOLE",         0x0,  1, 17, {1, 1}, {0, 0}},
    {"LNAMODE",         0x0,  2, 15, {0, 0}, {0, 1}},
    {"MIXERMODE",       0x0,  2, 13, {0, 0}, {0, 1}},
    {"FCEN",            0x0,  7,  6, {0, 0}, {0, 0}},
    {"FBW",             0x0,  3,  3, {0, 0}, {0, 0}},
    {"F3OR5",           0x0,  1,  2, {0, 0}, {0, 0}},
    {"FCENX",           0x0,  1,  1, {0, 0}, {0, 0}},
    {"FGAIN",           0x0,  1,  0, {0, 0}, {0, 0}},
    {"ANAIMON",         0x1,  1, 28, {1, 1}, {0, 0}},
    {"IQEN",            0x1,  1, 27, {0, 0}, {0, 0}},
    {"GAINREF",         0x1, 12, 15, {0, 0}, {0, 0}},
    {"SPI_SDIO_CONFIG", 0x1,  2, 13, {1, 1}, {0, 0}},
    {"AGCMODE",         0x1,  2, 11, {0, 0}, {0, 0}},
    {"FORMAT",          0x1,  2,  9, {1, 1}, {1, 1}},
    {"BITS",            0x1,  3,  6, {1, 1}, {2, 2}},
    {"DRVCFG",          0x1,  2,  4, {1, 1}, {0, 0}},
    {"DIEID",           0x1,  2,  0, {1, 1}, {0, 0}},
    {"GAININ",          0x2,  6, 22, {0, 0}, {0, 0}},
    {"HILODEN",         0x2,  1, 20, {1, 1}, {0, 0}},
    {"FHIPEN",          0x2,  1, 15, {0, 0}, {1, 1}},
    {"PGAIEN",          0x2,  1, 13, {0, 0}, {0, 0}},
    {"PGAQEN",          0x2,  1, 12, {0, 0}, {0, 0}},
    {"STRMEN",          0x2,  1, 11, {1, 1}, {0, 0}},
    {"STRMSTART",       0x2,  1, 10, {1, 1}, {0, 0}},
    {"STRMSTOP",        0x2,  1,  9, {1, 1}, {0, 0}},
    {"STRMBITS",        0x2,  2,  4, {1, 1}, {1, 1}},
    {"STAMPEN",         0x2,  1,  3, {1, 1}, {0, 0}},
    {"TIMESYNCEN",      0x2,  1,  2, {1, 1}, {0, 0}},
    {"DATASYNCEN",      0x2,  1,  1, {1, 1}, {0, 0}},
    {"STRMRST",         0x2,  1,  0, {1, 1}, {0, 0}},
    {"LOBAND",          0x3,  1, 28, {0, 0}, {0, 1}},
    {"REFOUTEN",        0x3,  1, 24, {1, 1}, {1, 1}},
    {"IXTAL",           0x3,  2, 19, {1, 1}, {1, 1}},
    {"ICP",             0x3,  1,  9, {1, 1}, {0, 0}},
    {"INT_PLL",         0x3,  1,  3, {0, 0}, {0, 0}},
    {"PWRSAV",          0x3,  1,  2, {1, 1}, {0, 0}},
    {"NDIV",            0x4, 15, 13, {0, 0}, {0, 0}},
    {"RDIV",            0x4, 10,  3, {0, 0}, {0, 0}},
    {"FDIV",            0x5, 20,  8, {0, 0}, {0, 0}},
    {"EXTADCCLK",       0x7,  1, 28, {1, 1}, {1, 1}},
    {"PREFRACDIV_SEL",  0xA,  1,  3, {0, 1}, {0, 0}},
    {"REFCLK_L_CNT",    0x7, 12, 16, {0, 1}, {0, 0}},
    {"REFCLK_M_CNT",    0x7, 12,  4, {0, 1}, {0, 0}},
    {"ADCCLK",          0x7,  1,  2, {0, 1}, {0, 0}},
    {"REFDIV",          0x3,  3, 29, {0, 1}, {0, 0}},
    {"FCLKIN",          0x7,  1,  3, {0, 1}, {0, 0}},
    {"ADCCLK_L_CNT",    0xA, 12, 16, {0, 1}, {0, 0}},
    {"ADCCLK_M_CNT",    0xA, 12,  4, {0, 1}, {0, 0}},
    {"CLKOUT_SEL",      0xA,  1,  2, {1, 1}, {1, 1}},
    {"MODE",            0x7,  1,  0, {1, 1}, {0, 0}},
    {"", 0, 0, 0, {0, 0}, {0, 0}}
};

static uint32_t bit_mask(const reg_field_t* reg)
{
    uint32_t mask = 0;
    int pos1 = reg->pos, pos2 = pos1 + reg->nbit;
    for (int i = 31; i >= 0; i--) {
        mask = (mask << 1) | (i >= pos1 && i < pos2 ? 1 : 0);
    }
    return mask;
}

// ---------------------------------------------------------------------------
// open / close (from sdr_usb_open / sdr_usb_close / sdr_dev_open)

fe_device::fe_device(int bus, int port)
{
    const uint16_t vid[] = {SDR_DEV_VID, SDR_DEV_VID};
    const uint16_t pid[] = {SDR_DEV_PID1, SDR_DEV_PID2};
    libusb_device** devs;
    struct libusb_device_descriptor desc;
    int i, j, ndev, ret;

    if ((ret = libusb_init(&d_ctx))) {
        throw std::runtime_error("libusb_init error (" + std::to_string(ret) + ")");
    }
    if ((ndev = libusb_get_device_list(d_ctx, &devs)) <= 0) {
        libusb_exit(d_ctx);
        throw std::runtime_error("USB device list get error");
    }
    for (i = 0; i < ndev; i++) {
        if (libusb_get_device_descriptor(devs[i], &desc) < 0) continue;
        if ((bus >= 0 && bus != libusb_get_bus_number(devs[i])) ||
            (port >= 0 && port != libusb_get_port_number(devs[i]))) continue;
        for (j = 0; j < 2; j++) {
            if (vid[j] == desc.idVendor && pid[j] == desc.idProduct) break;
        }
        if (j < 2) break;
    }
    if (i >= ndev) {
        libusb_free_device_list(devs, 1);
        libusb_exit(d_ctx);
        throw std::runtime_error(
            "Pocket SDR FE device not found (VID 04B4, PID 1004/00F1)");
    }
    if (libusb_open(devs[i], &d_h)) {
        libusb_free_device_list(devs, 1);
        libusb_exit(d_ctx);
        throw std::runtime_error("USB device open error (permissions?)");
    }
    libusb_free_device_list(devs, 1);

    for (int k = 0; k < MAX_UBUFF; k++) {
        if (!(d_transfer[k] = libusb_alloc_transfer(0))) {
            for (k--; k >= 0; k--) libusb_free_transfer(d_transfer[k]);
            libusb_close(d_h);
            libusb_exit(d_ctx);
            throw std::runtime_error("libusb_alloc_transfer error");
        }
    }
    libusb_claim_interface(d_h, SDR_DEV_IF);
    d_buff = new uint8_t[BUFF_SIZE];
}

fe_device::~fe_device()
{
    if (d_state) stop();
    libusb_release_interface(d_h, SDR_DEV_IF);
    libusb_close(d_h);
    for (int i = 0; i < MAX_UBUFF; i++) libusb_free_transfer(d_transfer[i]);
    libusb_exit(d_ctx);
    delete[] d_buff;
}

// vendor request (from sdr_usb_req)
bool fe_device::usb_req(int mode, uint8_t req, uint16_t val, uint8_t* data, int size)
{
    if (size > 64) return false;
    return libusb_control_transfer(d_h, mode ? USB_VR_OUT : USB_VR_IN, req, val,
                                   0, data, size, TO_REQUEST) >= size;
}

// ---------------------------------------------------------------------------
// device info (from read_dev_type / sdr_dev_get_info / read_MAX2771_stat)

int fe_device::read_dev_type(double* fx)
{
    uint8_t data[6] = {0};

    if (!usb_req(0, SDR_VR_STAT, 0, data, 6)) return -1;
    *fx = (((uint16_t)data[1] << 8) + data[2]) * 1e3; // TCXO freq (Hz)
    if ((data[3] >> 4) & 1) return -2; // Spider SDR: unsupported
    if ((data[0] >> 4) <= 2) return TYPE_POCKET_2CH; // F/W ver.1-2 (FE 2CH)
    if ((data[0] >> 4) == 3) return TYPE_POCKET_4CH; // F/W ver.3 (FE 4CH)
    if ((data[0] >> 4) == 4) return TYPE_POCKET_8CH; // F/W ver.4 (FE 8CH)
    return -1;
}

bool fe_device::get_info(info_t& info)
{
    double fx;
    int type = read_dev_type(&fx);

    if (type == -2) {
        fprintf(stderr, "pocketsdr: Spider SDR detected - not supported\n");
        return false;
    }
    if (type < 0) {
        fprintf(stderr, "pocketsdr: device status read error\n");
        return false;
    }
    info.nch = (type == TYPE_POCKET_2CH) ? 2 : ((type == TYPE_POCKET_4CH) ? 4 : 8);
    info.ns = (type == TYPE_POCKET_2CH) ? 1 : ((type == TYPE_POCKET_4CH) ? 2 : 4);

    for (int ch = 0; ch < info.nch; ch++) {
        static const double ratio[8] = {2.0, 0.25, 0.5, 1.0, 4.0};
        uint8_t data[4];
        uint32_t reg[11];

        for (int i = 0; i < 11; i++) {
            if (!usb_req(0, SDR_VR_REG_READ, (ch << 8) + i, data, 4)) return false;
            for (int j = 0; j < 4; j++) { // swap bytes
                *((uint8_t*)(reg + i) + j) = data[3 - j];
            }
        }
        uint32_t ENIQ = (reg[1] >> 27) & 0x1;
        uint32_t BITS = (reg[1] >> 6) & 0x7;
        uint32_t INT_PLL = (reg[3] >> 3) & 0x1;
        uint32_t NDIV = (reg[4] >> 13) & 0x7FFF;
        uint32_t RDIV = (reg[4] >> 3) & 0x3FF;
        uint32_t FDIV = (reg[5] >> 8) & 0xFFFFF;
        uint32_t REFDIV = (reg[3] >> 29) & 0x7;
        uint32_t FCLKIN = (reg[7] >> 3) & 0x1;
        uint32_t ADCCLK = (reg[7] >> 2) & 0x1;
        uint32_t REFCLK_L = (reg[7] >> 16) & 0xFFF;
        uint32_t REFCLK_M = (reg[7] >> 4) & 0xFFF;
        uint32_t ADCCLK_L = (reg[10] >> 16) & 0xFFF;
        uint32_t ADCCLK_M = (reg[10] >> 4) & 0xFFF;
        uint32_t PREFRACDIV = (reg[0xA] >> 3) & 0x1;

        if (ch == 0) {
            double fs = !PREFRACDIV ? fx : fx * REFCLK_L / (4096.0 - REFCLK_M + REFCLK_L);
            fs *= ADCCLK ? 1.0 : ratio[REFDIV];
            fs *= !FCLKIN ? 1.0 : ADCCLK_L / (4096.0 - ADCCLK_M + ADCCLK_L);
            info.fs = fs;
        }
        info.fo[ch] = fx / RDIV * (INT_PLL ? NDIV : NDIV + FDIV / 1048576.0);
        info.IQ[ch] = (BITS == 4 || ENIQ == 0) ? 1 : 2;
        info.bits[ch] = BITS == 2 ? 2 : (BITS == 4 ? 3 : 1);
    }
    return true;
}

// ---------------------------------------------------------------------------
// configuration write (from sdr_conf_write and helpers)

bool fe_device::write_conf(const std::string& file)
{
    double fx;
    int type = read_dev_type(&fx);

    if (type == -2) {
        fprintf(stderr, "pocketsdr: Spider SDR detected - not supported\n");
        return false;
    }
    if (type < 0) {
        fprintf(stderr, "pocketsdr: device status read error\n");
        return false;
    }
    int nch = (type == TYPE_POCKET_2CH) ? 2 : ((type == TYPE_POCKET_4CH) ? 4 : 8);
    uint32_t regs[MAX_RFCH][MAX_REG] = {{0}};

    // read current settings from device registers (read_regs)
    for (int i = 0; i < nch; i++) {
        for (int j = 0; j < MAX_REG; j++) {
            uint8_t data[4] = {0};
            if (!usb_req(0, SDR_VR_REG_READ, (uint16_t)((i << 8) + j), data, 4)) {
                fprintf(stderr, "pocketsdr: register read error [CH%d] 0x%X\n",
                        i + 1, j);
                return false;
            }
            regs[i][j] = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
                         ((uint32_t)data[2] << 8) | data[3];
        }
    }
    // set fixed value of settings (set_fixed)
    for (int i = 0; *MAX2771_field[i].field; i++) {
        for (int j = 0; j < nch; j++) {
            uint32_t val;
            if (!strcmp(MAX2771_field[i].field, "EXTADCCLK")) {
                val = (type == TYPE_POCKET_2CH && j == 0) ? 0 : 1;
            } else {
                if (!MAX2771_field[i].fix[j >= 1 ? 1 : 0]) continue;
                val = MAX2771_field[i].val[j >= 1 ? 1 : 0];
            }
            uint32_t mask = bit_mask(MAX2771_field + i);
            regs[j][MAX2771_field[i].addr] &= ~mask;
            regs[j][MAX2771_field[i].addr] |= (val << MAX2771_field[i].pos) & mask;
        }
    }
    // read settings from configuration file (read_config_key)
    FILE* fp = fopen(file.c_str(), "r");
    if (!fp) {
        fprintf(stderr, "pocketsdr: conf file open error: %s\n", file.c_str());
        return false;
    }
    char buff[128];
    int ch_mask[MAX_RFCH] = {0};
    for (int l = 0; fgets(buff, sizeof(buff), fp); l++) {
        uint32_t val;
        char key[32], *p;
        int i, ch;

        if ((p = strchr(buff, '#'))) *p = '\0';

        if (!strncmp(buff, "[CHALL]", 7)) {
            for (int j = 0; j < nch; j++) ch_mask[j] = 1;
            continue;
        }
        if (sscanf(buff, "[CH%d", &ch) == 1) {
            if (ch >= 1 && ch <= nch) {
                for (int j = 0; j < nch; j++) ch_mask[j] = (j + 1 == ch);
            } else {
                fprintf(stderr, "pocketsdr: conf invalid CH (%d): CH=%d\n",
                        l + 1, ch);
            }
            continue;
        }
        if (!(p = strchr(buff, '='))) continue;
        *p++ = '\0';
        if (sscanf(buff, "%31s", key) < 1) continue;
        for (i = 0; *MAX2771_field[i].field; i++) {
            if (!strcmp(key, MAX2771_field[i].field)) break;
        }
        if (!*MAX2771_field[i].field) {
            fprintf(stderr, "pocketsdr: conf invalid field (%d): %s\n", l + 1, key);
            continue;
        }
        if (sscanf(p, "%d", (int*)&val) < 1 && sscanf(p, "0x%X", &val) < 1) {
            fprintf(stderr, "pocketsdr: conf invalid value (%d): %s = %s\n",
                    l + 1, key, p);
            continue;
        }
        if (val >= ((uint32_t)1 << MAX2771_field[i].nbit)) {
            fprintf(stderr, "pocketsdr: conf invalid value (%d): %s = %d\n",
                    l + 1, key, val);
            continue;
        }
        uint32_t reg_mask = bit_mask(MAX2771_field + i);
        for (int j = 0; j < nch; j++) {
            if (!ch_mask[j]) continue;
            regs[j][MAX2771_field[i].addr] &= ~reg_mask;
            regs[j][MAX2771_field[i].addr] |= (val << MAX2771_field[i].pos) & reg_mask;
        }
    }
    fclose(fp);

    // write settings to device registers, except reserved/test regs (write_regs)
    for (int i = 0; i < nch; i++) {
        for (int j = 0; j < MAX_REG; j++) {
            if (j == 6 || j == 8 || j == 9) continue;
            uint8_t data[4];
            for (int k = 0; k < 4; k++) {
                data[k] = (uint8_t)(regs[i][j] >> (3 - k) * 8);
            }
            if (!usb_req(1, SDR_VR_REG_WRITE, (uint16_t)((i << 8) + j), data, 4)) {
                fprintf(stderr, "pocketsdr: register write error [CH%d] 0x%X\n",
                        i + 1, j);
                return false;
            }
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// streaming (from sdr_dev_start / sdr_dev_stop / sdr_dev_read / event_handler)

void fe_device::transfer_cb(struct libusb_transfer* transfer)
{
    static_cast<fe_device*>(transfer->user_data)->transfer_done(transfer);
}

void fe_device::transfer_done(struct libusb_transfer* transfer)
{
    if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
        if (d_state) {
            fprintf(stderr, "pocketsdr: libusb bulk transfer error (%d)\n",
                    transfer->status);
            std::lock_guard<std::mutex> lock(d_mtx);
            d_usb_error = true;
            d_cv.notify_all();
        }
        return;
    }
    {
        std::lock_guard<std::mutex> lock(d_mtx);
        d_wp += SIZE_UBUFF;
        if (d_wp - d_rp > BUFF_SIZE) {
            d_overruns++;
            d_rp = d_wp - BUFF_SIZE; // drop oldest data
        }
    }
    d_cv.notify_all();
    libusb_submit_transfer(transfer);
}

void fe_device::event_handler()
{
    struct sched_param param = {99};
    struct timeval to = {0, 100000};

    if (pthread_setschedparam(pthread_self(), SCHED_RR, &param)) {
        // not fatal: needs CAP_SYS_NICE / rtprio limits
        fprintf(stderr, "pocketsdr: real-time thread scheduling unavailable\n");
    }
    while (d_state) {
        if (libusb_handle_events_timeout(d_ctx, &to)) continue;
    }
}

bool fe_device::start()
{
    if (d_state) return false;

    d_rp = d_wp = 0;
    d_usb_error = false;
    for (int i = 0; i < MAX_UBUFF; i++) {
        int ret;
        libusb_fill_bulk_transfer(d_transfer[i], d_h, SDR_DEV_EP,
                                  d_buff + SIZE_UBUFF * i, SIZE_UBUFF,
                                  transfer_cb, this, TO_TRANSFER);
        if ((ret = libusb_submit_transfer(d_transfer[i]))) {
            fprintf(stderr, "pocketsdr: libusb_submit_transfer(%d) error (%d)\n",
                    i, ret);
            for (; i >= 0; i--) libusb_cancel_transfer(d_transfer[i]);
            return false;
        }
    }
    usb_req(1, SDR_VR_START, 0, nullptr, 0);
    d_state = true;
    d_thread = std::thread(&fe_device::event_handler, this);
    return true;
}

bool fe_device::stop()
{
    if (!d_state) return false;

    usb_req(1, SDR_VR_STOP, 0, nullptr, 0);
    d_state = false;
    d_cv.notify_all();
    if (d_thread.joinable()) d_thread.join();
    for (int i = 0; i < MAX_UBUFF; i++) {
        libusb_cancel_transfer(d_transfer[i]);
    }
    return true;
}

int fe_device::read(uint8_t* buff, int size, int timeout_ms)
{
    std::unique_lock<std::mutex> lock(d_mtx);
    if (!d_cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), [&] {
            return d_usb_error || !d_state || d_wp >= d_rp + size;
        })) {
        return 0; // timeout
    }
    if (d_usb_error) return -1;
    if (d_wp < d_rp + size) return 0; // stopped before enough data
    int rp = (int)(d_rp % BUFF_SIZE);
    lock.unlock();

    // safe without the lock: the writer only advances d_wp, and overrun
    // adjustment of d_rp only happens when the reader is > BUFF_SIZE behind
    if (rp + size <= BUFF_SIZE) {
        memcpy(buff, d_buff + rp, size);
    } else {
        memcpy(buff, d_buff + rp, BUFF_SIZE - rp);
        memcpy(buff + BUFF_SIZE - rp, d_buff, size - BUFF_SIZE + rp);
    }
    lock.lock();
    d_rp += size;
    return size;
}

} // namespace pocketsdr
} // namespace gr
