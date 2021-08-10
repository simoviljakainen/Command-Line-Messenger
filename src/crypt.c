#include <arpa/inet.h>
#include <inc/crypt.h>

void AES_256_GCM_128_encrypt(
    gcry_cipher_hd_t *aes256_gcm_handle, char *msg_in,
    size_t msg_len, Enc_msg *enc_msg);

int AES_256_GCM_128_decrypt(
    gcry_cipher_hd_t *aes256_gcm_handle, char *msg_out,
    size_t msg_len, Enc_msg *enc_msg);

void handle_libgcrypt_error(gcry_error_t err, char *file, int line) {
  fprintf(
      stderr,
      "LIBGCRYPT_ERROR %s:%d -- %s : %s\n",
      file, line, gcry_strsource(err), gcry_strerror(err));
  exit(EXIT_FAILURE);

  return;
}

void init_libgcrypt(void) {
  if (!gcry_check_version(MIN_LIBGCRYPT_VERSION)) {
    printf(
        "You are using old version of libgcrypt (%s)."
        "Required version is %s\n",
        gcry_check_version(NULL), MIN_LIBGCRYPT_VERSION);
    exit(EXIT_FAILURE);
  }

  gcry_control(GCRYCTL_USE_SECURE_RNDPOOL);

  gcry_control(GCRYCTL_SUSPEND_SECMEM_WARN);
  gcry_control(GCRYCTL_INIT_SECMEM, SEC_MEM_KIB, 0);
  gcry_control(GCRYCTL_RESUME_SECMEM_WARN);

  gcry_control(GCRYCTL_INITIALIZATION_FINISHED, 0);

  gcry_error_t err = GPG_ERR_NO_ERROR;

  if ((err = gcry_control(GCRYCTL_SELFTEST))) {
    HANDLE_LIBGCRYPT_ERROR(err);
  }

  return;
}

void init_AES_256_cipher(gcry_cipher_hd_t *aes256_gcm_handle) {
  gcry_error_t err = GPG_ERR_NO_ERROR;

  if ((err = gcry_cipher_open(aes256_gcm_handle,
                              GCRY_CIPHER_AES256, GCRY_CIPHER_MODE_GCM, GCRY_CIPHER_SECURE))) {
    HANDLE_LIBGCRYPT_ERROR(err);
  }
  /*
	void *key_256 = gcry_random_bytes_secure(
		BYTES_IN_256, GCRY_VERY_STRONG_RANDOM
	);*/

  char *key_256 = "1234567890123456789123123123123";  // fixed for testing

  if ((err = gcry_cipher_setkey(*aes256_gcm_handle, key_256, BYTES_IN_256))) {
    HANDLE_LIBGCRYPT_ERROR(err);
  }

  return;
}

void AES_256_GCM_128_encrypt(
    gcry_cipher_hd_t *aes256_gcm_handle, char *msg_in, size_t msg_len, Enc_msg *enc_msg) {
  gcry_error_t err = GPG_ERR_NO_ERROR;

  gcry_create_nonce(enc_msg->nonce, IV_BYTES);

  if ((err = gcry_cipher_setiv(
           *aes256_gcm_handle, enc_msg->nonce, IV_BYTES))) {
    HANDLE_LIBGCRYPT_ERROR(err);
  }

  if ((err = gcry_cipher_authenticate(
           *aes256_gcm_handle, enc_msg->aad, AAD_BYTES))) {
    HANDLE_LIBGCRYPT_ERROR(err);
  }

  if ((enc_msg->cipher_text = malloc(msg_len)) == NULL) {
    HANDLE_ERROR("Failed to allocate memory for ciphertext", 1);
  }

  if ((err = gcry_cipher_encrypt(
           *aes256_gcm_handle, enc_msg->cipher_text, msg_len, msg_in, msg_len))) {
    HANDLE_LIBGCRYPT_ERROR(err);
  }

  if ((err = gcry_cipher_gettag(*aes256_gcm_handle, enc_msg->tag, TAG_BYTES))) {
    HANDLE_LIBGCRYPT_ERROR(err);
  }

  return;
}

