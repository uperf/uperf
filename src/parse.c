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

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>
#include <inttypes.h>

#include "parse.h"
#include "uperf.h"
#include "flowops.h"
#include "workorder.h"

#include "workorder.h"
#include "numbers.h"
#include "protocol.h"
#include "flowops_library.h"
#include "sendfilev.h"

struct tokenid {
	int type;
	char *id;
};
static struct tokenid tlist[] =
		{ { TOKEN_PROFILE_START, 	"<profile"},
		{ TOKEN_PROFILE_END, 		"</profile>"},
		{ TOKEN_GROUP_START, 		"<group"},
		{ TOKEN_GROUP_END, 		"</group>"},
		{ TOKEN_TXN_START, 		"<transaction"},
		{ TOKEN_TXN_END, 		"</transaction"},
		{ TOKEN_FLOWOP_START, 		"<flowop"},
		{ TOKEN_XML_END, 		"/>"},
		{ TOKEN_NAME, 			"name="},
		{ TOKEN_ITERATIONS, 		"iterations="},
		{ TOKEN_TYPE, 			"type="},
		{ TOKEN_OPTIONS, 		"options="},
		{ TOKEN_DURATION, 		"duration="},
		{ TOKEN_NTHREADS, 		"nthreads="},
		{ TOKEN_NPROCESSES, 		"nprocs="},
		{ TOKEN_RATE, 			"rate="},
		};

static int
is_seperator(char c) {
	return (c < 33 && c > 0);
}

static char *parse_errors[100];
static int no_errs = 0;

static void
print_errors()
{
	int i;
	for (i = 0; i < no_errs; i++) {
		fprintf(stderr, "Error %d: %s\n", i+1, parse_errors[i]);
	}
}

static void
add_error(char *str)
{
	if (no_errs > 100) {
		print_errors();
		fprintf(stderr, "100 errors exceeded\n");
		exit(1);
	}
	parse_errors[no_errs++] = strdup(str);
}

/*
 * Return the next token. Words within quotes are considered
 * to be one token.
 * Return null if no more tokens
 */
static char *
next_token(char *s1)
{
	char symbol[1024];
	int i = 0;
	static char *p = NULL;

	if (s1 != NULL)
		p = s1;

	if (*p == '\0')
		return (NULL);
	while (is_seperator(*p))
		p++;
	if (*p == '\0')
		return (NULL);

	/* Remove comments. Comments begin with <!-- and end with --> */
	if (strncmp(p, "<!--", 4) == 0) {
		while (*p != '\0') {
			if ((*p == '>') && (*(p-1) == '-') &&
				(*(p-2) == '-')) {
				++p;
				break;
			}
			p++;
		}
		return (next_token(NULL));
	}
	/* Remove xml junk. begins with <? and end with ?> */
	if (p[0] == '<' && p[1] == '?') {
		while (*p != '\0') {
			if ((*p == '>') && (*(p-1) == '?')) {
				++p;
				break;
			}
			p++;
		}
		return (next_token(NULL));

	}
	while (!is_seperator(*p) && *p != '\0' && i < sizeof(symbol)) {
		if (*p == '"') {
			if (*p == '"')
				p++; /* Ignore " */
			while (*p != '"' && *p != '\0' && i < sizeof(symbol)) {
				/* replace newline by space */
				if (*p == '\n' || *p == '\r')
					*p = ' ';

				symbol[i++] = tolower(*p++);
			}
			if (*p == '"')
				p++; /* Ignore " */
			break;
		}
		symbol[i++] = tolower(*p++);
	}
	if (i < sizeof(symbol)) {
		symbol[i] = '\0';
		return (strdup(symbol));
	} else {
		add_error("token too long.");
		return (NULL);
	}

}

static int
token_type(char *token) {
	int i;

	if (token == NULL)
		return (TOKEN_ERROR);
	for (i = 0; i < sizeof (tlist)/sizeof (struct tokenid); i++) {
		if (strncmp(token, tlist[i].id, strlen(tlist[i].id)) == 0)
			return (tlist[i].type);
	}

	/* printf("Unknown token : %s\n", token); */
	return (TOKEN_ERROR);
}

/*
 * if s is of type name=value, return 'value', else s
 * If value begins with $, return the value of the environmental
 * variable
 */
