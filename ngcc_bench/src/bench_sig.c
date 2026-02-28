#include "bench_sig.h"

#include <stdlib.h>
#include <string.h>

#include "bench_core.h"
#include "kat_parser.h"

typedef struct {
    const ngcc_api_t *api;
    unsigned char *pk;
    unsigned char *sk;
    unsigned char *sn;
    unsigned char *msg;
    unsigned long long pk_cap;
    unsigned long long sk_cap;
    unsigned long long sn_cap;
    unsigned long long msg_len;
} sig_perf_ctx_t;



static int sig_run_once(const ngcc_api_t *api,
                        unsigned char *pk,
                        unsigned long long pk_cap,
                        unsigned char *sk,
                        unsigned long long sk_cap,
                        unsigned char *sn,
                        unsigned long long sn_cap,
                        unsigned char *msg,
                        unsigned long long msg_len) {
    unsigned long long pk_len = pk_cap;
    unsigned long long sk_len = sk_cap;
    unsigned long long sn_len = sn_cap;

    if (api->sig_keygen(pk, &pk_len, sk, &sk_len) != 0) {
        return -1;
    }
    if (pk_len == 0 || pk_len > pk_cap || sk_len == 0 || sk_len > sk_cap) {
        return -1;
    }

    if (api->sig_sign(sk, sk_len, msg, msg_len, sn, &sn_len) != 0) {
        return -1;
    }
    if (sn_len == 0 || sn_len > sn_cap) {
        return -1;
    }

    if (api->sig_verify(pk, pk_len, sn, sn_len, msg, msg_len) != 0) {
        return -1;
    }

    return 0;
}

static int sig_perf_op(void *ctx_ptr) {
    sig_perf_ctx_t *ctx = (sig_perf_ctx_t *) ctx_ptr;
    if (ngcc_fill_random(ctx->msg, (size_t) ctx->msg_len) != 0) {
        return -1;
    }
    return sig_run_once(ctx->api,
                        ctx->pk,
                        ctx->pk_cap,
                        ctx->sk,
                        ctx->sk_cap,
                        ctx->sn,
                        ctx->sn_cap,
                        ctx->msg,
                        ctx->msg_len);
}

int ngcc_sig_correctness(const ngcc_api_t *api, size_t msg_len) {
    unsigned long long pk_cap;
    unsigned long long sk_cap;
    unsigned long long sn_cap;
    unsigned char *pk = NULL;
    unsigned char *sk = NULL;
    unsigned char *sn = NULL;
    unsigned char *msg = NULL;
    int rc = -1;

    if (api == NULL || msg_len == 0 || msg_len > NGCC_MAX_BUFFER_LEN) {
        return -1;
    }

    pk_cap = api->sig_get_pk_len_bytes();
    sk_cap = api->sig_get_sk_len_bytes();
    sn_cap = api->sig_get_sn_len_bytes();
    if (!ngcc_is_valid_len(pk_cap) || !ngcc_is_valid_len(sk_cap) || !ngcc_is_valid_len(sn_cap)) {
        return -1;
    }

    pk = (unsigned char *) malloc((size_t) pk_cap);
    sk = (unsigned char *) malloc((size_t) sk_cap);
    sn = (unsigned char *) malloc((size_t) sn_cap);
    msg = (unsigned char *) malloc(msg_len);
    if (pk == NULL || sk == NULL || sn == NULL || msg == NULL) {
        goto out;
    }

    if (ngcc_fill_random(msg, msg_len) != 0) {
        goto out;
    }

    if (sig_run_once(api, pk, pk_cap, sk, sk_cap, sn, sn_cap, msg, (unsigned long long) msg_len) != 0) {
        goto out;
    }

    rc = 0;

out:
    free(pk);
    free(sk);
    free(sn);
    free(msg);
    return rc;
}

