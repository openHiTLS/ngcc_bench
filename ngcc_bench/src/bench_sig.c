#include "bench_sig.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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
    unsigned long long pk_len;
    unsigned long long sk_len;
    unsigned long long sn_len;
    unsigned long long msg_len;
} dsa_ctx_t;

static int validate_sig_caps(unsigned long long pk_cap,
                             unsigned long long sk_cap,
                             unsigned long long sn_cap) {
    return ngcc_is_valid_len(pk_cap) &&
           ngcc_is_valid_len(sk_cap) &&
           ngcc_is_valid_len(sn_cap);
}

static int dsa_keygen_once(const ngcc_api_t *api,
                           unsigned char *pk,
                           unsigned long long pk_cap,
                           unsigned long long *pk_len,
                           unsigned char *sk,
                           unsigned long long sk_cap,
                           unsigned long long *sk_len) {
    if (api->sig_keygen(pk, pk_len, sk, sk_len) != 0) {
        return -1;
    }
    if (*pk_len == 0 || *pk_len > pk_cap || *sk_len == 0 || *sk_len > sk_cap) {
        return -1;
    }
    return 0;
}

static int dsa_sign_once(const ngcc_api_t *api,
                         unsigned char *sk,
                         unsigned long long sk_len,
                         unsigned char *msg,
                         unsigned long long msg_len,
                         unsigned char *sn,
                         unsigned long long sn_cap,
                         unsigned long long *sn_len) {
    if (api->sig_sign(sk, sk_len, msg, msg_len, sn, sn_len) != 0) {
        return -1;
    }
    if (*sn_len == 0 || *sn_len > sn_cap) {
        return -1;
    }
    return 0;
}

static int dsa_verify_once(const ngcc_api_t *api,
                           unsigned char *pk,
                           unsigned long long pk_len,
                           unsigned char *sn,
                           unsigned long long sn_len,
                           unsigned char *msg,
                           unsigned long long msg_len) {
    return api->sig_verify(pk, pk_len, sn, sn_len, msg, msg_len) == 0 ? 0 : -1;
}

static int dsa_prepare_sign_case(dsa_ctx_t *ctx) {
    if (ngcc_fill_random(ctx->msg, (size_t) ctx->msg_len) != 0) {
        return -1;
    }

    ctx->pk_len = ctx->pk_cap;
    ctx->sk_len = ctx->sk_cap;
    if (dsa_keygen_once(ctx->api,
                        ctx->pk,
                        ctx->pk_cap,
                        &ctx->pk_len,
                        ctx->sk,
                        ctx->sk_cap,
                        &ctx->sk_len) != 0) {
        return -1;
    }

    ctx->sn_len = ctx->sn_cap;
    if (dsa_sign_once(ctx->api,
                      ctx->sk,
                      ctx->sk_len,
                      ctx->msg,
                      ctx->msg_len,
                      ctx->sn,
                      ctx->sn_cap,
                      &ctx->sn_len) != 0) {
        return -1;
    }

    return 0;
}

static int alloc_dsa_ctx(const ngcc_api_t *api, size_t msg_len, dsa_ctx_t *out_ctx) {
    if (api == NULL || out_ctx == NULL || msg_len == 0 || msg_len > NGCC_MAX_BUFFER_LEN) {
        return -1;
    }

    memset(out_ctx, 0, sizeof(*out_ctx));
    out_ctx->api = api;
    out_ctx->pk_cap = api->sig_get_pk_len_bytes();
    out_ctx->sk_cap = api->sig_get_sk_len_bytes();
    out_ctx->sn_cap = api->sig_get_sn_len_bytes();
    out_ctx->msg_len = (unsigned long long) msg_len;

    if (!validate_sig_caps(out_ctx->pk_cap, out_ctx->sk_cap, out_ctx->sn_cap)) {
        return -1;
    }

    out_ctx->pk = (unsigned char *) malloc((size_t) out_ctx->pk_cap);
    out_ctx->sk = (unsigned char *) malloc((size_t) out_ctx->sk_cap);
    out_ctx->sn = (unsigned char *) malloc((size_t) out_ctx->sn_cap);
    out_ctx->msg = (unsigned char *) malloc(msg_len);
    if (out_ctx->pk == NULL || out_ctx->sk == NULL || out_ctx->sn == NULL || out_ctx->msg == NULL) {
        free(out_ctx->pk);
        free(out_ctx->sk);
        free(out_ctx->sn);
        free(out_ctx->msg);
        memset(out_ctx, 0, sizeof(*out_ctx));
        return -1;
    }

    return 0;
}

