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

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>		/* basic socket definitions */
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>		/* basic system data types */
#include <sys/stat.h>
#include <unistd.h>
#include <netinet/tcp.h>
#include <inttypes.h>
#include <openssl/ssl.h>
#include <openssl/engine.h>
#include <pthread.h>
#include <string.h>
#include "logging.h"
#include "flowops.h"
#include "parse.h"
#include "workorder.h"
#include "protocol.h"
#include "generic.h"
#include "ssl.h"
#include "main.h"
#include "generic.h"


#define	LISTENQ		1024	/* 2nd argument to listen() */
#define	SA	struct sockaddr	/* typecasts of pointer arguments: */
#define	SOCK_PORT(sin) ((sin).sin_port)
#define	PASS  "password"

/* Structure private to the ssl protocol */
typedef struct ssl_private {
	SSL *ssl;
} ssl_private_t;

extern options_t options;

static protocol_t *protocol_ssl_new();
/* Utility functions */
static SSL_CTX *initialize_ctx(char *keyfile, char *password,
	const char *method);
static int pwd_cb(char *buf, int num, int rwflag, void *userdata);
static int load_engine(const char *engine_id);
static SSL_CTX *ctx;


/*
 * Openssl is not threadsafe if we do not provide the following
 * two callbacks
 */

static pthread_mutex_t *ssl_locks;

static void
locking_function(int mode, int n, char *file, int line)
{
	if (mode & CRYPTO_LOCK) {
		(void) pthread_mutex_lock(&(ssl_locks[n]));
	} else {
		(void) pthread_mutex_unlock(&(ssl_locks[n]));
	}
}

static unsigned long
id_function(void)
{
	return ((unsigned long) MY_THREAD_ID());
}

static int
my_ssl_error(SSL *ssl, int sz)
{
	char error[128];
	int ret;

	switch (ret = SSL_get_error(ssl, sz)) {
	case SSL_ERROR_NONE:
		return (UPERF_SUCCESS);
		/*
		 * These errors usually occur when duration expires.
		 * ignore them
		 */
	case SSL_ERROR_WANT_READ:
	case SSL_ERROR_WANT_WRITE:
	case SSL_ERROR_ZERO_RETURN:
		errno = EINTR;
		return (-1);
	case SSL_ERROR_SYSCALL:
		return (-1);
	default:
		printf("SSL Error ret=%d src=%s\n", ret,
		    ERR_error_string(ret, error));
		return (-1);
		break;
	}
	return (-1);

}

static int
file_present(char *file)
{
	struct stat rbuf;
	if (stat(file, &rbuf) != 0) {
		return (-1);
	}
	if (!S_ISREG(rbuf.st_mode)) {
		return (-1);
	}
	return (0);
}

static void
ssl_cleanup(void)
{
	ENGINE_cleanup();
}

/* Called only once during beginning of the program */
int
ssl_init(void *arg)
{
	int i;

	if (IS_MASTER(options)) {
		if (file_present("server.pem") == 0)
			ctx = initialize_ctx("server.pem", PASS, NULL);
		else if (file_present("../server.pem") == 0)
			ctx = initialize_ctx("../server.pem", PASS, NULL);
		else
			printf("Can't load server.pem\n");
	} else {
		if (file_present("client.pem") == 0)
			ctx = initialize_ctx("client.pem", PASS, NULL);
		else if (file_present("../client.pem") == 0)
			ctx = initialize_ctx("../client.pem", PASS, NULL);
		else
			printf("Can't load client.pem\n");
	}
	if (ctx == NULL)
		return (1);

	ssl_locks = malloc(CRYPTO_num_locks() * sizeof (pthread_mutex_t));
	for (i = 0; i < CRYPTO_num_locks(); i++) {
		pthread_mutex_init(&ssl_locks[i], 0);
	}
	CRYPTO_set_id_callback((unsigned long (*)())id_function);
	CRYPTO_set_locking_callback((void (*)())locking_function);

	/*
	 * We also register an atexit function to cleanup the ENGINE
	 * datastructures
	 */
	atexit(ssl_cleanup);
	return (0);
}

static ssize_t
ssl_read(SSL *ssl, char *ptr, size_t n)
{
	ssize_t nread;

	nread = SSL_read(ssl, ptr, n);
	return (my_ssl_error(ssl, nread) == 0 ? nread : -1);
}

static ssize_t
ssl_write(SSL *ssl, char *ptr, size_t n)
{
	ssize_t nwritten;

	nwritten = SSL_write(ssl, ptr, n);
	return (my_ssl_error(ssl, nwritten) == 0 ? nwritten : -1);
}

