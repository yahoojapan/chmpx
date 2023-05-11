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
 * CREATE:   Fri Jun 16 2018
 * REVISION:
 *
 */

#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>

#include "chmcommon.h"
#include "chmssgnutls.h"
#include "chmutil.h"
#include "chmdbg.h"

using namespace	std;

//------------------------------------------------------
// Utilities/Symbols for GnuTLS
//------------------------------------------------------
//
// For error code and string
//
#define	CHM_GNUTLS_ERR_PRN_FORM				" [GnuTLS Error Info(%d: %s)] "
#define	CHM_GNUTLS_ERR_PRN_ARGS(error)		error, gnutls_strerror(error)

//
// Common Cipher
//
#define	CHM_GNUTLS_CIPHERS_PREFIX			"NORMAL:-ARCFOUR-128:-CTYPE-ALL:+CTYPE-X509"

//------------------------------------------------------
// Structure
//------------------------------------------------------
//
// CHMPX SSL Context structure for GnuTLS
//
// This structure pointer is typedef as ChmSSCtx which is I/F to ChmEventSock.
//
typedef struct chm_gnutls_ss_context{
	string	strServerCert;
	string	strServerPKey;
	string	strSlaveCert;
	string	strSlavePKey;

	explicit chm_gnutls_ss_context(const char* pServerCert = NULL, const char* pServerPKey = NULL, const char* pSlaveCert = NULL, const char* pSlavePKey = NULL) : 
		strServerCert(CHMEMPTYSTR(pServerCert) ? "" : pServerCert), strServerPKey(CHMEMPTYSTR(pServerPKey) ? "" : pServerPKey), strSlaveCert(CHMEMPTYSTR(pSlaveCert) ? "" : pSlaveCert), strSlavePKey(CHMEMPTYSTR(pSlavePKey) ? "" : pSlavePKey)
	{
	}
	explicit chm_gnutls_ss_context(const struct chm_gnutls_ss_context* other) :
		strServerCert(other ? other->strServerCert : ""), strServerPKey(other ? other->strServerPKey : ""), strSlaveCert(other ? other->strSlaveCert : ""), strSlavePKey(other ? other->strSlavePKey : "")
	{
	}

	bool set(bool is_server, const char* pcert, const char* pkey)
	{
		if(CHMEMPTYSTR(pcert)){
			ERR_CHMPRN("Parameters are wrong.");
			return false;
		}
		if(is_server){
			strServerCert	= CHMEMPTYSTR(pcert) ? "" : pcert;
			strServerPKey	= CHMEMPTYSTR(pkey) ? "" : pkey;
		}else{
			strSlaveCert	= CHMEMPTYSTR(pcert) ? "" : pcert;
			strSlavePKey	= CHMEMPTYSTR(pkey) ? "" : pkey;
		}
		return true;
	}

	bool set(const char* pservercert, const char* pserverkey, const char* pslavecert, const char* pslavekey)
	{
		if(!set(true, pservercert, pserverkey) || !set(false, pslavecert, pslavekey)){
			return false;
		}
		return true;
	}
}ChmSSCtxEnt;

//
// Session Structure
//
// [NOTE]
// ChmSSSession is a pointer type casted void* to ChmSSSessionEnt structure.
// And we we use default priority, then this structure has gnutls_priority_t
// variables but it is not used now. If you do not use default, you can set
// gnutls_priority_t value.
//
// cppcheck-suppress noOperatorEq
// cppcheck-suppress noCopyConstructor
typedef struct chm_gnutls_session{
	bool								is_server;
	ChmSSCtx							SSCtx;
	gnutls_certificate_credentials_t	cert_cred;
	gnutls_session_t					session;
	gnutls_priority_t					priority_cache;

	explicit chm_gnutls_session(bool is_svr = false, ChmSSCtx ctx = NULL, gnutls_certificate_credentials_t cert = NULL, gnutls_session_t ses = NULL, gnutls_priority_t pricache = NULL) :
		is_server(is_svr), SSCtx(NULL), cert_cred(cert), session(ses), priority_cache(pricache)
	{
		// cppcheck-suppress noOperatorEq
		// cppcheck-suppress noCopyConstructor
		SSCtx = new ChmSSCtxEnt(ctx);
	}

	~chm_gnutls_session()
	{
		clear();
	}

	void clear(void)
	{
		// [NOTE] do not change is_server value.

		CHM_Delete(SSCtx);

		if(priority_cache){
			gnutls_priority_deinit(priority_cache);
			priority_cache = NULL;
		}
		if(session){
			gnutls_deinit(session);
			session = NULL;
		}
		if(cert_cred){
			gnutls_certificate_free_credentials(cert_cred);
			cert_cred = NULL;
		}
	}

	void set(ChmSSCtx ctx)
	{
		CHM_Delete(SSCtx);
		SSCtx = new ChmSSCtxEnt(ctx);
	}

	void set(gnutls_certificate_credentials_t cert)
	{
		if(cert_cred){
			gnutls_certificate_free_credentials(cert_cred);
		}
		cert_cred = cert;
	}

	void set(gnutls_session_t ses)
	{
		if(session){
			gnutls_deinit(session);
		}
		session = ses;
	}

	void set(gnutls_priority_t pricache)
	{
		if(priority_cache){
			gnutls_priority_deinit(priority_cache);
		}
		priority_cache = pricache;
	}

	void set(bool is_svr, ChmSSCtx ctx, gnutls_certificate_credentials_t cert, gnutls_session_t ses, gnutls_priority_t pricache)
	{
		is_server = is_svr;
		set(ctx);
		set(cert);
		set(ses);
		set(pricache);
	}
}ChmSSSessionEnt;

