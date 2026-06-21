# Library Contract

被测 `.so` 必须导出 `ngcc_bench` 需要的全部符号。  
加载器当前是“全量解析”模式：缺失任意符号都会加载失败。

## 1. Hash API

```c
int CryptHash(int digest_len_bits,
              const unsigned char *msg,
              unsigned long long msg_len_bits,
              unsigned char *digest);
```

## 2. SIG API

```c
unsigned long long sig_get_pk_len_bytes(void);
unsigned long long sig_get_sk_len_bytes(void);
unsigned long long sig_get_sn_len_bytes(void);
int sig_keygen(unsigned char *pk, unsigned long long *pk_len_bytes,
               unsigned char *sk, unsigned long long *sk_len_bytes);
int sig_sign(unsigned char *sk, unsigned long long sk_len_bytes,
             unsigned char *m, unsigned long long m_len_bytes,
             unsigned char *sn, unsigned long long *sn_len_bytes);
int sig_verify(unsigned char *pk, unsigned long long pk_len_bytes,
               unsigned char *sn, unsigned long long sn_len_bytes,
               unsigned char *m, unsigned long long m_len_bytes);
```

## 3. KEM API

```c
unsigned long long kem_get_pk_len_bytes(void);
unsigned long long kem_get_sk_len_bytes(void);
unsigned long long kem_get_ss_len_bytes(void);
unsigned long long kem_get_ct_len_bytes(void);
int kem_keygen(unsigned char *pk, unsigned long long *pk_len_bytes,
               unsigned char *sk, unsigned long long *sk_len_bytes);
int kem_enc(unsigned char *pk, unsigned long long pk_len_bytes,
            unsigned char *ss, unsigned long long *ss_len_bytes,
            unsigned char *ct, unsigned long long *ct_len_bytes);
int kem_dec(unsigned char *sk, unsigned long long sk_len_bytes,
            unsigned char *ct, unsigned long long ct_len_bytes,
            unsigned char *ss, unsigned long long *ss_len_bytes);
```

## 4. KEX API

```c
unsigned long long kex_get_passes_num(void);
unsigned long long kex_get_pk_len_bytes(void);
unsigned long long kex_get_sk_len_bytes(void);
unsigned long long kex_get_sta_len_bytes(void);
unsigned long long kex_get_stb_len_bytes(void);
unsigned long long kex_get_ss_len_bytes(void);
unsigned long long kex_get_total_msg_len_bytes(void);

int kex_init_a(unsigned char *pka, unsigned long long *pka_len_bytes,
               unsigned char *ska, unsigned long long *ska_len_bytes,
               unsigned char *sta, unsigned long long *sta_len_bytes);
int kex_init_b(unsigned char *pkb, unsigned long long *pkb_len_bytes,
               unsigned char *skb, unsigned long long *skb_len_bytes,
               unsigned char *stb, unsigned long long *stb_len_bytes);

int kex_generate_pass1_msg_a(unsigned char *ska, unsigned long long ska_len_bytes,
                             unsigned char *pkb, unsigned long long pkb_len_bytes,
                             unsigned char *sta, unsigned long long *sta_len_bytes,
                             unsigned char *m1, unsigned long long *m1_len_bytes);
int kex_generate_pass2_msg_b(unsigned char *skb, unsigned long long skb_len_bytes,
                             unsigned char *pka, unsigned long long pka_len_bytes,
                             unsigned char *m1, unsigned long long m1_len_bytes,
                             unsigned char *stb, unsigned long long *stb_len_bytes,
                             unsigned char *m2, unsigned long long *m2_len_bytes);
int kex_generate_pass3_msg_a(unsigned char *ska, unsigned long long ska_len_bytes,
                             unsigned char *pkb, unsigned long long pkb_len_bytes,
                             unsigned char *m2, unsigned long long m2_len_bytes,
                             unsigned char *sta, unsigned long long *sta_len_bytes,
                             unsigned char *m3, unsigned long long *m3_len_bytes);
int kex_derive_ss_a(unsigned char *ska, unsigned long long ska_len_bytes,
                    unsigned char *pkb, unsigned long long pkb_len_bytes,
                    unsigned char *mb, unsigned long long mb_len_bytes,
                    unsigned char *sta, unsigned long long sta_len_bytes,
                    unsigned char *ssa, unsigned long long *ssa_len_bytes);
int kex_derive_ss_b(unsigned char *skb, unsigned long long skb_len_bytes,
                    unsigned char *pka, unsigned long long pka_len_bytes,
                    unsigned char *ma, unsigned long long ma_len_bytes,
                    unsigned char *stb, unsigned long long stb_len_bytes,
                    unsigned char *ssb, unsigned long long *ssb_len_bytes);
```

## 5. 行为约定

- 除 KEX pass 函数外，所有测试路径将“`0` 视为成功，非 0 视为失败”。
- KEX pass 函数使用三态返回值：`0` 表示交换尚未完成、应继续下一 pass；`1` 表示交换已完成；负数表示错误。其他正数返回值无效。
- `*_get_*_len_bytes()` 返回值必须大于 0，且不超过 `64 MiB`（框架上限检查）。
- Hash `msg_len` 上限为 `16 MiB`。
- SIG/KEM/KEX 的长度缓冲上限检查为 `64 MiB`。

## 6. 常见加载失败

如果缺失符号，程序会输出：

```text
missing symbol: <symbol_name>
error: failed to load library: /path/to/lib.so
```

排查建议：

- 用 `nm -D /path/to/lib.so` 检查导出符号名是否与契约完全一致。
- 确认编译时未被 C++ 名字改编（需要 `extern "C"`）。
