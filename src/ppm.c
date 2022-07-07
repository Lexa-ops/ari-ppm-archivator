#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include "ppm.h"

enum
{
    MAX_TOT_FR = 0x3fff,
    FIRST_QTR = (0xffff + 1) / 4,
    HALF = (0xffff + 1) / 2,
    THIRD_QTR = (0xffff + 1) / 4 * 3,
    SECTION_SIZE = 0xffff,
    CODE_SIZE = 256,
    FIRST_BIT = 0x80,
    START_ORDER=5,
    MAX_MEM_USE=0x60000000,
    AGGRESSION = 4
};

typedef struct ContextModel{
    int esc;
    int count[CODE_SIZE];
    struct ContextModel *next[CODE_SIZE];
    struct ContextModel *prev;
}ContextModel;

ContextModel cm, *stack[START_ORDER + 1];
int SP, context [START_ORDER], l_bord, h_bord, cur_value, bits_to_follow, ORDER;
FILE *ifp, *ofp;
size_t VOLUME;

void init_model (void)
{
    ORDER = START_ORDER;
    for (int i = 0; i < CODE_SIZE; i++) {
        cm.count[i] = 1;
        cm.next[i] = NULL;
    }
    cm.esc = 1;
    cm.prev = NULL;
    memset(context, 0, ORDER * sizeof(int));
    SP = 0;
    l_bord = 0;
    h_bord = SECTION_SIZE;
    bits_to_follow = 0;
    VOLUME = 0;
}

void rescale (ContextModel *CM)
{
    for (int i = 0; i < CODE_SIZE; i++) {
        CM->count[i] -= CM->count[i] / 2;
    }
}

void update_model (int c)
{
    while (SP) {
        SP--;
        int tot_freq = 0;
        for (int i = 0; i < CODE_SIZE; i++) {
            tot_freq += stack[SP]->count[i];
        }
        if (tot_freq + stack[SP]->esc >= MAX_TOT_FR) {
            rescale(stack[SP]);
        }
        if (!stack[SP]->count[c]) {
            stack[SP]->esc += AGGRESSION;
        }
        stack[SP]->count[c] += AGGRESSION;
    }
}

int read_bit_ppm(void)
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

void write_bit_ppm(int bit, int flag)
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

void bits_plus_follow_ppm(int bit)
{
    write_bit_ppm(bit, 0);
    for (; bits_to_follow > 0; (bits_to_follow)--)
        write_bit_ppm(!bit, 0);
}

void ari_encode(int cum_freq_under, int length, int divider)
{
    int l_new;
    l_new = l_bord + cum_freq_under * (h_bord - l_bord + 1) / divider;
    h_bord = l_bord + (cum_freq_under + length) * (h_bord - l_bord + 1) / divider - 1;
    l_bord = l_new;
    while (1) {
        if (h_bord < HALF)
            bits_plus_follow_ppm(0);
        else if (l_bord >= HALF) {
            bits_plus_follow_ppm(1);
            l_bord -= HALF;
            h_bord -= HALF;
        } else if ((l_bord >= FIRST_QTR) && (h_bord < THIRD_QTR)) {
            bits_to_follow++;
            l_bord -= FIRST_QTR;
            h_bord -= FIRST_QTR;
        } else {
            break;
        }
        l_bord += l_bord;
        h_bord += h_bord + 1;
    }
}

void free_tree(ContextModel *cur_mod)
{
    for (int i = 0; i < CODE_SIZE; i++) {
        if (cur_mod->next[i]) {
            free_tree(cur_mod->next[i]);
        }
    }
    free(cur_mod);
}

