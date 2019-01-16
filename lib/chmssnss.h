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

#include <nss.h>
#include <pk11pub.h>

#include <map>
#include <string>

#include "chmeventsock.h"

//---------------------------------------------------------
// Common type for ChmEventSock
//---------------------------------------------------------
typedef	struct chm_nss_ss_context*					ChmSSCtx;					// this type is opaque

//---------------------------------------------------------
// Internal structure and map
//---------------------------------------------------------
typedef struct chm_nss_cert*					ChmSSCert;
typedef std::map<std::string, ChmSSCert>		chmsscertmap_t;
typedef std::list<PK11GenericObject*>			chmpk11list_t;

//---------------------------------------------------------
// ChmSecureSock Class for OpenSSL
//---------------------------------------------------------
class ChmSecureSock
{
	protected:
		static bool				is_self_sigined;								// For DEBUG(but we do not use this for NSS)
		static int				pr_init_count;									// flag for NSPR initialization
		static int				nss_init_count;									// flag for NSS initialization
		static bool				is_verify_peer;									// Verify Peer mode on server
		static chmss_ver_t		ssl_min_ver;									// SSL/TLS minimum version
		static SECMODModule*	PKCS11_LoadedModule;							// loaded module for PKCS11
		static SECMODModule*	PEM_LoadedModule;								// loaded module for PEM
		static PRLock*			FindSlotLock;									// Lock object(by PR) for slot
		static PRLock*			CertMapLock;									// Lock object(by PR) for Cert map

	protected:
		NSSInitContext*			nss_ctx;										// NSS context for both server/slave

	protected:
		static bool InitLibrary(NSSInitContext** ctx, const char* CApath, const char* CAfile, bool is_verify_peer);
		static bool FreeLibrary(NSSInitContext* ctx);
		static bool InitLoadExtModule(void);
		static std::string& GetCAPath(void);									// CA directory path if exists
		static std::string& GetCAFile(void);									// CA file path if exists
		static std::string& GetNssdbDir(void);									// NSSDB directory path in configuration like SSL_DIR environment
		static chmsscertmap_t& GetCertMap(void);								// Cert etc map
		static bool SetCA(const char* CApath, const char* CAfile);
		static bool SetMinVersion(PRFileDesc* model);
		static std::string GetSessionInfo(PRFileDesc* session);
		static bool LoadCACerts(chmpk11list_t& pk11objlist);
		static bool MakeNickname(const char* pCertFile, std::string& strNickname);
		static bool CheckCertNicknameType(const char* pCertFile, const char* pPrivateKeyFile, std::string& strNickname, std::string& strCertKey, bool& isExistFile);
		static PK11GenericObject* GeneratePK11GenericObject(const char* filename, bool is_private_key);
		static bool LoadCertPrivateKey(const char* pCertFile, const char* pPrivateKeyFile, PK11GenericObject** ppk11CertObj, PK11GenericObject** ppk11PKeyObj, CERTCertificate** ppCert, SECKEYPrivateKey** ppPrivateKey);
		static bool SetBlockingMode(PRFileDesc* SSSession, bool is_blocking);
		static SECStatus AuthCertificateCallback(void* arg, PRFileDesc* fd, PRBool checksig, PRBool isServer);
		static SECStatus ClientAuthDataHookCallback(void* arg, PRFileDesc* sock, struct CERTDistNamesStr* caNames, struct CERTCertificateStr** pRetCert, struct SECKEYPrivateKeyStr** pRetKey);
		static SECStatus BadCertCallback(void* arg, PRFileDesc* fd);
		static PRFileDesc* GetModelDescriptor(bool is_server, bool is_verify_peer);
		static bool IsSafeSSLContextEx(const char* server_cert, const char* server_prikey, const char* slave_cert, const char* slave_prikey);
		static ChmSSCtx GetSSLContextEx(const char* server_cert, const char* server_prikey, const char* slave_cert, const char* slave_prikey);
		static void FreeSSLContextEx(ChmSSCtx ctx);
		static void FreeSSLSessionEx(ChmSSSession sslsession);

	public:
		static void AllowSelfSignedCert(void);									// For only debugging
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

#endif	// CHMSSOPENSSL_H

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
