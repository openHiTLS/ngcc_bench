#include "bench_kex.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "bench_core.h"
#include "kat_parser.h"

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

    if (!ngcc_is_valid_len(pk_cap) || !ngcc_is_valid_len(sk_cap) || !ngcc_is_valid_len(sta_cap) || !ngcc_is_valid_len(stb_cap) ||
        !ngcc_is_valid_len(msg_cap) || !ngcc_is_valid_len(ss_cap)) {
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

static unsigned long long kex_field_to_u64(const ngcc_kat_field_t *f) {
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

static int kex_check_field_len(const char *field_name, const ngcc_kat_field_t *data_field,
                               const ngcc_kat_field_t *len_field) {
    unsigned long long expected;
    if (len_field == NULL || data_field == NULL) {
        return 0;
    }
    expected = kex_field_to_u64(len_field);
    if (expected == 0) {
        return 0;
    }
    if ((unsigned long long) data_field->len != expected) {
        fprintf(stderr, "[kex][kat] error: %s length mismatch: "
                "file says %llu bytes, data has %zu bytes\n",
                field_name, expected, data_field->len);
        return -1;
    }
    return 0;
}

static int path_is_directory_kex(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return 0;
    }
    return S_ISDIR(st.st_mode);
}

/* Maximum number of KEX passes to scan for (M1..M{N}, Pass{N}_Sta/Stb). */
#define KEX_MAX_PASS_SCAN 10

static int kex_field_populated(const ngcc_kat_field_t *f) {
    return f != NULL && f->data != NULL && f->len > 0;
}

static int verify_kex_kat_vectors(const ngcc_api_t *api,
                                  const ngcc_kat_file_t *kat,
                                  unsigned long long *io_total,
                                  unsigned long long *io_passed,
                                  unsigned long long *io_failed) {
    unsigned long long pk_cap;
    unsigned long long sk_cap;
    unsigned long long sta_cap;
    unsigned long long stb_cap;
    unsigned long long msg_cap;
    unsigned long long ss_cap;
    unsigned char *ss_out = NULL;
    size_t i;

    pk_cap = api->kex_get_pk_len_bytes();
    sk_cap = api->kex_get_sk_len_bytes();
    sta_cap = api->kex_get_sta_len_bytes();
    stb_cap = api->kex_get_stb_len_bytes();
    msg_cap = api->kex_get_total_msg_len_bytes();
    ss_cap = api->kex_get_ss_len_bytes();
    if (!ngcc_is_valid_len(pk_cap) || !ngcc_is_valid_len(sk_cap) || !ngcc_is_valid_len(sta_cap) ||
        !ngcc_is_valid_len(stb_cap) || !ngcc_is_valid_len(msg_cap) || !ngcc_is_valid_len(ss_cap)) {
        return -1;
    }

    ss_out = (unsigned char *) malloc((size_t) ss_cap);
    if (ss_out == NULL) {
        return -1;
    }

    for (i = 0; i < kat->count; ++i) {
        const ngcc_kat_vector_t *vec = &kat->vectors[i];

        const ngcc_kat_field_t *ska = ngcc_kat_get_field(vec, "SKa");
        const ngcc_kat_field_t *pkb = ngcc_kat_get_field(vec, "PKb");
        const ngcc_kat_field_t *pka = ngcc_kat_get_field(vec, "PKa");
        const ngcc_kat_field_t *skb = ngcc_kat_get_field(vec, "SKb");
        const ngcc_kat_field_t *ss = ngcc_kat_get_field(vec, "SS");

        /* ma = last message from A (odd pass), mb = last message from B (even pass),
         * sta_field = last state of A, stb_field = last state of B.
         * Initialized to Init_Sta / Init_Stb as defaults for protocols that
         * derive SS directly from the initial state. */
        const ngcc_kat_field_t *ma_field = NULL;
        const ngcc_kat_field_t *mb_field = NULL;
        const ngcc_kat_field_t *sta_field = ngcc_kat_get_field(vec, "Init_Sta");
        const ngcc_kat_field_t *stb_field = ngcc_kat_get_field(vec, "Init_Stb");

        int has_a = 0;
        int has_b = 0;
        int case_failed = 0;
        int len_failed = 0;
        int pass;

        /* Skip vectors with empty SS (blank template) */
        if (ss == NULL || ss->data == NULL || ss->len == 0) {
            continue;
        }

        /* Scan M1..M{N} and corresponding Pass{N}_Sta / Pass{N}_Stb to
         * find the last message and state for each party.
         *   Odd passes (1,3,5...) → message from A, state update for Sta
         *   Even passes (2,4,6...) → message from B, state update for Stb
         * Also validate _Len consistency for each populated field. */
        for (pass = 1; pass <= KEX_MAX_PASS_SCAN; ++pass) {
            char m_name[16];
            char m_len_name[24];
            char st_name[24];
            char st_len_name[32];
            const ngcc_kat_field_t *msg;
            const ngcc_kat_field_t *state;

            snprintf(m_name, sizeof(m_name), "M%d", pass);
            msg = ngcc_kat_get_field(vec, m_name);
            if (!kex_field_populated(msg)) {
                break;  /* no more passes */
            }

            /* validate M{n}_Len */
            snprintf(m_len_name, sizeof(m_len_name), "M%d_Len", pass);
            if (kex_check_field_len(m_name, msg, ngcc_kat_get_field(vec, m_len_name)) != 0) {
                len_failed = 1;
                break;
            }

            if (pass % 2 == 1) {
                /* Odd pass: message from A */
                ma_field = msg;
                snprintf(st_name, sizeof(st_name), "Pass%d_Sta", pass);
                state = ngcc_kat_get_field(vec, st_name);
                if (kex_field_populated(state)) {
                    sta_field = state;
                    snprintf(st_len_name, sizeof(st_len_name), "Pass%d_Sta_Len", pass);
                    if (kex_check_field_len(st_name, state, ngcc_kat_get_field(vec, st_len_name)) != 0) {
                        len_failed = 1;
                        break;
                    }
                }
            } else {
                /* Even pass: message from B */
                mb_field = msg;
                snprintf(st_name, sizeof(st_name), "Pass%d_Stb", pass);
                state = ngcc_kat_get_field(vec, st_name);
                if (kex_field_populated(state)) {
                    stb_field = state;
                    snprintf(st_len_name, sizeof(st_len_name), "Pass%d_Stb_Len", pass);
                    if (kex_check_field_len(st_name, state, ngcc_kat_get_field(vec, st_len_name)) != 0) {
                        len_failed = 1;
                        break;
                    }
                }
            }
        }

        (*io_total)++;

        if (len_failed) {
            (*io_failed)++;
            continue;
        }

        /* Validate Init_Sta_Len / Init_Stb_Len if present */
        if (kex_field_populated(ngcc_kat_get_field(vec, "Init_Sta"))) {
            if (kex_check_field_len("Init_Sta",
                                    ngcc_kat_get_field(vec, "Init_Sta"),
                                    ngcc_kat_get_field(vec, "Init_Sta_Len")) != 0) {
                (*io_failed)++;
                continue;
            }
        }
        if (kex_field_populated(ngcc_kat_get_field(vec, "Init_Stb"))) {
            if (kex_check_field_len("Init_Stb",
                                    ngcc_kat_get_field(vec, "Init_Stb"),
                                    ngcc_kat_get_field(vec, "Init_Stb_Len")) != 0) {
                (*io_failed)++;
                continue;
            }
        }

        /* Validate key and SS length fields */
        if (kex_check_field_len("SKa", ska, ngcc_kat_get_field(vec, "SKa_Len")) != 0 ||
            kex_check_field_len("PKb", pkb, ngcc_kat_get_field(vec, "PKb_Len")) != 0 ||
            kex_check_field_len("SKb", skb, ngcc_kat_get_field(vec, "SKb_Len")) != 0 ||
            kex_check_field_len("PKa", pka, ngcc_kat_get_field(vec, "PKa_Len")) != 0 ||
            kex_check_field_len("SS", ss, ngcc_kat_get_field(vec, "SS_Len")) != 0) {
            (*io_failed)++;
            continue;
        }

        /* Check if we can verify side A: needs ska, pkb, mb (last B msg), sta */
        has_a = (kex_field_populated(ska) && kex_field_populated(pkb) &&
                 kex_field_populated(mb_field) && kex_field_populated(sta_field));
        /* Check if we can verify side B: needs skb, pka, ma (last A msg), stb */
        has_b = (kex_field_populated(skb) && kex_field_populated(pka) &&
                 kex_field_populated(ma_field) && kex_field_populated(stb_field));

        if (!has_a && !has_b) {
            (*io_total)--; /* undo: not a testable vector */
            continue;
        }

        if (has_a) {
            unsigned long long ss_out_len = ss_cap;

            if (ska->len > sk_cap || pkb->len > pk_cap ||
                mb_field->len > msg_cap || sta_field->len > sta_cap ||
                ss->len > ss_cap) {
                case_failed = 1;
            } else if (api->kex_derive_ss_a((unsigned char *) ska->data,
                                            (unsigned long long) ska->len,
                                            (unsigned char *) pkb->data,
                                            (unsigned long long) pkb->len,
                                            (unsigned char *) mb_field->data,
                                            (unsigned long long) mb_field->len,
                                            (unsigned char *) sta_field->data,
                                            (unsigned long long) sta_field->len,
                                            ss_out,
                                            &ss_out_len) != 0) {
                case_failed = 1;
            } else if (ss_out_len != (unsigned long long) ss->len ||
                       memcmp(ss_out, ss->data, ss->len) != 0) {
                case_failed = 1;
            }
        }

        if (!case_failed && has_b) {
            unsigned long long ss_out_len = ss_cap;

            if (skb->len > sk_cap || pka->len > pk_cap ||
                ma_field->len > msg_cap || stb_field->len > stb_cap ||
                ss->len > ss_cap) {
                case_failed = 1;
            } else if (api->kex_derive_ss_b((unsigned char *) skb->data,
                                            (unsigned long long) skb->len,
                                            (unsigned char *) pka->data,
                                            (unsigned long long) pka->len,
                                            (unsigned char *) ma_field->data,
                                            (unsigned long long) ma_field->len,
                                            (unsigned char *) stb_field->data,
                                            (unsigned long long) stb_field->len,
                                            ss_out,
                                            &ss_out_len) != 0) {
                case_failed = 1;
            } else if (ss_out_len != (unsigned long long) ss->len ||
                       memcmp(ss_out, ss->data, ss->len) != 0) {
                case_failed = 1;
            }
        }

        if (case_failed) {
            (*io_failed)++;
        } else {
            (*io_passed)++;
        }
    }

    free(ss_out);
    return 0;
}

int ngcc_kex_correctness_kat_file(const ngcc_api_t *api,
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

    if (!path_is_directory_kex(kat_path)) {
        fprintf(stderr, "[kex][kat] error: --kat path is not a directory: %s\n", kat_path);
        return -1;
    }

    dir = opendir(kat_path);
    if (dir == NULL) {
        fprintf(stderr, "[kex][kat] error: cannot open directory: %s\n", kat_path);
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
        if (path_is_directory_kex(file_path)) {
            continue;
        }

        if (strncmp(entry->d_name, "KAT_KEX_", 8) != 0) {
            fprintf(stderr, "[kex][kat] error: unrecognized KAT file: %s\n", entry->d_name);
            closedir(dir);
            goto done;
        }

        memset(&kat, 0, sizeof(kat));
        if (ngcc_kat_parse_file(file_path, &kat) != 0) {
            fprintf(stderr, "[kex][kat] error: failed to parse %s\n", entry->d_name);
            closedir(dir);
            goto done;
        }

        file_count++;
        printf("[kex][kat] testing %s (%zu vectors) ...\n", entry->d_name, kat.count);
        verify_kex_kat_vectors(api, &kat, &total, &passed, &failed);
        printf("[kex][kat] %s: total=%llu passed=%llu failed=%llu\n",
               entry->d_name, total, passed, failed);
        ngcc_kat_free(&kat);
    }
    closedir(dir);

    if (file_count == 0) {
        fprintf(stderr, "[kex][kat] error: no KAT_KEX_ files found in: %s\n", kat_path);
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

int ngcc_kex_performance(const ngcc_api_t *api,
                         const ngcc_perf_config_t *cfg,
                         ngcc_perf_result_t *out_result) {
    kex_perf_ctx_t ctx;
    ngcc_perf_config_t local_cfg;
    int rc = -1;

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

    if (!ngcc_is_valid_len(ctx.pk_cap) || !ngcc_is_valid_len(ctx.sk_cap) || !ngcc_is_valid_len(ctx.sta_cap) ||
        !ngcc_is_valid_len(ctx.stb_cap) || !ngcc_is_valid_len(ctx.msg_cap) || !ngcc_is_valid_len(ctx.ss_cap)) {
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
        goto cleanup;
    }

    local_cfg = *cfg;
    local_cfg.bytes_per_op = ctx.msg_cap;
    if (ngcc_run_performance_op(&local_cfg, kex_perf_op, &ctx, out_result) != 0) {
        goto cleanup;
    }

    rc = 0;

cleanup:
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
    return rc;
}