int AES_256_GCM_128_decrypt(
    gcry_cipher_hd_t *aes256_gcm_handle, char *msg_out, size_t msg_len, Enc_msg *enc_msg) {
  gcry_error_t err = GPG_ERR_NO_ERROR;

  if ((err = gcry_cipher_setiv(
           *aes256_gcm_handle, enc_msg->nonce, IV_BYTES))) {
    HANDLE_LIBGCRYPT_ERROR(err);
  }

  if ((err = gcry_cipher_authenticate(
           *aes256_gcm_handle, enc_msg->aad, AAD_BYTES))) {
    HANDLE_LIBGCRYPT_ERROR(err);
  }

  if ((err = gcry_cipher_decrypt(
           *aes256_gcm_handle, msg_out, msg_len, enc_msg->cipher_text, msg_len))) {
    HANDLE_LIBGCRYPT_ERROR(err);
  }

  if ((err = gcry_cipher_checktag(*aes256_gcm_handle, enc_msg->tag, TAG_BYTES))) {
    return 1;
  }

  return 0;
}

char *encrypt_packet(
    char *packet, uint16_t size, int *new_size, gcry_cipher_hd_t *aes_gcm, uint16_t ctr) {
  Enc_msg enc_msg;

  uint16_t n_ctr = htons(ctr);
  uint16_t n_size = htons(size);

  memcpy(enc_msg.aad, &n_ctr, CTR_BYTES);
  memcpy(enc_msg.aad + CTR_BYTES, &n_size, SIZE_BYTES);

  char *enc_packet;

  if ((enc_packet = (char *)malloc(PACKET_MAX_BYTES)) == NULL) {
    HANDLE_ERROR("Failed to allocate memory for a packet", 1);
  }

  int offset = 0;

  memcpy(enc_packet, enc_msg.aad, AAD_BYTES);
  offset += AAD_BYTES;

  AES_256_GCM_128_encrypt(aes_gcm, packet, size, &enc_msg);

  memcpy(enc_packet + offset, enc_msg.nonce, IV_BYTES);
  offset += IV_BYTES;

  memcpy(enc_packet + offset, enc_msg.tag, TAG_BYTES);
  offset += TAG_BYTES;

  memcpy(enc_packet + offset, enc_msg.cipher_text, size);

  *new_size = offset + size;

  clean_enc_msg(&enc_msg);

  return enc_packet;
}

char *decrypt_packet(char *packet, gcry_cipher_hd_t *aes_gcm, uint16_t cur_ctr) {
  Enc_msg enc_msg;

  int offset = 0;
  uint16_t size, ctr;

  memcpy(&ctr, packet, CTR_BYTES);
  memcpy(enc_msg.aad, &ctr, CTR_BYTES);
  ctr = ntohs(ctr);
  offset += CTR_BYTES;

  if (ctr != (cur_ctr + 1))
    return NULL;  //rejected, possibly a replay attack

  memcpy(&size, packet + offset, SIZE_BYTES);
  memcpy(enc_msg.aad + CTR_BYTES, &size, SIZE_BYTES);
  size = ntohs(size);
  offset += SIZE_BYTES;

  if (size > MAX_MSG_SIZE)
    return NULL;  //rejected, message too long

  memcpy(enc_msg.nonce, packet + offset, IV_BYTES);
  offset += IV_BYTES;

  memcpy(enc_msg.tag, packet + offset, TAG_BYTES);
  offset += TAG_BYTES;

  if ((enc_msg.cipher_text = malloc(size)) == NULL) {
    HANDLE_ERROR("Failed to allocate memory for a packet", 1);
  }

  memcpy(enc_msg.cipher_text, packet + offset, size);

  char *msg_out;
  if ((msg_out = (char *)malloc(size)) == NULL) {
    HANDLE_ERROR("Failed to allocate memory for a packet", 1);
  }

  if (AES_256_GCM_128_decrypt(aes_gcm, msg_out, size, &enc_msg)) {
    clean_enc_msg(&enc_msg);
    return NULL;  //rejected, the tag does not match
  }

  clean_enc_msg(&enc_msg);

  return msg_out;
}

void clean_cipher(gcry_cipher_hd_t *aes256_gcm_handle) {
  gcry_cipher_close(*aes256_gcm_handle);

  return;
}

void clean_enc_msg(Enc_msg *enc_msg) {
  free(enc_msg->cipher_text);
  return;
}
