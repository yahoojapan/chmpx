/*
 * CHMPX
 *
 * Copyright 2014 Yahoo Japan Corporation.
 *
 * CHMPX is inprocess data exchange by MQ with consistent hashing.
 * CHMPX is made for the purpose of the construction of
 * original messaging system and the offer of the client
 * library.
 * CHMPX transfers messages between the client and the server/
 * slave. CHMPX based servers are dispersed by consistent
 * hashing and are automatically laid out. As a result, it
 * provides a high performance, a high scalability.
 *
 * For the full copyright and license information, please view
 * the license file that was distributed with this source code.
 *
 * AUTHOR:   Takeshi Nakatani
 * CREATE:   Mon May 13 2018
 * REVISION:
 *
 */

#ifndef	CHMSS_H
#define	CHMSS_H

//---------------------------------------------------------
// Utility Macros
//---------------------------------------------------------
#define	CVT_ESTR_NULL(pstr)					(CHMEMPTYSTR(pstr) ? NULL : pstr)

//---------------------------------------------------------
// ChmSecureSock class by each SSL/TLS Library
//---------------------------------------------------------
// [NOTE]
// It is easier to make ChmSecureSock class a virtual class with base class.
// However, instead of switching the SSL / TLS library at run time, the
// implementation of the class is fixed at link time, and since it is only
// one entity, it is not virtualized.
//
class ChmSecureSock;

//---------------------------------------------------------
// Typedefs for ChmSecureSock class
//---------------------------------------------------------
typedef	void*								ChmSSSession;

//---------------------------------------------------------
// Common symbols/macros for ChmSecureSock::CheckResultSSL
//---------------------------------------------------------
#define	CHKRESULTSSL_TYPE_CON				0						// SSL_accept / SSL_connect
#define	CHKRESULTSSL_TYPE_RW				1						// SSL_read / SSL_write
#define	CHKRESULTSSL_TYPE_SD				2						// SSL_shutdown
#define	IS_SAFE_CHKRESULTSSL_TYPE(type)		(CHKRESULTSSL_TYPE_CON == type || CHKRESULTSSL_TYPE_RW == type || CHKRESULTSSL_TYPE_SD == type)

//---------------------------------------------------------
// Symbols for ChmSecureSock::SetExtValue method
//---------------------------------------------------------
#define	CHM_NSS_NSSDB_DIR_KEY				"NSSDB_DIR"

//---------------------------------------------------------
// Switch SSL/TLS library
//---------------------------------------------------------
#ifdef  SSL_TLS_TYPE_OPENSSL
// OpenSSL
#include "chmssopenssl.h"

#elif   SSL_TLS_TYPE_NSS
// NSS
#include "chmssnss.h"

#elif   SSL_TLS_TYPE_GNUTLS
// GnuTLS
#include "chmssgnutls.h"
#endif

#endif	// CHMSS_H

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noexpandtab sw=4 ts=4 fdm=marker
 * vim<600: noexpandtab sw=4 ts=4
 */