void finish_encode(void)
{
    ContextModel *cur_mod = &cm;
    int k;
    for (k = ORDER - 1; k >= 0; k--) {
        if (cur_mod->next[context[k]]) {
            cur_mod = cur_mod->next[context[k]];
        } else {
            break;
        }
    }
    for (int i = k + 1; i < ORDER + 1; i++) {
        int tot_freq = 0;
        for (int j = 0; j < CODE_SIZE; j++) {
            tot_freq += i > 0 && cur_mod->next[context[i - 1]] && cur_mod->next[context[i - 1]]->count[j] != 0 ? 0 : cur_mod->count[j];
        }
        ari_encode(tot_freq, cur_mod->esc, tot_freq + cur_mod->esc);
        cur_mod = cur_mod->prev;
    }

    bits_to_follow++;
    if (l_bord < FIRST_QTR) {
        bits_plus_follow_ppm(0);
    } else {
        bits_plus_follow_ppm(1);
    }
    write_bit_ppm(0, 1);
    fflush(ofp);

    for (int i = 0; i < CODE_SIZE; i++) {
        if (cm.next[i]) {
            free_tree(cm.next[i]);
        }
    }
}

int encode_sym (ContextModel *CM, int c)
{
    stack[SP++] = CM;
    int cum_freq_under = 0, tot_freq = 0, incl_freq;
    for (int i = 0; i < CODE_SIZE; i++) {
        incl_freq = SP > 1 && CM->next[context[SP - 2]]->count[i] != 0 ? 0 : CM->count[i];
        if (i < c) {
            cum_freq_under += incl_freq;
        }
        tot_freq += incl_freq;
    }
    if (CM->count[c]) {
        ari_encode(cum_freq_under, CM->count[c], tot_freq + CM->esc);
        return 1;
    } else {
        if (CM->esc) {
            ari_encode(tot_freq, CM->esc, tot_freq + CM->esc);
        }
        return 0;
    }
}

int decrease_order(ContextModel *CM, int cur_ord)
{
    for (int i = 0; i < CODE_SIZE; i++) {
        if (CM->next[i]) {
            decrease_order(CM->next[i], cur_ord + 1);
            if (cur_ord >= ORDER - 1) {
                CM->next[i] = NULL;
            }
        }
    }
    if (cur_ord >= ORDER) {
        free(CM);
        VOLUME -= sizeof(ContextModel);
        return 1;
    }
    return 0;
}

void compress_ppm (const char *input_name, const char *output_name)
{
    ifp = fopen(input_name, "rb");
    ofp = fopen(output_name, "wb");

    init_model();
    int c, success;
    ContextModel *cur_mod;
    while ((c = getc(ifp)) != EOF) {
        cur_mod = &cm;
        if(VOLUME>MAX_MEM_USE){
            decrease_order(&cm, 0);
            for (int i = 0; i < ORDER - 1; i++) {
                context[i] = context[i + 1];
            }
            ORDER--;
        }
        for (int i = ORDER - 1; i >= 0; i--) {
            if (cur_mod->next[context[i]]) {
                cur_mod = cur_mod->next[context[i]];
            } else {
                cur_mod->next[context[i]] = malloc(sizeof(ContextModel));
                VOLUME+=sizeof(ContextModel);
                cur_mod->next[context[i]]->prev = cur_mod;
                cur_mod = cur_mod->next[context[i]];
                memset(cur_mod->count, 0, CODE_SIZE * sizeof(int));
                memset(cur_mod->next, 0, CODE_SIZE * sizeof(ContextModel *));
                cur_mod->esc = 0;
            }
        }

        for (int i = 0; i < ORDER + 1; i++) {
            success = encode_sym(cur_mod, c);
            if (success) {
                break;
            }
            cur_mod = cur_mod->prev;
        }

        update_model(c);
        for (int i = 0; i < ORDER - 1; i++) {
            context[i] = context[i + 1];
        }
        context[ORDER - 1] = c;
    }

    finish_encode();
}

void ari_start_decode(void)
{
    cur_value = 0;
    for (int i = 0; i < 2; i++) {
        cur_value = (int) ((unsigned int) cur_value << 8);
        cur_value += getc(ifp);
    }
}

