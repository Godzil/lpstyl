/*****************************************************************************
	adsp.c

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
*****************************************************************************/
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netatalk/at.h>
#include <atalk/nbp.h>
#include <string.h>
#include <termios.h>
#include <fcntl.h>
#include <errno.h>

#include "adsp.h"

/* XXX -- For debugging only? */
#include <stdio.h>
#include <signal.h>

/* local prototypes */
int adsp_send_control(struct adsp_endp *endp, int flags);

int adsp_send_data(struct adsp_endp *endp,
						int flags,
						char *data,
						int len);

int adsp_send_attn(struct adsp_endp *endp,
						int flags,
						u_int16_t code,
						char *data,
						int len);

int adsp_recv_packet(struct adsp_endp *endp, int block);

int adsp_process_packet(char *pkt, 
					int pktsize, 
					struct adsp_endp *endp);

int adsp_setup_endp(struct adsp_socket *s,
					struct adsp_endp *endp,
					struct sockaddr_at *addr);

/* This is in libatalk (in nbp_util.c), but it doesn't look like the
	prototype is in any header files.
*/
int nbp_name(char *name, char **objp, char **typep, char **zonep);

int adsp_open_socket(struct adsp_socket *s)
{
	struct sockaddr_at addr;

	/* In case we fail */
	if((s->socket = socket(AF_APPLETALK, SOCK_DGRAM, 0)) < 0)
	{
		return(s->socket);
	}

	bzero(&addr, sizeof(struct sockaddr_at));
	addr.sat_len = sizeof(struct sockaddr_at);
	addr.sat_family = AF_APPLETALK;
	addr.sat_addr.s_net = ATADDR_ANYNET;
	addr.sat_addr.s_node = ATADDR_ANYNODE;
	addr.sat_port = ATADDR_ANYPORT;

	if(bind(s->socket, (struct sockaddr*)&addr, addr.sat_len) < 0)
	{
		s->socket = -1;
		return(-1);
	}

	{
		int len = sizeof(struct sockaddr_at);
		if(getsockname(s->socket, (struct sockaddr*)&addr, &len) < 0)
		{
			s->socket = -1;
			return(-1);
		}
	}

	bcopy(&addr, &(s->local_addr), sizeof(struct sockaddr_at));

	/* XXX -- This could be done a lot better. */
	s->lastConnID = getpid();
	return(0);
}

int adsp_close_socket(struct adsp_socket *s)
{
	close(s->socket);
	return(0);
}

int adsp_recv_packet(struct adsp_endp *endp, int block)
{
	char pkt[1024];
	size_t len;
	struct sockaddr_at from;
	int fromlen;
	int result = 0;
	int retries = 0;
	int rcv_result;


	/* Set a the receive timeout on the socket. */
	{
		struct timeval tv;
		if(block == 1)
		{
			/* Normal timeout will be 30 seconds */
			tv.tv_sec = 30;
			tv.tv_usec = 0;
		}
		else if(block == 2)
		{
			/* Special case for attn retries -- 5 seconds, don't transmit
				keepalives.
			*/
			tv.tv_sec = 5;
			tv.tv_usec = 0;
		}
		else
		{
			/* A timeout of 0.1 second is close enough to non-blocking
				for our purposes. 
			*/
			tv.tv_sec = 0;
			tv.tv_usec = 100000;
		}
		if(setsockopt(endp->local_socket->socket, 
				SOL_SOCKET, 
				SO_RCVTIMEO, 
				&tv,
				sizeof(struct timeval)) < 0)
		{
			perror("setsockopt");
		}
	}

	while(1)
	{
		errno = 0;
		fromlen = sizeof(struct sockaddr_at);
		len = 1024;
		bcopy(&(endp->remote_addr), &from, fromlen);
		rcv_result = recvfrom(endp->local_socket->socket,
				pkt,
				len,
				0,
				(struct sockaddr*)&(from),
				&fromlen);

		if(rcv_result == -1)
		{
			if(errno == EWOULDBLOCK)
			{
				/* The timer expired. */
				if(block == 1)
				{
					/* Blocking.  Send a keepalive and retry. */
					switch(endp->state)
					{
						case ADSP_STATE_CLOSED:
							/* There are times when we don't want to retry. */
							return(-1);
						break;
					}

					if(retries < 4)
					{
						/* Send an echo request packet and try again. */
						adsp_send_control(endp, 
								ADSPOP_PROBE_ACK | ADSPFLAG_ACK);
						retries++;
						continue;
					}
					else
					{
						/* That's all the retries.  The connection is dead. */
						adsp_send_control(endp, ADSPOP_CLOSE);
						endp->state = ADSP_STATE_CLOSED;
							return(-1);
					}
				}
				else
				{
					/* Non-blocking, no packet pending. */
					return(0);
				}
			}
			else
			{
				/* Unknown error.  Bail. */
				perror("recvfrom");
				return(-1);
			}
		}
		else
		{
			len = rcv_result;
			break;
		}
	}

	if(pkt[0] != DDPTYPE_ADSP)
		result = -1;

	if(result == 0)
	{
		bcopy(&from, &(endp->remote_addr), sizeof(struct sockaddr_at));
		result = adsp_process_packet(pkt+1, len-1, endp);
	}

	return(result);
}

