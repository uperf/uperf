/*
 * Copyright (C) 2008 Sun Microsystems
 *
 * This file is part of uperf.
 *
 * uperf is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3
 * as published by the Free Software Foundation.
 *
 * uperf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with uperf.  If not, see http://www.gnu.org/licenses/.
 */

/*
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved. Use is
 * subject to license terms.
 */

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif /* HAVE_CONFIG_H */

#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */
#include <strings.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/sctp.h>
#include <assert.h>
#include "logging.h"
#include "uperf.h"
#include "flowops.h"
#include "workorder.h"
#include "protocol.h"
#include "generic.h"


static void
set_sctp_options(int fd, int family, flowop_options_t *f)
{
	if (f == NULL) {
		return;
	}
	if (f->wndsz > 0) {
		generic_set_socket_buffer(fd, f->wndsz);
		generic_verify_socket_buffer(fd, f->wndsz);
	}
	if (FO_SCTP_NODELAY(f)) {
		const int on = 1;

		if (setsockopt(fd, IPPROTO_SCTP, SCTP_NODELAY,
		               &on, (socklen_t)sizeof (int))) {
			uperf_log_msg(UPERF_LOG_WARN, errno,
			             "Cannot disable Nagle Algorithm for SCTP");
		}
	}
	if ((f->sctp_in_streams > 0) || (f->sctp_out_streams > 0)) {
		struct sctp_initmsg init;

		memset(&init, 0, sizeof(struct sctp_initmsg));
		init.sinit_max_instreams = f->sctp_in_streams;
		init.sinit_num_ostreams = f->sctp_out_streams;
		if (setsockopt(fd, IPPROTO_SCTP, SCTP_INITMSG,
		               &init, (socklen_t)sizeof(struct sctp_initmsg)) < 0) {
			uperf_log_msg(UPERF_LOG_WARN, errno, "Cannot set SCTP streams");
		}
	}
	if ((f->sctp_rto_min > 0) ||
	    (f->sctp_rto_max > 0) ||
	    (f->sctp_rto_initial > 0)) {
		struct sctp_rtoinfo rtoinfo;

		memset(&rtoinfo, 0, sizeof(struct sctp_rtoinfo));
		rtoinfo.srto_min = f->sctp_rto_min;
		rtoinfo.srto_max = f->sctp_rto_max;
		rtoinfo.srto_initial = f->sctp_rto_initial;
		if (setsockopt(fd, IPPROTO_SCTP, SCTP_RTOINFO,
		               &rtoinfo, (socklen_t)sizeof(struct sctp_rtoinfo)) < 0) {
			uperf_log_msg(UPERF_LOG_WARN, errno,
			              "Cannot set SCTP RTO parameters");
		}
	}
	if ((f->sctp_sack_delay > 0) || (f->sctp_sack_frequency > 0)) {
#if defined(SCTP_DELAYED_SACK)
		struct sctp_sack_info sackinfo;

		memset(&sackinfo, 0, sizeof(struct sctp_sack_info));
		sackinfo.sack_delay = f->sctp_sack_delay;
		sackinfo.sack_freq = f->sctp_sack_frequency;
		if (setsockopt(fd, IPPROTO_SCTP, SCTP_DELAYED_SACK,
		               &sackinfo, (socklen_t)sizeof(struct sctp_sack_info)) < 0) {
			uperf_log_msg(UPERF_LOG_WARN, errno,
			              "Cannot set SCTP SACK parameters");
		}
#else
		uperf_log_msg(UPERF_LOG_WARN, ENOPROTOOPT,
		              "Cannot set SCTP SACK parameters");
#endif
	}
	if (f->sctp_max_burst_size > 0) {
		struct sctp_assoc_value maxburst;

		memset(&maxburst, 0, sizeof(struct sctp_assoc_value));
		maxburst.assoc_value = f->sctp_max_burst_size;
		if (setsockopt(fd, IPPROTO_SCTP, SCTP_MAX_BURST,
		               &maxburst, (socklen_t)sizeof(struct sctp_assoc_value)) < 0) {
			uperf_log_msg(UPERF_LOG_WARN, errno,
			              "Cannot set max. burst parameter for SCTP");
		}
	}
	if (f->sctp_max_fragment_size > 0) {
		struct sctp_assoc_value maxfrag;

		memset(&maxfrag, 0, sizeof(struct sctp_assoc_value));
		maxfrag.assoc_value = f->sctp_max_fragment_size;
		if (setsockopt(fd, IPPROTO_SCTP, SCTP_MAXSEG,
		               &maxfrag, (socklen_t)sizeof(struct sctp_assoc_value)) < 0) {
			uperf_log_msg(UPERF_LOG_WARN, errno,
			              "Cannot set max. fragment size of SCTP");
		}
	}
	if ((f->sctp_hb_interval > 0) || (f->sctp_path_mtu > 0)) {
		struct sctp_paddrparams param;

		memset(&param, 0, sizeof(struct sctp_paddrparams));
		param.spp_address.ss_family = family;
#if defined(UPERF_FREEBSD) || defined(UPERF_DARWIN)
		switch (family) {
		case AF_INET:
			param.spp_address.ss_len = sizeof(struct sockaddr_in);
			break;
		case AF_INET6:
			param.spp_address.ss_len = sizeof(struct sockaddr_in6);
			break;
		}
#endif
		param.spp_hbinterval = f->sctp_hb_interval;
		param.spp_pathmtu = f->sctp_path_mtu;
		if (f->sctp_hb_interval > 0) {
			param.spp_flags |= SPP_HB_ENABLE;
		}
		if (f->sctp_path_mtu > 0) {
			param.spp_flags |= SPP_PMTUD_DISABLE;
		}
		if (setsockopt(fd, IPPROTO_SCTP, SCTP_PEER_ADDR_PARAMS,
		               &param, (socklen_t)sizeof(struct sctp_paddrparams)) < 0) {
			uperf_log_msg(UPERF_LOG_WARN, errno,
			              "Cannot set path defaults for SCTP");
		}
	}
}