static void free_dsa_ctx(dsa_ctx_t *ctx) {
    if (ctx == NULL) {
        return;
    }
    free(ctx->pk);
    free(ctx->sk);
    free(ctx->sn);
    free(ctx->msg);
    memset(ctx, 0, sizeof(*ctx));
}

int ngcc_dsa_correctness(const ngcc_api_t *api, size_t msg_len) {
    dsa_ctx_t ctx;
    int rc = -1;

    if (alloc_dsa_ctx(api, msg_len, &ctx) != 0) {
        return -1;
    }

    if (dsa_prepare_sign_case(&ctx) != 0) {
        goto out;
    }

    if (dsa_verify_once(api, ctx.pk, ctx.pk_len, ctx.sn, ctx.sn_len, ctx.msg, ctx.msg_len) != 0) {
        goto out;
    }

    rc = 0;

out:
    free_dsa_ctx(&ctx);
    return rc;
}

int ngcc_dsa_keygen_correctness(const ngcc_api_t *api) {
    unsigned long long pk_cap;
    unsigned long long sk_cap;
    unsigned long long pk_len;
    unsigned long long sk_len;
    unsigned char *pk = NULL;
    unsigned char *sk = NULL;
    int rc = -1;

    if (api == NULL) {
        return -1;
    }

    pk_cap = api->sig_get_pk_len_bytes();
    sk_cap = api->sig_get_sk_len_bytes();
    if (!ngcc_is_valid_len(pk_cap) || !ngcc_is_valid_len(sk_cap)) {
        return -1;
    }

    pk = (unsigned char *) malloc((size_t) pk_cap);
    sk = (unsigned char *) malloc((size_t) sk_cap);
    if (pk == NULL || sk == NULL) {
        goto out;
    }

    pk_len = pk_cap;
    sk_len = sk_cap;
    if (dsa_keygen_once(api, pk, pk_cap, &pk_len, sk, sk_cap, &sk_len) != 0) {
        goto out;
    }

    rc = 0;

out:
    free(pk);
    free(sk);
    return rc;
}

int ngcc_dsa_sig_correctness(const ngcc_api_t *api, size_t msg_len) {
    dsa_ctx_t ctx;
    int rc = -1;

    if (alloc_dsa_ctx(api, msg_len, &ctx) != 0) {
        return -1;
    }

    if (dsa_prepare_sign_case(&ctx) != 0) {
        goto out;
    }

    if (dsa_verify_once(api, ctx.pk, ctx.pk_len, ctx.sn, ctx.sn_len, ctx.msg, ctx.msg_len) != 0) {
        goto out;
    }

    rc = 0;

out:
    free_dsa_ctx(&ctx);
    return rc;
}

int ngcc_dsa_verify_correctness(const ngcc_api_t *api, size_t msg_len) {
    dsa_ctx_t ctx;
    int rc = -1;
    unsigned char saved_byte;

    if (alloc_dsa_ctx(api, msg_len, &ctx) != 0) {
        return -1;
    }

    if (dsa_prepare_sign_case(&ctx) != 0) {
        goto out;
    }

    if (dsa_verify_once(api, ctx.pk, ctx.pk_len, ctx.sn, ctx.sn_len, ctx.msg, ctx.msg_len) != 0) {
        goto out;
    }

    saved_byte = ctx.msg[0];
    ctx.msg[0] ^= 0x01U;
    if (dsa_verify_once(api, ctx.pk, ctx.pk_len, ctx.sn, ctx.sn_len, ctx.msg, ctx.msg_len) == 0) {
        goto out;
    }
    ctx.msg[0] = saved_byte;

    rc = 0;

out:
    free_dsa_ctx(&ctx);
    return rc;
}

