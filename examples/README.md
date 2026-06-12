# Examples

## pocketsdr_psd.grc — live spectrum in GNU Radio Companion

Open with `gnuradio-companion pocketsdr_psd.grc` (with `GRC_BLOCKS_PATH`, `PYTHONPATH` and `LD_LIBRARY_PATH` pointing at the gr-pocketsdr install prefix, see the top-level README) and run. Configure the device first with PocketSDR's `pocket_conf`, or set the block's *Config File* parameter.

## gnss-sdr_pocketsdr_L1.conf — live GPS L1 C/A PVT with gnss-sdr

Requires gnss-sdr built with the `Pocket_SDR_Signal_Source` adapter:

```sh
cd gnss-sdr && mkdir -p build && cd build
cmake -DCMAKE_PREFIX_PATH=<gr-pocketsdr prefix> -DENABLE_POCKETSDR=ON ..
make -j$(nproc)
```

Then, with the FE plugged into a USB 3.0 port and an active GNSS antenna
with sky view:

1. Edit `SignalSource.conf_file` in `gnss-sdr_pocketsdr_L1.conf` to the full path of `PocketSDR/conf/pocket_L1L5_20MHz.conf` (the adapter writes it to
   the device on start), or leave it empty and configure the device beforehand with `pocket_conf`.

2. Run from an empty working directory — gnss-sdr writes RINEX, NMEA, KML, GPX, GeoJSON and ephemeris XML files into the current directory:

   ```sh
   mkdir run && cd run
   <gnss-sdr build>/install/gnss-sdr --config_file=../gnss-sdr_pocketsdr_L1.conf
   ```

3. Wait for `Tracking of GPS L1 C/A signal started ...` messages, then (after ~1-2 min of ephemeris collection) green `Position at ...` lines: the live PVT fix. Stop with Ctrl-C.

If needed, set CPU governor to `performance` first (`sudo cpupower frequency-set -g performance`) to avoid buffer overruns at 20 Msps.

## gnss-sdr_pocketsdr_L1_file_validation.conf — offline validation

Same downstream chain fed from a `pocket_dump` capture instead of the live
device; useful to separate configuration problems from source problems:

```sh
gnss-sdr --config_file=gnss-sdr_pocketsdr_L1_file_validation.conf \
         --signal_source=<capture>.bin
```

The capture must be CH1 GPS L1, IQ zero-IF, 20 Msps, INT8X2 format
 for this configuration file (`pocket_dump` output with `pocket_L1L5_20MHz.conf`). Otherwise, adjust `SignalSource.sampling_frequency`,
`InputFilter.sampling_frequency`/`decimation_factor` and
`GNSS-SDR.internal_fs_sps` to match your capture.