//------------------------------------------------------
// Class Variables
//------------------------------------------------------
bool		ChmSecureSock::is_self_sigined			= false;
int			ChmSecureSock::init_count				= 0;
bool		ChmSecureSock::is_verify_peer			= false;
chmss_ver_t	ChmSecureSock::ssl_min_ver				= CHM_SSLTLS_VER_DEFAULT;	// SSL/TLS minimum version

//------------------------------------------------------
// Class Methods
//------------------------------------------------------
bool ChmSecureSock::InitLibrary(const char* CApath, const char* CAfile, bool is_verify_peer)
{
	if(0 < ChmSecureSock::init_count){
		++(ChmSecureSock::init_count);
		return true;
	}
	++(ChmSecureSock::init_count);

	// [NOTE]
	// Since GnuTLS 3.3.0 this function is no longer necessary to be explicitly called.
	// But we call this.
	//
	int	resgnutls;
	if(GNUTLS_E_SUCCESS != (resgnutls = gnutls_global_init())){
		ERR_CHMPRN("Failed initializing by gnutls_global_init" CHM_GNUTLS_ERR_PRN_FORM, CHM_GNUTLS_ERR_PRN_ARGS(resgnutls));
		--(ChmSecureSock::init_count);
		return false;
	}

	if(!ChmSecureSock::SetCA(CApath, CAfile)){
		ERR_CHMPRN("Wrong CA parameters: CA path=\"%s\", CA file=\"%s\"", (CHMEMPTYSTR(CApath) ? "null" : CApath), (CHMEMPTYSTR(CAfile) ? "null" : CAfile));
		return false;
	}

	// verify peer mode on server
	ChmSecureSock::is_verify_peer = is_verify_peer;

	return true;
}

bool ChmSecureSock::FreeLibrary(void)
{
	int	tmpcnt = ChmSecureSock::init_count;
	if(0 < ChmSecureSock::init_count){
		--(ChmSecureSock::init_count);
	}
	if(tmpcnt == ChmSecureSock::init_count || 0 < ChmSecureSock::init_count){
		return true;
	}

	// [NOTE]
	// Since GnuTLS 3.3.0 this function is no longer necessary to be explicitly called.
	// But we call this.
	//
	gnutls_global_deinit();

	return true;
}

// [NOTE]
// To avoid static object initialization order problem(SIOF)
//
string& ChmSecureSock::GetCAPath(void)
{
	static string	ca_path;
	return ca_path;
}

// [NOTE]
// To avoid static object initialization order problem(SIOF)
//
string& ChmSecureSock::GetCAFile(void)
{
	static string	ca_file;
	return ca_file;
}

bool ChmSecureSock::SetCA(const char* CApath, const char* CAfile)
{
	if(!CHMEMPTYSTR(CApath) && !CHMEMPTYSTR(CAfile)){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}
	if(!CHMEMPTYSTR(CApath)){
		ChmSecureSock::GetCAPath().assign(CApath);
	}
	if(!CHMEMPTYSTR(CAfile)){
		ChmSecureSock::GetCAFile().assign(CAfile);
	}
	return true;
}

const char* ChmSecureSock::LibraryName(void)
{
	return "GnuTLS";
}

bool ChmSecureSock::IsCAPathAllowFile(void)
{
	return true;
}

bool ChmSecureSock::IsCertAllowName(void)
{
	return false;
}