static unsigned long long sig_field_to_u64(const ngcc_kat_field_t *f) {
    unsigned long long v = 0;
    size_t i;
    if (f == NULL || f->data == NULL || f->len != 8) {
        return 0;
    }
    for (i = 0; i < 8; i++) {
        v = (v << 8) | f->data[i];
    }
    return v;
}

static int sig_check_field_len(const char *field_name, const ngcc_kat_field_t *data_field,
                               const ngcc_kat_field_t *len_field) {
    unsigned long long expected;
    if (len_field == NULL || data_field == NULL) {
        return 0;  /* no len field to validate */
    }
    expected = sig_field_to_u64(len_field);
    if (expected == 0) {
        return 0;  /* empty len value, skip */
    }
    if ((unsigned long long) data_field->len != expected) {
        fprintf(stderr, "[sig][kat] error: %s length mismatch: "
                "file says %llu bytes, data has %zu bytes\n",
                field_name, expected, data_field->len);
        return -1;
    }
    return 0;
}

static int path_is_directory_sig(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return 0;
    }
    return S_ISDIR(st.st_mode);
}

static int verify_sig_kat_vectors(const ngcc_api_t *api,
                                  const ngcc_kat_file_t *kat,
                                  unsigned long long *io_total,
                                  unsigned long long *io_passed,
                                  unsigned long long *io_failed) {
    unsigned long long pk_cap;
    unsigned long long sn_cap;
    size_t i;

    pk_cap = api->sig_get_pk_len_bytes();
    sn_cap = api->sig_get_sn_len_bytes();
    if (!ngcc_is_valid_len(pk_cap) || !ngcc_is_valid_len(sn_cap)) {
        return -1;
    }

    for (i = 0; i < kat->count; ++i) {
        const ngcc_kat_vector_t *vec = &kat->vectors[i];
        const ngcc_kat_field_t *pk = ngcc_kat_get_field(vec, "PK");
        const ngcc_kat_field_t *msg = ngcc_kat_get_field(vec, "M");
        const ngcc_kat_field_t *sn = ngcc_kat_get_field(vec, "Sn");

        /* Skip vectors with empty output (blank template) */
        if (pk == NULL || pk->data == NULL || pk->len == 0) {
            continue;
        }
        if (sn == NULL || sn->data == NULL || sn->len == 0) {
            continue;
        }
        if (msg == NULL || msg->data == NULL) {
            continue;
        }

        /* Validate _Len fields match actual data length */
        if (sig_check_field_len("PK", pk, ngcc_kat_get_field(vec, "PK_Len")) != 0 ||
            sig_check_field_len("M", msg, ngcc_kat_get_field(vec, "M_Len")) != 0 ||
            sig_check_field_len("Sn", sn, ngcc_kat_get_field(vec, "Sn_Len")) != 0) {
            (*io_failed)++;
            continue;
        }

        (*io_total)++;
        if (pk->len > pk_cap || sn->len > sn_cap || msg->len > NGCC_MAX_BUFFER_LEN) {
            (*io_failed)++;
            continue;
        }
        if (dsa_verify_once(api,
                            (unsigned char *) pk->data,
                            (unsigned long long) pk->len,
                            (unsigned char *) sn->data,
                            (unsigned long long) sn->len,
                            (unsigned char *) msg->data,
                            (unsigned long long) msg->len) != 0) {
            (*io_failed)++;
            continue;
        }
        (*io_passed)++;
    }

    return 0;
}

