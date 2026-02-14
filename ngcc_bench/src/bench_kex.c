#include "bench_kex.h"

#include <stdlib.h>
#include <string.h>

#define NGCC_MAX_BUFFER_LEN (64ULL * 1024ULL * 1024ULL)

typedef struct {
    const ngcc_api_t *api;
    unsigned char *pka;
    unsigned char *ska;
    unsigned char *sta;
    unsigned char *pkb;
    unsigned char *skb;
    unsigned char *stb;
    unsigned char *m1;
    unsigned char *m2;
    unsigned char *m3;
    unsigned char *ssa;
    unsigned char *ssb;
    unsigned long long pk_cap;
    unsigned long long sk_cap;
    unsigned long long sta_cap;
    unsigned long long stb_cap;
    unsigned long long msg_cap;
    unsigned long long ss_cap;
} kex_perf_ctx_t;

static int is_valid_len(unsigned long long n) {
    return n > 0 && n <= NGCC_MAX_BUFFER_LEN;
}

static int kex_run_once(const ngcc_api_t *api,
                        unsigned char *pka,
                        unsigned char *ska,
                        unsigned char *sta,
                        unsigned char *pkb,
                        unsigned char *skb,
                        unsigned char *stb,
                        unsigned char *m1,
                        unsigned char *m2,
                        unsigned char *m3,
                        unsigned char *ssa,
                        unsigned char *ssb,
                        unsigned long long pk_cap,
                        unsigned long long sk_cap,
                        unsigned long long sta_cap,
                        unsigned long long stb_cap,
                        unsigned long long msg_cap,
                        unsigned long long ss_cap) {
    unsigned long long pka_len = pk_cap;
    unsigned long long ska_len = sk_cap;
    unsigned long long sta_len = sta_cap;
    unsigned long long pkb_len = pk_cap;
    unsigned long long skb_len = sk_cap;
    unsigned long long stb_len = stb_cap;
    unsigned long long m1_len = msg_cap;
    unsigned long long m2_len = msg_cap;
    unsigned long long m3_len = msg_cap;
    unsigned long long ssa_len = ss_cap;
    unsigned long long ssb_len = ss_cap;

    if (api->kex_init_a(pka, &pka_len, ska, &ska_len, sta, &sta_len) != 0) {
        return -1;
    }
    if (api->kex_init_b(pkb, &pkb_len, skb, &skb_len, stb, &stb_len) != 0) {
        return -1;
    }

    if (pka_len == 0 || pka_len > pk_cap ||
        pkb_len == 0 || pkb_len > pk_cap ||
        ska_len == 0 || ska_len > sk_cap ||
        skb_len == 0 || skb_len > sk_cap ||
        sta_len == 0 || sta_len > sta_cap ||
        stb_len == 0 || stb_len > stb_cap) {
        return -1;
    }

    if (api->kex_generate_pass1_msg_a(ska, ska_len, pkb, pkb_len, sta, &sta_len, m1, &m1_len) != 0) {
        return -1;
    }
    if (m1_len == 0 || m1_len > msg_cap || sta_len == 0 || sta_len > sta_cap) {
        return -1;
    }

    if (api->kex_generate_pass2_msg_b(skb, skb_len, pka, pka_len, m1, m1_len, stb, &stb_len, m2, &m2_len) != 0) {
        return -1;
    }
    if (m2_len == 0 || m2_len > msg_cap || stb_len == 0 || stb_len > stb_cap) {
        return -1;
    }

    if (api->kex_generate_pass3_msg_a(ska, ska_len, pkb, pkb_len, m2, m2_len, sta, &sta_len, m3, &m3_len) != 0) {
        return -1;
    }
    if (m3_len == 0 || m3_len > msg_cap || sta_len == 0 || sta_len > sta_cap) {
        return -1;
    }

    if (api->kex_derive_ss_a(ska, ska_len, pkb, pkb_len, m2, m2_len, sta, sta_len, ssa, &ssa_len) != 0) {
        return -1;
    }
    if (api->kex_derive_ss_b(skb, skb_len, pka, pka_len, m3, m3_len, stb, stb_len, ssb, &ssb_len) != 0) {
        return -1;
    }

    if (ssa_len == 0 || ssa_len > ss_cap || ssb_len == 0 || ssb_len > ss_cap || ssa_len != ssb_len) {
        return -1;
    }

    if (memcmp(ssa, ssb, (size_t) ssa_len) != 0) {
        return -1;
    }

    return 0;
}