bool ChmSecureSock::SetMinVersion(gnutls_session_t session)
{
	if(!session){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	int			resgnutls;
	const char*	errdetail	= NULL;
	string		strPriority	= CHM_GNUTLS_CIPHERS_PREFIX;

	// set(initialize) priority
	if(GNUTLS_E_SUCCESS != (resgnutls = gnutls_set_default_priority(session))){
		ERR_CHMPRN("Failed to set default priority" CHM_GNUTLS_ERR_PRN_FORM, CHM_GNUTLS_ERR_PRN_ARGS(resgnutls));
		return false;
	}

	// set new priority list
	switch(ChmSecureSock::ssl_min_ver){
		case	CHM_SSLTLS_VER_TLSV1_3:
			strPriority += ":-VERS-SSL3.0:+VERS-TLS-ALL:-VERS-TLS1.0:-VERS-TLS1.1:-VERS-TLS1.2";
			break;

		case	CHM_SSLTLS_VER_TLSV1_2:
			strPriority += ":-VERS-SSL3.0:+VERS-TLS-ALL:-VERS-TLS1.0:-VERS-TLS1.1";
			break;

		case	CHM_SSLTLS_VER_TLSV1_1:
			strPriority += ":-VERS-SSL3.0:+VERS-TLS-ALL:-VERS-TLS1.0";
			break;

		case	CHM_SSLTLS_VER_TLSV1_0:
		case	CHM_SSLTLS_VER_DEFAULT:		// Default is after TLS v1.0
			strPriority += ":-VERS-SSL3.0:+VERS-TLS-ALL";
			break;

		case	CHM_SSLTLS_VER_SSLV3:
			strPriority += ":+VERS-TLS-ALL:+VERS-SSL3.0";
			break;

		case	CHM_SSLTLS_VER_SSLV2:
			MSG_CHMPRN("NOTICE - SSLv2 is specified for SSL/TLS minimum version, but GnuTLS does not allow it.");
			strPriority += ":+VERS-TLS-ALL:+VERS-SSL3.0";
			break;

		default:
			ERR_CHMPRN("SSL/TLS minimum version value(%s) is something wrong, thus we use %s setting as default", CHM_GET_STR_SSLTLS_VERSION(ChmSecureSock::ssl_min_ver), CHM_GET_STR_SSLTLS_VERSION(CHM_SSLTLS_VER_SSLV3));
			strPriority += ":+VERS-TLS-ALL:+VERS-SSL3.0";
			break;
	}

	// set priority list
	if(GNUTLS_E_SUCCESS != (resgnutls = gnutls_priority_set_direct(session, strPriority.c_str(), &errdetail))){
		if(GNUTLS_E_INVALID_REQUEST == resgnutls){
			ERR_CHMPRN("Failed to set SSL/TLS minimum version, error priolity string is %s" CHM_GNUTLS_ERR_PRN_FORM, CHMEMPTYSTR(errdetail) ? "(null)" : errdetail, CHM_GNUTLS_ERR_PRN_ARGS(resgnutls));
		}else{
			ERR_CHMPRN("Failed to set SSL/TLS minimum version" CHM_GNUTLS_ERR_PRN_FORM, CHM_GNUTLS_ERR_PRN_ARGS(resgnutls));
		}
		return false;
	}
	return true;
}

bool ChmSecureSock::SetSslMinVersion(chmss_ver_t ssver)
{
	if(ssver < CHM_SSLTLS_VER_DEFAULT || CHM_SSLTLS_VER_MAX < ssver){
		ERR_CHMPRN("Parameter is wrong SSL/TLS minimum protocol version(%s)", CHM_GET_STR_SSLTLS_VERSION(ssver));
		return false;
	}
	ChmSecureSock::ssl_min_ver = ssver;

	return true;
}

bool ChmSecureSock::SetExtValue(const char* key, const char* value)
{
	// Nothing to do
	(void)key;
	(void)value;

	return true;
}

bool ChmSecureSock::LoadCACerts(gnutls_certificate_credentials_t& cert_cred)
{
	int	resgnutls;

	// set default system trusted CA certs
	if(ChmSecureSock::GetCAPath().empty() && ChmSecureSock::GetCAFile().empty()){
		if(0 > (resgnutls = gnutls_certificate_set_x509_system_trust(cert_cred))){
			if(GNUTLS_E_UNIMPLEMENTED_FEATURE == resgnutls){
				MSG_CHMPRN("Not support gnutls_certificate_set_x509_system_trust" CHM_GNUTLS_ERR_PRN_FORM, CHM_GNUTLS_ERR_PRN_ARGS(resgnutls));
			}else{
				WAN_CHMPRN("Failed to load system trusted CA" CHM_GNUTLS_ERR_PRN_FORM ", but continue...", CHM_GNUTLS_ERR_PRN_ARGS(resgnutls));
			}
		}else{
			if(0 == resgnutls){
				WAN_CHMPRN("No CA certification is loaded.");
			}else{
				MSG_CHMPRN("CA certifications(%d) are loaded.", resgnutls);
			}
		}
	}else{
		if(!ChmSecureSock::GetCAPath().empty()){
			// [NOTE]
			// We should use gnutls_certificate_set_x509_trust_dir() function, but we want to
			// get detail error for each failed cert.
			// Then we open CA directory and load each certs by manually here.
			//
			DIR*			pdir;
			struct dirent*	dent;
			if(NULL == (pdir = opendir(ChmSecureSock::GetCAPath().c_str()))){
				ERR_CHMPRN("Could not open directory(%s) for CA certs by errno(%d).", ChmSecureSock::GetCAPath().c_str(), errno);
				return false;
			}
			while(NULL != (dent = readdir(pdir))){
				if(0 == strcmp(dent->d_name, ".") || 0 == strcmp(dent->d_name, "..")){
					continue;
				}
				string	certfile = ChmSecureSock::GetCAPath() + string("/") + dent->d_name;

				if(0 > (resgnutls = gnutls_certificate_set_x509_trust_file(cert_cred, certfile.c_str(), GNUTLS_X509_FMT_PEM))){
					WAN_CHMPRN("Failed to load CA file(%s) in CA directory(%s)" CHM_GNUTLS_ERR_PRN_FORM ", but continue...", dent->d_name, ChmSecureSock::GetCAPath().c_str(), CHM_GNUTLS_ERR_PRN_ARGS(resgnutls));
				}else{
					if(0 == resgnutls){
						WAN_CHMPRN("No CA certification is loaded from CA file(%s) in CA directory(%s).", dent->d_name, ChmSecureSock::GetCAPath().c_str());
					}else{
						MSG_CHMPRN("CA certifications(%d) are loaded from CA file(%s) in CA directory(%s).", resgnutls, dent->d_name, ChmSecureSock::GetCAPath().c_str());
					}
				}
			}
			if(0 != closedir(pdir)){
				ERR_CHMPRN("Failed to close directory(%s) for CA certs by errno(%d), but continue...", ChmSecureSock::GetCAPath().c_str(), errno);
			}
		}
		if(!ChmSecureSock::GetCAFile().empty()){
			if(0 > (resgnutls = gnutls_certificate_set_x509_trust_file(cert_cred, ChmSecureSock::GetCAFile().c_str(), GNUTLS_X509_FMT_PEM))){
				ERR_CHMPRN("Failed to load CA file(%s)" CHM_GNUTLS_ERR_PRN_FORM, ChmSecureSock::GetCAFile().c_str(), CHM_GNUTLS_ERR_PRN_ARGS(resgnutls));
				return false;
			}
			if(0 == resgnutls){
				ERR_CHMPRN("No CA certification is loaded from CA file(%s).", ChmSecureSock::GetCAPath().c_str());
				return false;
			}
			MSG_CHMPRN("CA certifications(%d) are loaded from CA file(%s).", resgnutls, ChmSecureSock::GetCAPath().c_str());
		}
	}
	return true;
}

bool ChmSecureSock::CheckResultSSL(int sock, ChmSSSession sslsession, long action_result, int type, bool& is_retry, bool& is_close, int retrycnt, suseconds_t waittime)
{
	if(CHM_INVALID_SOCK == sock || !sslsession || !IS_SAFE_CHKRESULTSSL_TYPE(type)){
		ERR_CHMPRN("Parameters are wrong.");
		is_retry = false;
		return false;
	}
	ChmSSSessionEnt*	ssl = reinterpret_cast<ChmSSSessionEnt*>(sslsession);

	if(CHMEVENTSOCK_RETRY_DEFAULT == retrycnt){
		// cppcheck-suppress uselessAssignmentPtrArg
		// cppcheck-suppress uselessAssignmentArg
		retrycnt = ChmEventSock::DEFAULT_RETRYCNT;
		waittime = ChmEventSock::DEFAULT_WAIT_SOCKET;
	}

	bool	result = true;
	if(action_result < 0){
		// something error is occurred
		switch(action_result){
			case	GNUTLS_E_INTERRUPTED:
			case	GNUTLS_E_AGAIN:
				// both send and recv need to retry.
				DMP_CHMPRN("SSL action result(%ld) needs retry to do" CHM_GNUTLS_ERR_PRN_FORM, action_result, CHM_GNUTLS_ERR_PRN_ARGS(static_cast<int>(action_result)));
				is_retry	= true;
				is_close	= false;
				result		= false;
				break;

			case	GNUTLS_E_REHANDSHAKE:
				if(ssl->is_server){
					MSG_CHMPRN("SSL action result(%ld) is got on server" CHM_GNUTLS_ERR_PRN_FORM ", maybe server is going to close.", action_result, CHM_GNUTLS_ERR_PRN_ARGS(static_cast<int>(action_result)));
				}else{
					MSG_CHMPRN("SSL action result(%ld) is got on client(slave)" CHM_GNUTLS_ERR_PRN_FORM ", maybe this message is ignore or reply GNUTLS_A_NO_RENEGOTIATION.", action_result, CHM_GNUTLS_ERR_PRN_ARGS(static_cast<int>(action_result)));
				}
				if(0 == gnutls_error_is_fatal(static_cast<int>(action_result))){
					// retry with not fatal error
					is_retry	= true;
					is_close	= false;
					result		= false;
				}else{
					// break with fatal error
					is_retry	= false;
					is_close	= false;
					result		= false;
				}
				break;

			default:
				MSG_CHMPRN("SSL action result(%ld) is got" CHM_GNUTLS_ERR_PRN_FORM, action_result, CHM_GNUTLS_ERR_PRN_ARGS(static_cast<int>(action_result)));
				if(0 == gnutls_error_is_fatal(static_cast<int>(action_result))){
					// retry with not fatal error
					is_retry	= true;
					is_close	= false;
					result		= false;
				}else{
					// break with fatal error
					is_retry	= false;
					is_close	= false;
					result		= false;
				}
				break;
		}
	}else if(action_result == 0){
		// Do we need to check deep in error code?
		MSG_CHMPRN("SSL action result(%ld) is close socket or EOF" CHM_GNUTLS_ERR_PRN_FORM, action_result, CHM_GNUTLS_ERR_PRN_ARGS(static_cast<int>(action_result)));
		is_retry	= false;
		is_close	= true;				// whether closing socket or not.
		result		= false;
	}else{	// 0 < action_result
		DMP_CHMPRN("SSL action result(%ld) is succeed", action_result);
		is_retry	= false;
		is_close	= false;
		result		= true;
	}

	return result;
}

bool ChmSecureSock::IsSafeSSLContextEx(const char* server_cert, const char* server_prikey, const char* slave_cert, const char* slave_prikey)
{
	if((CHMEMPTYSTR(server_cert) || CHMEMPTYSTR(server_prikey)) && (ChmSecureSock::is_verify_peer && (CHMEMPTYSTR(slave_cert) || CHMEMPTYSTR(slave_prikey)))){
		return false;
	}
	return true;
}

ChmSSCtx ChmSecureSock::GetSSLContextEx(const char* server_cert, const char* server_prikey, const char* slave_cert, const char* slave_prikey)
{
	if((CHMEMPTYSTR(server_cert) || CHMEMPTYSTR(server_prikey)) && (ChmSecureSock::is_verify_peer && (CHMEMPTYSTR(slave_cert) || CHMEMPTYSTR(slave_prikey)))){
		ERR_CHMPRN("Parameters are wrong.");
		return NULL;
	}

	ChmSSCtx	pctx = new ChmSSCtxEnt();
	if((!CHMEMPTYSTR(server_cert) || !CHMEMPTYSTR(server_prikey)) && !pctx->set(true, server_cert, server_prikey)){
		ERR_CHMPRN("Failed to make SSL context for server cert/private key.");
		CHM_Delete(pctx);
		return NULL;
	}
	if(ChmSecureSock::is_verify_peer && !pctx->set(false, slave_cert, slave_prikey)){
		ERR_CHMPRN("Failed to make SSL context for slave cert/private key.");
		CHM_Delete(pctx);
		return NULL;
	}
	return pctx;
}

void ChmSecureSock::FreeSSLContextEx(ChmSSCtx ctx)
{
	if(ctx){
		ChmSSCtxEnt*	pCtx = reinterpret_cast<ChmSSCtxEnt*>(ctx);
		CHM_Delete(pCtx);
	}
}

ChmSSSession ChmSecureSock::AcceptSSL(ChmSSCtx ctx, int sock, int con_retrycnt, suseconds_t con_waittime)
{
	int									resgnutls;
	gnutls_certificate_credentials_t	cert_cred	= NULL;
	gnutls_session_t					session		= NULL;
	char*								pDescSession;

	if(!ctx || CHM_INVALID_SOCK == sock){
		ERR_CHMPRN("Parameters are wrong");
		return NULL;
	}

	if(GNUTLS_E_SUCCESS != (resgnutls = gnutls_certificate_allocate_credentials(&cert_cred))){
		ERR_CHMPRN("Failed to allocate credential" CHM_GNUTLS_ERR_PRN_FORM, CHM_GNUTLS_ERR_PRN_ARGS(resgnutls));
		return NULL;
	}

	// Load CA certs
	if(!ChmSecureSock::LoadCACerts(cert_cred)){
		ERR_CHMPRN("Failed to load trusted CA certs");
		gnutls_certificate_free_credentials(cert_cred);
		return NULL;
	}

	// Load server cert
	if(ctx->strServerPKey.empty()){
		if(0 > (resgnutls = gnutls_certificate_set_x509_crl_file(cert_cred, ctx->strServerCert.c_str(), GNUTLS_X509_FMT_PEM))){
			ERR_CHMPRN("Failed to load server cert(cert file=%s) allocate credential" CHM_GNUTLS_ERR_PRN_FORM, ctx->strServerCert.c_str(), CHM_GNUTLS_ERR_PRN_ARGS(resgnutls));
			gnutls_certificate_free_credentials(cert_cred);
			return NULL;
		}
		if(0 == resgnutls){
			ERR_CHMPRN("No server cert is loaded from cert file(%s).", ctx->strServerCert.c_str());
			gnutls_certificate_free_credentials(cert_cred);
			return NULL;
		}
		MSG_CHMPRN("Server certs(%d) are loaded from cert file(%s).", resgnutls, ctx->strServerCert.c_str());

	}else{
		if(GNUTLS_E_SUCCESS != (resgnutls = gnutls_certificate_set_x509_key_file(cert_cred, ctx->strServerCert.c_str(), ctx->strServerPKey.c_str(), GNUTLS_X509_FMT_PEM))){
			ERR_CHMPRN("Failed to load server cert(cert file=%s, key file=%s) allocate credential" CHM_GNUTLS_ERR_PRN_FORM, ctx->strServerCert.c_str(), ctx->strServerPKey.c_str(), CHM_GNUTLS_ERR_PRN_ARGS(resgnutls));
			gnutls_certificate_free_credentials(cert_cred);
			return NULL;
		}
		MSG_CHMPRN("Server cert is loaded from cert file(%s) and key file(%s).", ctx->strServerCert.c_str(), ctx->strServerPKey.c_str());
	}

	// initialize session
	if(GNUTLS_E_SUCCESS != (resgnutls = gnutls_init(&session, GNUTLS_SERVER | GNUTLS_NONBLOCK))){
		ERR_CHMPRN("Failed to initialize server session" CHM_GNUTLS_ERR_PRN_FORM, CHM_GNUTLS_ERR_PRN_ARGS(resgnutls));
		gnutls_certificate_free_credentials(cert_cred);
		return NULL;
	}

	// set(initialize) priority
	if(!ChmSecureSock::SetMinVersion(session)){
		ERR_CHMPRN("Failed to set SSL/TLS minimum version");
		gnutls_deinit(session);
		gnutls_certificate_free_credentials(cert_cred);
		return NULL;
	}

	// set credentials to session
	if(GNUTLS_E_SUCCESS != (resgnutls = gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE, cert_cred))){
		ERR_CHMPRN("Failed to set credential to session" CHM_GNUTLS_ERR_PRN_FORM, CHM_GNUTLS_ERR_PRN_ARGS(resgnutls));
		gnutls_deinit(session);
		gnutls_certificate_free_credentials(cert_cred);
		return NULL;
	}

	// set verify peer
	if(ChmSecureSock::is_verify_peer){
		gnutls_certificate_server_set_request(session, GNUTLS_CERT_REQUIRE);						// not GNUTLS_CERT_REQUEST
	}else{
		gnutls_certificate_server_set_request(session, GNUTLS_CERT_IGNORE);
	}

	// set timeout
	//
	// [NOTE]
	// We do not have a plan for using custom pull function(and set GNUTLS_NONBLOCK to session),
	// then we do not set timeout call back function by gnutls_transport_set_pull_timeout_function().
	//
	unsigned int	timeout_ms = static_cast<unsigned int>((con_waittime / 1000) * con_retrycnt);
	gnutls_handshake_set_timeout(session, timeout_ms);

	// set socket to session
	gnutls_transport_set_int(session, sock);

	// handshake
	for(bool isLoop = true; isLoop; isLoop = (GNUTLS_E_SUCCESS != resgnutls && 0 == gnutls_error_is_fatal(resgnutls))){
		if(GNUTLS_E_SUCCESS != (resgnutls = gnutls_handshake(session))){
			WAN_CHMPRN("Failed to handshake on server session" CHM_GNUTLS_ERR_PRN_FORM, CHM_GNUTLS_ERR_PRN_ARGS(resgnutls));
		}
	}
	if(GNUTLS_E_SUCCESS != resgnutls){
		ERR_CHMPRN("Fatal failed to handshake on server session" CHM_GNUTLS_ERR_PRN_FORM, CHM_GNUTLS_ERR_PRN_ARGS(resgnutls));
		gnutls_deinit(session);
		gnutls_certificate_free_credentials(cert_cred);
		return NULL;
	}

	// print information for session
	if(NULL == (pDescSession = gnutls_session_get_desc(session))){
		WAN_CHMPRN("Could not get description for server session");
	}else{
		MSG_CHMPRN("Connected session(handshaked) : %s", pDescSession);
		gnutls_free(pDescSession);
	}

	ChmSSSessionEnt*	sslsession = new ChmSSSessionEnt(true, ctx, cert_cred, session, NULL);		// SERVER type

	return reinterpret_cast<void*>(sslsession);
}