int ngcc_dsa_verify_correctness_kat_file(const ngcc_api_t *api,
                                         const char *kat_path,
                                         unsigned long long *out_total,
                                         unsigned long long *out_passed,
                                         unsigned long long *out_failed) {
    DIR *dir;
    struct dirent *entry;
    unsigned long long total = 0;
    unsigned long long passed = 0;
    unsigned long long failed = 0;
    int file_count = 0;
    int rc = -1;

    if (api == NULL || kat_path == NULL) {
        return -1;
    }

    if (!path_is_directory_sig(kat_path)) {
        fprintf(stderr, "[sig][kat] error: --kat path is not a directory: %s\n", kat_path);
        return -1;
    }

    dir = opendir(kat_path);
    if (dir == NULL) {
        fprintf(stderr, "[sig][kat] error: cannot open directory: %s\n", kat_path);
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        ngcc_kat_file_t kat;
        char file_path[2048];
        int len;

        if (entry->d_name[0] == '.') {
            continue;
        }
        len = snprintf(file_path, sizeof(file_path), "%s/%s", kat_path, entry->d_name);
        if (len < 0 || len >= (int) sizeof(file_path)) {
            continue;
        }
        if (path_is_directory_sig(file_path)) {
            continue;
        }

        if (strncmp(entry->d_name, "KAT_SIG_", 8) != 0) {
            fprintf(stderr, "[sig][kat] error: unrecognized KAT file: %s\n", entry->d_name);
            closedir(dir);
            goto done;
        }

        memset(&kat, 0, sizeof(kat));
        if (ngcc_kat_parse_file(file_path, &kat) != 0) {
            fprintf(stderr, "[sig][kat] error: failed to parse %s\n", entry->d_name);
            closedir(dir);
            goto done;
        }

        file_count++;
        printf("[sig][kat] testing %s (%zu vectors) ...\n", entry->d_name, kat.count);
        verify_sig_kat_vectors(api, &kat, &total, &passed, &failed);
        printf("[sig][kat] %s: total=%llu passed=%llu failed=%llu\n",
               entry->d_name, total, passed, failed);
        ngcc_kat_free(&kat);
    }
    closedir(dir);

    if (file_count == 0) {
        fprintf(stderr, "[sig][kat] error: no KAT_SIG_ files found in: %s\n", kat_path);
        goto done;
    }

    rc = (total > 0 && failed == 0) ? 0 : -1;

done:
    if (out_total != NULL) {
        *out_total = total;
    }
    if (out_passed != NULL) {
        *out_passed = passed;
    }
    if (out_failed != NULL) {
        *out_failed = failed;
    }
    return rc;
}

static int dsa_perf_op(void *ctx_ptr) {
    dsa_ctx_t *ctx = (dsa_ctx_t *) ctx_ptr;

    if (dsa_prepare_sign_case(ctx) != 0) {
        return -1;
    }

    return dsa_verify_once(ctx->api, ctx->pk, ctx->pk_len, ctx->sn, ctx->sn_len, ctx->msg, ctx->msg_len);
}

int ngcc_dsa_performance(const ngcc_api_t *api,
                         size_t msg_len,
                         const ngcc_perf_config_t *cfg,
                         ngcc_perf_result_t *out_result) {
    dsa_ctx_t ctx;
    ngcc_perf_config_t local_cfg;
    int rc = -1;

    if (cfg == NULL || out_result == NULL || alloc_dsa_ctx(api, msg_len, &ctx) != 0) {
        return -1;
    }

    local_cfg = *cfg;
    local_cfg.bytes_per_op = (unsigned long long) msg_len;
    if (ngcc_run_performance_op(&local_cfg, dsa_perf_op, &ctx, out_result) != 0) {
        goto out;
    }

    rc = 0;

out:
    free_dsa_ctx(&ctx);
    return rc;
}

static int dsa_keygen_perf_op(void *ctx_ptr) {
    dsa_ctx_t *ctx = (dsa_ctx_t *) ctx_ptr;

    ctx->pk_len = ctx->pk_cap;
    ctx->sk_len = ctx->sk_cap;
    return dsa_keygen_once(ctx->api,
                           ctx->pk,
                           ctx->pk_cap,
                           &ctx->pk_len,
                           ctx->sk,
                           ctx->sk_cap,
                           &ctx->sk_len);
}