static char *
get_symbol(char *s)
{
	char *s1 = s;
	while (*s1 != '\0') {
		if (*s1++ == '=') {
			s = s1;
			break;
		}
	}
	if (s[0] == '$') {
		char *tmp = getenv((s+1));
		if (tmp == NULL || *tmp == '\0') {
			char msg[1024];
			snprintf(msg, 1024, "%s is not set", s);
			add_error(msg);
		} else {
			return (tmp);
		}
	}
	return (s);
}

/* Parse the file. return 0 on success */
static int
parse(char *buffer, struct symbol *list)
{
	struct symbol *curr, *last;
	char *token, *pt;
	char sym[1024];

	last = list;
	token = next_token(buffer);
	pt = list->symbol;
	curr = NULL;


	if (token == NULL) {
		printf("No profile found\n");
		return (1);
	}

	do {
		int merge = 0;
		if ((curr) && (curr->symbol)) {
			pt = curr->symbol;
		}

		if ((token[0] == '>') && (token[1] == '\0')) {
			free(token);
			continue;
		}

		if (token[0] == '=')
			merge = 1;
		else if (pt[strlen(pt) - 1] == '=')
			merge = 1;
		else if ((strlen(pt) == 2) && (strncmp(pt, "</", 2) == 0))
			merge = 1;
		else if ((strlen(pt) == 1) && (pt[0] == '<'))
			merge = 1;

		if (merge == 1) {
			/* merge this with prev symbol */
			strlcpy(sym, curr->symbol, 1024);
			strlcat(sym, token, 1024);
			if (curr->symbol)
				free(curr->symbol);
			curr->type = token_type(sym);
			curr->symbol = get_symbol(strdup(sym));
		} else {
			curr = calloc(1, sizeof (struct symbol));
			curr->type = token_type(token);
			if (curr->type != TOKEN_ERROR)
				curr->symbol = get_symbol(token);
			else
				curr->symbol = token;
			curr->prev = last;
			curr->next = NULL;
			last->next = curr;
			pt = curr->symbol;
			last = curr;
		}
	} while ((token = next_token(NULL)) != NULL);

	return (0);
}

#if defined(DEBUG) || defined(TEST_PARSE)

static void
print_symbols(struct symbol *list) {
	while (list) {
		printf("Print symbol: %s %d\n", list->symbol, list->type);
		list = list->next;
	}
}

void
print_txn_t(txn_t *t)
{
	(void) printf("\ttxn_t [");
	(void) printf("iter = %"PRIu64", ", t->iter);
	(void) printf("nflowops = %d, ", t->nflowop);
	(void) printf("duration = %"PRIu64", ", t->duration);
	(void) printf("rate = %s", t->rate_str);
	(void) printf("]\n");
}

void
print_flowop_t(flowop_t *t)
{
	(void) printf("\t\tflowop_t [");
	(void) printf("type = %s, ", t->name);
	(void) printf("local = %s, ", t->options.localhost);
	(void) printf("remote = %s, ", t->options.remotehost);
	(void) printf("protocol = %d, ", t->options.protocol);
	(void) printf("duration = %"PRIu64"", t->options.duration);
	(void) printf("]\n");
}

void
print_group_t(group_t *t)
{
	(void) printf("group_t [");
	(void) printf("endian = 0x%x, ", t->endian);
	(void) printf("nthreads = %d, ", t->nthreads);
	(void) printf("ntxn = %d", t->ntxn);
	(void) printf("]\n");
}

void
print_group_t_recurse(group_t *t)
{
	txn_t *tptr;
	flowop_t *fptr;
	print_group_t(t);
	for (tptr = t->tlist; tptr; tptr = tptr->next) {
		print_txn_t(tptr);
		for (fptr = tptr->flist; fptr; fptr = fptr->next)
			print_flowop_t(fptr);
	}
}

static void
print_workorder_t(workorder_t *w) {
	int i;
	for (i = 0; i < w->ngrp; i++)
		print_group_t_recurse(&w->grp[i]);
}

#endif /* DEBUG */

static int
string2int(char *value)
{
	if (!value) {
		return (-1);
	}
	int val = string_to_int(value);
	if (val < 0) {
		char err[1024];
		snprintf(err, 1024, "Cannot parse %s", value);
		add_error(err);
	}
	return (val);
}