int ngcc_sig_correctness_kat_file(const ngcc_api_t *api,
                                  const char *kat_path,
                                  unsigned long long *out_total,
                                  unsigned long long *out_passed,
                                  unsigned long long *out_failed) {
    ngcc_kat_file_t kat;
    unsigned long long pk_cap;
    unsigned long long sn_cap;
    size_t i;
    unsigned long long total = 0;
    unsigned long long passed = 0;
    unsigned long long failed = 0;
    int rc = 1;

    if (api == NULL || kat_path == NULL) {
        return -1;
    }

    memset(&kat, 0, sizeof(kat));
    if (ngcc_kat_parse_file(kat_path, &kat) != 0) {
        return -1;
    }

    pk_cap = api->sig_get_pk_len_bytes();
    sn_cap = api->sig_get_sn_len_bytes();
    if (!ngcc_is_valid_len(pk_cap) || !ngcc_is_valid_len(sn_cap)) {
        rc = -1;
        goto out;
    }

    for (i = 0; i < kat.count; ++i) {
        const ngcc_kat_vector_t *vec = &kat.vectors[i];
        static const char *const k_pk_alias[] = {"PK", "PUBLICKEY"};
        static const char *const k_msg_alias[] = {"MSG", "INPUT", "M", "MESSAGE"};
        static const char *const k_sn_alias[] = {"SN", "SIG", "SIGNATURE", "SM", "OUTPUT"};
        const ngcc_kat_field_t *pk = ngcc_kat_get_field_any(vec, k_pk_alias, sizeof(k_pk_alias) / sizeof(k_pk_alias[0]));
        const ngcc_kat_field_t *msg = ngcc_kat_get_field_any(vec, k_msg_alias, sizeof(k_msg_alias) / sizeof(k_msg_alias[0]));
        const ngcc_kat_field_t *sn = ngcc_kat_get_field_any(vec, k_sn_alias, sizeof(k_sn_alias) / sizeof(k_sn_alias[0]));
        if (pk == NULL || msg == NULL || sn == NULL) {
            continue;
        }

        total++;
        if (pk->len == 0 || pk->len > pk_cap || msg->len > NGCC_MAX_BUFFER_LEN ||
            sn->len == 0 || sn->len > sn_cap) {
            failed++;
            continue;
        }
        if (api->sig_verify((unsigned char *) pk->data,
                            (unsigned long long) pk->len,
                            (unsigned char *) sn->data,
                            (unsigned long long) sn->len,
                            (unsigned char *) msg->data,
                            (unsigned long long) msg->len) != 0) {
            failed++;
            continue;
        }
        passed++;
    }

    if (total > 0) {
        rc = (failed == 0) ? 0 : -1;
    }

out:
    if (out_total != NULL) {
        *out_total = total;
    }
    if (out_passed != NULL) {
        *out_passed = passed;
    }
    if (out_failed != NULL) {
        *out_failed = failed;
    }
    ngcc_kat_free(&kat);
    return rc;
}

int ngcc_sig_performance(const ngcc_api_t *api,
                         size_t msg_len,
                         const ngcc_perf_config_t *cfg,
                         ngcc_perf_result_t *out_result) {
    sig_perf_ctx_t ctx;
    ngcc_perf_config_t local_cfg;
    int rc = -1;

    if (api == NULL || cfg == NULL || out_result == NULL || msg_len == 0 || msg_len > NGCC_MAX_BUFFER_LEN) {
        return -1;
    }

    memset(&ctx, 0, sizeof(ctx));
    ctx.api = api;
    ctx.pk_cap = api->sig_get_pk_len_bytes();
    ctx.sk_cap = api->sig_get_sk_len_bytes();
    ctx.sn_cap = api->sig_get_sn_len_bytes();
    ctx.msg_len = (unsigned long long) msg_len;

    if (!ngcc_is_valid_len(ctx.pk_cap) || !ngcc_is_valid_len(ctx.sk_cap) || !ngcc_is_valid_len(ctx.sn_cap)) {
        return -1;
    }

    ctx.pk = (unsigned char *) malloc((size_t) ctx.pk_cap);
    ctx.sk = (unsigned char *) malloc((size_t) ctx.sk_cap);
    ctx.sn = (unsigned char *) malloc((size_t) ctx.sn_cap);
    ctx.msg = (unsigned char *) malloc(msg_len);
    if (ctx.pk == NULL || ctx.sk == NULL || ctx.sn == NULL || ctx.msg == NULL) {
        goto cleanup;
    }

    local_cfg = *cfg;
    local_cfg.bytes_per_op = (unsigned long long) msg_len;
    if (ngcc_run_performance_op(&local_cfg, sig_perf_op, &ctx, out_result) != 0) {
        goto cleanup;
    }

    rc = 0;

cleanup:
    free(ctx.pk);
    free(ctx.sk);
    free(ctx.sn);
    free(ctx.msg);
    return rc;
}