int ngcc_dsa_keygen_performance(const ngcc_api_t *api,
                                const ngcc_perf_config_t *cfg,
                                ngcc_perf_result_t *out_result) {
    dsa_ctx_t ctx;
    ngcc_perf_config_t local_cfg;
    int rc = -1;

    if (api == NULL || cfg == NULL || out_result == NULL) {
        return -1;
    }

    memset(&ctx, 0, sizeof(ctx));
    ctx.api = api;
    ctx.pk_cap = api->sig_get_pk_len_bytes();
    ctx.sk_cap = api->sig_get_sk_len_bytes();

    if (!ngcc_is_valid_len(ctx.pk_cap) || !ngcc_is_valid_len(ctx.sk_cap)) {
        return -1;
    }

    ctx.pk = (unsigned char *) malloc((size_t) ctx.pk_cap);
    ctx.sk = (unsigned char *) malloc((size_t) ctx.sk_cap);
    if (ctx.pk == NULL || ctx.sk == NULL) {
        goto out;
    }

    local_cfg = *cfg;
    local_cfg.bytes_per_op = ctx.pk_cap + ctx.sk_cap;
    if (ngcc_run_performance_op(&local_cfg, dsa_keygen_perf_op, &ctx, out_result) != 0) {
        goto out;
    }

    rc = 0;

out:
    free(ctx.pk);
    free(ctx.sk);
    return rc;
}

static int dsa_sig_perf_op(void *ctx_ptr) {
    dsa_ctx_t *ctx = (dsa_ctx_t *) ctx_ptr;

    if (ngcc_fill_random(ctx->msg, (size_t) ctx->msg_len) != 0) {
        return -1;
    }

    ctx->sn_len = ctx->sn_cap;
    return dsa_sign_once(ctx->api,
                         ctx->sk,
                         ctx->sk_len,
                         ctx->msg,
                         ctx->msg_len,
                         ctx->sn,
                         ctx->sn_cap,
                         &ctx->sn_len);
}

int ngcc_dsa_sig_performance(const ngcc_api_t *api,
                             size_t msg_len,
                             const ngcc_perf_config_t *cfg,
                             ngcc_perf_result_t *out_result) {
    dsa_ctx_t ctx;
    ngcc_perf_config_t local_cfg;
    int rc = -1;

    if (alloc_dsa_ctx(api, msg_len, &ctx) != 0 || cfg == NULL || out_result == NULL) {
        return -1;
    }

    ctx.pk_len = ctx.pk_cap;
    ctx.sk_len = ctx.sk_cap;
    if (dsa_keygen_once(api,
                        ctx.pk,
                        ctx.pk_cap,
                        &ctx.pk_len,
                        ctx.sk,
                        ctx.sk_cap,
                        &ctx.sk_len) != 0) {
        goto out;
    }

    local_cfg = *cfg;
    local_cfg.bytes_per_op = (unsigned long long) msg_len;
    if (ngcc_run_performance_op(&local_cfg, dsa_sig_perf_op, &ctx, out_result) != 0) {
        goto out;
    }

    rc = 0;

out:
    free_dsa_ctx(&ctx);
    return rc;
}

static int dsa_verify_perf_op(void *ctx_ptr) {
    dsa_ctx_t *ctx = (dsa_ctx_t *) ctx_ptr;
    return dsa_verify_once(ctx->api, ctx->pk, ctx->pk_len, ctx->sn, ctx->sn_len, ctx->msg, ctx->msg_len);
}

int ngcc_dsa_verify_performance(const ngcc_api_t *api,
                                size_t msg_len,
                                const ngcc_perf_config_t *cfg,
                                ngcc_perf_result_t *out_result) {
    dsa_ctx_t ctx;
    ngcc_perf_config_t local_cfg;
    int rc = -1;

    if (cfg == NULL || out_result == NULL || alloc_dsa_ctx(api, msg_len, &ctx) != 0) {
        return -1;
    }

    if (dsa_prepare_sign_case(&ctx) != 0) {
        goto out;
    }

    local_cfg = *cfg;
    local_cfg.bytes_per_op = (unsigned long long) msg_len;
    if (ngcc_run_performance_op(&local_cfg, dsa_verify_perf_op, &ctx, out_result) != 0) {
        goto out;
    }

    rc = 0;

out:
    free_dsa_ctx(&ctx);
    return rc;
}
