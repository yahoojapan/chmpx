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
 * hashing and are automatically layouted. As a result, it
 * provides a high performance, a high scalability.
 *
 * For the full copyright and license information, please view
 * the license file that was distributed with this source code.
 *
 * AUTHOR:   Takeshi Nakatani
 * CREATE:   Fri Jun 16 2018
 * REVISION:
 *
 */

#ifndef	CHMSSGNUTLS_H
#define	CHMSSGNUTLS_H

#include <gnutls/gnutls.h>

#include "chmeventsock.h"

//---------------------------------------------------------
// Common type for ChmEventSock
//---------------------------------------------------------
typedef	struct chm_gnutls_ss_context*		ChmSSCtx;						// this type is opaque

//---------------------------------------------------------
// ChmSecureSock Class for OpenSSL
//---------------------------------------------------------
class ChmSecureSock
{
	protected:
		static bool			is_self_sigined;								// For DEBUG(but we do not use this for GnuTLS)
		static int			init_count;										// flag for initialization
		static bool			is_verify_peer;									// Verify Peer mode on server
		static chmss_ver_t	ssl_min_ver;									// SSL/TLS minimum version

	protected:
		static bool InitLibrary(const char* CApath, const char* CAfile, bool is_verify_peer);
		static bool FreeLibrary(void);
		static std::string& GetCAPath(void);								// CA directory path if exists
		static std::string& GetCAFile(void);								// CA file path if exists
		static bool SetCA(const char* CApath, const char* CAfile);
		static bool SetMinVersion(gnutls_session_t session);
		static bool LoadCACerts(gnutls_certificate_credentials_t& cert_cred);
		static bool IsSafeSSLContextEx(const char* server_cert, const char* server_prikey, const char* slave_cert, const char* slave_prikey);
		static ChmSSCtx GetSSLContextEx(const char* server_cert, const char* server_prikey, const char* slave_cert, const char* slave_prikey);
		static void FreeSSLContextEx(ChmSSCtx ctx);

	public:
		static void AllowSelfSignedCert(void);								// For only debugging(but we do not use this for GnuTLS)
		static void DenySelfSignedCert(void);
		static const char* LibraryName(void);
		static bool IsCAPathAllowFile(void);
		static bool IsCertAllowName(void);
		static bool SetSslMinVersion(chmss_ver_t ssver);
		static bool SetExtValue(const char* key, const char* value);

		static bool CheckResultSSL(int sock, ChmSSSession sslsession, long action_result, int type, bool& is_retry, bool& is_close, int retrycnt = CHMEVENTSOCK_RETRY_DEFAULT, suseconds_t waittime = CHMEVENTSOCK_TIMEOUT_DEFAULT);
		static ChmSSSession AcceptSSL(ChmSSCtx ctx, int sock, int con_retrycnt = CHMEVENTSOCK_RETRY_DEFAULT, suseconds_t con_waittime = CHMEVENTSOCK_TIMEOUT_DEFAULT);
		static ChmSSSession ConnectSSL(ChmSSCtx ctx, int sock, int con_retrycnt = CHMEVENTSOCK_RETRY_DEFAULT, suseconds_t con_waittime = CHMEVENTSOCK_TIMEOUT_DEFAULT);
		static bool ShutdownSSL(int sock, ChmSSSession sslsession, int con_retrycnt = CHMEVENTSOCK_RETRY_DEFAULT, suseconds_t con_waittime = CHMEVENTSOCK_TIMEOUT_DEFAULT);
		static int Read(ChmSSSession sslsession, void* pbuf, int length);
		static int Write(ChmSSSession sslsession, const void* pbuf, int length);

		ChmSecureSock(const char* CApath = NULL, const char* CAfile = NULL, bool is_verify_peer = false);
		virtual ~ChmSecureSock();

		bool Clean(void);

		bool IsSafeSSLContext(const char* server_cert, const char* server_prikey, const char* slave_cert, const char* slave_prikey);
		ChmSSCtx GetSSLContext(const char* server_cert, const char* server_prikey, const char* slave_cert, const char* slave_prikey);
		void FreeSSLContext(ChmSSCtx ctx);
};

#endif	// CHMSSGNUTLS_H

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