static char *
strip(char *str)
{
	char *s, *e, *t;

	if (!str)
		return (0);

	s = str;
	e = s + strlen(str);
	t = e - 1;
	while (s < e && *s == ' ')
		s++;
	while (t > s && *t == ' ')
		t--;
	if ((s != str) || (t != e -1)) {
		memmove(str, s, t + 1 - s);
		str[t + 1 - s] = '\0';
	}

	return (str);
}

/*
 * Parse the size= parameter. It currently accpets
 * size=8k, size=8192 or size=rand(1,8k)
 * Return a +ve number on success; else -1
 */
static int
parse_size(flowop_t *f, char *value)
{
	int size = 0;
	assert(value);
	size = string_to_int(value);

	if (size > 0)
		return (size);
	if (strncasecmp(value, "rand(", 5) == 0) {
		char *cmin, *cmax;
		cmin = strtok(value, "(");
		cmin = strtok(NULL, ",");
		cmax = strtok(NULL, ")");
		f->options.rand_sz_min = string2int(strip(cmin));
		f->options.rand_sz_max = string2int(strip(cmax));
		f->options.flag |= O_SIZE_RAND;
		/* Seed the random generator here */
		srand(GETHRTIME());
		if (f->options.rand_sz_min > 0 && f->options.rand_sz_max > 0)
			return (0);
	}
	return (-1);
}

static uint64_t
string2nsec(char *value)
{
	uint64_t val = string_to_nsec(value);
	if (val == 0) {
		char err[1024];
		snprintf(err, 1024, "Cannot convert %s to nsecs", value);
		add_error(err);
	}
	return (val);
}

