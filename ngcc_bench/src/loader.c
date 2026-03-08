#include "loader.h"

#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

/* TEST_MASK_* constants live in cli_types.h */
#include "cli_types.h"

static int load_symbol(void *handle, const char *name, void **fn_ptr) {
    dlerror();
    *fn_ptr = dlsym(handle, name);
    if (dlerror() != NULL || *fn_ptr == NULL) {
        fprintf(stderr, "[ERROR][loader] missing symbol: %s\n", name);
        return -1;
    }
    return 0;
}

/* ── Per-group loaders ──────────────────────────────────────────── */

static int load_hash_symbols(void *handle, ngcc_api_t *api) {
    if (load_symbol(handle, "CryptHash", (void **) &api->CryptHash) != 0) {
        return -1;
    }
    return 0;
}

static int load_sig_symbols(void *handle, ngcc_api_t *api) {
    if (load_symbol(handle, "sig_get_pk_len_bytes", (void **) &api->sig_get_pk_len_bytes) != 0 ||
        load_symbol(handle, "sig_get_sk_len_bytes", (void **) &api->sig_get_sk_len_bytes) != 0 ||
        load_symbol(handle, "sig_get_sn_len_bytes", (void **) &api->sig_get_sn_len_bytes) != 0 ||
        load_symbol(handle, "sig_keygen", (void **) &api->sig_keygen) != 0 ||
        load_symbol(handle, "sig_sign", (void **) &api->sig_sign) != 0 ||
        load_symbol(handle, "sig_verify", (void **) &api->sig_verify) != 0) {
        return -1;
    }
    return 0;
}

static int load_kem_symbols(void *handle, ngcc_api_t *api) {
    if (load_symbol(handle, "kem_get_pk_len_bytes", (void **) &api->kem_get_pk_len_bytes) != 0 ||
        load_symbol(handle, "kem_get_sk_len_bytes", (void **) &api->kem_get_sk_len_bytes) != 0 ||
        load_symbol(handle, "kem_get_ss_len_bytes", (void **) &api->kem_get_ss_len_bytes) != 0 ||
        load_symbol(handle, "kem_get_ct_len_bytes", (void **) &api->kem_get_ct_len_bytes) != 0 ||
        load_symbol(handle, "kem_keygen", (void **) &api->kem_keygen) != 0 ||
        load_symbol(handle, "kem_enc", (void **) &api->kem_enc) != 0 ||
        load_symbol(handle, "kem_dec", (void **) &api->kem_dec) != 0) {
        return -1;
    }
    return 0;
}

static int load_kex_symbols(void *handle, ngcc_api_t *api) {
    if (load_symbol(handle, "kex_get_passes_num", (void **) &api->kex_get_passes_num) != 0 ||
        load_symbol(handle, "kex_get_pk_len_bytes", (void **) &api->kex_get_pk_len_bytes) != 0 ||
        load_symbol(handle, "kex_get_sk_len_bytes", (void **) &api->kex_get_sk_len_bytes) != 0 ||
        load_symbol(handle, "kex_get_sta_len_bytes", (void **) &api->kex_get_sta_len_bytes) != 0 ||
        load_symbol(handle, "kex_get_stb_len_bytes", (void **) &api->kex_get_stb_len_bytes) != 0 ||
        load_symbol(handle, "kex_get_ss_len_bytes", (void **) &api->kex_get_ss_len_bytes) != 0 ||
        load_symbol(handle, "kex_get_total_msg_len_bytes", (void **) &api->kex_get_total_msg_len_bytes) != 0 ||
        load_symbol(handle, "kex_init_a", (void **) &api->kex_init_a) != 0 ||
        load_symbol(handle, "kex_init_b", (void **) &api->kex_init_b) != 0 ||
        load_symbol(handle, "kex_generate_pass1_msg_a", (void **) &api->kex_generate_pass1_msg_a) != 0 ||
        load_symbol(handle, "kex_generate_pass2_msg_b", (void **) &api->kex_generate_pass2_msg_b) != 0 ||
        load_symbol(handle, "kex_generate_pass3_msg_a", (void **) &api->kex_generate_pass3_msg_a) != 0 ||
        load_symbol(handle, "kex_derive_ss_a", (void **) &api->kex_derive_ss_a) != 0 ||
        load_symbol(handle, "kex_derive_ss_b", (void **) &api->kex_derive_ss_b) != 0) {
        return -1;
    }
    return 0;
}

/* ── Public API ─────────────────────────────────────────────────── */

int ngcc_load_library(const char *lib_path, unsigned int test_mask,
                      ngcc_library_t *out_lib) {
    void *handle;
    ngcc_api_t *api;

    if (lib_path == NULL || out_lib == NULL) {
        return -1;
    }

    memset(out_lib, 0, sizeof(*out_lib));

    handle = dlopen(lib_path, RTLD_NOW);
    if (handle == NULL) {
        fprintf(stderr, "[ERROR][loader] dlopen failed: %s\n", dlerror());
        return -1;
    }

    out_lib->handle = handle;
    api = &out_lib->api;

    if ((test_mask & TEST_MASK_HASH) && load_hash_symbols(handle, api) != 0) {
        ngcc_unload_library(out_lib);
        return -1;
    }
    if ((test_mask & (TEST_MASK_DSA | TEST_MASK_DSA_KEYGEN | TEST_MASK_DSA_SIG | TEST_MASK_DSA_VERIFY)) &&
        load_sig_symbols(handle, api) != 0) {
        ngcc_unload_library(out_lib);
        return -1;
    }
    if ((test_mask & (TEST_MASK_KEM | TEST_MASK_KEM_KEYGEN | TEST_MASK_KEM_ENCAP | TEST_MASK_KEM_DECAP)) &&
        load_kem_symbols(handle, api) != 0) {
        ngcc_unload_library(out_lib);
        return -1;
    }
    if ((test_mask & TEST_MASK_KEX) && load_kex_symbols(handle, api) != 0) {
        ngcc_unload_library(out_lib);
        return -1;
    }

    return 0;
}

void ngcc_unload_library(ngcc_library_t *lib) {
    if (lib == NULL) {
        return;
    }

    if (lib->handle != NULL) {
        dlclose(lib->handle);
    }

    memset(lib, 0, sizeof(*lib));
}
