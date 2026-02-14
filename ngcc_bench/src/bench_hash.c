#include "bench_hash.h"

#include <stdlib.h>
#include <string.h>

#define NGCC_MAX_MSG_LEN (16U * 1024U * 1024U)

typedef struct {
    const ngcc_api_t *api;
    int digest_len_bits;
    unsigned char *msg;
    size_t msg_len;
    unsigned char *digest;
    size_t digest_len;
} hash_perf_ctx_t;

static int hash_run_once(const ngcc_api_t *api,
                         int digest_len_bits,
                         const unsigned char *msg,
                         size_t msg_len,
                         unsigned char *digest,
                         size_t digest_len) {
    if (api->CryptHash(digest_len_bits, msg, (unsigned long long) msg_len * 8ULL, digest) != 0) {
        return -1;
    }

    if (digest_len == 0) {
        return -1;
    }

    return 0;
}

static int hash_perf_op(void *ctx_ptr) {
    hash_perf_ctx_t *ctx = (hash_perf_ctx_t *) ctx_ptr;
    return hash_run_once(ctx->api,
                         ctx->digest_len_bits,
                         ctx->msg,
                         ctx->msg_len,
                         ctx->digest,
                         ctx->digest_len);
}

int ngcc_hash_correctness(const ngcc_api_t *api, int digest_len_bits, size_t msg_len) {
    unsigned char *msg = NULL;
    unsigned char *digest_a = NULL;
    unsigned char *digest_b = NULL;
    size_t digest_len;
    int rc = -1;

    if (api == NULL || digest_len_bits <= 0 || msg_len == 0 || msg_len > NGCC_MAX_MSG_LEN) {
        return -1;
    }

    digest_len = (size_t) ((digest_len_bits + 7) / 8);
    if (digest_len == 0) {
        return -1;
    }

    msg = (unsigned char *) malloc(msg_len);
    digest_a = (unsigned char *) malloc(digest_len);
    digest_b = (unsigned char *) malloc(digest_len);
    if (msg == NULL || digest_a == NULL || digest_b == NULL) {
        goto out;
    }

    if (ngcc_fill_random(msg, msg_len) != 0) {
        goto out;
    }

    if (hash_run_once(api, digest_len_bits, msg, msg_len, digest_a, digest_len) != 0) {
        goto out;
    }

    if (hash_run_once(api, digest_len_bits, msg, msg_len, digest_b, digest_len) != 0) {
        goto out;
    }

    if (memcmp(digest_a, digest_b, digest_len) != 0) {
        goto out;
    }

    rc = 0;

out:
    free(msg);
    free(digest_a);
    free(digest_b);
    return rc;
}

int ngcc_hash_performance(const ngcc_api_t *api,
                          int digest_len_bits,
                          size_t msg_len,
                          const ngcc_perf_config_t *cfg,
                          ngcc_perf_result_t *out_result) {
    hash_perf_ctx_t ctx;

    if (api == NULL || cfg == NULL || out_result == NULL || digest_len_bits <= 0 || msg_len == 0 ||
        msg_len > NGCC_MAX_MSG_LEN) {
        return -1;
    }

    memset(&ctx, 0, sizeof(ctx));
    ctx.api = api;
    ctx.digest_len_bits = digest_len_bits;
    ctx.msg_len = msg_len;
    ctx.digest_len = (size_t) ((digest_len_bits + 7) / 8);
    if (ctx.digest_len == 0) {
        return -1;
    }

    ctx.msg = (unsigned char *) malloc(ctx.msg_len);
    ctx.digest = (unsigned char *) malloc(ctx.digest_len);
    if (ctx.msg == NULL || ctx.digest == NULL) {
        free(ctx.msg);
        free(ctx.digest);
        return -1;
    }

    if (ngcc_fill_random(ctx.msg, ctx.msg_len) != 0) {
        free(ctx.msg);
        free(ctx.digest);
        return -1;
    }

    if (ngcc_run_performance_op(cfg, hash_perf_op, &ctx, out_result) != 0) {
        free(ctx.msg);
        free(ctx.digest);
        return -1;
    }

    free(ctx.msg);
    free(ctx.digest);
    return 0;
}
