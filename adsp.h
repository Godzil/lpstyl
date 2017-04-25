/******************************************************************************
	adsp.h

	This file is part of the lpstyl package.

	Copyright (C) 1996-2000 Monroe Williams (monroe@pobox.com)
	All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions
	are met:
	1. Redistributions of source code must retain the above copyright
		notice, this list of conditions and the following disclaimer.
	2. Redistributions in binary form must reproduce the above copyright
		notice, this list of conditions and the following disclaimer in the
		documentation and/or other materials provided with the distribution.

	THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
	IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
	OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
	IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
	INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
	NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
	DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
	THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
	(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
	OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
******************************************************************************/
#include <machine/endian.h>

struct adsphdr {
    u_int16_t	src_conn_id;
	u_int32_t	first_byte_seq;
	u_int32_t	next_rcv_seq;
    u_int16_t	rcv_window;
	u_int8_t	flags;
} __attribute__((packed));

struct adsp_data
{
	char		data[572];
} __attribute__((packed));

struct adsp_attn_data
{
	u_int16_t	code;
	char		data[570];
} __attribute__((packed));

struct adsp_cntl_data
{
	u_int16_t	version;
	u_int16_t	dst_conn_id;
	u_int32_t	attn_rcv_seq;
} __attribute__((packed));


struct adsp_cntl_packet
{
	struct adsphdr hdr;
	struct adsp_cntl_data data;
} __attribute__((packed));

struct adsp_attn_packet
{
	struct adsphdr hdr;
	struct adsp_attn_data data;
} __attribute__((packed));

struct adsp_packet
{
	struct adsphdr hdr;
	char data[572];
} __attribute__((packed));

#ifndef DDPTYPE_ADSP
	#define DDPTYPE_ADSP 7
#endif

enum
{
	ADSP_VERSION = 0x0100,
	SZ_ADSPHDR = 13,

	ADSPOP_MASK			= 0x0F,
	ADSPOP_PROBE_ACK	= 0x00,
	ADSPOP_OPEN_REQ		= 0x01,
	ADSPOP_OPEN_ACK		= 0x02,
	ADSPOP_OPEN_REQ_ACK	= 0x03,
	ADSPOP_OPEN_NAK		= 0x04,
	ADSPOP_CLOSE		= 0x05,
	ADSPOP_RESET_REQ	= 0x06,
	ADSPOP_RESET_ACK	= 0x07,
	ADSPOP_RETRANS		= 0x08,

	ADSPFLAG_CONTROL	= 0x80,
	ADSPFLAG_ACK		= 0x40,
	ADSPFLAG_EOM		= 0x20,
	ADSPFLAG_ATTN		= 0x10,

	ADSP_RECV_BUFFER_SIZE	= 2048,
	ADSP_ATTN_BUFFER_SIZE	= 570,
	ADSP_MAX_DATA_SIZE		= 572,

	ADSP_STATE_CLOSED		= 0,
	ADSP_STATE_OPEN_SENT,
	ADSP_STATE_OPEN_RCVD,
	ADSP_STATE_OPEN,
	ADSP_STATE_HALF_CLOSED

};

/* State for an ADSP socket. */
struct adsp_socket
{
	int					socket;
	struct sockaddr_at	local_addr;
	u_int16_t			lastConnID;
};

/* Endpoint state for an ADSP connection. */
struct adsp_endp
{
	/* connection state */
	struct adsp_socket	*local_socket;
	struct sockaddr_at	remote_addr;
	u_int16_t			connID;
	u_int16_t			remote_connID;
	u_int16_t			state;
	
	/* sequencing variables */
	u_int32_t	send_seq;		/* next byte to transmit */
	u_int32_t	oldest_seq;		/* oldest byte in local send queue */
	u_int32_t	rmt_window_seq;	/* last byte of remote window */
	u_int32_t	recv_seq;		/* next byte to receive */
	u_int32_t	recv_window;	/* local window size */

	/* receive ring buffer */
	char	recv_buffer[ADSP_RECV_BUFFER_SIZE];
	int		recv_next_input;
	int		recv_next_output;

	/* attention message handling */
	u_int32_t	attn_send_seq;	/* next outgoing sequence number */
	u_int32_t	attn_recv_seq;	/* next incoming sequence number */
	int			attn_valid;		/* true if an attention message is pending. */
	char		attn_buffer[ADSP_ATTN_BUFFER_SIZE];
	int			attn_size;		/* length of the message in the buffer */
	u_int16_t	attn_code;		/* code on this message */
};


int adsp_open_socket(struct adsp_socket *s);
int adsp_close_socket(struct adsp_socket *s);

int adsp_connect(	struct adsp_socket *s, 
					struct adsp_endp *endp,
					char *name);

int adsp_disconnect(struct adsp_endp *endp);

int adsp_listen(struct adsp_socket *s,
				struct adsp_endp *endp);
int adsp_accept(struct adsp_endp *endp);

int adsp_idle(struct adsp_endp *endp);

int adsp_read(struct adsp_endp *endp, char *data, int len);
int adsp_read_nonblock(struct adsp_endp *endp, char *data, int len);
int adsp_write(struct adsp_endp *endp, char *data, int len);

/* There must be at least ADSP_RECV_BUFFER_SIZE bytes at *data to receive
	the message.
*/
int adsp_read_attn(struct adsp_endp *endp, u_int16_t *code, char *data);

int adsp_write_attn(struct adsp_endp *endp, 
					u_int16_t code, 
					char *data, 
					int len);

int adsp_fwd_reset(struct adsp_endp *endp);