int adsp_process_packet(char *pkt, 
					int pktsize, 
					struct adsp_endp *endp)
{
	int result = 1;
	struct adsphdr *hdr = (struct adsphdr*)pkt;
	char *data = pkt + SZ_ADSPHDR;
	int len = pktsize - SZ_ADSPHDR;
	int needsAck = 0;
	int op = hdr->flags & ADSPOP_MASK;
	int32_t temp;

	/* convert the header from network to host byte order. */
	hdr->src_conn_id = ntohs(hdr->src_conn_id);
	hdr->first_byte_seq = ntohl(hdr->first_byte_seq);
	hdr->next_rcv_seq = ntohl(hdr->next_rcv_seq);
	hdr->rcv_window = ntohs(hdr->rcv_window);

	if(hdr->flags & ADSPFLAG_ACK)
		needsAck = 1;

	if(hdr->flags & ADSPFLAG_ATTN)
	{
		struct adsp_attn_data *attn = (struct adsp_attn_data *)data;
		attn->code = ntohs(attn->code);

		/* attention packet */
		if(endp->attn_send_seq + 1 == hdr->next_rcv_seq)
		{
			/* Update our state. */
			endp->attn_send_seq += 1;
		}

		if(hdr->flags & ADSPFLAG_CONTROL)
		{
			/* This is an attention ack. */
		}
		else
		{
			/* This is an attention data packet. */
			if((hdr->first_byte_seq == endp->attn_recv_seq) && 
					!endp->attn_valid)
			{
				/* This is the right sequence number, and we have room to
					store it.
				*/
				endp->attn_valid = 1;
				endp->attn_code = attn->code;
				endp->attn_size = len - 2;
				bcopy(&(attn->data), endp->attn_buffer, endp->attn_size);

				/* For next time... */
				endp->attn_recv_seq++;

				/* Send an ack. */
				adsp_send_attn(endp, ADSPFLAG_CONTROL, attn->code, NULL, 0);
			}
			else if(hdr->first_byte_seq + 1 == endp->attn_recv_seq)
			{
				/* The ack got lost and they retransmitted.  
					Send another ack. 
				*/
				adsp_send_attn(endp, ADSPFLAG_CONTROL, attn->code, NULL, 0);
			}
			else
			{
				/* Bad sequence number or no room for attention msg.
					This packet will drop.
				*/
			}
		}
	}
	else if(hdr->flags & ADSPFLAG_CONTROL)
	{
		struct adsp_cntl_data *cntl = (struct adsp_cntl_data *)data;
		cntl->version = ntohs(cntl->version);
		cntl->dst_conn_id = ntohs(cntl->dst_conn_id);
		cntl->attn_rcv_seq = ntohl(cntl->attn_rcv_seq);

		/* control packet */
		endp->oldest_seq = hdr->next_rcv_seq;
		endp->rmt_window_seq = hdr->next_rcv_seq + hdr->rcv_window - 1;

		switch(op)
		{
			case ADSPOP_OPEN_REQ:
			case ADSPOP_OPEN_REQ_ACK:
			case ADSPOP_OPEN_ACK:
				/* Handshaking is going well. */
				endp->remote_connID = hdr->src_conn_id;
				endp->send_seq = hdr->next_rcv_seq;
				endp->attn_recv_seq = cntl->attn_rcv_seq;

				/* This changes the connection state. */
				if(op == ADSPOP_OPEN_REQ)
					endp->state = ADSP_STATE_OPEN_RCVD;
				else
					endp->state = ADSP_STATE_OPEN;

				if(op == ADSPOP_OPEN_REQ_ACK)
				{
					adsp_send_control(endp, ADSPOP_OPEN_ACK);
					needsAck = 0;
				}
#if 0
				/* This is handled from inside adsp_accept(). */
				else if(op == ADSPOP_OPEN_REQ)
				{
					adsp_send_control(endp, ADSPOP_OPEN_REQ_ACK);
					needsAck = 0;
				}
#endif
			break;

			case ADSPOP_PROBE_ACK:
				endp->oldest_seq = hdr->next_rcv_seq;
				endp->rmt_window_seq = hdr->next_rcv_seq + hdr->rcv_window - 1;
			break;

			case ADSPOP_RESET_REQ:
				temp = hdr->first_byte_seq - endp->recv_seq;
				if((temp >= 0) && (temp <= endp->recv_window))
				{
					/* This is a legal forward reset.  Do it. */
					endp->recv_seq = hdr->first_byte_seq;
					endp->recv_next_input = endp->recv_next_output;
					endp->recv_window = ADSP_RECV_BUFFER_SIZE;
				}

				/* Ack regardless. */
				adsp_send_control(endp, ADSPOP_RESET_ACK);
				needsAck = 0;
			break;

			case ADSPOP_RESET_ACK:
				/* XXX -- something should happen here. */
			break;

			case ADSPOP_RETRANS:
				/* Slow down, we lost someone... */
				endp->send_seq = hdr->next_rcv_seq;
				endp->rmt_window_seq = hdr->next_rcv_seq + hdr->rcv_window - 1;
			break;

			/* XXX -- handle all the cases here... */
			default:
				endp->state = ADSP_STATE_CLOSED;
			break;

		}
	}
	else
	{
		/* data packet */
		endp->oldest_seq = hdr->next_rcv_seq;
		endp->rmt_window_seq = hdr->next_rcv_seq + hdr->rcv_window - 1;

		if(endp->recv_seq == hdr->first_byte_seq)
		{
			if(len <= endp->recv_window)
			{
				if(len > 0)
				{
					int i;
					/* This packet contains data.  
						pump it into the ring buffer.  
					*/
					for(i=0; i < len; i++)
					{
						endp->recv_buffer[endp->recv_next_input++] = data[i];
						endp->recv_next_input %= ADSP_RECV_BUFFER_SIZE;
					}
					endp->recv_window -= len;
					endp->recv_seq += len;

					needsAck = 1;
				}

				if(hdr->flags & ADSPFLAG_EOM)
				{
					/* The EOM consumes a sequence number. */
					endp->recv_seq++;

					/* It also means we should push the 
						results to the client. 
					*/
					result = 2;

					needsAck = 1;
				}

			}
			else
			{
				/* The input packet exceeded our window.  Drop it. */
			}
		}
		else
		{
			/* out-of-sequence packet.  Drop it on the floor. */
			/* adsp_send_control(endp, ADSPOP_RETRANS | ADSPFLAG_ACK); */
		}
	}