ChmSSSession ChmSecureSock::ConnectSSL(ChmSSCtx ctx, int sock, int con_retrycnt, suseconds_t con_waittime)
{
	int									resgnutls;
	gnutls_certificate_credentials_t	cert_cred	= NULL;
	gnutls_session_t					session		= NULL;
	char*								pDescSession;

	if(!ctx || CHM_INVALID_SOCK == sock){
		ERR_CHMPRN("Parameters are wrong");
		return NULL;
	}

	if(GNUTLS_E_SUCCESS != (resgnutls = gnutls_certificate_allocate_credentials(&cert_cred))){
		ERR_CHMPRN("Failed to allocate credential" CHM_GNUTLS_ERR_PRN_FORM, CHM_GNUTLS_ERR_PRN_ARGS(resgnutls));
		return NULL;
	}

	// Load CA certs
	if(!ChmSecureSock::LoadCACerts(cert_cred)){
		ERR_CHMPRN("Failed to load trusted CA certs");
		gnutls_certificate_free_credentials(cert_cred);
		return NULL;
	}

	// Load slave cert
	if(ChmSecureSock::is_verify_peer && !ctx->strSlaveCert.empty()){
		if(ctx->strSlavePKey.empty()){
			if(0 > (resgnutls = gnutls_certificate_set_x509_crl_file(cert_cred, ctx->strSlaveCert.c_str(), GNUTLS_X509_FMT_PEM))){
				ERR_CHMPRN("Failed to load slave cert(cert file=%s) allocate credential" CHM_GNUTLS_ERR_PRN_FORM, ctx->strSlaveCert.c_str(), CHM_GNUTLS_ERR_PRN_ARGS(resgnutls));
				gnutls_certificate_free_credentials(cert_cred);
				return NULL;
			}
			if(0 == resgnutls){
				ERR_CHMPRN("No slave cert is loaded from cert file(%s).", ctx->strSlaveCert.c_str());
				gnutls_certificate_free_credentials(cert_cred);
				return NULL;
			}
			MSG_CHMPRN("Server certs(%d) are loaded from cert file(%s).", resgnutls, ctx->strSlaveCert.c_str());

		}else{
			if(GNUTLS_E_SUCCESS != (resgnutls = gnutls_certificate_set_x509_key_file(cert_cred, ctx->strSlaveCert.c_str(), ctx->strSlavePKey.c_str(), GNUTLS_X509_FMT_PEM))){
				ERR_CHMPRN("Failed to load slave cert(cert file=%s, key file=%s) allocate credential" CHM_GNUTLS_ERR_PRN_FORM, ctx->strSlaveCert.c_str(), ctx->strSlavePKey.c_str(), CHM_GNUTLS_ERR_PRN_ARGS(resgnutls));
				gnutls_certificate_free_credentials(cert_cred);
				return NULL;
			}
			MSG_CHMPRN("Slave cert is loaded from cert file(%s) and key file(%s).", ctx->strSlaveCert.c_str(), ctx->strSlavePKey.c_str());
		}
	}else if(ChmSecureSock::is_verify_peer && ctx->strSlaveCert.empty()){
		ERR_CHMPRN("Slave cert file path is empty even though verify peer is true");
		gnutls_certificate_free_credentials(cert_cred);
		return NULL;
	}

	// initialize session
	if(GNUTLS_E_SUCCESS != (resgnutls = gnutls_init(&session, GNUTLS_CLIENT | GNUTLS_NONBLOCK))){
		ERR_CHMPRN("Failed to initialize client session" CHM_GNUTLS_ERR_PRN_FORM, CHM_GNUTLS_ERR_PRN_ARGS(resgnutls));
		gnutls_certificate_free_credentials(cert_cred);
		return NULL;
	}

	// set(initialize) priority
	if(!ChmSecureSock::SetMinVersion(session)){
		ERR_CHMPRN("Failed to set SSL/TLS minimum version");
		gnutls_deinit(session);
		gnutls_certificate_free_credentials(cert_cred);
		return NULL;
	}
	gnutls_session_enable_compatibility_mode(session);												// [NOTE] For client applications which require maximum compatibility

	// set credentials to session
	if(GNUTLS_E_SUCCESS != (resgnutls = gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE, cert_cred))){
		ERR_CHMPRN("Failed to set credential to session" CHM_GNUTLS_ERR_PRN_FORM, CHM_GNUTLS_ERR_PRN_ARGS(resgnutls));
		gnutls_deinit(session);
		gnutls_certificate_free_credentials(cert_cred);
		return NULL;
	}

	// set timeout
	//
	// [NOTE]
	// We do not have a plan for using custom pull function(and set GNUTLS_NONBLOCK to session),
	// then we do not set timeout call back function by gnutls_transport_set_pull_timeout_function().
	//
	unsigned int	timeout_ms = static_cast<unsigned int>((con_waittime / 1000) * con_retrycnt);
	gnutls_handshake_set_timeout(session, timeout_ms);

	// set socket to session
	gnutls_transport_set_int(session, sock);

	// handshake
	for(bool isLoop = true; isLoop; isLoop = (GNUTLS_E_SUCCESS != resgnutls && 0 == gnutls_error_is_fatal(resgnutls))){
		if(GNUTLS_E_SUCCESS != (resgnutls = gnutls_handshake(session))){
			WAN_CHMPRN("Failed to handshake on slave session" CHM_GNUTLS_ERR_PRN_FORM, CHM_GNUTLS_ERR_PRN_ARGS(resgnutls));
		}
	}
	if(GNUTLS_E_SUCCESS != resgnutls){
		ERR_CHMPRN("Fatal failed to handshake on slave session" CHM_GNUTLS_ERR_PRN_FORM, CHM_GNUTLS_ERR_PRN_ARGS(resgnutls));
		gnutls_deinit(session);
		gnutls_certificate_free_credentials(cert_cred);
		return NULL;
	}

	// print information for session
	if(NULL == (pDescSession = gnutls_session_get_desc(session))){
		WAN_CHMPRN("Could not get description for slave session");
	}else{
		MSG_CHMPRN("Connected session(handshaked) : %s", pDescSession);
		gnutls_free(pDescSession);
	}

	ChmSSSessionEnt*	sslsession = new ChmSSSessionEnt(false, ctx, cert_cred, session, NULL);		// CLIENT(SLAVE) type

	return reinterpret_cast<void*>(sslsession);
}