void ari_decode(int cum_freq_under, int length, int divider)
{
    int l_new;
    l_new = l_bord + cum_freq_under * (h_bord - l_bord + 1) / divider;
    h_bord = l_bord + (cum_freq_under + length) * (h_bord - l_bord + 1) / divider - 1;
    l_bord = l_new;
    while (1) {
        if (h_bord < HALF) {}
        else if (l_bord >= HALF) {
            l_bord -= HALF;
            h_bord -= HALF;
            cur_value -= HALF;
        } else if ((l_bord >= FIRST_QTR) && (h_bord < THIRD_QTR)) {
            l_bord -= FIRST_QTR;
            h_bord -= FIRST_QTR;
            cur_value -= FIRST_QTR;
        } else {
            break;
        }
        l_bord += l_bord;
        h_bord += h_bord + 1;
        cur_value += cur_value + read_bit_ppm();
    }
}

int decode_sym (ContextModel *CM, int *c)
{
    stack[SP++] = CM;
    if (!CM->esc) {
        return 0;
    }
    int tot_freq = 0;
    for (int i = 0; i < CODE_SIZE; i++) {
        tot_freq += SP > 1 && CM->next[context[SP - 2]]->count[i] != 0 ? 0 : CM->count[i];
    }
    int cum_freq = ((cur_value - l_bord + 1) * (tot_freq + CM->esc) - 1) / (h_bord - l_bord + 1);
    if (cum_freq < tot_freq) {
        int cum_freq_under = 0, incl_freq;
        int i = 0;
        while (1) {
            incl_freq = SP > 1 && CM->next[context[SP - 2]]->count[i] != 0 ? 0 : CM->count[i];
            if ((cum_freq_under + incl_freq) <= cum_freq) {
                cum_freq_under += incl_freq;
            } else {
                break;
            }
            i++;
        }
        ari_decode(cum_freq_under, CM->count[i], tot_freq + CM->esc);
        *c = i;
        return 1;
    } else {
        ari_decode(tot_freq, CM->esc, tot_freq + CM->esc);
        return 0;
    }
}

void finish_decode(void)
{
    for (int i = 0; i < CODE_SIZE; i++) {
        if (cm.next[i]) {
            free_tree(cm.next[i]);
        }
    }
}

void decompress_ppm (const char *input_name, const char *output_name)
{
    ifp = fopen(input_name, "rb");
    ofp = fopen(output_name, "wb");

    init_model();
    int c, success;
    ContextModel *cur_mod;
    ari_start_decode();
    while (1) {
        cur_mod = &cm;
        if(VOLUME>MAX_MEM_USE){
            decrease_order(&cm, 0);
            for (int i = 0; i < ORDER - 1; i++) {
                context[i] = context[i + 1];
            }
            ORDER--;
        }
        for (int i = ORDER - 1; i >= 0; i--) {
            if (cur_mod->next[context[i]]) {
                cur_mod = cur_mod->next[context[i]];
            } else {
                cur_mod->next[context[i]] = malloc(sizeof(ContextModel));
                VOLUME+=sizeof(ContextModel);
                cur_mod->next[context[i]]->prev = cur_mod;
                cur_mod = cur_mod->next[context[i]];
                memset(cur_mod->count, 0, CODE_SIZE * sizeof(int));
                memset(cur_mod->next, 0, CODE_SIZE * sizeof(ContextModel *));
                cur_mod->esc = 0;
            }
        }
        for (int i = 0; i < ORDER + 1; i++) {
            success = decode_sym(cur_mod, &c);
            if (success) {
                goto cont_label;
            }
            cur_mod = cur_mod->prev;
        }
        goto exit_label;
        cont_label:

        update_model(c);
        for (int i = 0; i < ORDER - 1; i++) {
            context[i] = context[i + 1];
        }
        context[ORDER - 1] = c;
        putc(c, ofp);
    }

    exit_label:
    finish_decode();
}