static int
protocol_ssl_listen(protocol_t *p, void *o)
{
	char msg[128];

	if (generic_socket(p, AF_INET6, IPPROTO_TCP) != UPERF_SUCCESS) {
		if (generic_socket(p, AF_INET, IPPROTO_TCP) != UPERF_SUCCESS) {
			(void) snprintf(msg, 128, "%s: Cannot create socket", "tcp");
			uperf_log_msg(UPERF_LOG_ERROR, errno, msg);
			return (UPERF_FAILURE);
		}
	}
	set_tcp_options(p->fd, (flowop_options_t *)o);

	return (generic_listen(p, IPPROTO_TCP, o));
}

static protocol_t *
protocol_ssl_accept(protocol_t *p, void *options)
{
	protocol_t *newp;
	struct sockaddr_in remote;
	socklen_t addrlen;
	int ret;
	ssl_private_t *new_ssl_p;
	char hostname[128];
	flowop_options_t *flowop_options = (flowop_options_t *) options;
	BIO *sbio;

	newp = protocol_ssl_new();
	new_ssl_p = (ssl_private_t *) newp->_protocol_p;
	addrlen = (socklen_t) sizeof (remote);
	uperf_debug("ssl - ssl obj waiting for accept\n");
	newp->fd = accept(p->fd, (struct sockaddr *) &remote,
		&addrlen);
	if (newp->fd < 0) {
		uperf_log_msg(UPERF_LOG_ERROR, errno, "accept");
		return (NULL);
	}
	if (getnameinfo((const struct sockaddr *) & remote, addrlen,
			hostname, sizeof (hostname), NULL, 0, 0) == 0) {
		uperf_debug("ssl - Connection from %s:%d\n", hostname,
			SOCK_PORT(remote));
		strlcpy(newp->host, hostname, sizeof (newp->host));
		newp->port = SOCK_PORT(remote);
	}
	if (flowop_options) {
		if ((load_engine(flowop_options->engine)) == -1) {
			uperf_info(
"ssl - Engine %s does NOT exist! Using the default OpenSSL softtoken",
			flowop_options->engine);
		}
	}
	sbio = BIO_new_socket(newp->fd, BIO_NOCLOSE);
	if (!(new_ssl_p->ssl = SSL_new(ctx))) {
		uperf_log_msg(UPERF_LOG_ERROR, 0, "SSL_new error");
		return (NULL);
	}
	SSL_set_bio(new_ssl_p->ssl, sbio, sbio);

	ret = SSL_accept(new_ssl_p->ssl);
	if (my_ssl_error(new_ssl_p->ssl, ret) == 0) {
		return (newp);
	} else {
		return (0);
	}
}

static int
protocol_ssl_connect(protocol_t *p, void *options)
{
	struct sockaddr_storage serv;
	BIO *sbio;
	int status;

	ssl_private_t *ssl_p = (ssl_private_t *) p->_protocol_p;
	flowop_options_t *flowop_options = (flowop_options_t *) options;

	uperf_debug("ssl - Connecting to %s:%d\n", p->host, p->port);

	if (name_to_addr(p->host, &serv)) {
		/* Error is already reported by name_to_addr, so just return */
		return (UPERF_FAILURE);
	}
	if (generic_socket(p, serv.ss_family, IPPROTO_TCP) < 0) {
		return (UPERF_FAILURE);
	}
	set_tcp_options(p->fd, flowop_options);
	if (generic_connect(p, &serv) < 0) {
		return (UPERF_FAILURE);
	}
	if (flowop_options && (strcmp(flowop_options->engine, ""))) {
		status = load_engine(flowop_options->engine);
		if (status == -1) {
			uperf_info(
"ssl - Engine %s does NOT exist! Using the default OpenSSL softtoken",
			flowop_options->engine);
		}
	}
	if ((ssl_p->ssl = SSL_new(ctx)) == NULL) {
		ulog_err("Error initializng SSL");
		return (-1);
	}
	sbio = BIO_new_socket(p->fd, BIO_NOCLOSE);
	SSL_set_bio(ssl_p->ssl, sbio, sbio);

	status = SSL_connect(ssl_p->ssl);
	if (status <= 0) {
		uperf_log_msg(UPERF_LOG_ERROR, 0, "ssl connect error");
		return (-1);
	}
	return (0);
}

static int
protocol_ssl_disconnect(protocol_t *p)
{
	int r;
	ssl_private_t *ssl_p = (ssl_private_t *) p->_protocol_p;

	if (!p)
		return (0);

	/*
	 * workaround for the bug calling protocol_disconnect multiple
	 * times
	 */
	if (ssl_p->ssl != NULL) {
		r = SSL_shutdown(ssl_p->ssl);

		if ((!r) && (IS_SLAVE(options))) {
			shutdown(p->fd, 1);
			r = SSL_shutdown(ssl_p->ssl);
		}
		switch (r) {
		case 1:
		case 0:
		case -1:
			break;
		default:
			uperf_log_msg(UPERF_LOG_ERROR, 0,
				"ssl shutdown failed");
			return (-1);
		}

		if (!ssl_p->ssl)
			SSL_free(ssl_p->ssl);
	}			/* workaround ends */
	return (close(p->fd));
}