static int
parse_option(char *option, flowop_t *flowop)
{
	char *key;	/* Key option */
	char *value;	/* Value for the key */
	char *tmp;
	char err[128];
	if (strcasecmp(option, "tcp_nodelay") == 0) {
		flowop->options.flag |= O_TCP_NODELAY;
		return (UPERF_SUCCESS);
	} else if (strcasecmp(option, "busy") == 0) {
		flowop->options.flag |= O_THINK_BUSY;
		return (UPERF_SUCCESS);
	} else if (strcasecmp(option, "idle") == 0) {
		flowop->options.flag |= O_THINK_IDLE;
		return (UPERF_SUCCESS);
	} else if (strcasecmp(option, "canfail") == 0) {
		flowop->options.flag |= O_CANFAIL;
		return (UPERF_SUCCESS);
	} else if (strcasecmp(option, "non_blocking") == 0) {
		flowop->options.flag |= O_NONBLOCKING;
		return (UPERF_SUCCESS);
	}
#ifdef HAVE_SCTP
	else if (strcasecmp(option, "sctp_unordered") == 0) {
		flowop->options.flag |= O_SCTP_UNORDERED;
		return (UPERF_SUCCESS);
	} else if (strcasecmp(option, "sctp_nodelay") == 0) {
		flowop->options.flag |= O_SCTP_NODELAY;
		return (UPERF_SUCCESS);
	}
#endif
	else {
		key = strtok(option, "=");
		value = strtok(NULL, " ");

		if (value == NULL) {
			snprintf(err, sizeof (err),
				"option %s is not of type key=value",
				option);
			add_error(err);
			return (UPERF_FAILURE);
		}

		if (value[0] == '$') {
			tmp = getenv(&value[1]);
			if (tmp == NULL) {
				snprintf(err, sizeof (err),
					"Env variable %s = %s not set",
					key, value);
				add_error(err);
				return (UPERF_FAILURE);
			}
			value = tmp;
		}

		if (strcasecmp(key, "size") == 0) {
			flowop->options.size = parse_size(flowop, value);
			if (flowop->options.size == -1) {
				snprintf(err, sizeof (err),
				    "Could not parse %s", value);
				add_error(err);
				return (UPERF_FAILURE);
			}
		} else if (strcasecmp(key, "rsize") == 0) {
			flowop->options.rsize = string2int(value);
		} else if (strcasecmp(key, "nfiles") == 0) {
			flowop->options.nfiles = string2int(value);
		} else if (strcasecmp(key, "dir") == 0) {
			strlcpy(flowop->options.dir, value, PATHMAX);
			if (sendfile_init(value) != 0) {
				snprintf(err, sizeof (err),
					"Error initializing dir: %s",
					value);
				add_error(err);
				return (UPERF_FAILURE);
			}
		} else if (strcasecmp(key, "count") == 0) {
			flowop->options.count = atoi(value);
		} else if (strcasecmp(key, "port") == 0) {
			flowop->options.port = atoi(value);
		} else if (strcasecmp(key, "protocol") == 0) {
			flowop->options.protocol = protocol_type(value);
			if (protocol_type(value) == PROTOCOL_UNSUPPORTED) {
				snprintf(err, sizeof (err),
					"Protocol %s not supported",
					value);
				add_error(err);
				return (UPERF_FAILURE);
			}
		} else if (strcasecmp(key, "conn") == 0) {
			if ((flowop->p_id = atoi(value)) <= 0) {
				snprintf(err, sizeof (err),
				    "connection id (conn=%s) should be > 0",
				    value);
				add_error(err);
				return (UPERF_FAILURE);
			}
		} else if (strcasecmp(key, "remotehost") == 0) {
			strlcpy(flowop->options.remotehost, value, MAXHOSTNAME);
		} else if (strcasecmp(key, "localhost") == 0) {
			strlcpy(flowop->options.localhost, value, MAXHOSTNAME);
		} else if (strcasecmp(key, "wndsz") == 0) {
			flowop->options.wndsz = string2int(value);
		} else if (strcasecmp(key, "timeout") == 0) {
			flowop->options.poll_timeout = string2nsec(value);
			if (flowop->options.poll_timeout == 0) {
				snprintf(err, sizeof (err),
					"Cannot understand timeout:%s",
					value);
				add_error(err);
				return (UPERF_FAILURE);
			}
		} else if (strcasecmp(key, "duration") == 0) {
			flowop->options.duration = string2nsec(value);
			if (flowop->options.duration == 0) {
				snprintf(err, sizeof (err),
					"Cannot understand duration:%s",
					value);
				add_error(err);
				return (UPERF_FAILURE);
			}
		} else if (strcasecmp(key, "encaps") == 0) {
			int res;

			res = string2int(value);
			if ((res >= 0) && (res <= 65535)) {
				flowop->options.encaps_port = res;
			} else {
				snprintf(err, sizeof(err),
				         "Cannot understand encaps:%s", value);
				add_error(err);
				return (UPERF_FAILURE);
			}
		} else if (strcasecmp(key, "cc") == 0) {
			strlcpy(flowop->options.cc, value, sizeof(flowop->options.cc));
		} else if (strcasecmp(key, "stack") == 0) {
			strlcpy(flowop->options.stack, value, sizeof(flowop->options.stack));
		}
#ifdef HAVE_SCTP
		else if (strcasecmp(key, "sctp_rto_min") == 0) {
			int res;

			res = string2int(value);
			if (res >= 0) {
				flowop->options.sctp_rto_min = res;
			} else {
				snprintf(err, sizeof(err),
				         "Cannot understand sctp_rto_min:%s", value);
				add_error(err);
				return (UPERF_FAILURE);
			}
		} else if (strcasecmp(key, "sctp_rto_max") == 0) {
			int res;

			res = string2int(value);
			if (res >= 0) {
				flowop->options.sctp_rto_max = res;
			} else {
				snprintf(err, sizeof(err),
				         "Cannot understand sctp_rto_max:%s", value);
				add_error(err);
				return (UPERF_FAILURE);
			}
		} else if (strcasecmp(key, "sctp_rto_initial") == 0) {
			int res;

			res = string2int(value);
			if (res >= 0) {
				flowop->options.sctp_rto_initial = res;
			} else {
				snprintf(err, sizeof(err),
				         "Cannot understand sctp_rto_initial:%s", value);
				add_error(err);
				return (UPERF_FAILURE);
			}
		} else if (strcasecmp(key, "sctp_sack_delay") == 0) {
			int res;

			res = string2int(value);
			if (res >= 0) {
				flowop->options.sctp_sack_delay = res;
			} else {
				snprintf(err, sizeof(err),
				         "Cannot understand sctp_sack_delay:%s", value);
				add_error(err);
				return (UPERF_FAILURE);
			}
		} else if (strcasecmp(key, "sctp_sack_frequency") == 0) {
			int res;

			res = string2int(value);
			if (res >= 0) {
				flowop->options.sctp_sack_frequency = res;
			} else {
				snprintf(err, sizeof(err),
				         "Cannot understand sctp_sack_frequency:%s", value);
				add_error(err);
				return (UPERF_FAILURE);
			}
		} else if (strcasecmp(key, "sctp_max_burst_size") == 0) {
			int res;

			res = string2int(value);
			if (res >= 0) {
				flowop->options.sctp_max_burst_size = res;
			} else {
				snprintf(err, sizeof(err),
				         "Cannot understand sctp_max_burst_size:%s", value);
				add_error(err);
				return (UPERF_FAILURE);
			}
		} else if (strcasecmp(key, "sctp_max_fragment_size") == 0) {
			int res;

			res = string2int(value);
			if (res >= 0) {
				flowop->options.sctp_max_fragment_size = res;
			} else {
				snprintf(err, sizeof(err),
				         "Cannot understand sctp_max_fragment_size:%s", value);
				add_error(err);
				return (UPERF_FAILURE);
			}
		} else if (strcasecmp(key, "sctp_hb_interval") == 0) {
			int res;

			res = string2int(value);
			if (res >= 0) {
				flowop->options.sctp_hb_interval = res;
			} else {
				snprintf(err, sizeof(err),
				         "Cannot understand sctp_hb_interval:%s", value);
				add_error(err);
				return (UPERF_FAILURE);
			}
		} else if (strcasecmp(key, "sctp_path_mtu") == 0) {
			int res;

			res = string2int(value);
			if (res >= 0) {
				flowop->options.sctp_path_mtu = res;
			} else {
				snprintf(err, sizeof(err),
				         "Cannot understand sctp_path_mtu:%s", value);
				add_error(err);
				return (UPERF_FAILURE);
			}
		} else if (strcasecmp(key, "sctp_in_streams") == 0) {
			int res;
			
			res = string2int(value);
			if ((res > 0) && (res <= 65535)) {
				flowop->options.sctp_in_streams = res;
			} else {
				snprintf(err, sizeof(err),
				         "Cannot understand sctp_in_streams:%s", value);
				add_error(err);
				return (UPERF_FAILURE);
			}
		} else if (strcasecmp(key, "sctp_out_streams") == 0) {
			int res;

			res = string2int(value);
			if ((res > 0) && (res <= 65535)) {
				flowop->options.sctp_out_streams = res;
			} else {
				snprintf(err, sizeof(err),
				         "Cannot understand sctp_out_streams:%s", value);
				add_error(err);
				return (UPERF_FAILURE);
			}
		} else if (strcasecmp(key, "sctp_stream_id") == 0) {
			int res;

			res = string2int(value);
			if ((res >= 0) && (res <= 65535)) {
				flowop->options.sctp_stream_id = res;
			} else {
				snprintf(err, sizeof(err),
				         "Cannot understand sctp_stream_id:%s", value);
				add_error(err);
				return (UPERF_FAILURE);
			}
		} else if (strcasecmp(key, "sctp_pr_value") == 0) {
			int res;

			res = string2int(value);
			if (res >= 0) {
				flowop->options.sctp_pr_value = res;
			} else {
				snprintf(err, sizeof(err),
				         "Cannot understand sctp_pr_value:%s", value);
				add_error(err);
				return (UPERF_FAILURE);
			}
		} else if (strcasecmp(key, "sctp_pr_policy") == 0) {
			strlcpy(flowop->options.sctp_pr_policy, value, 8);
		}
#endif
#ifdef HAVE_SSL
		else if (strcasecmp(key, "engine") == 0) {
			strlcpy(flowop->options.engine, value,
				sizeof (flowop->options.engine));
		} else if (strcasecmp(key, "cipher") == 0) {
			strcpy(flowop->options.cipher, value);
		} else if (strcasecmp(key, "method") == 0) {
			strcpy(flowop->options.method, value);
		}
#endif
		else {
			snprintf(err, sizeof (err),
				"parser - Option unrecognized %s=%s",
				key, value);
			add_error(err);
			return (UPERF_FAILURE);
		}
	}
	return (UPERF_SUCCESS);
}

