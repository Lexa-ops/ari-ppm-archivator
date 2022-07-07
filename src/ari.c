#include "ari.h"
#include <stdio.h>

enum
{
    MAX_TOT_FR = 0x1fff,
    FIRST_QTR = (0xffff + 1) / 4,
    HALF = (0xffff + 1) / 2,
    THIRD_QTR = (0xffff + 1) / 4 * 3,
    SECTION_SIZE = 0xffff,
    CODE_SIZE = 257,
    FIRST_BIT = 0x80,
    NUM_OF_TABLES = 3
};
int AGRESSION[NUM_OF_TABLES] = {5, 15, 26};

int read_bit(FILE *ifp)
{
    static int buf;
    static unsigned mask = 0;
    if (mask == 0) {
        mask = FIRST_BIT;
        buf = getc(ifp);
        if (buf == EOF) {
            return EOF;
        }
    }
    int ret = (int) (buf & mask);
    mask >>= 1;
    if (ret) {
        return 1;
    } else {
        return 0;
    }
}

void write_bit(FILE *ofp, int bit, int flag)
{
    static int buf = 0;
    static unsigned mask = FIRST_BIT;
    if (flag == 1) {
        putc(buf, ofp);
        return;
    }
    if (mask == 0) {
        mask = FIRST_BIT;
        putc(buf, ofp);
        buf = 0;
    }
    if (bit) {
        buf |= (int) mask;
    }
    mask >>= 1;
}

void bits_plus_follow(FILE *ofp, int bit, int *bits_to_follow)
{
    write_bit(ofp, bit, 0);
    for (; *bits_to_follow > 0; (*bits_to_follow)--)
        write_bit(ofp, !bit, 0);
}

void table_update(int freq[NUM_OF_TABLES][CODE_SIZE], int b[NUM_OF_TABLES][CODE_SIZE+1], int c)
{
    for (int j = 0; j < NUM_OF_TABLES; j++) {
        if (b[j][CODE_SIZE] > MAX_TOT_FR) {
            for (int i = 0; i < CODE_SIZE; i++) {
                freq[j][i] -= freq[j][i]/2;
            }
        }
        freq[j][c] += AGRESSION[j];
        b[j][0] = 0;
        for (int i = 1; i <= CODE_SIZE; i++) {
            b[j][i] = b[j][i - 1] + freq[j][i - 1];
        }
    }
}

void compress_ari(const char *ifile, const char *ofile)
{
    FILE *ifp = (FILE *) fopen(ifile, "rb");
    FILE *ofp = (FILE *) fopen(ofile, "wb");

    int freq[NUM_OF_TABLES][CODE_SIZE];
    int b[NUM_OF_TABLES][CODE_SIZE + 1];
    for (int j = 0; j < NUM_OF_TABLES; j++) {
        b[j][0] = 0;
        for (int i = 0; i < CODE_SIZE; i++) {
            freq[j][i] = 1;
            b[j][i + 1] = b[j][i] + freq[j][i];
        }
    }

    int max_sec, l_next, h_next, c,
            l = 0,
            h = SECTION_SIZE,
            range = h - l + 1,
            bits_to_follow = 0,
            endflag = 0,
            chosen = 0;
    size_t iteration=0;
    while (endflag != 1) {
        if ((c = getc(ifp)) == EOF) {
            endflag++;
        }
        if (c == -1) {
            c = CODE_SIZE - 1;
        }

        l_next = l + b[chosen][c] * range / b[chosen][CODE_SIZE];
        h = l + b[chosen][c + 1] * range / b[chosen][CODE_SIZE] - 1;
        l = l_next;

        while (1) {
            if (h < HALF) {
                bits_plus_follow(ofp, 0, &bits_to_follow);
            } else if (l >= HALF) {
                bits_plus_follow(ofp, 1, &bits_to_follow);
                l -= HALF;
                h -= HALF;
            } else if ((l >= FIRST_QTR) && (h < THIRD_QTR)) {
                bits_to_follow++;
                l -= FIRST_QTR;
                h -= FIRST_QTR;
            } else break;
            l += l;
            h += h + 1;
        }
        range = h - l + 1;
        max_sec = 0;
        for (int i = 0; i < NUM_OF_TABLES; i++) {
            l_next = l + b[i][c] * range / b[i][CODE_SIZE];
            h_next = l + b[i][c + 1] * range / b[i][CODE_SIZE] - 1;
            if (h_next > l_next && h_next - l_next > max_sec) {
                max_sec = h_next - l_next;
                chosen = i;
            }
        }
        table_update(freq, b, c);
        iteration++;
    }
    bits_to_follow++;
    if (l < FIRST_QTR) {
        bits_plus_follow(ofp, 0, &bits_to_follow);
    } else {
        bits_plus_follow(ofp, 1, &bits_to_follow);
    }
    write_bit(ofp, 0, 1);
    fflush(ofp);

    fclose(ifp);
    fclose(ofp);
}

void decompress_ari(const char *ifile, const char *ofile)
{
    FILE *ifp = (FILE *) fopen(ifile, "rb");
    FILE *ofp = (FILE *) fopen(ofile, "wb");

    int freq[NUM_OF_TABLES][CODE_SIZE];
    int b[NUM_OF_TABLES][CODE_SIZE + 1];
    for (int j = 0; j < NUM_OF_TABLES; j++) {
        b[j][0] = 0;
        for (int i = 0; i < CODE_SIZE; i++) {
            freq[j][i] = 1;
            b[j][i + 1] = b[j][i] + freq[j][i];
        }
    }

    int max_sec, l_next, h_next, c, cum_freq, cum_freq_under,
            l = 0,
            h = SECTION_SIZE,
            range = h - l + 1,
            chosen = 0,
            value = 0;

    for (int i = 0; i < 2; i++) {
        value = (int) ((unsigned int) value << 8);
        value += getc(ifp);
    }
    while (1) {
        cum_freq = ((value - l + 1) * b[chosen][CODE_SIZE] - 1) / (h - l + 1);
        c = 0;
        cum_freq_under = 0;
        while (1) {
            if ((cum_freq_under + freq[chosen][c]) <= cum_freq) {
                cum_freq_under += freq[chosen][c];
            } else {
                break;
            }
            c++;
        }
        if (c == 256) {
            break;
        }

        l_next = l + b[chosen][c] * range / b[chosen][CODE_SIZE];
        h = l + b[chosen][c + 1] * range / b[chosen][CODE_SIZE] - 1;
        l = l_next;

        while (1) {
            if (h < HALF);
            else if (l >= HALF) {
                value -= HALF;
                l -= HALF;
                h -= HALF;
            } else if ((l >= FIRST_QTR) && (h < THIRD_QTR)) {
                value -= FIRST_QTR;
                l -= FIRST_QTR;
                h -= FIRST_QTR;
            } else {
                break;
            }
            l += l;
            h += h + 1;
            value += value + read_bit(ifp);
        }
        putc((int) c, ofp);

        range = h - l + 1;
        max_sec = 0;
        for (int i = 0; i < NUM_OF_TABLES; i++) {
            l_next = l + b[i][c] * range / b[i][CODE_SIZE];
            h_next = l + b[i][c + 1] * range / b[i][CODE_SIZE] - 1;
            if (h_next > l_next && h_next - l_next > max_sec) {
                max_sec = h_next - l_next;
                chosen = i;
            }
        }
        table_update(freq, b, c);
    }

    fclose(ifp);
    fclose(ofp);
}