static int kex_perf_op(void *ctx_ptr) {
    kex_perf_ctx_t *ctx = (kex_perf_ctx_t *) ctx_ptr;
    return kex_run_once(ctx->api,
                        ctx->pka,
                        ctx->ska,
                        ctx->sta,
                        ctx->pkb,
                        ctx->skb,
                        ctx->stb,
                        ctx->m1,
                        ctx->m2,
                        ctx->m3,
                        ctx->ssa,
                        ctx->ssb,
                        ctx->pk_cap,
                        ctx->sk_cap,
                        ctx->sta_cap,
                        ctx->stb_cap,
                        ctx->msg_cap,
                        ctx->ss_cap);
}

int ngcc_kex_correctness(const ngcc_api_t *api) {
    unsigned long long pk_cap;
    unsigned long long sk_cap;
    unsigned long long sta_cap;
    unsigned long long stb_cap;
    unsigned long long msg_cap;
    unsigned long long ss_cap;

    unsigned char *pka = NULL;
    unsigned char *ska = NULL;
    unsigned char *sta = NULL;
    unsigned char *pkb = NULL;
    unsigned char *skb = NULL;
    unsigned char *stb = NULL;
    unsigned char *m1 = NULL;
    unsigned char *m2 = NULL;
    unsigned char *m3 = NULL;
    unsigned char *ssa = NULL;
    unsigned char *ssb = NULL;
    int rc = -1;

    if (api == NULL) {
        return -1;
    }

    pk_cap = api->kex_get_pk_len_bytes();
    sk_cap = api->kex_get_sk_len_bytes();
    sta_cap = api->kex_get_sta_len_bytes();
    stb_cap = api->kex_get_stb_len_bytes();
    msg_cap = api->kex_get_total_msg_len_bytes();
    ss_cap = api->kex_get_ss_len_bytes();

    if (!is_valid_len(pk_cap) || !is_valid_len(sk_cap) || !is_valid_len(sta_cap) || !is_valid_len(stb_cap) ||
        !is_valid_len(msg_cap) || !is_valid_len(ss_cap)) {
        return -1;
    }

    pka = (unsigned char *) malloc((size_t) pk_cap);
    ska = (unsigned char *) malloc((size_t) sk_cap);
    sta = (unsigned char *) malloc((size_t) sta_cap);
    pkb = (unsigned char *) malloc((size_t) pk_cap);
    skb = (unsigned char *) malloc((size_t) sk_cap);
    stb = (unsigned char *) malloc((size_t) stb_cap);
    m1 = (unsigned char *) malloc((size_t) msg_cap);
    m2 = (unsigned char *) malloc((size_t) msg_cap);
    m3 = (unsigned char *) malloc((size_t) msg_cap);
    ssa = (unsigned char *) malloc((size_t) ss_cap);
    ssb = (unsigned char *) malloc((size_t) ss_cap);

    if (pka == NULL || ska == NULL || sta == NULL || pkb == NULL || skb == NULL || stb == NULL ||
        m1 == NULL || m2 == NULL || m3 == NULL || ssa == NULL || ssb == NULL) {
        goto out;
    }

    if (kex_run_once(api,
                     pka,
                     ska,
                     sta,
                     pkb,
                     skb,
                     stb,
                     m1,
                     m2,
                     m3,
                     ssa,
                     ssb,
                     pk_cap,
                     sk_cap,
                     sta_cap,
                     stb_cap,
                     msg_cap,
                     ss_cap) != 0) {
        goto out;
    }

    rc = 0;

out:
    free(pka);
    free(ska);
    free(sta);
    free(pkb);
    free(skb);
    free(stb);
    free(m1);
    free(m2);
    free(m3);
    free(ssa);
    free(ssb);
    return rc;
}

