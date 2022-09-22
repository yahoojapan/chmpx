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

#ifndef	CHMSSOPENSSL_H
#define	CHMSSOPENSSL_H

#include <openssl/ssl.h>
#include <openssl/err.h>

#include "chmeventsock.h"

//---------------------------------------------------------
// Common type for ChmEventSock
//---------------------------------------------------------
typedef	SSL_CTX*					ChmSSCtx;

//---------------------------------------------------------
// ChmSecureSock Class for OpenSSL
//---------------------------------------------------------
class ChmSecureSock
{
	protected:
		static int			CHM_SSL_VERIFY_DEPTH;							// SSL cert verify depth
		static const char*	strVerifyCBDataIndex;							// string keyword for data index
		static int			verify_cb_data_index;							// Data index for verify callback function
		static int			ssl_session_id;									// Session ID
		static bool			is_self_sigined;								// For DEBUG
		static int			init_count;										// flag for initialization
		static bool			is_verify_peer;									// Verify Peer mode on server
		static chmss_ver_t	ssl_min_ver;									// SSL/TLS minimum version

	protected:
		ChmSSCtx			svr_sslctx;										// SSL Context for server
		ChmSSCtx			slv_sslctx;										// SSL Context for slave

	protected:
		static bool InitLibrary(const char* CApath, const char* CAfile, bool is_verify_peer);
		static bool FreeLibrary(void);
		static std::string& GetCAPath(void);								// CA directory path if exists
		static std::string& GetCAFile(void);								// CA file path if exists
		static bool SetCA(const char* CApath, const char* CAfile);
		static bool SetMinVersion(SSL_CTX* ctx);
		static std::string GetSessionInfo(SSL* ssl);
		static ChmSSCtx MakeSSLContext(const char* server_cert, const char* server_prikey, const char* slave_cert, const char* slave_prikey);
		static int VerifyCallBackSSL(int preverify_ok, X509_STORE_CTX* store_ctx);
		static ChmSSSession HandshakeSSL(ChmSSCtx ctx, int sock, bool is_accept, int con_retrycnt = CHMEVENTSOCK_RETRY_DEFAULT, suseconds_t con_waittime = CHMEVENTSOCK_TIMEOUT_DEFAULT);

	public:
		static void AllowSelfSignedCert(void);								// For only debugging
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

		explicit ChmSecureSock(const char* CApath = NULL, const char* CAfile = NULL, bool is_verify_peer = false);
		virtual ~ChmSecureSock();

		bool Clean(void);

		bool IsSafeSSLContext(const char* server_cert, const char* server_prikey, const char* slave_cert, const char* slave_prikey);
		ChmSSCtx GetSSLContext(const char* server_cert, const char* server_prikey, const char* slave_cert, const char* slave_prikey);
		void FreeSSLContext(ChmSSCtx ctx);
};

#endif	// CHMSSOPENSSL_H

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