static int
flowop_parse_options(char *str_options, flowop_t *flowop)
{
	char	*options;	/* Temp string to get the token */
	char	opt_arr[100][100];	/* Array to copy the tokens */
	int	i = 0, j;			/* Iterators */
	char	*delimiters = " \t";

	flowop->options.count = 1;	/* Default count */
	flowop->options.size = 0;	/* Default size */
	flowop->options.duration = 0;	/* Default duration */
	flowop->p_id = UPERF_ANY_CONNECTION;

	for (options = strtok(str_options, delimiters);
	    options != NULL;
	    options = strtok(NULL, delimiters)) {
		if (isalpha(options[0]))
			(void) strlcpy(opt_arr[i++], options, 100);
	}
	for (j = 0; j < i; j++) {
		parse_option(opt_arr[j], flowop);
	}

	return (UPERF_SUCCESS);
}

static workorder_t *
build_worklist(struct symbol *list)
{
	static workorder_t w;
	group_t *curr_grp = NULL;
	txn_t *curr_txn = NULL;
	flowop_t *curr_flowop = NULL;
	char err[1024];
	int txnid = 0;
	int fid = 0;
	int in_group = 0;
	int in_txn = 0;

	w.ngrp = 0;
	bzero(&w, sizeof (workorder_t));

	while (list) {
		switch (list->type) {
		case TOKEN_PROFILE_START:
			break;
		case TOKEN_PROFILE_END:
			break;
		case TOKEN_GROUP_START:
			curr_grp = &w.grp[w.ngrp++];
			bzero(curr_grp, sizeof (group_t));
			curr_grp->endian = UPERF_ENDIAN_VALUE;
			curr_txn = 0;
			curr_flowop = 0;
			txnid = 0;
			in_group = 1;
			snprintf(curr_grp->name, UPERF_NAME_LEN, "Group%d",
			    w.ngrp - 1);
			break;
		case TOKEN_GROUP_END:
			in_group = 0;
			break;
		case TOKEN_TXN_START:
			if (!in_group) {
				snprintf(err, sizeof (err),
				    "No current group");
				add_error(err);
				return (NULL);
			}
			curr_grp->ntxn++;
			if (curr_txn == 0) {
				curr_grp->tlist = calloc(1, sizeof (txn_t));
				curr_txn = curr_grp->tlist;
			} else {
				curr_txn->next = calloc(1, sizeof (txn_t));
				curr_txn = curr_txn->next;
			}
			snprintf(curr_txn->name, UPERF_NAME_LEN, "Txn%d",
			    txnid);
			curr_txn->txnid = txnid++;
			curr_txn->iter = 1;
			curr_flowop = 0;
			fid = 0;
			in_txn = 1;
			break;
		case TOKEN_TXN_END:
			in_txn = 0;
			break;
		case TOKEN_FLOWOP_START:
			if (!in_txn) {
				snprintf(err, sizeof (err),
				    "No current transaction");
				add_error(err);
				return (NULL);
			}
			curr_txn->nflowop++;
			if (curr_flowop == NULL) {
				curr_txn->flist = calloc(1, sizeof (flowop_t));
				curr_flowop = curr_txn->flist;
			} else {
				curr_flowop->next =
					calloc(1, sizeof (flowop_t));
				curr_flowop = curr_flowop->next;
			}
			curr_flowop->options.count = 1;
			curr_flowop->id = fid++;
			break;
		case TOKEN_XML_END:
			break;
		case TOKEN_NAME:
			if (in_group) {
				strlcpy(curr_grp->name, list->symbol, UPERF_NAME_LEN);
			} else {
				strlcpy(w.name, list->symbol, NAMELEN);
			}
			break;
		case TOKEN_ITERATIONS:
			if (!in_txn) {
				snprintf(err, sizeof (err),
				    "No current transaction");
				add_error(err);
				return (NULL);
			}
			curr_txn->iter = string2int(list->symbol);
			break;
		case TOKEN_TYPE:
			curr_flowop->type = flowop_type(list->symbol);
			if (curr_flowop->type == FLOWOP_ERROR) {
				snprintf(err, sizeof (err),
				    "Unknown flowop %s", list->symbol);
				add_error(err);
				return (NULL);
			}
			curr_flowop->execute = flowop_get_execute_func(
			    curr_flowop->type);
			snprintf(curr_flowop->name, sizeof (curr_flowop->name), "%s",
			    list->symbol);
			break;
		case TOKEN_OPTIONS:
			flowop_parse_options(list->symbol, curr_flowop);
			break;
		case TOKEN_RATE:
			if (!in_txn) {
				snprintf(err, sizeof (err),
				    "No current transaction");
				add_error(err);
				return (NULL);
			}
			strlcpy(curr_txn->rate_str, list->symbol, NAMELEN);
			curr_txn->rate_count = string2int(list->symbol);
			break;
		case TOKEN_DURATION:
			if (!in_txn) {
				snprintf(err, sizeof (err),
				    "No current transaction");
				add_error(err);
				return (NULL);
			}
			curr_txn->duration = string2nsec(list->symbol);
			curr_txn->iter = 1;
			break;
		case TOKEN_NTHREADS:
			if (!in_group) {
				snprintf(err, sizeof (err),
				    "No current group");
				add_error(err);
				return (NULL);
			}
			curr_grp->nthreads = string2int(list->symbol);
			curr_grp->strand_flag |= STRAND_TYPE_THREAD;
			break;
		case TOKEN_NPROCESSES:
#ifdef  STRAND_THREAD_ONLY
			snprintf(err, sizeof (err),
				"Processes are not supported on this platform");
			add_error(err);
			return (NULL);
#else
			if (!in_group) {
				snprintf(err, sizeof (err),
				    "No current group");
				add_error(err);
				return (NULL);
			}
			curr_grp->nthreads = string2int(list->symbol);
			curr_grp->strand_flag |= STRAND_TYPE_PROCESS;
			break;
#endif /* STRAND_THREAD_ONLY */
		case TOKEN_ERROR:
			snprintf(err, sizeof (err),
				"Unknown symbol: %s", list->symbol);
			add_error(err);
			return (NULL);
		}
		list = list->next;
	}

	return (&w);
}

