# GNSS-SDR integration

The `Pocket_SDR_Signal_Source` adapter lives in the[GNSS-SDR](https://github.com/gnss-sdr/gnss-sdr) source tree, not in this
module — this directory carries it as a patch until it is merged upstream.

For now, the integration is provided as a patch to be applied to GNSS-SDR source code. I will be sending a PR to GNSS-SDR with the updates soon.  

The patch was generated against gnss-sdr `next` @ `d272d1eb` (v0.0.21-126). To apply and build:

```sh
git clone https://github.com/gnss-sdr/gnss-sdr
cd gnss-sdr
git checkout d272d1eb          # pinned commit the patch is known to apply to
git am <path-to>/0001-Add-Pocket_SDR_Signal_Source-for-Pocket-SDR-FE-front.patch
mkdir build && cd build
cmake -DCMAKE_PREFIX_PATH=<gr-pocketsdr install prefix> -DENABLE_POCKETSDR=ON ..
make -j$(nproc)
```

The built receiver is at `gnss-sdr/install/gnss-sdr` (in-tree, nothing is
installed system-wide). Example configuration files are in `../examples/`.
