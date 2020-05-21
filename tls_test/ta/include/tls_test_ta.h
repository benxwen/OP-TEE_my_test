#ifndef TA_TLS_TEST_H
#define TA_TLS_TEST_H


/*
 * This UUID is generated with uuidgen
 * the ITU-T UUID generator at http://www.itu.int/ITU-T/asn1/uuid.html
 */
#define TA_TLS_TEST_UUID \
	{ 0xe7d490f5, 0x0b64, 0x4474, \
		{ 0x8b, 0x3b, 0xc0, 0xed, 0x7f, 0x37, 0x19, 0xd3} }
	
/* The function IDs implemented in this TA */

/*
 * TA_TLS_OPEN_CMD
 * [INPUT] 	param[0].value.a - port
 * [INPUT] 	param[1] (memref) - server address
 * 			param[2] unused
 * 			param[3] unused
 */
#define TA_TLS_OPEN_CMD 0

/*
 * TA_TLS_CLOSE_CMD
 * param[0] unused
 * param[1] unused
 * param[2] unused
 * param[3] unused
 */
#define TA_TLS_CLOSE_CMD 1

/*
 * TA_TLS_SEND_CMD
 * [INPUT] 	param[0] (memref) - message
 * [OUTPUT] param[1].value.a - bytes sent
 * 			param[2] unused
 * 			param[3] unused
 */
#define TA_TLS_SEND_CMD 2

/*
 * TA_TLS_RECV_CMD
 * [OUTPUT] param[0] (memref) - message
 * 			param[1] unused
 * 			param[2] unused
 * 			param[3] unused
 */
#define TA_TLS_RECV_CMD 3

#endif /*TA_TLS_TEST_H*/