## 目标
- 使用C语言开发一个特定函数的Benchmak程序

## 核心内容
- 实现要调用这些接口API_cryptHash - 哈希算法接口                                                                                     
                                                                                                                   
  int CryptHash(int digest_len_bits,                                                                               
                const unsigned char *msg,                                                                          
                unsigned long long msg_len_bits,                                                                   
                unsigned char *digest);                                                                            
                                                                                                                   
  API_PKC - 签名(SIG)接口                                                                                          
                                                                                                                   
  // 获取密钥/签名长度                                                                                             
  unsigned long long sig_get_pk_len_bytes();   // 公钥长度                                                         
  unsigned long long sig_get_sk_len_bytes();   // 私钥长度                                                         
  unsigned long long sig_get_sn_len_bytes();   // 签名长度                                                         
                                                                                                                   
  // 密钥生成                                                                                                      
  int sig_keygen(unsigned char *pk, unsigned long long *pk_len_bytes,                                              
                 unsigned char *sk, unsigned long long *sk_len_bytes);                                             
                                                                                                                   
  // 签名                                                                                                          
  int sig_sign(unsigned char *sk, unsigned long long sk_len_bytes,                                                 
               unsigned char *m, unsigned long long m_len_bytes,                                                   
               unsigned char *sn, unsigned long long *sn_len_bytes);                                               
                                                                                                                   
  // 验签                                                                                                          
  int sig_verify(unsigned char *pk, unsigned long long pk_len_bytes,                                               
                 unsigned char *sn, unsigned long long sn_len_bytes,                                               
                 unsigned char *m, unsigned long long m_len_bytes);                                                
                                                                                                                   
  API_PKC - 密钥封装(KEM)接口                                                                                      
                                                                                                                   
  // 获取密钥/密文长度                                                                                             
  unsigned long long kem_get_pk_len_bytes();   // 公钥长度                                                         
  unsigned long long kem_get_sk_len_bytes();   // 私钥长度                                                         
  unsigned long long kem_get_ss_len_bytes();   // 共享密钥长度                                                     
  unsigned long long kem_get_ct_len_bytes();   // 密文长度                                                         
                                                                                                                   
  // 密钥生成                                                                                                      
  int kem_keygen(unsigned char *pk, unsigned long long *pk_len_bytes,                                              
                 unsigned char *sk, unsigned long long *sk_len_bytes);                                             
                                                                                                                   
  // 封装                                                                                                          
  int kem_enc(unsigned char *pk, unsigned long long pk_len_bytes,                                                  
              unsigned char *ss, unsigned long long *ss_len_bytes,                                                 
              unsigned char *ct, unsigned long long *ct_len_bytes);                                                
                                                                                                                   
  // 解封装                                                                                                        
  int kem_dec(unsigned char *sk, unsigned long long sk_len_bytes,                                                  
              unsigned char *ct, unsigned long long ct_len_bytes,                                                  
              unsigned char *ss, unsigned long long *ss_len_bytes);                                                
                                                                                                                   
  API_PKC - 密钥交换(KEX)接口                                                                                      
                                                                                                                   
  // 获取参数长度                                                                                                  
  unsigned long long kex_get_passes_num();         // 交换轮数                                                     
  unsigned long long kex_get_pk_len_bytes();       // 长期公钥长度                                                 
  unsigned long long kex_get_sk_len_bytes();       // 长期私钥长度                                                 
  unsigned long long kex_get_sta_len_bytes();      // 发起方状态信息长度                                           
  unsigned long long kex_get_stb_len_bytes();      // 响应方状态信息长度                                           
  unsigned long long kex_get_ss_len_bytes();       // 共享密钥长度                                                 
  unsigned long long kex_get_total_msg_len_bytes(); // 所有消息总长度                                              
                                                                                                                   
  // 初始化                                                                                                        
  int kex_init_a(unsigned char *pka, unsigned long long *pka_len_bytes,                                            
                 unsigned char *ska, unsigned long long *ska_len_bytes,                                            
                 unsigned char *sta, unsigned long long *sta_len_bytes);                                           
                                                                                                                   
  int kex_init_b(unsigned char *pkb, unsigned long long *pkb_len_bytes,                                            
                 unsigned char *skb, unsigned long long *skb_len_bytes,                                            
                 unsigned char *stb, unsigned long long *stb_len_bytes);                                           
                                                                                                                   
  // 生成第1轮消息（发起方）                                                                                       
  int kex_generate_pass1_msg_a(unsigned char *ska, unsigned long long ska_len_bytes,                               
                                unsigned char *pkb, unsigned long long pkb_len_bytes,                              
                                unsigned char *sta, unsigned long long *sta_len_bytes,                             
                                unsigned char *m1, unsigned long long *m1_len_bytes);                              
                                                                                                                   
  // 生成第2轮消息（响应方）                                                                                       
  int kex_generate_pass2_msg_b(unsigned char *skb, unsigned long long skb_len_bytes,                               
                                unsigned char *pka, unsigned long long pka_len_bytes,                              
                                unsigned char *m1, unsigned long long m1_len_bytes,                                
                                unsigned char *stb, unsigned long long *stb_len_bytes,                             
                                unsigned char *m2, unsigned long long *m2_len_bytes);                              
                                                                                                                   
  // 生成第3轮消息（发起方）                                                                                       
  int kex_generate_pass3_msg_a(unsigned char *ska, unsigned long long ska_len_bytes,                               
                                unsigned char *pkb, unsigned long long pkb_len_bytes,                              
                                unsigned char *m2, unsigned long long m2_len_bytes,                                
                                unsigned char *sta, unsigned long long *sta_len_bytes,                             
                                unsigned char *m3, unsigned long long *m3_len_bytes);                              
                                                                                                                   
  // 导出共享密钥                                                                                                  
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

- 要测试功能正确性
- 测试性能，时钟周期数
- 运算吞吐量 ops/s
- 要有内存的测试想，静态内存和峰值内存
- 项目使用CMake构建系统
- 需要有稳定性测试，6小时或者随机3000组测试数据

## 约束
- 以上函数符号由其他lib提供，需要通过dlopen打开，程序需要指定测试程序的lib，linux是.so