bool ChmSecureSock::ShutdownSSL(int sock, ChmSSSession sslsession, int con_retrycnt, suseconds_t con_waittime)
{
	(void)sock;
	(void)con_retrycnt;
	(void)con_waittime;

	int					resgnutls;
	ChmSSSessionEnt*	ssl		= reinterpret_cast<ChmSSSessionEnt*>(sslsession);

	if(!ssl || !ssl->session){
		ERR_CHMPRN("Parameters are wrong");
		return false;
	}

	// [NOTE]
	// We think that it should not need to call gnutls_bye() function here,
	// because it has a possibility that CHMPX is implemented by another
	// SSL/TLS library does not understand this logic.
	//
	// 
	if(GNUTLS_E_SUCCESS != (resgnutls = gnutls_bye(ssl->session, GNUTLS_SHUT_WR))){					// [NOTE] we do not need to wait reply, then not set GNUTLS_SHUT_RDWR.
		ERR_CHMPRN("Failed to call gnutls_bye" CHM_GNUTLS_ERR_PRN_FORM ", but continue...", CHM_GNUTLS_ERR_PRN_ARGS(resgnutls));
	}

	// close session and context
	CHM_Delete(ssl);

	return true;
}

int ChmSecureSock::Read(ChmSSSession sslsession, void* pbuf, int length)
{
	ChmSSSessionEnt*	ssl		= reinterpret_cast<ChmSSSessionEnt*>(sslsession);
	ssize_t				recvlen	= GNUTLS_E_INVALID_SESSION;

	if(!ssl || !ssl->session){
		ERR_CHMPRN("Parameters are wrong");
	}else{
		recvlen = gnutls_record_recv(ssl->session, pbuf, static_cast<size_t>(length));
	}
	return static_cast<int>(recvlen);
}