	if(needsAck)
	{
		/* Sender requested an ack, and one hasn't been sent yet. */
		adsp_send_control(endp, ADSPOP_PROBE_ACK);
	}

	return(result);
}


int adsp_send_data(struct adsp_endp *endp,
						int flags,
						char *data,
						int len)
{
	struct
	{
		char ddptype;
		struct adsp_packet pkt;
	} __attribute__((packed)) p;
 
	p.ddptype = DDPTYPE_ADSP;
	p.pkt.hdr.src_conn_id = htons(endp->connID);
	p.pkt.hdr.first_byte_seq = htonl(endp->send_seq);
	p.pkt.hdr.next_rcv_seq = htonl(endp->recv_seq);
	p.pkt.hdr.rcv_window = htons(endp->recv_window);
	p.pkt.hdr.flags = flags;

	if(data != NULL && len != 0)
		bcopy(data, &(p.pkt.data), len);

	if(sendto(endp->local_socket->socket, 
			  &p, 
			  1 + sizeof(struct adsphdr) + len,
			  0,
			  (struct sockaddr*)&(endp->remote_addr),
			  sizeof(struct sockaddr_at)) < 0)
	{
		return(-1);
	}

	endp->send_seq += len;
	if(flags & ADSPFLAG_EOM)
		endp->send_seq += 1;

	return(0);
}

