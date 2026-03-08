#ifndef NGCC_API_H
#define NGCC_API_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int (*CryptHash)(int digest_len_bits,
                     const unsigned char *msg,
                     unsigned long long msg_len_bits,
                     unsigned char *digest);

    unsigned long long (*sig_get_pk_len_bytes)(void);
    unsigned long long (*sig_get_sk_len_bytes)(void);
    unsigned long long (*sig_get_sn_len_bytes)(void);
    int (*sig_keygen)(unsigned char *pk, unsigned long long *pk_len_bytes,
                      unsigned char *sk, unsigned long long *sk_len_bytes);
    int (*sig_sign)(unsigned char *sk, unsigned long long sk_len_bytes,
                    unsigned char *m, unsigned long long m_len_bytes,
                    unsigned char *sn, unsigned long long *sn_len_bytes);
    int (*sig_verify)(unsigned char *pk, unsigned long long pk_len_bytes,
                      unsigned char *sn, unsigned long long sn_len_bytes,
                      unsigned char *m, unsigned long long m_len_bytes);

    unsigned long long (*kem_get_pk_len_bytes)(void);
    unsigned long long (*kem_get_sk_len_bytes)(void);
    unsigned long long (*kem_get_ss_len_bytes)(void);
    unsigned long long (*kem_get_ct_len_bytes)(void);
    int (*kem_keygen)(unsigned char *pk, unsigned long long *pk_len_bytes,
                      unsigned char *sk, unsigned long long *sk_len_bytes);
    int (*kem_enc)(unsigned char *pk, unsigned long long pk_len_bytes,
                   unsigned char *ss, unsigned long long *ss_len_bytes,
                   unsigned char *ct, unsigned long long *ct_len_bytes);
    int (*kem_dec)(unsigned char *sk, unsigned long long sk_len_bytes,
                   unsigned char *ct, unsigned long long ct_len_bytes,
                   unsigned char *ss, unsigned long long *ss_len_bytes);

    unsigned long long (*kex_get_passes_num)(void);
    unsigned long long (*kex_get_pk_len_bytes)(void);
    unsigned long long (*kex_get_sk_len_bytes)(void);
    unsigned long long (*kex_get_sta_len_bytes)(void);
    unsigned long long (*kex_get_stb_len_bytes)(void);
    unsigned long long (*kex_get_ss_len_bytes)(void);
    unsigned long long (*kex_get_total_msg_len_bytes)(void);

    int (*kex_init_a)(unsigned char *pka, unsigned long long *pka_len_bytes,
                      unsigned char *ska, unsigned long long *ska_len_bytes,
                      unsigned char *sta, unsigned long long *sta_len_bytes);
    int (*kex_init_b)(unsigned char *pkb, unsigned long long *pkb_len_bytes,
                      unsigned char *skb, unsigned long long *skb_len_bytes,
                      unsigned char *stb, unsigned long long *stb_len_bytes);

    int (*kex_generate_pass1_msg_a)(unsigned char *ska, unsigned long long ska_len_bytes,
                                    unsigned char *pkb, unsigned long long pkb_len_bytes,
                                    unsigned char *sta, unsigned long long *sta_len_bytes,
                                    unsigned char *m1, unsigned long long *m1_len_bytes);

    int (*kex_generate_pass2_msg_b)(unsigned char *skb, unsigned long long skb_len_bytes,
                                    unsigned char *pka, unsigned long long pka_len_bytes,
                                    unsigned char *m1, unsigned long long m1_len_bytes,
                                    unsigned char *stb, unsigned long long *stb_len_bytes,
                                    unsigned char *m2, unsigned long long *m2_len_bytes);

    int (*kex_generate_pass3_msg_a)(unsigned char *ska, unsigned long long ska_len_bytes,
                                    unsigned char *pkb, unsigned long long pkb_len_bytes,
                                    unsigned char *m2, unsigned long long m2_len_bytes,
                                    unsigned char *sta, unsigned long long *sta_len_bytes,
                                    unsigned char *m3, unsigned long long *m3_len_bytes);

    int (*kex_derive_ss_a)(unsigned char *ska, unsigned long long ska_len_bytes,
                           unsigned char *pkb, unsigned long long pkb_len_bytes,
                           unsigned char *mb, unsigned long long mb_len_bytes,
                           unsigned char *sta, unsigned long long sta_len_bytes,
                           unsigned char *ssa, unsigned long long *ssa_len_bytes);

    int (*kex_derive_ss_b)(unsigned char *skb, unsigned long long skb_len_bytes,
                           unsigned char *pka, unsigned long long pka_len_bytes,
                           unsigned char *ma, unsigned long long ma_len_bytes,
                           unsigned char *stb, unsigned long long stb_len_bytes,
                           unsigned char *ssb, unsigned long long *ssb_len_bytes);
} ngcc_api_t;

#ifdef __cplusplus
}
#endif

#endif
