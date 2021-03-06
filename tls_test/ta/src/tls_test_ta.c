#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>


#include <tls_test_ta.h>

#include "certs/_.lisha.ufsc.br.pem.h"
#include "credentials/secret_credentials.h"

//#include <socket_handler.h>
//#include <tls_handler.h>
#include <connections_handler.h>
#include <my_post.h>
#include <crypto.h>
#include <serial_package.h>

#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509.h>
#include <mbedtls/debug.h>
#include <mbedtls/certs.h>
#include <mbedtls/error.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct tls_handle_t {

	struct HttpHeader_t * httpHeader;
	struct credentials_t * credentials;

	struct cipher_handle_t cipher;	

	struct connections_handle_t conn;
};

TEE_Result TA_CreateEntryPoint(void)
{
	DMSG(" tls_test");

	return CODE_SUCCESS;
}

void TA_DestroyEntryPoint(void)
{
	DMSG(" tls_test");
}

TEE_Result TA_OpenSessionEntryPoint(uint32_t param_types,
		TEE_Param __maybe_unused params[4],
		void **sess_ctx)
{
	uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE);
	if (param_types != exp_param_types)
		return TEE_ERROR_BAD_PARAMETERS;

	/* Unused parameters */
	(void)&params;

	struct tls_handle_t *tls_handle;
	
	tls_handle = TEE_Malloc(sizeof(struct tls_handle_t), 0);
	if (!tls_handle)
		return CODE_ERROR_OUT_OF_MEMORY;

	struct HttpHeader_t httpHeader = { \
        .method = POST, \
        .path = API_GET, \
        .content_type = JSON, \
        .hostname = "iot.lisha.ufsc.br", \
        .content_length = 0};

	uint32_t size = sizeof(httpHeader);
	tls_handle->httpHeader = TEE_Malloc( size, 0);
	if (!tls_handle->httpHeader)
		return CODE_ERROR_OUT_OF_MEMORY;

	memcpy(tls_handle->httpHeader, &httpHeader, size);
	
	struct credentials_t credentials = { \
		.domain = SECRET_DOMAIN, \
		.username = SECRET_USERNAME, \
		.password = SECRET_PASSWORD};

	size = sizeof(credentials);
	tls_handle->credentials = TEE_Malloc( size, 0);
	if (!tls_handle->credentials)
		return CODE_ERROR_OUT_OF_MEMORY;
	memcpy(tls_handle->credentials, &credentials, size);
	

	char key[16] = {0x99, 0xF3, 0xCC, 0xA3, 0xFC, 0xC7, 0x10, 0x76, 0xAC, 0x16,
          0x86, 0x41, 0xD9, 0x06, 0xCE, 0xB5};
	int key_size = 16;
	uint32_t algorithm = TEE_ALG_AES_CTR;

	initialize_crypto(&tls_handle->cipher, algorithm, TEE_MODE_ENCRYPT, key, key_size);

	*sess_ctx = (void *)tls_handle;
	return CODE_SUCCESS;
}

void TA_CloseSessionEntryPoint(void *sess_ctx)
{
	struct tls_handle_t *tls_handle = (struct tls_handle_t *)sess_ctx;
	
	finish_crypto(&tls_handle->cipher);

	TEE_Free(tls_handle->credentials);
	TEE_Free(tls_handle->httpHeader);

	TEE_Free(tls_handle);
}

static TEE_Result ta_tls_close(void *sess_ctx, uint32_t param_types,
	TEE_Param params[4])
{
	struct tls_handle_t *tls_handle = (struct tls_handle_t *)sess_ctx;
	
	(void)&params;
	uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE);

	if (param_types != exp_param_types)
		return TEE_ERROR_BAD_PARAMETERS;
	
	close_conections(&tls_handle->conn);

	return CODE_SUCCESS;
}


static TEE_Result ta_tls_open(void *sess_ctx, uint32_t param_types,
	TEE_Param params[4])
{
	struct tls_handle_t *tls_handle = (struct tls_handle_t *)sess_ctx;
	
	uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INPUT,
						   TEE_PARAM_TYPE_MEMREF_INPUT,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE);

	if (param_types != exp_param_types)
		return TEE_ERROR_BAD_PARAMETERS;

	return open_connections(&tls_handle->conn, params[1].memref.buffer, params[1].memref.size, params[0].value.a, lisha_ca_crt, lisha_ca_crt_len);
}