int adsp_send_attn(struct adsp_endp *endp,
						int flags,
						u_int16_t code,
						char *data,
						int len)
{
	struct
	{
		char ddptype;
		struct adsp_attn_packet pkt;
	} p;
 
	p.ddptype = DDPTYPE_ADSP;
	p.pkt.hdr.src_conn_id = htons(endp->connID);
	p.pkt.hdr.first_byte_seq = htonl(endp->attn_send_seq);
	p.pkt.hdr.next_rcv_seq = htonl(endp->attn_recv_seq);
	p.pkt.hdr.rcv_window = htons(0);
	p.pkt.hdr.flags = flags | ADSPFLAG_ATTN;
	p.pkt.data.code = htons(code);

	if(data != NULL && len > 0)
		bcopy(data, &(p.pkt.data.data), len);

	if(sendto(endp->local_socket->socket, 
			  &p, 
			  1 + sizeof(struct adsphdr) + 2 + len,
			  0,
			  (struct sockaddr*)&(endp->remote_addr),
			  sizeof(struct sockaddr_at)) < 0)
	{
		return(-1);
	}
 
	return(0);
}

int adsp_send_control(struct adsp_endp *endp,
						int flags)
{
	int sendSize;
	struct
	{
		char ddptype;
		struct adsp_cntl_packet pkt;
	} p;
 
	p.ddptype = DDPTYPE_ADSP;
	p.pkt.hdr.src_conn_id = htons(endp->connID);
	p.pkt.hdr.first_byte_seq = htonl(endp->send_seq);
	p.pkt.hdr.next_rcv_seq = htonl(endp->recv_seq);
	p.pkt.hdr.rcv_window = htons(endp->recv_window);
	p.pkt.hdr.flags = ADSPFLAG_CONTROL | flags;

	switch(flags & ADSPOP_MASK)
	{
		default:
			sendSize = SZ_ADSPHDR + 1;
		break;

		case ADSPOP_OPEN_REQ:
		case ADSPOP_OPEN_ACK:
		case ADSPOP_OPEN_REQ_ACK:
		case ADSPOP_OPEN_NAK:
			sendSize = sizeof(p);
			p.pkt.data.version = htons(ADSP_VERSION);
			p.pkt.data.dst_conn_id = htons(endp->remote_connID);
			p.pkt.data.attn_rcv_seq = htonl(endp->attn_recv_seq);
		break;
	}
 
	if(sendto(endp->local_socket->socket, 
			  &p, 
			  sendSize,
			  0,
			  (struct sockaddr*)&(endp->remote_addr),
			  sizeof(struct sockaddr_at)) < 0)
	{
		return(-1);
	}
 
	return(0);
}

