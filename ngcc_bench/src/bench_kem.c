#include "bench_kem.h"

#include <stdlib.h>
#include <string.h>

#define NGCC_MAX_BUFFER_LEN (64ULL * 1024ULL * 1024ULL)

typedef struct {
    const ngcc_api_t *api;
    unsigned char *pk;
    unsigned char *sk;
    unsigned char *ct;
    unsigned char *ss_a;
    unsigned char *ss_b;
    unsigned long long pk_cap;
    unsigned long long sk_cap;
    unsigned long long ct_cap;
    unsigned long long ss_cap;
} kem_perf_ctx_t;

static int is_valid_len(unsigned long long n) {
    return n > 0 && n <= NGCC_MAX_BUFFER_LEN;
}

static int kem_run_once(const ngcc_api_t *api,
                        unsigned char *pk,
                        unsigned long long pk_cap,
                        unsigned char *sk,
                        unsigned long long sk_cap,
                        unsigned char *ct,
                        unsigned long long ct_cap,
                        unsigned char *ss_a,
                        unsigned char *ss_b,
                        unsigned long long ss_cap) {
    unsigned long long pk_len = pk_cap;
    unsigned long long sk_len = sk_cap;
    unsigned long long ct_len = ct_cap;
    unsigned long long ss_a_len = ss_cap;
    unsigned long long ss_b_len = ss_cap;

    if (api->kem_keygen(pk, &pk_len, sk, &sk_len) != 0) {
        return -1;
    }
    if (pk_len == 0 || pk_len > pk_cap || sk_len == 0 || sk_len > sk_cap) {
        return -1;
    }

    if (api->kem_enc(pk, pk_len, ss_a, &ss_a_len, ct, &ct_len) != 0) {
        return -1;
    }
    if (ss_a_len == 0 || ss_a_len > ss_cap || ct_len == 0 || ct_len > ct_cap) {
        return -1;
    }

    if (api->kem_dec(sk, sk_len, ct, ct_len, ss_b, &ss_b_len) != 0) {
        return -1;
    }
    if (ss_b_len == 0 || ss_b_len > ss_cap || ss_a_len != ss_b_len) {
        return -1;
    }

    if (memcmp(ss_a, ss_b, (size_t) ss_a_len) != 0) {
        return -1;
    }

    return 0;
}

static int kem_perf_op(void *ctx_ptr) {
    kem_perf_ctx_t *ctx = (kem_perf_ctx_t *) ctx_ptr;
    return kem_run_once(ctx->api,
                        ctx->pk,
                        ctx->pk_cap,
                        ctx->sk,
                        ctx->sk_cap,
                        ctx->ct,
                        ctx->ct_cap,
                        ctx->ss_a,
                        ctx->ss_b,
                        ctx->ss_cap);
}

int ngcc_kem_correctness(const ngcc_api_t *api) {
    unsigned long long pk_cap;
    unsigned long long sk_cap;
    unsigned long long ct_cap;
    unsigned long long ss_cap;
    unsigned char *pk = NULL;
    unsigned char *sk = NULL;
    unsigned char *ct = NULL;
    unsigned char *ss_a = NULL;
    unsigned char *ss_b = NULL;
    int rc = -1;

    if (api == NULL) {
        return -1;
    }

    pk_cap = api->kem_get_pk_len_bytes();
    sk_cap = api->kem_get_sk_len_bytes();
    ct_cap = api->kem_get_ct_len_bytes();
    ss_cap = api->kem_get_ss_len_bytes();
    if (!is_valid_len(pk_cap) || !is_valid_len(sk_cap) || !is_valid_len(ct_cap) || !is_valid_len(ss_cap)) {
        return -1;
    }

    pk = (unsigned char *) malloc((size_t) pk_cap);
    sk = (unsigned char *) malloc((size_t) sk_cap);
    ct = (unsigned char *) malloc((size_t) ct_cap);
    ss_a = (unsigned char *) malloc((size_t) ss_cap);
    ss_b = (unsigned char *) malloc((size_t) ss_cap);
    if (pk == NULL || sk == NULL || ct == NULL || ss_a == NULL || ss_b == NULL) {
        goto out;
    }

    if (kem_run_once(api, pk, pk_cap, sk, sk_cap, ct, ct_cap, ss_a, ss_b, ss_cap) != 0) {
        goto out;
    }

    rc = 0;

out:
    free(pk);
    free(sk);
    free(ct);
    free(ss_a);
    free(ss_b);
    return rc;
}

int ngcc_kem_performance(const ngcc_api_t *api,
                         const ngcc_perf_config_t *cfg,
                         ngcc_perf_result_t *out_result) {
    kem_perf_ctx_t ctx;

    if (api == NULL || cfg == NULL || out_result == NULL) {
        return -1;
    }

    memset(&ctx, 0, sizeof(ctx));
    ctx.api = api;
    ctx.pk_cap = api->kem_get_pk_len_bytes();
    ctx.sk_cap = api->kem_get_sk_len_bytes();
    ctx.ct_cap = api->kem_get_ct_len_bytes();
    ctx.ss_cap = api->kem_get_ss_len_bytes();

    if (!is_valid_len(ctx.pk_cap) || !is_valid_len(ctx.sk_cap) || !is_valid_len(ctx.ct_cap) ||
        !is_valid_len(ctx.ss_cap)) {
        return -1;
    }

    ctx.pk = (unsigned char *) malloc((size_t) ctx.pk_cap);
    ctx.sk = (unsigned char *) malloc((size_t) ctx.sk_cap);
    ctx.ct = (unsigned char *) malloc((size_t) ctx.ct_cap);
    ctx.ss_a = (unsigned char *) malloc((size_t) ctx.ss_cap);
    ctx.ss_b = (unsigned char *) malloc((size_t) ctx.ss_cap);
    if (ctx.pk == NULL || ctx.sk == NULL || ctx.ct == NULL || ctx.ss_a == NULL || ctx.ss_b == NULL) {
        free(ctx.pk);
        free(ctx.sk);
        free(ctx.ct);
        free(ctx.ss_a);
        free(ctx.ss_b);
        return -1;
    }

    if (ngcc_run_performance_op(cfg, kem_perf_op, &ctx, out_result) != 0) {
        free(ctx.pk);
        free(ctx.sk);
        free(ctx.ct);
        free(ctx.ss_a);
        free(ctx.ss_b);
        return -1;
    }

    free(ctx.pk);
    free(ctx.sk);
    free(ctx.ct);
    free(ctx.ss_a);
    free(ctx.ss_b);
    return 0;
}