/* returns the port number */
/* ARGSUSED */
static int
protocol_sctp_listen(protocol_t *p, void *options)
{
	flowop_options_t *flowop_options = (flowop_options_t *)options;
	char msg[128];

	if (generic_socket(p, AF_INET6, IPPROTO_SCTP) != UPERF_SUCCESS) {
		if (generic_socket(p, AF_INET, IPPROTO_SCTP) != UPERF_SUCCESS) {
			(void) snprintf(msg, 128, "%s: Cannot create socket", "sctp");
			uperf_log_msg(UPERF_LOG_ERROR, errno, msg);
			return (UPERF_FAILURE);
		} else {
			set_sctp_options(p->fd, AF_INET, flowop_options);
		}
	} else {
		set_sctp_options(p->fd, AF_INET6, flowop_options);
	}

	return (generic_listen(p, IPPROTO_SCTP, options));
}

static int
protocol_sctp_connect(protocol_t *p, void *options)
{
#ifdef SCTP_REMOTE_UDP_ENCAPS_PORT
	struct sctp_udpencaps encap;
#endif
	struct sockaddr_storage serv;
	flowop_options_t *flowop_options = (flowop_options_t *)options;
	char msg[128];

	uperf_debug("sctp: Connecting to %s:%d\n", p->host, p->port);

	if (name_to_addr(p->host, &serv)) {
		/* Error is already reported by name_to_addr, so just return */
		return (UPERF_FAILURE);
	}
	if (generic_socket(p, serv.ss_family, IPPROTO_SCTP) < 0) {
		return (UPERF_FAILURE);
	}
	set_sctp_options(p->fd, serv.ss_family, flowop_options);
	if ((flowop_options != NULL) && (flowop_options->encaps_port > 0)) {
		uperf_debug("Using UDP encapsulation with remote port %u\n", flowop_options->encaps_port);
#ifdef SCTP_REMOTE_UDP_ENCAPS_PORT
		memset(&encap, 0, sizeof(struct sctp_udpencaps));
		encap.sue_address.ss_family = serv.ss_family;
		encap.sue_port = htons((uint16_t)flowop_options->encaps_port);
		if (setsockopt(p->fd, IPPROTO_SCTP, SCTP_REMOTE_UDP_ENCAPS_PORT, &encap, sizeof(struct sctp_udpencaps)) < 0) {
			(void) snprintf(msg, 128,
			    "tcp: Enabling UDP encapsulation to port %d failed",
			    flowop_options->encaps_port);
			uperf_log_msg(UPERF_LOG_ERROR, errno, msg);
			return (UPERF_FAILURE);
		}
#else
		(void) snprintf(msg, 128,
		    "tcp: Enabling UDP encapsulation to port %d not supported",
		    flowop_options->encaps_port);
		uperf_log_msg(UPERF_LOG_ERROR, errno, msg);
		return (UPERF_FAILURE);
#endif
	}
	if (generic_connect(p, &serv) < 0) {
		return (UPERF_FAILURE);
	}
	return (UPERF_SUCCESS);
}