int adsp_setup_endp(struct adsp_socket *s,
					struct adsp_endp *endp,
					struct sockaddr_at *addr)
{
	endp->local_socket = s;
	if(addr != NULL)
		bcopy(addr, &(endp->remote_addr), sizeof(struct sockaddr_at));
	endp->connID = ++(s->lastConnID);
	endp->remote_connID = 0;
	endp->state = ADSP_STATE_CLOSED;

	endp->send_seq = 0;
	endp->oldest_seq = 0;
	endp->rmt_window_seq = 0;
	endp->recv_seq = 0;
	endp->recv_window = ADSP_RECV_BUFFER_SIZE;
	endp->recv_next_input = 0;
	endp->recv_next_output = 0;

	endp->attn_send_seq = 0;
	endp->attn_recv_seq = 0;
	endp->attn_valid = 0;

	return(0);
}


int adsp_connect(	struct adsp_socket *s, 
					struct adsp_endp *endp,
					char *name)
{
	int count;
	struct nbpnve nn;
	char *Obj = "=";
	char *Type = "=";
	char *Zone = "*";

	nbp_name(name, &Obj, &Type, &Zone);
	count = nbp_lookup(Obj, Type, Zone, &nn, 1);
	if(count < 1)
		return(-1);
	
	/* Looking up the name worked.  Set up the connection state. */
	adsp_setup_endp(s, endp, &(nn.nn_sat));

	/* Start the handshaking process. */
	if(adsp_send_control(endp, ADSPOP_OPEN_REQ) < 0)
		return(-1);

	endp->state = ADSP_STATE_OPEN_SENT;

	while(endp->state == ADSP_STATE_OPEN_SENT)
	{
		if(adsp_recv_packet(endp, 1) < 0)
			return(-1);
	}

	if(endp->state != ADSP_STATE_OPEN)
		return(-1);

	return(0);
}

int adsp_disconnect(struct adsp_endp *endp)
{
	if(endp->state == ADSP_STATE_OPEN)
		adsp_send_control(endp, ADSPOP_CLOSE);

	endp->state = ADSP_STATE_CLOSED;

	return(0);
}

int adsp_listen(struct adsp_socket *s,
				struct adsp_endp *endp)
{
	adsp_setup_endp(s, endp, NULL);
	endp->state = ADSP_STATE_CLOSED;

	/* We'll need to set up enough of the remote address to
		be able to receive packets.
	*/
	bzero(&(endp->remote_addr), sizeof(struct sockaddr_at));
	endp->remote_addr.sat_len = sizeof(struct sockaddr_at);
	endp->remote_addr.sat_family = AF_APPLETALK;
	endp->remote_addr.sat_addr.s_net = ATADDR_ANYNET;
	endp->remote_addr.sat_addr.s_node = ATADDR_ANYNODE;
	endp->remote_addr.sat_port = ATADDR_ANYPORT;

	while(endp->state == ADSP_STATE_CLOSED)
	{
		adsp_recv_packet(endp, 1);
	}

	if(endp->state == ADSP_STATE_OPEN_RCVD)
		return(1);
	
	return(0);
}

int adsp_accept(struct adsp_endp *endp)
{
	if(endp->state != ADSP_STATE_OPEN_RCVD)
		return(-1);
	
	if(adsp_send_control(endp, ADSPOP_OPEN_REQ_ACK) < 0)
		return(-1);
	
	endp->state = ADSP_STATE_OPEN_SENT;

	while(endp->state == ADSP_STATE_OPEN_SENT)
	{
		if(adsp_recv_packet(endp, 1) < 0)
			return(-1);
	}

	if(endp->state != ADSP_STATE_OPEN)
		return(-1);

	return(0);
}