int ngcc_kex_performance(const ngcc_api_t *api,
                         const ngcc_perf_config_t *cfg,
                         ngcc_perf_result_t *out_result) {
    kex_perf_ctx_t ctx;

    if (api == NULL || cfg == NULL || out_result == NULL) {
        return -1;
    }

    memset(&ctx, 0, sizeof(ctx));
    ctx.api = api;
    ctx.pk_cap = api->kex_get_pk_len_bytes();
    ctx.sk_cap = api->kex_get_sk_len_bytes();
    ctx.sta_cap = api->kex_get_sta_len_bytes();
    ctx.stb_cap = api->kex_get_stb_len_bytes();
    ctx.msg_cap = api->kex_get_total_msg_len_bytes();
    ctx.ss_cap = api->kex_get_ss_len_bytes();

    if (!is_valid_len(ctx.pk_cap) || !is_valid_len(ctx.sk_cap) || !is_valid_len(ctx.sta_cap) ||
        !is_valid_len(ctx.stb_cap) || !is_valid_len(ctx.msg_cap) || !is_valid_len(ctx.ss_cap)) {
        return -1;
    }

    ctx.pka = (unsigned char *) malloc((size_t) ctx.pk_cap);
    ctx.ska = (unsigned char *) malloc((size_t) ctx.sk_cap);
    ctx.sta = (unsigned char *) malloc((size_t) ctx.sta_cap);
    ctx.pkb = (unsigned char *) malloc((size_t) ctx.pk_cap);
    ctx.skb = (unsigned char *) malloc((size_t) ctx.sk_cap);
    ctx.stb = (unsigned char *) malloc((size_t) ctx.stb_cap);
    ctx.m1 = (unsigned char *) malloc((size_t) ctx.msg_cap);
    ctx.m2 = (unsigned char *) malloc((size_t) ctx.msg_cap);
    ctx.m3 = (unsigned char *) malloc((size_t) ctx.msg_cap);
    ctx.ssa = (unsigned char *) malloc((size_t) ctx.ss_cap);
    ctx.ssb = (unsigned char *) malloc((size_t) ctx.ss_cap);

    if (ctx.pka == NULL || ctx.ska == NULL || ctx.sta == NULL || ctx.pkb == NULL || ctx.skb == NULL ||
        ctx.stb == NULL || ctx.m1 == NULL || ctx.m2 == NULL || ctx.m3 == NULL || ctx.ssa == NULL ||
        ctx.ssb == NULL) {
        free(ctx.pka);
        free(ctx.ska);
        free(ctx.sta);
        free(ctx.pkb);
        free(ctx.skb);
        free(ctx.stb);
        free(ctx.m1);
        free(ctx.m2);
        free(ctx.m3);
        free(ctx.ssa);
        free(ctx.ssb);
        return -1;
    }

    if (ngcc_run_performance_op(cfg, kex_perf_op, &ctx, out_result) != 0) {
        free(ctx.pka);
        free(ctx.ska);
        free(ctx.sta);
        free(ctx.pkb);
        free(ctx.skb);
        free(ctx.stb);
        free(ctx.m1);
        free(ctx.m2);
        free(ctx.m3);
        free(ctx.ssa);
        free(ctx.ssb);
        return -1;
    }

    free(ctx.pka);
    free(ctx.ska);
    free(ctx.sta);
    free(ctx.pkb);
    free(ctx.skb);
    free(ctx.stb);
    free(ctx.m1);
    free(ctx.m2);
    free(ctx.m3);
    free(ctx.ssa);
    free(ctx.ssb);
    return 0;
}
