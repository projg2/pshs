/* pshs -- SSL/TLS support
 * (c) 2014 Michał Górny
 * 2-clause BSD-licensed
 */

#include "config.h"

#include "ssl.h"

#include <functional>
#include <memory>
#include <stdexcept>

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
static std::unique_ptr<SSL_CTX, std::function<void(SSL_CTX*)>> ssl;

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

	X509_NAME* name;
	unsigned char sha256_buf[32];
	unsigned int i;

	SSL_load_error_strings();
	SSL_library_init();

	std::unique_ptr<EVP_PKEY, std::function<void(EVP_PKEY*)>>
		pkey{EVP_PKEY_new(), EVP_PKEY_free};
	std::unique_ptr<X509, std::function<void(X509*)>>
		x509{X509_new(), X509_free};
	/* XXX: settable params */
	std::unique_ptr<RSA, std::function<void(RSA*)>>
		rsa{RSA_generate_key(2048, RSA_F4, key_progress_cb, 0), RSA_free};

	if (!pkey || !x509 || !rsa)
		throw std::bad_alloc();

	if (!EVP_PKEY_assign_RSA(pkey.get(), rsa.get()))
		throw std::runtime_error("EVP_PKEY_assign_RSA() failed");

	if (!X509_set_pubkey(x509.get(), pkey.get()))
		throw std::runtime_error("X509_set_pubkey() failed");

	/* X509v3 */
	X509_set_version(x509.get(), 2);
	/* Semi-random serial number to avoid repetitions */
	ASN1_INTEGER_set(X509_get_serialNumber(x509.get()), time(NULL));
	/* Valid for 24 hours */
	X509_gmtime_adj(X509_get_notBefore(x509.get()), 0);
	X509_gmtime_adj(X509_get_notAfter(x509.get()), 60*60*24);

	/* Set subject & issuer */
	name = X509_get_subject_name(x509.get());

	X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC,
			(const unsigned char*) "pshs", -1, -1, 0);
	X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
			(const unsigned char*) extip, -1, -1, 0);

	/* Self-signed => issuer = subject */
	X509_set_issuer_name(x509.get(), name);

	if (!X509_sign(x509.get(), pkey.get(), EVP_sha512()))
		throw std::runtime_error("X509_sign() failed");

	ssl = {SSL_CTX_new(SSLv23_server_method()), SSL_CTX_free};
	if (!ssl)
		throw std::bad_alloc();

	if (!SSL_CTX_use_certificate(ssl.get(), x509.get()))
		throw std::runtime_error("SSL_CTX_use_certificate() failed");

	if (!SSL_CTX_use_PrivateKey(ssl.get(), pkey.get()))
		throw std::runtime_error("SSL_CTX_use_PrivateKey() failed");

	evhttp_set_bevcb(http, https_bev_callback, ssl.get());

	/* print fingerprint */
	if (!X509_digest(x509.get(), EVP_sha256(), sha256_buf, &i))
		throw std::runtime_error("X509_digest() failed");

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
	ssl.reset(nullptr);
#endif
}