static TEE_Result ta_tls_send(void *sess_ctx, uint32_t param_types,
	TEE_Param params[4])
{
	struct tls_handle_t *tls_handle = (struct tls_handle_t *)sess_ctx;
	int ret;
	
	uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,
						   TEE_PARAM_TYPE_VALUE_OUTPUT,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE);

	if (param_types != exp_param_types)
		return TEE_ERROR_BAD_PARAMETERS;

	buffer_t encrypted_data;
	buffer_t plain_data;
	serial_header_t header;
	buffer_t iv;
	
	dismount_serial_package(&params[0], &header, &encrypted_data);
	
	iv.buffer = header.iv;
	iv.buffer_size = sizeof(header.iv);
	plain_data.buffer = TEE_Malloc(encrypted_data.buffer_size, 0);
	plain_data.buffer_size = encrypted_data.buffer_size;

	decrypt_data(&tls_handle->cipher, &iv, \
                &encrypted_data, &plain_data);

	int request_size = 512;
	buffer_t request = { .buffer = TEE_Malloc(request_size, 0), .buffer_size = request_size};

	path_t path = get_request_path_of_data_package(&plain_data);
	set_request_path_in_header(tls_handle->httpHeader, path);

	mount_request( &request, tls_handle->httpHeader, data_package_to_json, &plain_data, tls_handle->credentials);
	TEE_Free(plain_data.buffer);

	ret = send_data(&tls_handle->conn, request.buffer, request.buffer_size);
	TEE_Free(request.buffer);

	params[1].value.a = 0;

	if(ret <  0)
		return CODE_ERROR_COMMUNICATION;

	#define HTTPS_RESPONSE_BUFFER_SIZE 1024
	buffer_t response = {.buffer = TEE_Malloc(HTTPS_RESPONSE_BUFFER_SIZE, 0), .buffer_size = HTTPS_RESPONSE_BUFFER_SIZE};

	int response_size = recv_data(&tls_handle->conn, \
											response.buffer, response.buffer_size);
	if (response_size >= 0) {
		params[1].value.a = get_response_code(&response);
		while (response_size > 0)
		{
			response_size = recv_data(&tls_handle->conn, \
											response.buffer, response.buffer_size);
		}
		DMSG("\nresponse_code: %d", params[1].value.a);
	}

	if (!params[1].value.a)
		return CODE_ERROR_CANCEL;
	return CODE_SUCCESS;
}

static TEE_Result ta_tls_recv(void *sess_ctx, uint32_t param_types,
	TEE_Param params[4])
{
	struct tls_handle_t *tls_handle = (struct tls_handle_t *)sess_ctx;
	
	uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_OUTPUT,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE);

	if (param_types != exp_param_types)
		return TEE_ERROR_BAD_PARAMETERS;

	params[0].memref.size = recv_data( \
				&tls_handle->conn, \
				params[0].memref.buffer, \
				params[0].memref.size);
	
	return CODE_SUCCESS;
}

static TEE_Result test_encrypt_data(void *sess_ctx, uint32_t param_types,
	TEE_Param params[4])
{
	struct tls_handle_t *tls_handle = (struct tls_handle_t *)sess_ctx;
	
	uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_OUTPUT,
						   TEE_PARAM_TYPE_VALUE_INPUT,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE);

	if (param_types != exp_param_types)
		return TEE_ERROR_BAD_PARAMETERS;

	serial_header_t header;
	buffer_t encrypted_data;
	buffer_t plain_buffer;

	uint32_t total_size = 0;

	unsigned char iv_char[16];
	buffer_t iv = { .buffer_size = 16, .buffer = iv_char};
	gerate_iv(&iv);

	if (params[1].value.a == 0) {
		serie_t serie = { \
					.version = 17, \
					.unit = 2224179556, \
					.x = 0, \
					.y = 1, \
					.z = 2, \
					.dev = 0, \
					.r = 0, \
					.t0 = 1594080000000000, \
					.t1 = 1594291176706000 };
		create_data_package(SERIE, &plain_buffer, (void *) &serie);
	} else {
		record_t record = { \
					.version = 17, \
					.unit = 2224179556, \
					.value = 10, \
					.uncertainty = 0, \
					.x = 0, \
					.y = 1, \
					.z = 2, \
					.t = 1596751999293922, \
					.dev = 0};
		create_data_package(RECORD, &plain_buffer, (void *) &record);	
	}

	encrypted_data.buffer = TEE_Malloc(plain_buffer.buffer_size, 0);
	encrypted_data.buffer_size = plain_buffer.buffer_size;

	encrypt_data(&tls_handle->cipher, &iv, \
                &plain_buffer, &encrypted_data);
	TEE_Free(plain_buffer.buffer);

	total_size = sizeof(header) + encrypted_data.buffer_size;
	if (params[0].memref.size < total_size) {
		TEE_Free(encrypted_data.buffer);
		return CODE_ERROR_SHORT_BUFFER;
	}

	header.encrypted_size = encrypted_data.buffer_size;
	memcpy(header.iv, iv.buffer, iv.buffer_size);

	mount_serial_package(&params[0], &header, &encrypted_data);
	params[0].memref.size = total_size;

	TEE_Free(encrypted_data.buffer);
	
	return CODE_SUCCESS;
}


TEE_Result TA_InvokeCommandEntryPoint(void *sess_ctx,
			uint32_t cmd_id,
			uint32_t param_types, TEE_Param params[4])
{
	switch (cmd_id) {
	case TA_TLS_OPEN_CMD:
		return ta_tls_open(sess_ctx, param_types, params);
	case TA_TLS_CLOSE_CMD:
		return ta_tls_close(sess_ctx, param_types, params);
	case TA_TLS_SEND_CMD:
		return ta_tls_send(sess_ctx, param_types, params);
	case TA_TLS_RECV_CMD:
		return ta_tls_recv(sess_ctx, param_types, params);
	case TEST_ENCRYPT_DATA:
		return test_encrypt_data(sess_ctx, param_types, params);
	default:
		return CODE_ERROR_NOT_SUPPORTED;
	}
}