static int
protocol_ssl_read(protocol_t *p, void *buffer, int size, void *options)
{
	ssl_private_t *ssl_p = (ssl_private_t *) p->_protocol_p;

	uperf_debug("ssl - Reading %d bytes from %s:%d\n", size,
            p->host[0] == '\0' ? "Unknown" : p->host,
            p->port);

	return (ssl_read(ssl_p->ssl, buffer, size));
}

static int
protocol_ssl_write(protocol_t *p, void *buffer, int size, void *options)
{
	ssl_private_t *ssl_p = (ssl_private_t *) p->_protocol_p;

	uperf_debug("ssl - Writing %d bytes to %s:%d\n", size,
            p->host[0] == '\0' ? "Unknown" : p->host,
		p->port);
	return (ssl_write(ssl_p->ssl, buffer, size));
}

static int
protocol_ssl_undefined(protocol_t *p, void *options)
{
	uperf_error("Undefined function in protocol called\n");
	return (-1);
}


SSL_CTX *
initialize_ctx(char *keyfile, char *password, const char *method)
{
#if OPENSSL_VERSION_NUMBER > 0x000908fff
	const SSL_METHOD *meth;
#else
	SSL_METHOD *meth;
#endif
	SSL_CTX *ctx;

	SSL_library_init();

	if (method != NULL && strcasecmp(method, "tls") == 0) {
		meth = TLS_method();
	} else
		meth = SSLv23_method();

	if (!(ctx = SSL_CTX_new(meth))) {
		printf("Error getting SSL CTX\n");
		return (0);
	}
	if (!SSL_CTX_use_certificate_chain_file(ctx, keyfile)) {
		printf("Error getting SSL CTX:1\n");
		return (0);
	}
	SSL_CTX_set_default_passwd_cb(ctx, pwd_cb);

	if (!SSL_CTX_use_PrivateKey_file(ctx, keyfile, SSL_FILETYPE_PEM)) {
		printf("Error getting SSL CTX:2\n");
		return (0);
	}
	if (!SSL_CTX_check_private_key(ctx)) {
		printf("Error getting SSL CTX:3\n");
		return (0);
	}

	return (ctx);
}

static int
pwd_cb(char *buf, int num, int rwflag, void *userdata)
{
	char *pass = PASS;

	if (num < strlen(pass) + 1) {
		return (0);
	}
	strcpy(buf, pass);
	return (strlen(pass));
}

static int
load_engine(const char *engine_id)
{
	ENGINE *e;

	if (!engine_id)
		return (-1);
	ENGINE_load_builtin_engines();
	e = ENGINE_by_id(engine_id);
	if (!e) {
		return (-1);
	}
	ENGINE_register_complete(e);
	return (0);
}

static protocol_t *
protocol_ssl_new()
{
	protocol_t *newp;
	ssl_private_t *new_ssl_p;

	if ((newp = calloc(1, sizeof(protocol_t))) == NULL) {
		perror("calloc");
		return (NULL);
	}
	if ((new_ssl_p = calloc(1, sizeof(ssl_private_t))) == NULL) {
		perror("calloc");
		return (NULL);
	}
	newp->_protocol_p = new_ssl_p;
	newp->connect = protocol_ssl_connect;
	newp->disconnect = protocol_ssl_disconnect;
	newp->listen = protocol_ssl_listen;
	newp->accept = protocol_ssl_accept;
	newp->read = protocol_ssl_read;
	newp->write = protocol_ssl_write;
	newp->wait = protocol_ssl_undefined;
	newp->type = PROTOCOL_SSL;
	return (newp);
}

protocol_t *
protocol_ssl_create(char *host, int port)
{
	protocol_t *newp;

	if ((newp = protocol_ssl_new()) == NULL) {
		return (NULL);
	}
	if (strlen(host) == 0) {
		(void) strlcpy(newp->host, "lOcAlHoSt", MAXHOSTNAME);
	} else {
		(void) strlcpy(newp->host, host, MAXHOSTNAME);
	}
	newp->port = port;
	uperf_debug("ssl - Creating SSL Protocol to %s (%s):%d\n", host, port);
	return (newp);
}

void
ssl_fini(protocol_t *p)
{
	if (!p) {
		return;
	}
	protocol_ssl_disconnect(p);
	if (p->_protocol_p) {
		free(p->_protocol_p);
	}
	free(p);
	uperf_debug("ssl - destroyed ssl object successfully\n");
}
