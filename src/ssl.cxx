/* pshs -- SSL/TLS support
 * (c) 2014 Michał Górny
 * 2-clause BSD-licensed
 */

#include "config.h"

#include "ssl.h"

#include <assert.h>
#include <stdio.h>
#include <time.h>

#ifdef HAVE_LIBSSL
#	include <event2/bufferevent.h>
#	include <event2/bufferevent_ssl.h>

#	include <openssl/asn1.h>
#	include <openssl/evp.h>
#	include <openssl/rsa.h>
#	include <openssl/ssl.h>
#	include <openssl/x509.h>
#endif

#ifdef HAVE_LIBSSL
static SSL_CTX* ssl;

static void key_progress_cb(int p, int n, void* arg)
{
	char c;

	switch (p)
	{
		case 0: c = '.'; break;
		case 1: c = '+'; break;
		case 2: c = '*'; break;
		case 3: c = '\n'; break;
		default: c = '?';
	}

	fputc(c, stderr);
}

static struct bufferevent* https_bev_callback(struct event_base* evb, void* data)
{
	SSL_CTX* ctx = (SSL_CTX*) data;

	return bufferevent_openssl_socket_new(evb, -1, SSL_new(ctx),
			BUFFEREVENT_SSL_ACCEPTING, BEV_OPT_CLOSE_ON_FREE);
}
#endif

SSLMod::SSLMod(evhttp* http, const char* extip, bool enable)
	: enabled(false)
{
	if (!enable)
		return;

#ifdef HAVE_LIBSSL

	EVP_PKEY* pkey;
	X509* x509;
	X509_NAME* name;
	RSA* rsa;
	unsigned char sha256_buf[32];
	unsigned int i;

	SSL_load_error_strings();
	SSL_library_init();

	pkey = EVP_PKEY_new();
	if (!pkey)
	{
		fputs("EVP_PKEY_new() failed to allocate new private key.\n", stderr);
		return;
	}

	x509 = X509_new();
	if (!x509)
	{
		fputs("X509_new() failed to allocate new certificate.\n", stderr);
		EVP_PKEY_free(pkey);
		return;
	}

	/* XXX: settable params */
	rsa = RSA_generate_key(2048, RSA_F4, key_progress_cb, 0);
	if (!rsa)
	{
		fputs("RSA_generate_key() failed to generate the private key.\n", stderr);
		X509_free(x509);
		EVP_PKEY_free(pkey);
		return;
	}

	if (!EVP_PKEY_assign_RSA(pkey, rsa))
	{
		fputs("EVP_PKEY_assign_RSA() failed.\n", stderr);
		RSA_free(rsa);
		X509_free(x509);
		EVP_PKEY_free(pkey);
		return;
	}

	if (!X509_set_pubkey(x509, pkey))
	{
		fputs("X509_set_pubkey() failed.\n", stderr);
		X509_free(x509);
		EVP_PKEY_free(pkey);
		return;
	}

	/* X509v3 */
	X509_set_version(x509, 2);
	/* Semi-random serial number to avoid repetitions */
	ASN1_INTEGER_set(X509_get_serialNumber(x509), time(NULL));
	/* Valid for 24 hours */
	X509_gmtime_adj(X509_get_notBefore(x509), 0);
	X509_gmtime_adj(X509_get_notAfter(x509), 60*60*24);

	/* Set subject & issuer */
	name = X509_get_subject_name(x509);

	X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC,
			(const unsigned char*) "pshs", -1, -1, 0);
	X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
			(const unsigned char*) extip, -1, -1, 0);

	/* Self-signed => issuer = subject */
	X509_set_issuer_name(x509, name);

	if (!X509_sign(x509, pkey, EVP_sha512()))
	{
		fputs("X509_sign() failed.\n", stderr);
		X509_free(x509);
		EVP_PKEY_free(pkey);
		return;
	}

	ssl = SSL_CTX_new(SSLv23_server_method());
	if (!ssl)
	{
		fputs("SSL_CTX_new() failed.\n", stderr);
		X509_free(x509);
		EVP_PKEY_free(pkey);
		return;
	}

	if (!SSL_CTX_use_certificate(ssl, x509))
	{
		fputs("SSL_CTX_use_certificate() failed.\n", stderr);
		SSL_CTX_free(ssl);
		X509_free(x509);
		EVP_PKEY_free(pkey);
		return;
	}
	X509_free(x509);

	if (!SSL_CTX_use_PrivateKey(ssl, pkey))
	{
		fputs("SSL_CTX_use_PrivateKey() failed.\n", stderr);
		SSL_CTX_free(ssl);
		EVP_PKEY_free(pkey);
		return;
	}
	EVP_PKEY_free(pkey);

	evhttp_set_bevcb(http, https_bev_callback, ssl);

	/* print fingerprint */
	if (!X509_digest(x509, EVP_sha256(), sha256_buf, &i))
	{
		fputs("X509_digest() failed.\n", stderr);
		SSL_CTX_free(ssl);
		return;
	}

	assert(i == sizeof(sha256_buf));
	fputs("Certificate fingerprint:\n", stderr);
	for (i = 0; i < sizeof(sha256_buf); ++i)
	{
		fprintf(stderr, "%02hhX%c", sha256_buf[i],
				-i % (sizeof(sha256_buf) / 2) == 1 ? '\n' : ' ');
	}

	enabled = true;

#else

	fputs("SSL/TLS support disabled at build time.\n", stderr);

#endif
}

SSLMod::~SSLMod()
{
	if (!enabled)
		return;

#ifdef HAVE_LIBSSL
	SSL_CTX_free(ssl);
#endif
}