workorder_t *
parse_app_profile(char *filename)
{
	struct stat stat_buf;
	int fd;
	int i;
	off_t size;
	char *buffer;
	struct symbol list = { "START", 0, NULL, NULL};
	struct symbol *p;
	workorder_t *w;

	if (stat(filename, &stat_buf) != 0) {
		fprintf(stderr, "Cannot stat file %s: %s\n", filename,
			strerror(errno));
		return (0);
	}
	size = stat_buf.st_size;
	if ((fd = open(filename, O_RDONLY)) == -1) {
		fprintf(stderr, "Cannot open file %s: %s\n", filename,
			strerror(errno));
		return (0);
	}
	if (size == 0) {
		fprintf(stderr, "Zero byte file!\n");
		return (0);
	}
	if ((buffer = calloc(size, sizeof (char))) == NULL) {
		fprintf(stderr, "Cannot malloc %ld bytes: %s\n", (long)size,
			strerror(errno));
		return (0);

	}
	if (read(fd, buffer, size) != size) {
		fprintf(stderr, "Cannot read %ld bytes from %s: %s\n", (long)size,
			filename, strerror(errno));
		return (0);
	}
	buffer[size - 1] = '\0';

	parse(buffer, &list);
#ifdef DEBUG
	print_symbols(list.next);
#endif /* DEBUG */
	w = build_worklist(list.next);
	if (w == NULL || no_errs > 0) {
		print_errors();
	}
	/* Free list.next */
	p = list.next;
	while (p) {
		struct symbol *q = p->next;
		free(p);
		p = q;
	}
	for (i = 0; i < no_errs; i++)
		free(parse_errors[i]);
	free(buffer);
	close(fd);
	if (w == NULL || no_errs > 0)
		return (NULL);

#ifdef DEBUG
	print_workorder_t(w);
#endif
	return (w);
}

#ifdef TEST_PARSE
int
main(int argc, char *argv[])
{
	workorder_t *w;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s xml_file\n", argv[0]);
		exit(1);
	}

	w = parse_app_profile(argv[1]);
	print_workorder_t(w);
}
#endif
