
/* TODO put headers in some other header file */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sodium.h>
#include <gcrypt.h>

#include <inc/message.h>
#include <inc/socket_utilities.h>
#include "window_manager.h"

#define MIN_LIBGCRYPT_VERSION "1.9.2"
#define BYTES_IN_256 32
#define IV_BYTES 12
#define TAG_BYTES 16
#define SEC_MEM_KIB 16384

#define CTR_BYTES 2
#define SIZE_BYTES 2
#define AAD_BYTES CTR_BYTES + SIZE_BYTES
#define HEADER_BYTES AAD_BYTES + IV_BYTES + TAG_BYTES
#define PACKET_MAX_BYTES HEADER_BYTES + MAX_MSG_SIZE

typedef struct _enc_msg{
	uint8_t aad[AAD_BYTES];
	void *cipher_text;
	uint8_t nonce[IV_BYTES];
	uint8_t tag[TAG_BYTES];
}Enc_msg;

void handle_libgcrypt_error(gcry_error_t err, char *file, int line){
	fprintf(
		stderr,
		"LIBGCRYPT_ERROR %s:%d -- %s : %s\n",
		file, line, gcry_strsource(err), gcry_strerror (err)
	);
	exit(EXIT_FAILURE);

	return;
}

#include <errno.h>

void handle_error(char *msg, int show_err, char *file, int line){
	fprintf(
		stderr,
		"ERROR(%d) %s:%d -- %s:%s\n",
		errno,
		file, line,
        (msg != NULL) ? msg : "-",
		(show_err) ? strerror(errno): "-"
	);
	exit(EXIT_FAILURE);

	return;
}

#define HANDLE_ERROR(msg, show_err) handle_error(msg, show_err, __FILE__, __LINE__);


#define HANDLE_LIBGCRYPT_ERROR(err) handle_libgcrypt_error(err, __FILE__, __LINE__);

void init_libgcrypt(void){

	if(!gcry_check_version(MIN_LIBGCRYPT_VERSION)){
		printf(
			"You are using old version of libgcrypt (%s)." 
			"Required version is %s\n",
			gcry_check_version(NULL), MIN_LIBGCRYPT_VERSION
		);
		exit(EXIT_FAILURE);
	}

	gcry_control(GCRYCTL_USE_SECURE_RNDPOOL);

	gcry_control(GCRYCTL_SUSPEND_SECMEM_WARN);
	gcry_control(GCRYCTL_INIT_SECMEM, SEC_MEM_KIB, 0);
	gcry_control(GCRYCTL_RESUME_SECMEM_WARN);

	gcry_control(GCRYCTL_INITIALIZATION_FINISHED, 0);

	gcry_error_t err = GPG_ERR_NO_ERROR;

	if((err = gcry_control(GCRYCTL_SELFTEST))){
		HANDLE_LIBGCRYPT_ERROR(err);
	}

	return;
}

void init_AES_256_cipher(gcry_cipher_hd_t *aes256_gcm_handle){
	gcry_error_t err = GPG_ERR_NO_ERROR;

	if((err = gcry_cipher_open(aes256_gcm_handle,
		GCRY_CIPHER_AES256, GCRY_CIPHER_MODE_GCM, GCRY_CIPHER_SECURE))){

		HANDLE_LIBGCRYPT_ERROR(err);
	}

	void *key_256 = gcry_random_bytes_secure(
		BYTES_IN_256, GCRY_VERY_STRONG_RANDOM
	);

	if((err = gcry_cipher_setkey(*aes256_gcm_handle, key_256, BYTES_IN_256))){
		HANDLE_LIBGCRYPT_ERROR(err);
	}

	return;
}

void clean_cipher(gcry_cipher_hd_t *aes256_gcm_handle){
	gcry_cipher_close(*aes256_gcm_handle);

	return;
}

void AES_256_GCM_128_encrypt(
	gcry_cipher_hd_t *aes256_gcm_handle, char *msg_in, size_t msg_len, Enc_msg *enc_msg){

	gcry_error_t err = GPG_ERR_NO_ERROR;

	gcry_create_nonce(enc_msg->nonce, IV_BYTES);
	
	if((err = gcry_cipher_setiv(
		*aes256_gcm_handle, enc_msg->nonce, IV_BYTES))){

		HANDLE_LIBGCRYPT_ERROR(err);
	}

	if((err = gcry_cipher_authenticate(
		*aes256_gcm_handle, enc_msg->aad, AAD_BYTES))){

		HANDLE_LIBGCRYPT_ERROR(err);
	}

	if((enc_msg->cipher_text = malloc(msg_len)) == NULL){
		HANDLE_ERROR("Failed to allocate memory for ciphertext", 1);
	}

	if((err = gcry_cipher_encrypt(
		*aes256_gcm_handle, enc_msg->cipher_text, msg_len, msg_in, msg_len))){

		HANDLE_LIBGCRYPT_ERROR(err);
	}

	if((err = gcry_cipher_gettag(*aes256_gcm_handle, enc_msg->tag, TAG_BYTES))){
		HANDLE_LIBGCRYPT_ERROR(err);
	}

	return;
}