int
protocol_sctp_write(protocol_t *p, void *buffer, int size, void *options)
#if defined(HAVE_SCTP_SENDV)
{
	ssize_t len;
	struct iovec iov;
	struct sctp_sendv_spa info;

	iov.iov_base = buffer;
	iov.iov_len = size;

	memset(&info, 0, sizeof(struct sctp_sendv_spa));
	if (options) {
		flowop_options_t *flowops;

		flowops = (flowop_options_t *) options;
		info.sendv_sndinfo.snd_sid = flowops->sctp_stream_id;
		if (FO_SCTP_UNORDERED(flowops)) {
			info.sendv_sndinfo.snd_flags |= SCTP_UNORDERED;
		}
		info.sendv_flags |= SCTP_SEND_SNDINFO_VALID;
#ifdef SCTP_PR_SCTP_TTL
		if (strcasecmp(flowops->sctp_pr_policy, "ttl") == 0) {
			info.sendv_prinfo.pr_policy = SCTP_PR_SCTP_TTL;
			info.sendv_prinfo.pr_value = flowops->sctp_pr_value;
			info.sendv_flags |= SCTP_SEND_PRINFO_VALID;
		}
#endif
#ifdef SCTP_PR_SCTP_RTX
		if (strcasecmp(flowops->sctp_pr_policy, "rtx") == 0) {
			info.sendv_prinfo.pr_policy = SCTP_PR_SCTP_RTX;
			info.sendv_prinfo.pr_value = flowops->sctp_pr_value;
			info.sendv_flags |= SCTP_SEND_PRINFO_VALID;
		}
#endif
#ifdef SCTP_PR_SCTP_BUF
		if (strcasecmp(flowops->sctp_pr_policy, "buf") == 0) {
			info.sendv_prinfo.pr_policy = SCTP_PR_SCTP_BUF;
			info.sendv_prinfo.pr_value = flowops->sctp_pr_value;
			info.sendv_flags |= SCTP_SEND_PRINFO_VALID;
		}
#endif
	}

	if ((len = sctp_sendv(p->fd, &iov, 1, NULL, 0,
	                      &info, sizeof(struct sctp_sendv_spa), SCTP_SENDV_SPA, 0)) < 0) {
		uperf_log_msg(UPERF_LOG_WARN, errno, "Cannot sctp_sendv ");
	}
	return (len);
}
#elif defined(HAVE_SCTP_SENDMSG)
{
	ssize_t len;
	uint32_t flags;
	uint16_t stream_no;
	uint32_t timetolive;

	flags = 0;
	timetolive = 0;
	if (options) {
		flowop_options_t *flowops;

		flowops = (flowop_options_t *) options;
		if (FO_SCTP_UNORDERED(flowops)) {
			flags |= SCTP_UNORDERED;
		}
		stream_no = flowops->sctp_stream_id;
		if (strcasecmp(flowops->sctp_pr_policy, "ttl") == 0) {
			timetolive = flowops->sctp_pr_value;
#if defined(UPERF_FREEBSD) || defined(UPERF_DARWIN)
#ifdef SCTP_PR_SCTP_TTL
			flags |= SCTP_PR_SCTP_TTL;
#endif
#endif
		}
#if defined(UPERF_FREEBSD) || defined(UPERF_DARWIN)
#ifdef SCTP_PR_SCTP_RTX
		if (strcasecmp(flowops->sctp_pr_policy, "rtx") == 0) {
			timetolive = flowops->sctp_pr_value;
			flags |= SCTP_PR_SCTP_RTX;
		}
#endif
#ifdef SCTP_PR_SCTP_BUF
		if (strcasecmp(flowops->sctp_pr_policy, "buf") == 0) {
			timetolive = flowops->sctp_pr_value;
			flags |= SCTP_PR_SCTP_BUF;
		}
#endif
#endif
	} else {
		stream_no = 0;
	}

	if ((len = sctp_sendmsg(p->fd, buffer, size, NULL, 0, htonl(0), flags,
	                        stream_no, timetolive, 0)) < 0) {
		uperf_log_msg(UPERF_LOG_WARN, errno, "Cannot sctp_sendmsg ");
	}
	return (len);
}
#else
{
	return (generic_write(p, buffer, size, options));
}
#endif

static protocol_t *protocol_sctp_accept(protocol_t *p, void *options);

static protocol_t *
protocol_sctp_new()
{
	protocol_t *newp;

	if ((newp = calloc(1, sizeof(protocol_t))) == NULL) {
		perror("calloc");
		return (NULL);
	}
	newp->connect = protocol_sctp_connect;
	newp->disconnect = generic_disconnect;
	newp->listen = protocol_sctp_listen;
	newp->accept = protocol_sctp_accept;
	newp->read = generic_read;
	newp->write = protocol_sctp_write;
	newp->wait = generic_undefined;
	newp->type = PROTOCOL_SCTP;
	(void) strlcpy(newp->host, "Init", MAXHOSTNAME);
	newp->fd = -1;
	newp->port = -1;
	return (newp);
}

static protocol_t *
protocol_sctp_accept(protocol_t *p, void *options)
{
	protocol_t *newp;

	if ((newp = protocol_sctp_new()) == NULL) {
		return (NULL);
	}
	if (generic_accept(p, newp, options) != 0) {
		return (NULL);
	}
	return (newp);
}

protocol_t *
protocol_sctp_create(char *host, int port)
{
	protocol_t *newp;

	if ((newp = protocol_sctp_new()) == NULL) {
		return (NULL);
	}
	if (strlen(host) == 0) {
		(void) strlcpy(newp->host, "localhost", MAXHOSTNAME);
	} else {
		(void) strlcpy(newp->host, host, MAXHOSTNAME);
	}
	newp->port = port;
	uperf_debug("sctp - Creating SCTP Protocol to %s:%d\n", host, port);
	return (newp);
}