int ChmSecureSock::Write(ChmSSSession sslsession, const void* pbuf, int length)
{
	ChmSSSessionEnt*	ssl		= reinterpret_cast<ChmSSSessionEnt*>(sslsession);
	ssize_t				sentlen	= GNUTLS_E_INVALID_SESSION;

	if(!ssl || !ssl->session){
		ERR_CHMPRN("Parameters are wrong");
	}else{
		sentlen = gnutls_record_send(ssl->session, pbuf, static_cast<size_t>(length));
	}
	return static_cast<int>(sentlen);
}

void ChmSecureSock::AllowSelfSignedCert(void)
{
	ChmSecureSock::is_self_sigined = true;
}

void ChmSecureSock::DenySelfSignedCert(void)
{
	ChmSecureSock::is_self_sigined = false;
}

//------------------------------------------------------
// Methods
//------------------------------------------------------
ChmSecureSock::ChmSecureSock(const char* CApath, const char* CAfile, bool is_verify_peer)
{
	if(!ChmSecureSock::InitLibrary(CApath, CAfile, is_verify_peer)){
		ERR_CHMPRN("Something error occurred in initializing.");
	}
}

ChmSecureSock::~ChmSecureSock(void)
{
	if(!ChmSecureSock::FreeLibrary()){
		ERR_CHMPRN("Something error occurred in destructor.");
	}
}

bool ChmSecureSock::Clean(void)
{
	// Nothing to do
	return true;
}

bool ChmSecureSock::IsSafeSSLContext(const char* server_cert, const char* server_prikey, const char* slave_cert, const char* slave_prikey)
{
	return ChmSecureSock::IsSafeSSLContextEx(server_cert, server_prikey, slave_cert, slave_prikey);
}

ChmSSCtx ChmSecureSock::GetSSLContext(const char* server_cert, const char* server_prikey, const char* slave_cert, const char* slave_prikey)
{
	return ChmSecureSock::GetSSLContextEx(server_cert, server_prikey, slave_cert, slave_prikey);
}

void ChmSecureSock::FreeSSLContext(ChmSSCtx ctx)
{
	ChmSecureSock::FreeSSLContextEx(ctx);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noexpandtab sw=4 ts=4 fdm=marker
 * vim<600: noexpandtab sw=4 ts=4
 */
