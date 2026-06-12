/*
 * Reference generator for the gr-pocketsdr bit-exact unpack test.
 *
 * gen_LUT() and the unpack loop in write_file() are copied VERBATIM from
 * PocketSDR app/pocket_dump/pocket_dump.c
 * (https://github.com/tomojitakasu/PocketSDR, commit 0ac643d),
 * Copyright (c) 2021-2026, T. Takasu, BSD 2-clause license
 * (see LICENSE.PocketSDR). This keeps the reference output independent of
 * the gr-pocketsdr implementation under test.
 *
 * Usage: gen_reference <raw_file> <ns> <ch> <IQ> <bits> <out_file>
 *   ns   raw bytes per sample period (1: FE 2CH, 2: FE 4CH, 4: FE 8CH)
 *   ch   channel index (0-based)
 *   IQ   1: I only (INT8 output), 2: I/Q (INT8X2 output)
 *   bits 2 or 3
 *
 * The committed tests/data/ref_*.bin files were produced by this program
 * from tests/data/raw_slice.bin.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* pocket_dump.c: generate lookup table (verbatim) */
static void gen_LUT(int8_t LUT_2b[][256], int8_t LUT_3b[][256])
{
    static const int8_t val_2b[] = {1, 3, -1, -3}; /* sign + magnitude */
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
}

int main(int argc, char** argv)
{
    static int8_t LUT_2b[4][256] = {{0}}, LUT_3b[4][256] = {{0}};

    if (argc < 7) {
        fprintf(stderr, "usage: %s raw_file ns ch IQ bits out_file\n", argv[0]);
        return 1;
    }
    int ns = atoi(argv[2]), ch = atoi(argv[3]), IQ = atoi(argv[4]);
    int bits = atoi(argv[5]);

    FILE* fp = fopen(argv[1], "rb");
    if (!fp) { perror(argv[1]); return 1; }
    fseek(fp, 0, SEEK_END);
    long nbytes = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    uint8_t* buff = (uint8_t*)malloc(nbytes);
    if (fread(buff, 1, nbytes, fp) != (size_t)nbytes) { perror("read"); return 1; }
    fclose(fp);

    int size = (int)(nbytes / ns); /* number of sample periods */
    int8_t* data = (int8_t*)malloc((size_t)size * IQ);

    gen_LUT(LUT_2b, LUT_3b);

    /* pocket_dump.c write_file(): unpack loop (verbatim) */
    {
        int pos = ch % 2 * 2;
        for (int i = 0, j = ch / 2; i < size; i++, j += ns) {
            if (IQ == 1) {
                if (bits == 2) {
                    data[i] = LUT_2b[pos][buff[j]];
                } else {
                    data[i] = LUT_3b[pos][buff[j]];
                }
            } else {
                data[i*2  ] = LUT_2b[pos  ][buff[j]];
                data[i*2+1] = LUT_2b[pos+1][buff[j]];
            }
        }
    }

    fp = fopen(argv[6], "wb");
    if (!fp) { perror(argv[6]); return 1; }
    fwrite(data, 1, (size_t)size * IQ, fp);
    fclose(fp);
    free(buff);
    free(data);
    return 0;
}
