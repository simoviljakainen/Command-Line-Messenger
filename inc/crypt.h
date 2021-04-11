#ifndef CRYPT_H
    #define CRYPT_H

    #include <inc/setting.h>
    #include <gcrypt.h>

    typedef struct _enc_msg{
        void *cipher_text;
        uint8_t aad[AAD_BYTES];
        uint8_t nonce[IV_BYTES];
        uint8_t tag[TAG_BYTES];
    }Enc_msg;

    void init_libgcrypt(void);
    void init_AES_256_cipher(gcry_cipher_hd_t *aes256_gcm_handle);

    char *encrypt_packet(
        char *packet, uint16_t size, int *new_size,
        gcry_cipher_hd_t *aes_gcm, uint16_t ctr
    );	

    char *decrypt_packet(
        char *packet, gcry_cipher_hd_t *aes_gcm, uint16_t cur_ctr);
	
    void clean_cipher(gcry_cipher_hd_t *aes256_gcm_handle);
    void clean_enc_msg(Enc_msg *enc_msg);

    #define HANDLE_LIBGCRYPT_ERROR(err) handle_libgcrypt_error(err, __FILE__, __LINE__);

#endif