int adsp_write(struct adsp_endp *endp, char *data, int len)
{
	u_int32_t bufferStart = endp->send_seq;
	u_int32_t bufferEnd = bufferStart + len;
	int32_t bytesNow, bytesToAck;
	int flags;
	int waitResult;

	/* We'll stay here until the other end has ack'ed the entire buffer.
		Technically this is inefficient, but at least it's easy.
	*/
	bytesToAck = bufferEnd - endp->oldest_seq;
	while(bytesToAck > 0)
	{
		flags = ADSPFLAG_ACK;
		/* Figure out how many bytes we want to try and transmit. */
		bytesNow = bufferEnd - endp->send_seq;
		if(bytesNow < 0)
			bytesNow = 0;

		if(bytesNow > ADSP_MAX_DATA_SIZE)
			bytesNow = ADSP_MAX_DATA_SIZE;
		if(bytesNow > (int32_t)(endp->rmt_window_seq - endp->send_seq))
			bytesNow = endp->rmt_window_seq - endp->send_seq;
		
		if(bytesNow > 0)
		{
			/* The last chunk of this write will be marked EOM. */
			if(bytesNow == bufferEnd - endp->send_seq)
				flags |= ADSPFLAG_EOM;

			if(adsp_send_data(endp, flags, 
					data + (endp->send_seq - bufferStart), 
					bytesNow) < 0)
			{
				return(-1);
			}
		}

		waitResult = adsp_recv_packet(endp, 1);
		if(waitResult < 0)
			return(-1);

		bytesToAck = bufferEnd - endp->oldest_seq;
	}

	return(len);
}

int adsp_idle(struct adsp_endp *endp)
{
	/* Deal with all pending incoming packets. */
	while(adsp_recv_packet(endp, 0) > 0)
		;

	return(0);
}

int adsp_read(struct adsp_endp *endp, char *data, int len)
{
	int bytesRead = 0;
	int rcv_result;

	do
	{

		rcv_result = adsp_recv_packet(endp, 1);
		if(rcv_result < 0)
		{
			return(-1);
		}
		
		while((bytesRead < len) && 
				(endp->recv_next_output != endp->recv_next_input))
		{
			data[bytesRead++] = endp->recv_buffer[endp->recv_next_output++];
			endp->recv_next_output %= ADSP_RECV_BUFFER_SIZE;
			endp->recv_window++;
		}
	} while((bytesRead < len) && (rcv_result != 0));

	return(bytesRead);
}

int adsp_read_nonblock(struct adsp_endp *endp, char *data, int len)
{
	int rcv_result;
	int bytesRead = 0;

	do
	{
		rcv_result = adsp_recv_packet(endp, 0);
		if(rcv_result < 0)
		{
			if(bytesRead == 0)
				bytesRead = -1;

			return(bytesRead);
		}

		while((bytesRead < len) && 
				(endp->recv_next_output != endp->recv_next_input))
		{
			data[bytesRead++] = endp->recv_buffer[endp->recv_next_output++];
			endp->recv_next_output %= ADSP_RECV_BUFFER_SIZE;
			endp->recv_window++;
		}


	} while((bytesRead < len) && (rcv_result > 0));

	return(bytesRead);
}

int adsp_read_attn(struct adsp_endp *endp, u_int16_t *code, char *data)
{
	int rcv_result;
	
	rcv_result = adsp_recv_packet(endp, 2);
	if(rcv_result < 0)
		return(-1);

	if(endp->attn_valid)
	{
		/* An attention message has been received. */
		endp->attn_valid = 0;
		*code = endp->attn_code;
		bcopy(endp->attn_buffer, data, endp->attn_size);
		return(endp->attn_size);
	}

	return(-1);
}

int adsp_write_attn(struct adsp_endp *endp, u_int16_t code, char *data, int len)
{
	int result = 0;
	u_int32_t initial_seq = endp->attn_send_seq;

	adsp_send_attn(endp, 0, code, data, len);
	do
	{
		result = adsp_recv_packet(endp, 1);
		if(result == -1)
			break;

		if(endp->attn_send_seq != initial_seq)
		{
			/* The packet has been acknowledged.  We're done. */
			return(0);
		}
	} while(1);

	return(result);
}
int adsp_fwd_reset(struct adsp_endp *endp)
{
	if(adsp_send_control(endp, ADSPOP_RESET_REQ) < 0)
		return(-1);

	/* XXX -- this should use timers and retries. */
	return(0);
}