int AES_256_GCM_128_decrypt(
	gcry_cipher_hd_t *aes256_gcm_handle, char *msg_out, size_t msg_len, Enc_msg *enc_msg){

	gcry_error_t err = GPG_ERR_NO_ERROR;
	
	if((err = gcry_cipher_setiv(
		*aes256_gcm_handle, enc_msg->nonce, IV_BYTES))){
			
		HANDLE_LIBGCRYPT_ERROR(err);
	}

	if((err = gcry_cipher_authenticate(
		*aes256_gcm_handle, enc_msg->aad, AAD_BYTES))){

		HANDLE_LIBGCRYPT_ERROR(err);
	}

	if((err = gcry_cipher_decrypt(
		*aes256_gcm_handle, msg_out, msg_len, enc_msg->cipher_text, msg_len))){
		
		HANDLE_LIBGCRYPT_ERROR(err);
	}

	if((err = gcry_cipher_checktag(*aes256_gcm_handle, enc_msg->tag, TAG_BYTES))){
		return 1;
	}

	return 0;
}

void clean_enc_msg(Enc_msg *enc_msg){
	free(enc_msg->cipher_text);
	return;
}

char *encrypt_packet(
	char *packet, size_t size, int *new_size, gcry_cipher_hd_t *aes_gcm, uint16_t ctr){
	
	Enc_msg enc_msg;

	uint16_t n_ctr = htons(ctr);
	uint16_t n_size = htons(size);

	memcpy(enc_msg.aad, &n_ctr, CTR_BYTES);
	memcpy(enc_msg.aad + CTR_BYTES, &n_size, SIZE_BYTES);
	
	char *enc_packet;

    if((enc_packet = (char *)malloc(PACKET_MAX_BYTES)) == NULL){
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
#define MIN_MSG_LEN 2
#define MIN_PACKET_SIZE HEADER_BYTES + MAX_USERNAME_LEN + ID_SIZE + MIN_MSG_LEN

char *decrypt_packet(char *packet, gcry_cipher_hd_t *aes_gcm, uint16_t *cur_ctr){
	Enc_msg enc_msg;

	int offset = 0;
	uint16_t size, ctr;

	memcpy(&ctr, packet, CTR_BYTES);
	memcpy(enc_msg.aad, &ctr, CTR_BYTES);
	ctr = ntohs(ctr);
	offset += CTR_BYTES;

	if(ctr != (*cur_ctr + 1))
		return NULL; //rejected, possibly a replay attack

	memcpy(&size, packet + offset, SIZE_BYTES);
	memcpy(enc_msg.aad + CTR_BYTES, &size, SIZE_BYTES);
	size = ntohs(size);
	offset += SIZE_BYTES;

	if(size > MAX_MSG_SIZE)
		return NULL; //rejected, message too long

    memcpy(enc_msg.nonce, packet + offset, IV_BYTES);
	offset += IV_BYTES;

	memcpy(enc_msg.tag, packet + offset, TAG_BYTES);
	offset += TAG_BYTES;

	if((enc_msg.cipher_text = malloc(size)) == NULL){
        HANDLE_ERROR("Failed to allocate memory for a packet", 1);
    }

	memcpy(enc_msg.cipher_text, packet + offset, size);

	char *msg_out;
	if((msg_out = (char *)malloc(size)) == NULL){
        HANDLE_ERROR("Failed to allocate memory for a packet", 1);
    }

	if(AES_256_GCM_128_decrypt(aes_gcm, msg_out, size, &enc_msg)){
		clean_enc_msg(&enc_msg);
		return NULL; //rejected, the tag does not match
	}

	clean_enc_msg(&enc_msg);

	return msg_out;
}

int main(int argc, char *argv[]){

	init_libgcrypt();

	gcry_cipher_hd_t aes256_gcm_handle;
	init_AES_256_cipher(&aes256_gcm_handle);

	char str[] = "Haista vitut hopmodfsaj jhdfgshjkdslhjk lagsdfhhjslfghlsj ddfg g fdslhjkdfghlk fgdlhjkfdghjlsdfsglhjkdfglj";

	int size, new_size;
	uint16_t ctr = 0;
	Msg msg = compose_message(str, "01", "Mikko");
	char *packet = message_to_ascii_packet(&msg, &size);
	size = 0;
	char *enc_packet = encrypt_packet(packet, size, &new_size, &aes256_gcm_handle, ctr+1);
	free(packet);

	char *clear_packet;
	if((clear_packet = decrypt_packet(enc_packet, &aes256_gcm_handle, &ctr)) == NULL){
		HANDLE_ERROR("Message malformed.", 0);
	}

	Msg clr = ascii_packet_to_message(clear_packet);

	printf("%d name: %s, id: %s, msg: %s\n", ctr, clr.username, clr.id, clr.msg);

	free(enc_packet); free(clear_packet);
	clean_cipher(&aes256_gcm_handle);
	
	gcry_control(GCRYCTL_TERM_SECMEM);

#if 0
	pthread_t window_thread;

	init_g(&read_head, &read_tail);

	/* Open window manager*/
	pthread_create(&window_thread, NULL, run_ncurses_window, NULL);
	pthread_join(window_thread, NULL);

	char buffer[256];
	while(1){
		fgets(buffer, 256, stdin);
		buffer[strlen(buffer)-1] = '\0';

		if(!strcmp(buffer, "stop"))
			break;

		add_message_to_queue(buffer, &read_head, &read_tail);
	}
	/*
	Msg *ptr = read_head;
	while(ptr != NULL){
		printf("%s\n", ptr->msg);
		ptr = ptr->next;
	}*/
#endif


    return 0;
}