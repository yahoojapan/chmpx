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

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <nss.h>
#include <ssl.h>
#include <prerror.h>
#include <cert.h>
#include <keyhi.h>
#include <prinit.h>
#include <secmod.h>
#include <secerr.h>
#include <prmem.h>
#include <sslproto.h>
#include <private/pprio.h>				// [NOTE] Private API for PR_ImportTCPSocket, no other way to turn a POSIX file descriptor into an NSPR socket.

#include <k2hash/k2hutil.h>

#include <list>

#include "chmcommon.h"
#include "chmeventsock.h"
#include "chmssnss.h"
#include "chmutil.h"
#include "chmdbg.h"

using namespace	std;

//------------------------------------------------------
// Utilities/Symbols for NSPR/NSS
//------------------------------------------------------
//
// For error code and string
//
#define	CHM_NSS_ERR_PRN_FORM			" [NSS Error Info(%d: %s)] "
#define	CHM_NSS_ERR_PRN_ARGS			PR_GetError(), PR_ErrorToName(PR_GetError())
#define	CHM_NSS_STS_PRN_ARGS(status)	(	SECSuccess		== status ? "SECSuccess"	: 				\
											SECFailure		== status ? "SECFailure"	: 				\
											SECWouldBlock	== status ?	"SECWouldBlock"	: "Unknown"	)

//
// Symbols
//
#define	CHM_NSS_ENV_CERT_DIR			"SSL_DIR"
#define	CHM_NSS_DEFAULT_CERT_DIR		"/etc/pki/nssdb"
#define	CHM_NSS_PKCS11_MODULE_CONF		"library=libnssckbi.so name=\"Root Certs\""				// or "library=libnssckbi.so name=trust"
#define	CHM_NSS_PEM_MODULE_CONF			"library=libnsspem.so name=PEM"
#define	CHM_NSS_CERT_PRIKEY_SLOT_STR	"PEM Token #1"
#define	CHM_NSS_CN_KEY_IN_ISSUER		"CN="
#define	CHM_NSS_CERTMAP_KEY_SEP			"::PRIVATE::"

//
// Macros
//
#define	CHM_NSS_SET_CKATTR(pattr, ckatype, value, length)					\
										if(reinterpret_cast<CK_ATTRIBUTE*>(pattr)){							\
											reinterpret_cast<CK_ATTRIBUTE*>(pattr)->type		= ckatype;	\
											reinterpret_cast<CK_ATTRIBUTE*>(pattr)->pValue		= value;	\
											reinterpret_cast<CK_ATTRIBUTE*>(pattr)->ulValueLen	= length;	\
										}

//------------------------------------------------------
// Structure
//------------------------------------------------------
//
// CHMPX SSL Context structure for NSS
//
// This structure pointer is typedef as ChmSSCtx which is I/F to ChmEventSock.
// And this structure's member variables are only pointer, and those are not
// destruct(free) in this.
//
typedef struct chm_nss_ss_context{
	string				strServerKey;
	CERTCertificate*	CERT_OBJ_server;
	SECKEYPrivateKey*	PKEY_OBJ_server;
	string				strSlaveKey;
	CERTCertificate*	CERT_OBJ_slave;
	SECKEYPrivateKey*	PKEY_OBJ_slave;

	chm_nss_ss_context(const char* pServer = NULL, CERTCertificate* cert_server = NULL, SECKEYPrivateKey* pkey_server = NULL, const char* pSlave = NULL, CERTCertificate* cert_slave = NULL, SECKEYPrivateKey* pkey_slave = NULL) :
		strServerKey(CHMEMPTYSTR(pServer) ? "" : pServer), CERT_OBJ_server(cert_server), PKEY_OBJ_server(pkey_server), strSlaveKey(CHMEMPTYSTR(pSlave) ? "" : pSlave), CERT_OBJ_slave(cert_slave), PKEY_OBJ_slave(pkey_slave)
	{
	}
	explicit chm_nss_ss_context(const struct chm_nss_ss_context* other) :
		strServerKey(other ? other->strServerKey : ""), CERT_OBJ_server(other ? other->CERT_OBJ_server : NULL), PKEY_OBJ_server(other ? other->PKEY_OBJ_server : NULL), strSlaveKey(other ? other->strSlaveKey : ""), CERT_OBJ_slave(other ? other->CERT_OBJ_slave : NULL), PKEY_OBJ_slave(other ? other->PKEY_OBJ_slave : NULL)
	{
	}

	bool set(bool is_server, const char* pkey, CERTCertificate* cert_obj, SECKEYPrivateKey* pkey_obj)
	{
		if(CHMEMPTYSTR(pkey) && (cert_obj || pkey_obj)){
			ERR_CHMPRN("Parameters are wrong.");
			return false;
		}
		if(is_server){
			strServerKey	= CHMEMPTYSTR(pkey) ? "" : pkey;
			CERT_OBJ_server	= cert_obj;
			PKEY_OBJ_server	= pkey_obj;
		}else{
			strSlaveKey		= CHMEMPTYSTR(pkey) ? "" : pkey;
			CERT_OBJ_slave	= cert_obj;
			PKEY_OBJ_slave	= pkey_obj;
		}
		return true;
	}

	bool set(const char* pServer, CERTCertificate* cert_server, SECKEYPrivateKey* pkey_server, const char* pSlave, CERTCertificate* cert_slave, SECKEYPrivateKey* pkey_slave)
	{
		if(!set(true, pServer, cert_server, pkey_server) || !set(false, pSlave, cert_slave, pkey_slave)){
			return false;
		}
		return true;
	}
}ChmSSCtxEnt;

// [NOTE]
// The certificate data managed inside the ChmSecureSock class.
// Multiple certificate data is registered in the map object, and it is used as a cache.
// Instead of creating an object for the same certificate every time, use the map object
// to divert the object. In the future, when the amount of data increases, it is necessary
// to periodically clean the cache.
//
typedef struct chm_nss_cert{
	string				CERT_path;
	string				PKEY_path;
	PK11GenericObject*	PK11_CERT_obj;
	PK11GenericObject*	PK11_PKEY_obj;
	CERTCertificate*	CERT_obj;
	SECKEYPrivateKey*	PKEY_obj;

	chm_nss_cert() : CERT_path(""), PKEY_path(""), PK11_CERT_obj(NULL), PK11_PKEY_obj(NULL), CERT_obj(NULL), PKEY_obj(NULL) {}

	~chm_nss_cert()
	{
		clear();
	}

	bool clear(void)
	{
		if(PK11_CERT_obj){
			PK11_DestroyGenericObject(PK11_CERT_obj);
			PK11_CERT_obj = NULL;
		}
		if(PK11_PKEY_obj){
			PK11_DestroyGenericObject(PK11_PKEY_obj);
			PK11_PKEY_obj = NULL;
		}
		if(CERT_obj){
			CERT_DestroyCertificate(CERT_obj);
			CERT_obj = NULL;
		}
		if(PKEY_obj){
			SECKEY_DestroyPrivateKey(PKEY_obj);
			PKEY_obj = NULL;
		}
		return true;
	}

	bool set(const char* cert, const char* prikey, PK11GenericObject* pCertObj, PK11GenericObject* pPKeyObj, CERTCertificate* pCert, SECKEYPrivateKey* pPKey)
	{
		if(CHMEMPTYSTR(cert) || !pCert || !pPKey){
			return false;
		}
		if(!clear()){
			return false;
		}
		CERT_path		= CHMEMPTYSTR(cert) ? "" : cert;
		PKEY_path		= CHMEMPTYSTR(prikey) ? "" : prikey;
		PK11_CERT_obj	= pCertObj;
		PK11_PKEY_obj	= pPKeyObj;
		CERT_obj		= pCert;
		PKEY_obj		= pPKey;
		return true;
	}
}ChmSSCertEnt;

//
// List for CA certs
//
inline void free_pk11objlist(chmpk11list_t& list)
{
	for(chmpk11list_t::iterator iter = list.begin(); iter != list.end(); list.erase(iter++)){
		PK11GenericObject*	pTmp = (*iter);
		if(pTmp){
			PK11_DestroyGenericObject(pTmp);
		}
	}
}

inline void add_pk11objlist(chmpk11list_t& list, PK11GenericObject* pk11obj)
{
	if(pk11obj){
		list.push_back(pk11obj);
	}
}

inline void move_pk11objlist(chmpk11list_t& srclist, chmpk11list_t& destlist)
{
	for(chmpk11list_t::iterator iter = srclist.begin(); iter != srclist.end(); srclist.erase(iter++)){
		PK11GenericObject*	pTmp = (*iter);
		if(pTmp){
			destlist.push_back(pTmp);
		}
	}
}

//
// Session Structure
//
// [NOTE]
// ChmSSSession is a pointer type casted void* to ChmSSSessionEnt structure.
//
// cppcheck-suppress unmatchedSuppression
// cppcheck-suppress noCopyConstructor
typedef struct chm_nss_session{
	ChmSSCtx		SSCtx;
	PRFileDesc*		SSSession;
	chmpk11list_t	pk11objs;

	chm_nss_session(ChmSSCtx ctx = NULL, PRFileDesc* session = NULL, chmpk11list_t* ppk11objs = NULL) : SSCtx(NULL), SSSession(session)
	{
		// cppcheck-suppress unmatchedSuppression
		// cppcheck-suppress noOperatorEq
		// cppcheck-suppress noCopyConstructor
		SSCtx = new ChmSSCtxEnt(ctx);
		if(ppk11objs){
			set(*ppk11objs);
		}
	}

	~chm_nss_session()
	{
		CHM_Delete(SSCtx);
		free_pk11objlist(pk11objs);
	}

	void set(ChmSSCtx ctx, PRFileDesc* session)
	{
		CHM_Delete(SSCtx);
		SSCtx		= new ChmSSCtxEnt(ctx);
		SSSession	= session;
	}

	void add(PK11GenericObject* pk11obj)
	{
		add_pk11objlist(pk11objs, pk11obj);
	}

	void set(chmpk11list_t& otherlist)
	{
		move_pk11objlist(pk11objs, otherlist);
	}
}ChmSSSessionEnt;

//------------------------------------------------------
// Local variables
//------------------------------------------------------
static const bool	chm_nss_true						= true;
static const bool	chm_nss_false						= false;

//------------------------------------------------------
// Class Variables
//------------------------------------------------------
bool			ChmSecureSock::is_self_sigined			= false;
int				ChmSecureSock::pr_init_count			= 0;
int				ChmSecureSock::nss_init_count			= 0;
bool			ChmSecureSock::is_verify_peer			= false;
chmss_ver_t		ChmSecureSock::ssl_min_ver				= CHM_SSLTLS_VER_DEFAULT;	// SSL/TLS minimum version
SECMODModule*	ChmSecureSock::PKCS11_LoadedModule		= NULL;
SECMODModule*	ChmSecureSock::PEM_LoadedModule			= NULL;
PRLock*			ChmSecureSock::FindSlotLock				= NULL;
PRLock*			ChmSecureSock::CertMapLock				= NULL;

//------------------------------------------------------
// Class Methods
//------------------------------------------------------
bool ChmSecureSock::InitLibrary(NSSInitContext** ctx, const char* CApath, const char* CAfile, bool is_verify_peer)
{
	if(!ctx){
		ERR_CHMPRN("Parameter is wrong");
		return false;
	}

	// [NOTE]
	// NSPR is now implicitly initialized, usually by the first NSPR function called by a program. 
	// But we call it here yet.
	//
	if(!PR_Initialized() || 0 == ChmSecureSock::pr_init_count){
		++(ChmSecureSock::pr_init_count);

		PR_Init(PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 0);
		if(!PR_Initialized()){
			ERR_CHMPRN("Failed initializing PR_Init" CHM_NSS_ERR_PRN_FORM, CHM_NSS_ERR_PRN_ARGS);
			--(ChmSecureSock::pr_init_count);
			return false;
		}
	}

	// NSS Initialize
	if(!NSS_IsInitialized() || 0 == ChmSecureSock::nss_init_count){
		++(ChmSecureSock::nss_init_count);

		// parameters
		NSSInitParameters	params;
		memset(&params, '\0', sizeof(params));
		params.length = sizeof(params);

		// get cert directory from environment
		string	certdir = ChmSecureSock::GetNssdbDir();
		if(certdir.empty()){
			if(!k2h_getenv(CHM_NSS_ENV_CERT_DIR, certdir)){
				// set default cert directory
				certdir = CHM_NSS_DEFAULT_CERT_DIR;
				MSG_CHMPRN("%s environment is not specified, then NSSDB directory path is \"%s\" as default.", CHM_NSS_ENV_CERT_DIR, certdir.c_str());
			}else{
				MSG_CHMPRN("Found %s environment, then NSSDB directory path is \"%s\".", CHM_NSS_ENV_CERT_DIR, certdir.c_str());
			}
		}
		certdir = string("sql:") + certdir;

		// initialize
		if(NULL == (*ctx = NSS_InitContext(certdir.c_str(), "", "", "", &params, NSS_INIT_READONLY | NSS_INIT_PK11RELOAD))){
			ERR_CHMPRN("Failed initializing context with cert dir(%s) " CHM_NSS_ERR_PRN_FORM "by NSS_InitContext, thus try no cert dir", certdir.c_str(), CHM_NSS_ERR_PRN_ARGS);

			if(NULL == (*ctx = NSS_InitContext("", "", "", "", &params, NSS_INIT_READONLY | NSS_INIT_NOCERTDB | NSS_INIT_NOMODDB | NSS_INIT_FORCEOPEN | NSS_INIT_NOROOTINIT | NSS_INIT_OPTIMIZESPACE | NSS_INIT_PK11RELOAD))){
				ERR_CHMPRN("Failed initializing context without cert dir" CHM_NSS_ERR_PRN_FORM "by NSS_InitContext.", CHM_NSS_ERR_PRN_ARGS);
				--(ChmSecureSock::nss_init_count);
				return false;
			}
		}

		// enable default all Ciphers
		if(SECSuccess != NSS_SetDomesticPolicy()){
			ERR_CHMPRN("Failed set domestic cipher by NSS_SetDomesticPolicy, but continue...");
		}
		// Set session ID cache configuration on server as default.
		if(SECSuccess != SSL_ConfigServerSessionIDCache(0, 100, (24 * 60 * 60), NULL)){
			ERR_CHMPRN("Failed SSL_ConfigServerSessionIDCache, but continue...");
		}
		SSL_ClearSessionCache();

		// load modules
		if(!ChmSecureSock::InitLoadExtModule()){
			ERR_CHMPRN("Something error occurred in loading modules, but continue...");
		}

		// verify peer mode on server
		ChmSecureSock::is_verify_peer = is_verify_peer;

		// store CA cache
		if(!ChmSecureSock::SetCA(CApath, CAfile)){
			ERR_CHMPRN("Something error occurred in setting CApath(%s), CAfile(%s), but continue...", (CHMEMPTYSTR(CApath) ? "empty" : CApath), (CHMEMPTYSTR(CAfile) ? "empty" : CAfile));
		}

		// lock object
		if(!ChmSecureSock::FindSlotLock){
			ChmSecureSock::FindSlotLock = PR_NewLock();
		}
		if(!ChmSecureSock::CertMapLock){
			ChmSecureSock::CertMapLock = PR_NewLock();
		}
	}
	return true;
}

bool ChmSecureSock::FreeLibrary(NSSInitContext* ctx)
{
	int	tmpcnt = ChmSecureSock::nss_init_count;
	if(0 < ChmSecureSock::nss_init_count){
		--(ChmSecureSock::nss_init_count);
	}
	if(tmpcnt != ChmSecureSock::nss_init_count && 0 == ChmSecureSock::nss_init_count){
		SECStatus	result;
		if(ChmSecureSock::PKCS11_LoadedModule){
			if(SECSuccess != (result = SECMOD_UnloadUserModule(ChmSecureSock::PKCS11_LoadedModule))){
				ERR_CHMPRN("Failed SECMOD_UnloadUserModule with %s for PKCS11" CHM_NSS_ERR_PRN_FORM ", but continue...", CHM_NSS_STS_PRN_ARGS(result), CHM_NSS_ERR_PRN_ARGS);
			}else{
				SECMOD_DestroyModule(ChmSecureSock::PKCS11_LoadedModule);
				ChmSecureSock::PKCS11_LoadedModule = NULL;
			}
		}
		if(ChmSecureSock::PEM_LoadedModule){
			if(SECSuccess != (result = SECMOD_UnloadUserModule(ChmSecureSock::PEM_LoadedModule))){
				ERR_CHMPRN("Failed SECMOD_UnloadUserModule with %s for PEM" CHM_NSS_ERR_PRN_FORM ", but continue...", CHM_NSS_STS_PRN_ARGS(result), CHM_NSS_ERR_PRN_ARGS);
			}else{
				SECMOD_DestroyModule(ChmSecureSock::PEM_LoadedModule);
				ChmSecureSock::PEM_LoadedModule = NULL;
			}
		}
		if(SECSuccess != (result = NSS_ShutdownContext(ctx))){
			if(SEC_ERROR_BUSY != PR_GetError()){
				ERR_CHMPRN("Failed NSS_ShutdownContext with %s" CHM_NSS_ERR_PRN_FORM, CHM_NSS_STS_PRN_ARGS(result), CHM_NSS_ERR_PRN_ARGS);
				++(ChmSecureSock::nss_init_count);
				return false;
			}
			WAN_CHMPRN("Failed NSS_ShutdownContext with %s" CHM_NSS_ERR_PRN_FORM ", but at SEC_ERROR_BUSY continue...", CHM_NSS_STS_PRN_ARGS(result), CHM_NSS_ERR_PRN_ARGS);
		}

		// clear CERT map
		PR_Lock(ChmSecureSock::CertMapLock);
		for(chmsscertmap_t::iterator iter = ChmSecureSock::GetCertMap().begin(); iter != ChmSecureSock::GetCertMap().end(); ChmSecureSock::GetCertMap().erase(iter++)){
			CHM_Delete(iter->second);
		}
		PR_Unlock(ChmSecureSock::CertMapLock);

		// lock object
		if(ChmSecureSock::FindSlotLock){
			PR_DestroyLock(ChmSecureSock::FindSlotLock);
			ChmSecureSock::FindSlotLock = NULL;
		}
		if(ChmSecureSock::CertMapLock){
			PR_DestroyLock(ChmSecureSock::CertMapLock);
			ChmSecureSock::CertMapLock = NULL;
		}
	}

	tmpcnt = ChmSecureSock::pr_init_count;
	if(0 < ChmSecureSock::pr_init_count){
		--(ChmSecureSock::pr_init_count);
	}
	if(tmpcnt != ChmSecureSock::pr_init_count && 0 == ChmSecureSock::pr_init_count){
		SSL_ClearSessionCache();
		PL_ArenaFinish();
		if(PR_SUCCESS != PR_Cleanup()){
			ERR_CHMPRN("Failed cleanup by PR_Cleanup, but continue...");
		}
	}
	return true;
}

bool ChmSecureSock::InitLoadExtModule(void)
{
	// PKCS11
	if(!ChmSecureSock::PKCS11_LoadedModule){
		char			szConfig[] = CHM_NSS_PKCS11_MODULE_CONF;
		SECMODModule*	pModule;
		if(NULL == (pModule = SECMOD_LoadUserModule(szConfig, NULL, PR_FALSE)) || !(pModule->loaded)){
			ERR_CHMPRN("Failed SECMOD_LoadUserModule for \"%s\"", szConfig);
			if(pModule){
				SECMOD_DestroyModule(pModule);
			}
			return false;
		}
		ChmSecureSock::PKCS11_LoadedModule = pModule;
	}
	// PEM
	if(!ChmSecureSock::PEM_LoadedModule){
		char			szConfig[] = CHM_NSS_PEM_MODULE_CONF;
		SECMODModule*	pModule;
		if(NULL == (pModule = SECMOD_LoadUserModule(szConfig, NULL, PR_FALSE)) || !(pModule->loaded)){
			ERR_CHMPRN("Failed SECMOD_LoadUserModule for \"%s\"", szConfig);
			ERR_CHMPRN("[NOTICE]");
			ERR_CHMPRN("*** This host does not have \"libnsspem.so\", then you could not load PEM CERTIFICATION FILES directly.");
			ERR_CHMPRN("*** You must use NSSDB with the certificate loaded in advance and specify certs with NICKNAME.");
			if(pModule){
				SECMOD_DestroyModule(pModule);
			}
			return false;
		}
		ChmSecureSock::PEM_LoadedModule = pModule;
	}
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

// [NOTE]
// To avoid static object initialization order problem(SIOF)
//
string& ChmSecureSock::GetNssdbDir(void)
{
	static string	conf_nssdb_dir;
	return conf_nssdb_dir;
}

// [NOTE]
// To avoid static object initialization order problem(SIOF)
//
chmsscertmap_t& ChmSecureSock::GetCertMap(void)
{
	static chmsscertmap_t	CertMap;
	return CertMap;
}

bool ChmSecureSock::SetCA(const char* CApath, const char* CAfile)
{
	if(!CHMEMPTYSTR(CApath) && !CHMEMPTYSTR(CAfile)){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}
	if(!CHMEMPTYSTR(CAfile)){
		ChmSecureSock::GetCAFile().assign(CAfile);
	}
	if(!CHMEMPTYSTR(CApath)){
		if(!is_dir_exist(CApath)){
			ERR_CHMPRN("CA path(%s) is not directory, but continue...", CApath);
		}
		ChmSecureSock::GetCAPath().assign(CApath);
	}
	return true;
}

const char* ChmSecureSock::LibraryName(void)
{
	return "NSS";
}

bool ChmSecureSock::IsCAPathAllowFile(void)
{
	return false;
}

bool ChmSecureSock::IsCertAllowName(void)
{
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

string ChmSecureSock::GetSessionInfo(PRFileDesc* session)
{
	string	strInfo("(error)");
	if(!session){
		ERR_CHMPRN("Parameter is wrong.");
		return strInfo;
	}
	SSLChannelInfo		channel;
	SSLCipherSuiteInfo	suite;

	if(SECSuccess != SSL_GetChannelInfo(session, &channel, sizeof(channel))){
		ERR_CHMPRN("Failed to get channel information for SSL/TLS session" CHM_NSS_ERR_PRN_FORM, CHM_NSS_ERR_PRN_ARGS);
		return strInfo;
	}
	if(sizeof(channel) != channel.length){
		ERR_CHMPRN("Succeed to get channel information for SSL/TLS session, but channel information length is not expected size.");
		return strInfo;
	}

	// protocol
	switch(channel.protocolVersion){
		case	SSL_LIBRARY_VERSION_TLS_1_3:
			strInfo = CHM_GET_STR_SSLTLS_VERSION(CHM_SSLTLS_VER_TLSV1_3);
			break;
		case	SSL_LIBRARY_VERSION_TLS_1_2:
			strInfo = CHM_GET_STR_SSLTLS_VERSION(CHM_SSLTLS_VER_TLSV1_2);
			break;
		case	SSL_LIBRARY_VERSION_TLS_1_1:
			strInfo = CHM_GET_STR_SSLTLS_VERSION(CHM_SSLTLS_VER_TLSV1_1);
			break;
		case	SSL_LIBRARY_VERSION_TLS_1_0:
			strInfo = CHM_GET_STR_SSLTLS_VERSION(CHM_SSLTLS_VER_TLSV1_0);
			break;
		case	SSL_LIBRARY_VERSION_3_0:
			strInfo = CHM_GET_STR_SSLTLS_VERSION(CHM_SSLTLS_VER_SSLV3);
			break;
		case	SSL_LIBRARY_VERSION_2:
			strInfo = CHM_GET_STR_SSLTLS_VERSION(CHM_SSLTLS_VER_SSLV2);
			break;
		default:
			strInfo = "unknown SSL/TLS version";
			break;
	}
	strInfo += " - ";

	// cipher suite
	if(SECSuccess == SSL_GetCipherSuiteInfo(channel.cipherSuite, &suite, sizeof suite)){
		if(!CHMEMPTYSTR(suite.cipherSuiteName)){
			strInfo += suite.cipherSuiteName;
		}else{
			strInfo += "empty cipher suite";
		}
	}else{
		ERR_CHMPRN("Failed to get Cipher suite for SSL/TLS session" CHM_NSS_ERR_PRN_FORM ", but continue...", CHM_NSS_ERR_PRN_ARGS);
		strInfo += "unknown cipher suite";
	}
	return strInfo;
}

bool ChmSecureSock::SetMinVersion(PRFileDesc* model)
{
	if(!model){
		ERR_CHMPRN("Parameter is wrong");
		return false;
	}
	SSLVersionRange	curvers	= {SSL_LIBRARY_VERSION_TLS_1_0, SSL_LIBRARY_VERSION_TLS_1_3};
	SSLVersionRange	newvers		= {0, 0};

	// Get current version range
	if(SECSuccess != SSL_VersionRangeGetDefault(ssl_variant_stream, &curvers)){
		ERR_CHMPRN("Could not get current SSL/TLS version range" CHM_NSS_ERR_PRN_FORM ", but continue...", CHM_NSS_ERR_PRN_ARGS);
		// reset
		curvers.min = 0;
		curvers.max = 0;
	}

	switch(ChmSecureSock::ssl_min_ver){
		case	CHM_SSLTLS_VER_TLSV1_3:
			newvers.max = curvers.max <= SSL_LIBRARY_VERSION_TLS_1_3 ? SSL_LIBRARY_VERSION_TLS_1_3 : curvers.max;
			newvers.min = SSL_LIBRARY_VERSION_TLS_1_3;
			break;

		case	CHM_SSLTLS_VER_TLSV1_2:
			newvers.max = curvers.max <= SSL_LIBRARY_VERSION_TLS_1_2 ? SSL_LIBRARY_VERSION_TLS_1_2 : curvers.max;
			newvers.min = SSL_LIBRARY_VERSION_TLS_1_2;
			break;

		case	CHM_SSLTLS_VER_TLSV1_1:
			newvers.max = curvers.max <= SSL_LIBRARY_VERSION_TLS_1_1 ? SSL_LIBRARY_VERSION_TLS_1_1 : curvers.max;
			newvers.min = SSL_LIBRARY_VERSION_TLS_1_1;
			break;

		case	CHM_SSLTLS_VER_TLSV1_0:
		case	CHM_SSLTLS_VER_DEFAULT:		// Default is after TLS v1.0
			newvers.max = curvers.max <= SSL_LIBRARY_VERSION_TLS_1_0 ? SSL_LIBRARY_VERSION_TLS_1_0 : curvers.max;
			newvers.min = SSL_LIBRARY_VERSION_TLS_1_0;
			break;

		case	CHM_SSLTLS_VER_SSLV3:
			newvers.max = curvers.max <= SSL_LIBRARY_VERSION_3_0 ? SSL_LIBRARY_VERSION_3_0 : curvers.max;
			newvers.min = SSL_LIBRARY_VERSION_3_0;
			break;

		case	CHM_SSLTLS_VER_SSLV2:
			MSG_CHMPRN("NOTICE - SSLv2 is allowed.");
			newvers.max = curvers.max <= SSL_LIBRARY_VERSION_2 ? SSL_LIBRARY_VERSION_2 : curvers.max;
			newvers.min = SSL_LIBRARY_VERSION_2;
			break;

		default:
			ERR_CHMPRN("SSL/TLS minimum version value(%s) is something wrong, thus we use %s setting as default", CHM_GET_STR_SSLTLS_VERSION(ChmSecureSock::ssl_min_ver), CHM_GET_STR_SSLTLS_VERSION(CHM_SSLTLS_VER_SSLV3));
			newvers.max = curvers.max <= SSL_LIBRARY_VERSION_TLS_1_0 ? SSL_LIBRARY_VERSION_TLS_1_0 : curvers.max;
			newvers.min = SSL_LIBRARY_VERSION_TLS_1_0;
			break;
	}

	// Do we need to change version range?
	if(newvers.min != curvers.min || newvers.max != curvers.max){
		if(SECSuccess != SSL_VersionRangeSet(model, &newvers)){
			ERR_CHMPRN("Could not set current SSL/TLS version range" CHM_NSS_ERR_PRN_FORM, CHM_NSS_ERR_PRN_ARGS);
			return false;
		}
	}
	return true;
}

bool ChmSecureSock::SetExtValue(const char* key, const char* value)
{
	if(CHMEMPTYSTR(key)){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(0 != strcmp(CHM_NSS_NSSDB_DIR_KEY, key)){
		ERR_CHMPRN("Unknown key(%s) for NSS.", key);
		return false;
	}
	if(CHMEMPTYSTR(value)){
		ChmSecureSock::GetNssdbDir().clear();
	}else{
		ChmSecureSock::GetNssdbDir().assign(value);
	}
	return true;
}

// [NOTE]
// This method is for loading PEM CA cert to NSSDB.
// But we do not use this, because it needs libnsspem.so and this fails to load PEM
// on not RHEL/CentOS.
// Thus user for chmpx must set CA cert to NSSDB before running it.
//
// [TODO]
// The CA certifications to be loaded here should be loaded only once. They should be
// loaded and cached in this class. But now we do not incorporate cache.
//
bool ChmSecureSock::LoadCACerts(chmpk11list_t& pk11objlist)
{
	if(!ChmSecureSock::PEM_LoadedModule){
		MSG_CHMPRN("CHMPX does not load PEM load module, then we could not load CA cert files(directory), but continue...");
		return true;
	}
	PK11GenericObject*	pPk11Obj = NULL;

	free_pk11objlist(pk11objlist);

	if(!ChmSecureSock::GetCAFile().empty()){
		if(NULL == (pPk11Obj = ChmSecureSock::GeneratePK11GenericObject(ChmSecureSock::GetCAFile().c_str(), false))){
			WAN_CHMPRN("Could not get PK11 object for CA file(%s), but continue...", ChmSecureSock::GetCAFile().c_str());
		}else{
			add_pk11objlist(pk11objlist, pPk11Obj);
		}
	}
	if(!ChmSecureSock::GetCAPath().empty()){
		PRDir*	dir;
		if(NULL == (dir = PR_OpenDir(ChmSecureSock::GetCAPath().c_str()))){
			WAN_CHMPRN("Could not open CA path(%s) directory, but continue...", ChmSecureSock::GetCAPath().c_str());
		}else{
			// [NOTE]
			// We should call PR_ReadDir with PR_SKIP_BOTH and PR_SKIP_HIDDEN flag.
			// But this parameter type is enum, thus we are going to get compile error
			// when specifying those.
			// Then we do not set PR_SKIP_HIDDEN flag.
			//
			// AND WE NEED TO CHECK FILE EXTENSION OR FORMAT BEFORE CALLING PK11 FUNCTION.
			//
			PRDirEntry*	entry;
			while(NULL != (entry = PR_ReadDir(dir, PR_SKIP_BOTH))){
				if(0 == strcmp(entry->name, ".") || 0 == strcmp(entry->name, "..")){
					continue;
				}
				string	certfile = ChmSecureSock::GetCAPath() + string("/") + entry->name;

				if(NULL == (pPk11Obj = ChmSecureSock::GeneratePK11GenericObject(certfile.c_str(), false))){
					WAN_CHMPRN("Could not get PK11 object for CA file(%s), then skip this and continue...", certfile.c_str());
				}else{
					add_pk11objlist(pk11objlist, pPk11Obj);
				}
			}
			PR_CloseDir(dir);
		}
	}
	return true;
}

// [NOTE]
// PK11_DestroyGenericObject() has a bug about that this dose not
// release resources allocated by PK11_CreateGenericObject() early
// enough.
// We should use PK11_CreateManagedGenericObject(), but it is introduced
// in NSS 3.34.
//
PK11GenericObject* ChmSecureSock::GeneratePK11GenericObject(const char* filename, bool is_private_key)
{
	PK11SlotInfo*		pk11Slot;
	CK_ATTRIBUTE		attrs[4];							// MAX is 4
	CK_OBJECT_CLASS		objClass	= is_private_key ? CKO_PRIVATE_KEY : CKO_CERTIFICATE;
	CK_BBOOL			ckToken		= CK_TRUE;
	CK_BBOOL			ckTrust		= CK_FALSE;
	PK11GenericObject*	pk11Obj;

	if(!is_file_exist(filename)){
		ERR_CHMPRN("Parameter is wrong.");
		return NULL;
	}

	// get slot
	PR_Lock(ChmSecureSock::FindSlotLock);
	if(NULL == (pk11Slot = PK11_FindSlotByName(CHM_NSS_CERT_PRIKEY_SLOT_STR))){
		ERR_CHMPRN("Could not find slot(%s) " CHM_NSS_ERR_PRN_FORM, CHM_NSS_CERT_PRIKEY_SLOT_STR, CHM_NSS_ERR_PRN_ARGS);
		PR_Unlock(ChmSecureSock::FindSlotLock);
		return NULL;
	}
	PR_Unlock(ChmSecureSock::FindSlotLock);

	// set attributes
	CHM_NSS_SET_CKATTR(&attrs[0], CKA_CLASS, &objClass, sizeof(CK_OBJECT_CLASS));
	CHM_NSS_SET_CKATTR(&attrs[1], CKA_TOKEN, &ckToken,	sizeof(CK_BBOOL));
	CHM_NSS_SET_CKATTR(&attrs[2], CKA_LABEL, const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(filename)), static_cast<CK_ULONG>(strlen(filename) + 1));
	if(!is_private_key){
		CHM_NSS_SET_CKATTR(&attrs[3], CKA_TRUST, &ckTrust,	sizeof(CK_BBOOL));
	}

	// generate PK11 object
	if(NULL == (pk11Obj = PK11_CreateGenericObject(pk11Slot, attrs, (is_private_key ? 3 : 4), PR_FALSE))){
		ERR_CHMPRN("Failed to generate PK11 generic object" CHM_NSS_ERR_PRN_FORM, CHM_NSS_ERR_PRN_ARGS);
		PK11_FreeSlot(pk11Slot);
		return NULL;
	}

	if(is_private_key){
		PK11SlotInfo*	tmpSlot;

		// This will force the token to be seen as re-inserted
		if(NULL != (tmpSlot = SECMOD_WaitForAnyTokenEvent(ChmSecureSock::PEM_LoadedModule, 0, 0))){
			PK11_FreeSlot(tmpSlot);
		}else{
			ERR_CHMPRN("Failed to call SECMOD_WaitForAnyTokenEvent" CHM_NSS_ERR_PRN_FORM ", but continue...", CHM_NSS_ERR_PRN_ARGS);
		}
		PK11_IsPresent(pk11Slot);

		if(SECSuccess != PK11_Authenticate(pk11Slot, PR_TRUE, NULL)){
			ERR_CHMPRN("Failed to call PK11_Authenticate" CHM_NSS_ERR_PRN_FORM, CHM_NSS_ERR_PRN_ARGS);
			PK11_DestroyGenericObject(pk11Obj);
			PK11_FreeSlot(pk11Slot);
			return NULL;
		}
	}
	PK11_FreeSlot(pk11Slot);

	return pk11Obj;
}

bool ChmSecureSock::MakeNickname(const char* pCertFile, string& strNickname)
{
	strNickname.clear();
	if(CHMEMPTYSTR(pCertFile)){
		return false;
	}
	if(is_file_safe_exist_ex(pCertFile, false)){
		strNickname	= CHM_NSS_CERT_PRIKEY_SLOT_STR;
		strNickname	+= ':';

		const char*	filename = strrchr(pCertFile, '/');
		if(!filename){
			strNickname	+= pCertFile;
		}else{
			strNickname	+= ++filename;
		}
	}else{
		strNickname	= pCertFile;
	}
	return true;
}

bool ChmSecureSock::CheckCertNicknameType(const char* pCertFile, const char* pPrivateKeyFile, string& strNickname, string& strCertKey, bool& isExistFile)
{
	if(CHMEMPTYSTR(pCertFile)){
		return false;
	}
	if(!ChmSecureSock::MakeNickname(pCertFile, strNickname)){
		return false;
	}
	strCertKey	= strNickname;

	if(is_file_safe_exist_ex(pCertFile, false)){
		isExistFile	= true;
	}else{
		isExistFile	= false;
		if(!CHMEMPTYSTR(pPrivateKeyFile)){
			strCertKey	+= CHM_NSS_CERTMAP_KEY_SEP;
			strCertKey	+= pPrivateKeyFile;
		}
	}
	return true;
}

bool ChmSecureSock::LoadCertPrivateKey(const char* pCertFile, const char* pPrivateKeyFile, PK11GenericObject** ppk11CertObj, PK11GenericObject** ppk11PKeyObj, CERTCertificate** ppCert, SECKEYPrivateKey** ppPrivateKey)
{
	if(CHMEMPTYSTR(pCertFile) || !ppk11CertObj || !ppk11PKeyObj || !ppCert || !ppPrivateKey){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}

	PK11GenericObject*	pk11CertObj	= NULL;
	PK11GenericObject*	pk11PKeyObj	= NULL;
	SECKEYPrivateKey*	pPrivateKey	= NULL;
	CERTCertificate*	pCert		= NULL;

	// check and get nickname
	string	strNickName;
	if(!ChmSecureSock::MakeNickname(pCertFile, strNickName)){
		ERR_CHMPRN("Failed to make nickname from certfile(%s)", CHMEMPTYSTR(pCertFile) ? "empty" : pCertFile);
		PK11_DestroyGenericObject(pk11CertObj);
		return false;
	}

	if(0 != strcmp(strNickName.c_str(), pCertFile)){
		// Case: pCertFile is existed file.
		//
		// generate Cert object
		if(NULL == (pk11CertObj = ChmSecureSock::GeneratePK11GenericObject(pCertFile, false))){
			ERR_CHMPRN("Failed to generate CERT object.");
			return false;
		}
		// get cert object
		if(NULL == (pCert = PK11_FindCertFromNickname(strNickName.c_str(), NULL))){
			ERR_CHMPRN("Failed to call PK11_FindCertFromNickname" CHM_NSS_ERR_PRN_FORM, CHM_NSS_ERR_PRN_ARGS);
			PK11_DestroyGenericObject(pk11CertObj);
			return false;
		}
		// generate Private Key object
		if(NULL == (pk11PKeyObj = ChmSecureSock::GeneratePK11GenericObject(pPrivateKeyFile, true))){
			ERR_CHMPRN("Failed to generate Private Key object.");
			if(pCert){
				CERT_DestroyCertificate(pCert);
			}
			PK11_DestroyGenericObject(pk11CertObj);
			return false;
		}
		// get Private Key
		if(NULL == (pPrivateKey = PK11_FindKeyByAnyCert(pCert, NULL))){
			ERR_CHMPRN("Failed to call PK11_FindKeyByAnyCert" CHM_NSS_ERR_PRN_FORM, CHM_NSS_ERR_PRN_ARGS);
			if(pCert){
				CERT_DestroyCertificate(pCert);
			}
			PK11_DestroyGenericObject(pk11CertObj);
			PK11_DestroyGenericObject(pk11PKeyObj);
			return false;
		}

	}else{
		// Case: pCertFile is nickname
		//
		if(CHMEMPTYSTR(pPrivateKeyFile)){
			// This case is only cert for nickname in nssdb.
			//
			// get cert object
			if(NULL == (pCert = PK11_FindCertFromNickname(strNickName.c_str(), NULL))){
				ERR_CHMPRN("Failed to call PK11_FindCertFromNickname" CHM_NSS_ERR_PRN_FORM, CHM_NSS_ERR_PRN_ARGS);
				return false;
			}
			// get Private Key
			if(NULL == (pPrivateKey = PK11_FindKeyByAnyCert(pCert, NULL))){
				ERR_CHMPRN("Failed to call PK11_FindKeyByAnyCert" CHM_NSS_ERR_PRN_FORM, CHM_NSS_ERR_PRN_ARGS);
				if(pCert){
					CERT_DestroyCertificate(pCert);
				}
				return false;
			}
		}else{
			// This case is some certs for nickname in nssdb.
			// Then need to search cert by private key buffer as "issuer:CN" from cert list.
			//
			CERTCertList*	pCertList;
			if(NULL == (pCertList = PK11_FindCertsFromNickname(strNickName.c_str(), NULL))){
				ERR_CHMPRN("Failed to call PK11_FindCertsFromNickname" CHM_NSS_ERR_PRN_FORM, CHM_NSS_ERR_PRN_ARGS);
				return false;
			}

			// search target cert
			for(CERTCertListNode* node = CERT_LIST_HEAD(pCertList); !CERT_LIST_END(node, pCertList); node = CERT_LIST_NEXT(node)){
				CERTCertificate*	pTmpCert = node->cert;
				char*				pIssuer;
				char*				pCN;
				if(!pTmpCert){
					WAN_CHMPRN("The cert object in cert list is NULL, thus skip it.");
					continue;
				}

				// get "issuer" as Ascii
				if(NULL == (pIssuer = CERT_NameToAscii(&(pTmpCert->issuer)))){
					MSG_CHMPRN("The cert object's issuer string in cert list is NULL, thus skip it.");
					continue;
				}

				// check "CN="
				if(NULL != (pCN = strstr(pIssuer, CHM_NSS_CN_KEY_IN_ISSUER))){
					pCN += strlen(CHM_NSS_CN_KEY_IN_ISSUER);

					// compare CN string
					if(0 == strncmp(pCN, pPrivateKeyFile, strlen(pPrivateKeyFile))){
						pCN += strlen(pPrivateKeyFile);

						// check end of string
						if('\0' == pCN[0] || ',' == pCN[0]){
							// found
							pCert = CERT_DupCertificate(pTmpCert);
							PR_Free(pIssuer);
							break;
						}else{
							MSG_CHMPRN("The cert object's issuer:CN string in cert list is not as same as \"%s\" key, thus skip it.", pPrivateKeyFile);
						}
					}else{
						MSG_CHMPRN("The cert object's issuer:CN string in cert list is not as same as \"%s\" key, thus skip it.", pPrivateKeyFile);
					}
				}else{
					MSG_CHMPRN("The cert object's issuer string in cert list does not have \"%s\" key, thus skip it.", CHM_NSS_CN_KEY_IN_ISSUER);
				}
				PR_Free(pIssuer);
			}
			// free
			CERT_DestroyCertList(pCertList);

			if(!pCert){
				ERR_CHMPRN("Could not find cert in cert list by nickname.");
				return false;
			}

			// get Private Key
			if(NULL == (pPrivateKey = PK11_FindKeyByAnyCert(pCert, NULL))){
				ERR_CHMPRN("Failed to call PK11_FindKeyByAnyCert" CHM_NSS_ERR_PRN_FORM, CHM_NSS_ERR_PRN_ARGS);
				if(pCert){
					CERT_DestroyCertificate(pCert);
				}
				return false;
			}
		}
	}
	*ppk11CertObj	= pk11CertObj;
	*ppk11PKeyObj	= pk11PKeyObj;
	*ppCert			= pCert;
	*ppPrivateKey	= pPrivateKey;

	return true;
}

bool ChmSecureSock::SetBlockingMode(PRFileDesc* SSSession, bool is_blocking)
{
	if(!SSSession){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	PRSocketOptionData			sock_opt;
	sock_opt.option				= PR_SockOpt_Nonblocking;
	sock_opt.value.non_blocking	= !is_blocking;

	if(PR_SUCCESS != PR_SetSocketOption(SSSession, &sock_opt)){
		ERR_CHMPRN("Failed to set blocking type to SSL session" CHM_NSS_ERR_PRN_FORM "by PR_Close", CHM_NSS_ERR_PRN_ARGS);
		return false;
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
	//ChmSSSessionEnt*	session = reinterpret_cast<ChmSSSessionEnt*>(sslsession);		// not used

	// Now not use these value
	/*
	if(CHMEVENTSOCK_RETRY_DEFAULT == retrycnt){
		retrycnt = ChmEventSock::DEFAULT_RETRYCNT;
		waittime = ChmEventSock::DEFAULT_WAIT_SOCKET;
	}
	*/

	bool	result = true;
	if(action_result < 0){
		if(PR_WOULD_BLOCK_ERROR == PR_GetError()){
			// sometimes gets PR_WOULD_BLOCK_ERROR on Nonblocking I/O, thus retry
			is_retry	= true;
			is_close	= false;
			result		= false;
		}else{
			MSG_CHMPRN("SSL action result(%ld) is failure" CHM_NSS_ERR_PRN_FORM "by PR_Close", action_result, CHM_NSS_ERR_PRN_ARGS);
			is_retry	= false;
			is_close	= false;
			result		= false;
		}
	}else if(action_result == 0){
		// Do we need to check deep in error code?
		MSG_CHMPRN("SSL action result(%ld) is close socket or EOF" CHM_NSS_ERR_PRN_FORM "by PR_Close", action_result, CHM_NSS_ERR_PRN_ARGS);
		is_retry	= false;
		is_close	= true;				// whether closing socket or not.
		result		= false;
	}else{	// 0 < action_result
		MSG_CHMPRN("SSL action result(%ld) is succeed", action_result);
		is_retry	= false;
		is_close	= false;
		result		= true;
	}
	return result;
}

// [NOTE]
// The processing content of this Callback function is almost the same as
// SSL_AuthCertificate. However, this bypasses verification of hostname.
//
SECStatus ChmSecureSock::AuthCertificateCallback(void* arg, PRFileDesc* fd, PRBool checksig, PRBool isServer)
{
	CERTCertificate*	certPeer		= NULL;
	SECStatus			resStatus		= SECSuccess;
	bool*				is_verify_peer	= reinterpret_cast<bool*>(arg);

	if(!is_verify_peer || !(*is_verify_peer)){
		// If not verify mode, thus return here.
		return resStatus;
	}

	// [NOTE]
	// If isServer is PR_TRUE, check the client's certificate as server side.
	// In the case of PR_FALSE, we validate the server's certificate as a client.
	//
	SECCertificateUsage	certUsage = isServer ? certificateUsageSSLClient : certificateUsageSSLServer;

	// Get Peer Cert
	if(NULL == (certPeer = SSL_PeerCertificate(fd))){
		ERR_CHMPRN("Failed to call SSL_PeerCertificate" CHM_NSS_ERR_PRN_FORM, CHM_NSS_ERR_PRN_ARGS);
		return SECFailure;
	}

	// [NOTE]
	// We do not use NSSDB which is set PIN.
	// If you need PIN, you can call SSL_RevealPinArg function for PIN argument here.
	//

	// Do Verify Cert with time now
	//
	// [NOTE]
	// Now we do not specify PIN argument(5'th param).
	//
	if(SECSuccess != (resStatus = CERT_VerifyCertificateNow(CERT_GetDefaultCertDB(), certPeer, checksig, certUsage, NULL, NULL))){
		ERR_CHMPRN("Failed to call CERT_VerifyCertificateNow with %s" CHM_NSS_ERR_PRN_FORM, CHM_NSS_STS_PRN_ARGS(resStatus), CHM_NSS_ERR_PRN_ARGS);
		CERT_DestroyCertificate(certPeer);
		return resStatus;
	}

	// [NOTE]
	// We do not check hostname in certification, but this is not a
	// good method for strict verification.
	// If you need it, you can call SSL_SetURL function before authenticating,
	// and call SSL_RevealURL function here, and check hostname.
	//

	CERT_DestroyCertificate(certPeer);

	return resStatus;
}

SECStatus ChmSecureSock::ClientAuthDataHookCallback(void* arg, PRFileDesc* sock, struct CERTDistNamesStr* caNames, struct CERTCertificateStr** pRetCert, struct SECKEYPrivateKeyStr** pRetKey)
{
	ChmSSCtx	ctx = reinterpret_cast<ChmSSCtx>(arg);
	if(!ctx){
		ERR_CHMPRN("SSL Context is null");
		return SECFailure;
	}

	if(ChmSecureSock::is_verify_peer && ctx->CERT_OBJ_slave && ctx->PKEY_OBJ_slave){
		// Copy(Duplicate)
		*pRetCert	= CERT_DupCertificate(ctx->CERT_OBJ_slave);
		*pRetKey	= SECKEY_CopyPrivateKey(ctx->PKEY_OBJ_slave);
	}else{
		string	strNickname;
		if(!ChmSecureSock::MakeNickname(ctx->strSlaveKey.c_str(), strNickname)){
			ERR_CHMPRN("Could not get nickname for slave cert");
			return SECFailure;
		}
		if(SECSuccess != NSS_GetClientAuthData(const_cast<void*>(reinterpret_cast<const void*>(strNickname.c_str())), sock, caNames, pRetCert, pRetKey)){
			ERR_CHMPRN("Failed to call default NSS_GetClientAuthData" CHM_NSS_ERR_PRN_FORM "by SSL_ImportFD", CHM_NSS_ERR_PRN_ARGS);
			return SECFailure;
		}
	}
	return SECSuccess;
}

// [NOTE]
// If you need to check error code, you can set following callback function.
// And checking the error code getting from PR_GetError(), and customizing
// allowing cert error(see: secerr.h).
//
// Examples: check and allow following error code.
//	SEC_ERROR_INVALID_AVA / SEC_ERROR_INVALID_TIME / SEC_ERROR_BAD_SIGNATURE / SEC_ERROR_EXPIRED_CERTIFICATE /
//	SEC_ERROR_UNKNOWN_ISSUER / SEC_ERROR_UNTRUSTED_CERT / SEC_ERROR_CERT_VALID / SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE / 
//	SEC_ERROR_CRL_EXPIRED / SEC_ERROR_CRL_BAD_SIGNATURE / SEC_ERROR_EXTENSION_VALUE_INVALID / SEC_ERROR_CA_CERT_INVALID / 
//	SEC_ERROR_CERT_USAGES_INVALID / SEC_ERROR_UNKNOWN_CRITICAL_EXTENSION
//
SECStatus ChmSecureSock::BadCertCallback(void* arg, PRFileDesc* fd)
{
	(void)arg;
	(void)fd;

	ERR_CHMPRN("Call BadCertCallback handler" CHM_NSS_ERR_PRN_FORM, CHM_NSS_ERR_PRN_ARGS);

	return SECFailure;
}

//
// [NOTE] About SSL_ImportFD().
// Even when it's time to close the file descriptor, always close the new PRFileDesc structure, never the old one.
//
PRFileDesc* ChmSecureSock::GetModelDescriptor(bool is_server, bool is_verify_peer)
{
	PRFileDesc*	tmp_model;
	PRFileDesc*	model;

	// Create model
	if(NULL == (tmp_model = PR_NewTCPSocket())){
		ERR_CHMPRN("Failed to make new model socket" CHM_NSS_ERR_PRN_FORM "by PR_NewTCPSocket", CHM_NSS_ERR_PRN_ARGS);
		return NULL;
	}
	if(NULL == (model = SSL_ImportFD(NULL, tmp_model))){
		ERR_CHMPRN("Failed to make new model SSL" CHM_NSS_ERR_PRN_FORM "by SSL_ImportFD", CHM_NSS_ERR_PRN_ARGS);
		if(PR_SUCCESS != PR_Close(tmp_model)){
			ERR_CHMPRN("Failed to close model" CHM_NSS_ERR_PRN_FORM "by PR_Close", CHM_NSS_ERR_PRN_ARGS);
		}
		return NULL;
	}

	//
	// Set options to model
	//
	// Use SSL
	if(SECSuccess != SSL_OptionSet(model, SSL_SECURITY, PR_TRUE)){
		ERR_CHMPRN("Failed to set to SSL_SECURITY(true) model descriptor" CHM_NSS_ERR_PRN_FORM, CHM_NSS_ERR_PRN_ARGS);
		if(PR_SUCCESS != PR_Close(model)){
			ERR_CHMPRN("Failed to close model" CHM_NSS_ERR_PRN_FORM "by PR_Close", CHM_NSS_ERR_PRN_ARGS);
		}
		return NULL;
	}
	// Verify Peer
	bool	local_is_verify_peer = (is_server ? is_verify_peer : false);
	if(SECSuccess != SSL_OptionSet(model, SSL_REQUEST_CERTIFICATE, (local_is_verify_peer ? PR_TRUE : PR_FALSE))){
		ERR_CHMPRN("Failed to set to SSL_REQUEST_CERTIFICATE(%s) model descriptor" CHM_NSS_ERR_PRN_FORM, (local_is_verify_peer ? "true" : "false"), CHM_NSS_ERR_PRN_ARGS);
		if(PR_SUCCESS != PR_Close(model)){
			ERR_CHMPRN("Failed to close model" CHM_NSS_ERR_PRN_FORM "by PR_Close", CHM_NSS_ERR_PRN_ARGS);
		}
		return NULL;
	}
	if(SECSuccess != SSL_OptionSet(model, SSL_REQUIRE_CERTIFICATE, (local_is_verify_peer ? PR_TRUE : PR_FALSE))){
		ERR_CHMPRN("Failed to set to SSL_REQUIRE_CERTIFICATE(%s) model descriptor" CHM_NSS_ERR_PRN_FORM, (local_is_verify_peer ? "true" : "false"), CHM_NSS_ERR_PRN_ARGS);
		if(PR_SUCCESS != PR_Close(model)){
			ERR_CHMPRN("Failed to close model" CHM_NSS_ERR_PRN_FORM "by PR_Close", CHM_NSS_ERR_PRN_ARGS);
		}
		return NULL;
	}
	// Server/Client mode
	if(SECSuccess != SSL_OptionSet(model, SSL_HANDSHAKE_AS_SERVER, (is_server ? PR_TRUE : PR_FALSE))){
		ERR_CHMPRN("Failed to set to SSL_HANDSHAKE_AS_SERVER(%s) model descriptor" CHM_NSS_ERR_PRN_FORM, (is_server ? "true" : "false"), CHM_NSS_ERR_PRN_ARGS);
		if(PR_SUCCESS != PR_Close(model)){
			ERR_CHMPRN("Failed to close model" CHM_NSS_ERR_PRN_FORM "by PR_Close", CHM_NSS_ERR_PRN_ARGS);
		}
		return NULL;
	}
	if(SECSuccess != SSL_OptionSet(model, SSL_HANDSHAKE_AS_CLIENT, (is_server ? PR_FALSE : PR_TRUE))){
		ERR_CHMPRN("Failed to set to SSL_HANDSHAKE_AS_CLIENT(%s) model descriptor" CHM_NSS_ERR_PRN_FORM, (is_server ? "false" : "true"), CHM_NSS_ERR_PRN_ARGS);
		if(PR_SUCCESS != PR_Close(model)){
			ERR_CHMPRN("Failed to close model" CHM_NSS_ERR_PRN_FORM "by PR_Close", CHM_NSS_ERR_PRN_ARGS);
		}
		return NULL;
	}
	// Set Half Dupled
	if(SECSuccess != SSL_OptionSet(model, SSL_ENABLE_FDX, PR_FALSE)){
		ERR_CHMPRN("Failed to set to SSL_ENABLE_FDX(false) model descriptor" CHM_NSS_ERR_PRN_FORM, CHM_NSS_ERR_PRN_ARGS);
		if(PR_SUCCESS != PR_Close(model)){
			ERR_CHMPRN("Failed to close model" CHM_NSS_ERR_PRN_FORM "by PR_Close", CHM_NSS_ERR_PRN_ARGS);
		}
		return NULL;
	}

	// SSL/TLS version
	//
	// [NOTE]
	// SSL2 is not enabled due to security problems.
	// Also, trying to make it effective makes an error.
	// Please check the following source code.
	// https://github.com/nss-dev/nss/blob/master/lib/ssl/sslsock.c#L686-L696
	//
	// At first, we enable SSL v3 and TLS
	//
	bool	is_one_succeed = false;
	#if	0	// SSL2
		if(SECSuccess != SSL_OptionSet(model, SSL_ENABLE_SSL2, PR_TRUE)){
			ERR_CHMPRN("Failed to set to SSL_ENABLE_SSL2(true) model descriptor" CHM_NSS_ERR_PRN_FORM ", but continue...", CHM_NSS_ERR_PRN_ARGS);
		}else{
			is_one_succeed = true;
			if(SECSuccess != SSL_OptionSet(model, SSL_V2_COMPATIBLE_HELLO, PR_TRUE)){
				ERR_CHMPRN("Failed to set to SSL_V2_COMPATIBLE_HELLO(true) model descriptor" CHM_NSS_ERR_PRN_FORM ", but continue...", CHM_NSS_ERR_PRN_ARGS);
			}
		}
	#endif
	if(SECSuccess != SSL_OptionSet(model, SSL_ENABLE_SSL3, PR_TRUE)){
		ERR_CHMPRN("Failed to set to SSL_ENABLE_SSL3(true) model descriptor" CHM_NSS_ERR_PRN_FORM ", but continue...", CHM_NSS_ERR_PRN_ARGS);
	}else{
		is_one_succeed = true;
	}
	if(SECSuccess != SSL_OptionSet(model, SSL_ENABLE_TLS, PR_TRUE)){
		ERR_CHMPRN("Failed to set to SSL_ENABLE_TLS(true) model descriptor" CHM_NSS_ERR_PRN_FORM ", but continue...", CHM_NSS_ERR_PRN_ARGS);
	}else{
		is_one_succeed = true;
	}
	if(!is_one_succeed){
		ERR_CHMPRN("Failed to set to any(SSL_ENABLE_SSL2/SSL_ENABLE_SSL3/SSL_ENABLE_TLS) model descriptor" CHM_NSS_ERR_PRN_FORM, CHM_NSS_ERR_PRN_ARGS);
		if(PR_SUCCESS != PR_Close(model)){
			ERR_CHMPRN("Failed to close model" CHM_NSS_ERR_PRN_FORM "by PR_Close", CHM_NSS_ERR_PRN_ARGS);
		}
		return NULL;
	}
	// Second, set minimum SSL/TLS version(range)
	if(!ChmSecureSock::SetMinVersion(model)){
		ERR_CHMPRN("Failed to set minimum SSL/TLS version range");
		return NULL;
	}

	// Enable Session Cache
	if(SECSuccess != SSL_OptionSet(model, SSL_NO_CACHE, PR_FALSE)){
		ERR_CHMPRN("Failed to set to SSL_NO_CACHE(false) model descriptor" CHM_NSS_ERR_PRN_FORM, CHM_NSS_ERR_PRN_ARGS);
		if(PR_SUCCESS != PR_Close(model)){
			ERR_CHMPRN("Failed to close model" CHM_NSS_ERR_PRN_FORM "by PR_Close", CHM_NSS_ERR_PRN_ARGS);
		}
		return NULL;
	}
	// Set rollback detection
	if(SECSuccess != SSL_OptionSet(model, SSL_ROLLBACK_DETECTION, PR_TRUE)){
		ERR_CHMPRN("Failed to set to SSL_ROLLBACK_DETECTION(true) model descriptor" CHM_NSS_ERR_PRN_FORM, CHM_NSS_ERR_PRN_ARGS);
		if(PR_SUCCESS != PR_Close(model)){
			ERR_CHMPRN("Failed to close model" CHM_NSS_ERR_PRN_FORM "by PR_Close", CHM_NSS_ERR_PRN_ARGS);
		}
		return NULL;
	}
	// [NOTE]
	// SSL_ENABLE_DEFLATE, etc is used as default value.
	//

	// set AuthCertificateHook for verifying incoming peer cert.
	if(SECSuccess != SSL_AuthCertificateHook(model, ChmSecureSock::AuthCertificateCallback, const_cast<void*>(reinterpret_cast<const void*>(is_verify_peer ? &chm_nss_true : &chm_nss_false)))){
		ERR_CHMPRN("Failed to set AuthCertificateHook" CHM_NSS_ERR_PRN_FORM, CHM_NSS_ERR_PRN_ARGS);
		if(PR_SUCCESS != PR_Close(model)){
			ERR_CHMPRN("Failed to close model" CHM_NSS_ERR_PRN_FORM "by PR_Close", CHM_NSS_ERR_PRN_ARGS);
		}
		return NULL;
	}

	// [NOTE]
	// Now we do not need to set callbacks for handshaking.
	// if you need those, you can call SSL_HandshakeCallback/SSL_BadCertHook
	// functions here.
	//
	//	if(SECSuccess != SSL_BadCertHook(model, ChmSecureSock::BadCertCallback, NULL)){
	//		ERR_CHMPRN("Failed to set BadCertHook" CHM_NSS_ERR_PRN_FORM, CHM_NSS_ERR_PRN_ARGS);
	//		if(PR_SUCCESS != PR_Close(model)){
	//			ERR_CHMPRN("Failed to close model" CHM_NSS_ERR_PRN_FORM "by PR_Close", CHM_NSS_ERR_PRN_ARGS);
	//		}
	//		return NULL;
	//	}
	//

	return model;
}

ChmSSSession ChmSecureSock::AcceptSSL(ChmSSCtx ctx, int sock, int con_retrycnt, suseconds_t con_waittime)
{
	PRFileDesc*		nspr_sock;
	PRFileDesc*		model;
	PRFileDesc*		SSSession;
	SSLKEAType		KEAType_cert;
	chmpk11list_t	CAPk11objs;

	if(!ctx || !ctx->CERT_OBJ_server){
		ERR_CHMPRN("SSL context is wrong.");
		return NULL;
	}
	if(CHM_INVALID_SOCK == sock){
		ERR_CHMPRN("sock parameter is wrong.");
		return NULL;
	}

	// load CA certs
	if(!ChmSecureSock::LoadCACerts(CAPk11objs)){
		ERR_CHMPRN("Failed to load CA certs");
		return NULL;
	}

	// [NOTE]
	// Private API, no other way to turn a POSIX file descriptor into an NSPR socket.
	//
	if(NULL == (nspr_sock = PR_ImportTCPSocket(sock))){
		ERR_CHMPRN("Could not convert sock(%d) to NSPR socket" CHM_NSS_ERR_PRN_FORM, sock, CHM_NSS_ERR_PRN_ARGS);
		return NULL;
	}

	// Create model descriptor
	if(NULL == (model = ChmSecureSock::GetModelDescriptor(true, ChmSecureSock::is_verify_peer))){
		ERR_CHMPRN("Failed to create new model descriptor.");
		if(PR_SUCCESS != PR_Close(nspr_sock)){
			ERR_CHMPRN("Failed to close NSPR Socket" CHM_NSS_ERR_PRN_FORM "by PR_Close", CHM_NSS_ERR_PRN_ARGS);
		}
		return NULL;
	}

	// Import model descriptor onto SSL socket
	if(NULL == (SSSession = SSL_ImportFD(model, nspr_sock))){
		ERR_CHMPRN("Failed to import SSL session from TCP socket" CHM_NSS_ERR_PRN_FORM "by SSL_ImportFD", CHM_NSS_ERR_PRN_ARGS);
		if(PR_SUCCESS != PR_Close(model)){
			ERR_CHMPRN("Failed to close model" CHM_NSS_ERR_PRN_FORM "by PR_Close", CHM_NSS_ERR_PRN_ARGS);
		}
		if(PR_SUCCESS != PR_Close(nspr_sock)){
			ERR_CHMPRN("Failed to close NSPR Socket" CHM_NSS_ERR_PRN_FORM "by PR_Close", CHM_NSS_ERR_PRN_ARGS);
		}
		return NULL;
	}
	PR_Close(model);

	// [NOTE]
	// We do not set callback functions, because the default call back function is enough for us.
	// If you need these, you can call SSL_AuthCertificateHook/SSL_BadCertHook/PK11_SetPasswordFunc/
	// SSL_SetPKCS11PinArg/SSL_RevealPinArg/etc and implement callback functions here.
	//
	// [For examples]
	// See: Server Side Sample
	//		https://github.com/servo/nss/blob/master/cmd/vfyserv/vfyserv.c
	// See: Sample Callback Functions
	//		https://github.com/servo/nss/blob/master/cmd/vfyserv/vfyutil.c
	//

	// get KEA type from cert
	KEAType_cert = NSS_FindCertKEAType(ctx->CERT_OBJ_server);

	// set CERT/PrivateKey/KEA to SSL session
	if(SECSuccess != SSL_ConfigSecureServer(SSSession, ctx->CERT_OBJ_server, ctx->PKEY_OBJ_server, KEAType_cert)){
		ERR_CHMPRN("Failed to set CERT/PrivateKey/KEA" CHM_NSS_ERR_PRN_FORM, CHM_NSS_ERR_PRN_ARGS);
		if(PR_SUCCESS != PR_Close(SSSession)){
			ERR_CHMPRN("Failed to close SSL Session" CHM_NSS_ERR_PRN_FORM "by PR_Close", CHM_NSS_ERR_PRN_ARGS);
		}
		return NULL;
	}

	// convert timeout value
	PRUint32	timeout_ms = PR_MillisecondsToInterval(static_cast<PRUint32>((con_waittime / 1000) * con_retrycnt));

	// Set Non-blocking socket to blocking
	if(!ChmSecureSock::SetBlockingMode(SSSession, true)){
		ERR_CHMPRN("Failed to set blocking type to SSL session");
		if(PR_SUCCESS != PR_Close(SSSession)){
			ERR_CHMPRN("Failed to close SSL Session" CHM_NSS_ERR_PRN_FORM "by PR_Close", CHM_NSS_ERR_PRN_ARGS);
		}
		return NULL;
	}

	// Loop handshake
	bool	is_succeed = false;
	for(int cnt = 0; cnt <= con_retrycnt; ++cnt){
		// Reset handshake
		if(SECSuccess != SSL_ResetHandshake(SSSession, PR_TRUE)){
			ERR_CHMPRN("Failed to reset handshake" CHM_NSS_ERR_PRN_FORM, CHM_NSS_ERR_PRN_ARGS);
			if(PR_SUCCESS != PR_Close(SSSession)){
				ERR_CHMPRN("Failed to close SSL Session" CHM_NSS_ERR_PRN_FORM "by PR_Close", CHM_NSS_ERR_PRN_ARGS);
			}
			break;
		}
		// Force do the handshake
		if(SECSuccess == SSL_ForceHandshakeWithTimeout(SSSession, timeout_ms)){
			// Succeed
			is_succeed = true;
			break;
		}
		ERR_CHMPRN("Failed to handshake with timeout" CHM_NSS_ERR_PRN_FORM ", do retry...", CHM_NSS_ERR_PRN_ARGS);
	}
	if(!is_succeed){
		ERR_CHMPRN("Timeouted or retry limited to handshake");
		if(PR_SUCCESS != PR_Close(SSSession)){
			ERR_CHMPRN("Failed to close SSL Session" CHM_NSS_ERR_PRN_FORM "by PR_Close", CHM_NSS_ERR_PRN_ARGS);
		}
		return NULL;
	}
	MSG_CHMPRN("Connected session(handshaked) : %s", ChmSecureSock::GetSessionInfo(SSSession).c_str());

	// Set blocking socket to Non-blocking
	if(!ChmSecureSock::SetBlockingMode(SSSession, false)){
		ERR_CHMPRN("Failed to set blocking type to SSL session");
		if(PR_SUCCESS != PR_Close(SSSession)){
			ERR_CHMPRN("Failed to close SSL Session" CHM_NSS_ERR_PRN_FORM "by PR_Close", CHM_NSS_ERR_PRN_ARGS);
		}
		return NULL;
	}

	ChmSSSessionEnt*	sslsession = new ChmSSSessionEnt(ctx, SSSession, &CAPk11objs);

	return reinterpret_cast<void*>(sslsession);
}

ChmSSSession ChmSecureSock::ConnectSSL(ChmSSCtx ctx, int sock, int con_retrycnt, suseconds_t con_waittime)
{
	PRFileDesc*		nspr_sock;
	PRFileDesc*		model;
	PRFileDesc*		SSSession;
	chmpk11list_t	CAPk11objs;

	if(!ctx || (ChmSecureSock::is_verify_peer && !ctx->CERT_OBJ_slave)){
		ERR_CHMPRN("SSL context is wrong.");
		return NULL;
	}
	if(CHM_INVALID_SOCK == sock){
		ERR_CHMPRN("sock parameter is wrong.");
		return NULL;
	}

	// load CA certs
	if(!ChmSecureSock::LoadCACerts(CAPk11objs)){
		ERR_CHMPRN("Failed to load CA certs");
		return NULL;
	}

	// [NOTE]
	// Private API, no other way to turn a POSIX file descriptor into an NSPR socket.
	//
	if(NULL == (nspr_sock = PR_ImportTCPSocket(sock))){
		ERR_CHMPRN("Could not convert sock(%d) to NSPR socket" CHM_NSS_ERR_PRN_FORM, sock, CHM_NSS_ERR_PRN_ARGS);
		return NULL;
	}

	// Create model descriptor
	if(NULL == (model = ChmSecureSock::GetModelDescriptor(false, ChmSecureSock::is_verify_peer))){
		ERR_CHMPRN("Failed to create new model descriptor.");
		if(PR_SUCCESS != PR_Close(nspr_sock)){
			ERR_CHMPRN("Failed to close NSPR Socket" CHM_NSS_ERR_PRN_FORM "by PR_Close", CHM_NSS_ERR_PRN_ARGS);
		}
		return NULL;
	}

	// Import model descriptor onto SSL socket
	if(NULL == (SSSession = SSL_ImportFD(model, nspr_sock))){
		ERR_CHMPRN("Failed to import SSL session from TCP socket" CHM_NSS_ERR_PRN_FORM "by SSL_ImportFD", CHM_NSS_ERR_PRN_ARGS);
		if(PR_SUCCESS != PR_Close(model)){
			ERR_CHMPRN("Failed to close model" CHM_NSS_ERR_PRN_FORM "by PR_Close", CHM_NSS_ERR_PRN_ARGS);
		}
		if(PR_SUCCESS != PR_Close(nspr_sock)){
			ERR_CHMPRN("Failed to close NSPR Socket" CHM_NSS_ERR_PRN_FORM "by PR_Close", CHM_NSS_ERR_PRN_ARGS);
		}
		return NULL;
	}
	PR_Close(model);

	// [NOTE]
	// We do not set callback functions, because the default call back function is enough for us.
	// If you need these, you can call SSL_AuthCertificateHook/SSL_BadCertHook/PK11_SetPasswordFunc/
	// SSL_SetPKCS11PinArg/SSL_RevealPinArg/etc and implement callback functions here.
	//
	// [For examples]
	// See: Server Side Sample
	//		https://github.com/servo/nss/blob/master/cmd/vfyserv/vfyserv.c
	// See: Sample Callback Functions
	//		https://github.com/servo/nss/blob/master/cmd/vfyserv/vfyutil.c
	//

	if(ChmSecureSock::is_verify_peer){
		// set callback for verify cert
		if(SECSuccess != SSL_GetClientAuthDataHook(SSSession, ChmSecureSock::ClientAuthDataHookCallback, reinterpret_cast<void*>(ctx))){
			ERR_CHMPRN("Failed to call SSL_GetClientAuthDataHook" CHM_NSS_ERR_PRN_FORM, CHM_NSS_ERR_PRN_ARGS);
			if(PR_SUCCESS != PR_Close(SSSession)){
				ERR_CHMPRN("Failed to close SSL Session" CHM_NSS_ERR_PRN_FORM "by PR_Close", CHM_NSS_ERR_PRN_ARGS);
			}
			return NULL;
		}
	}

	// convert timeout value
	PRUint32	timeout_ms = PR_MillisecondsToInterval(static_cast<PRUint32>((con_waittime / 1000) * con_retrycnt));

	// Set Non-blocking socket to blocking
	if(!ChmSecureSock::SetBlockingMode(SSSession, true)){
		ERR_CHMPRN("Failed to set blocking type to SSL session");
		if(PR_SUCCESS != PR_Close(SSSession)){
			ERR_CHMPRN("Failed to close SSL Session" CHM_NSS_ERR_PRN_FORM "by PR_Close", CHM_NSS_ERR_PRN_ARGS);
		}
		return NULL;
	}

	// Loop handshake
	bool	is_succeed = false;
	for(int cnt = 0; cnt <= con_retrycnt; ++cnt){
		// Reset handshake
		if(SECSuccess != SSL_ResetHandshake(SSSession, PR_FALSE)){
			ERR_CHMPRN("Failed to reset handshake" CHM_NSS_ERR_PRN_FORM, CHM_NSS_ERR_PRN_ARGS);
			break;
		}

		// Force do the handshake
		if(SECSuccess == SSL_ForceHandshakeWithTimeout(SSSession, timeout_ms)){
			// Succeed
			is_succeed = true;
			break;
		}
		ERR_CHMPRN("Failed to handshake with timeout" CHM_NSS_ERR_PRN_FORM ", do retry...", CHM_NSS_ERR_PRN_ARGS);
	}
	if(!is_succeed){
		ERR_CHMPRN("Timeouted or retry limited to handshake");
		if(PR_SUCCESS != PR_Close(SSSession)){
			ERR_CHMPRN("Failed to close SSL Session" CHM_NSS_ERR_PRN_FORM "by PR_Close", CHM_NSS_ERR_PRN_ARGS);
		}
		return NULL;
	}
	MSG_CHMPRN("Connected session(handshaked) : %s", ChmSecureSock::GetSessionInfo(SSSession).c_str());

	// Set blocking socket to Non-blocking
	if(!ChmSecureSock::SetBlockingMode(SSSession, false)){
		ERR_CHMPRN("Failed to set blocking type to SSL session");
		if(PR_SUCCESS != PR_Close(SSSession)){
			ERR_CHMPRN("Failed to close SSL Session" CHM_NSS_ERR_PRN_FORM "by PR_Close", CHM_NSS_ERR_PRN_ARGS);
		}
		return NULL;
	}

	ChmSSSessionEnt*	sslsession = new ChmSSSessionEnt(ctx, SSSession, &CAPk11objs);

	return reinterpret_cast<void*>(sslsession);
}

bool ChmSecureSock::ShutdownSSL(int sock, ChmSSSession sslsession, int con_retrycnt, suseconds_t con_waittime)
{
	(void)con_retrycnt;
	(void)con_waittime;

	if(CHM_INVALID_SOCK == sock || !sslsession){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}
	ChmSSSessionEnt*	session 	= reinterpret_cast<ChmSSSessionEnt*>(sslsession);
	PRFileDesc*			nspr_sock	= session->SSSession;

	if(PR_SUCCESS != PR_Close(nspr_sock)){
		ERR_CHMPRN("Failed to close SSL Session" CHM_NSS_ERR_PRN_FORM "by PR_Close", CHM_NSS_ERR_PRN_ARGS);
		return false;
	}
	ChmSecureSock::FreeSSLSessionEx(session);

	return true;
}

int ChmSecureSock::Read(ChmSSSession sslsession, void* pbuf, int length)
{
	if(!sslsession){
		ERR_CHMPRN("Parameter is wrong.");
		return -1;
	}
	ChmSSSessionEnt*	session 	= reinterpret_cast<ChmSSSessionEnt*>(sslsession);
	PRFileDesc*			nspr_sock	= session->SSSession;
	PRInt32				result;
	result = PR_Read(nspr_sock, pbuf, static_cast<PRInt32>(length));

	return static_cast<int>(result);
}

int ChmSecureSock::Write(ChmSSSession sslsession, const void* pbuf, int length)
{
	if(!sslsession){
		ERR_CHMPRN("Parameter is wrong.");
		return -1;
	}
	ChmSSSessionEnt*	session 	= reinterpret_cast<ChmSSSessionEnt*>(sslsession);
	PRFileDesc*			nspr_sock	= session->SSSession;
	PRInt32				result;
	result = PR_Write(nspr_sock, pbuf, static_cast<PRInt32>(length));

	return static_cast<int>(result);
}

void ChmSecureSock::AllowSelfSignedCert(void)
{
	ChmSecureSock::is_self_sigined = true;
}

void ChmSecureSock::DenySelfSignedCert(void)
{
	ChmSecureSock::is_self_sigined = false;
}

bool ChmSecureSock::IsSafeSSLContextEx(const char* server_cert, const char* server_prikey, const char* slave_cert, const char* slave_prikey)
{
	// [NOTE]
	// NSS does not officially support loading from PEM file.
	// Please note that the CERT argument is Nickname, not PEM filename.
	// However, in RHEL/CentOS system, libnsspam.so is supported by Redhat,
	// it is possible to load PEM files, CERT and PRIVATE KEY may be files.
	// Processing becomes complicated and costs for file confirmation are
	// incurred, so this function does not check PRIVATE KEY.
	//
	if(CHMEMPTYSTR(server_cert) && (ChmSecureSock::is_verify_peer && CHMEMPTYSTR(slave_cert))){
		return false;
	}
	return true;
}

ChmSSCtx ChmSecureSock::GetSSLContextEx(const char* server_cert, const char* server_prikey, const char* slave_cert, const char* slave_prikey)
{
	if(CHMEMPTYSTR(server_cert) && (ChmSecureSock::is_verify_peer && CHMEMPTYSTR(slave_cert))){
		ERR_CHMPRN("Parameters are wrong.");
		return NULL;
	}
	PK11GenericObject*	pk11CertObj	= NULL;
	PK11GenericObject*	pk11PKeyObj	= NULL;
	CERTCertificate*	pCert		= NULL;
	SECKEYPrivateKey*	pPrivateKey	= NULL;
	ChmSSCert			CertObj		= NULL;
	ChmSSCtx			pctx		= new ChmSSCtxEnt();
	string				strNickname("");
	string				strKey("");
	bool				isExistFile	= false;

	PR_Lock(ChmSecureSock::CertMapLock);

	// check existed server cert, and create if not exist
	if(!CHMEMPTYSTR(server_cert)){
		if(!ChmSecureSock::CheckCertNicknameType(server_cert, server_prikey, strNickname, strKey, isExistFile)){
			ERR_CHMPRN("Failed to check cert type and nickname, because server cert path or nickname is something wrong.");
			PR_Unlock(ChmSecureSock::CertMapLock);
			CHM_Delete(pctx);
			return NULL;
		}

		chmsscertmap_t::iterator	iter= ChmSecureSock::GetCertMap().find(strKey);
		if(ChmSecureSock::GetCertMap().end() == iter){
			// load
			if(!ChmSecureSock::LoadCertPrivateKey(server_cert, server_prikey, &pk11CertObj, &pk11PKeyObj, &pCert, &pPrivateKey)){
				ERR_CHMPRN("Failed to create SSL context for server");
				PR_Unlock(ChmSecureSock::CertMapLock);
				CHM_Delete(pctx);
				return NULL;
			}
			// create new
			CertObj = new ChmSSCertEnt();
			if(!CertObj->set(server_cert, server_prikey, pk11CertObj, pk11PKeyObj, pCert, pPrivateKey)){
				ERR_CHMPRN("Could not create Cert object");
				if(pk11CertObj){
					PK11_DestroyGenericObject(pk11CertObj);
				}
				if(pk11PKeyObj){
					PK11_DestroyGenericObject(pk11PKeyObj);
				}
				if(pCert){
					CERT_DestroyCertificate(pCert);
				}
				if(pPrivateKey){
					SECKEY_DestroyPrivateKey(pPrivateKey);
				}
				PR_Unlock(ChmSecureSock::CertMapLock);
				CHM_Delete(pctx);
				CHM_Delete(CertObj);
				return NULL;
			}
			ChmSecureSock::GetCertMap()[strKey] = CertObj;
		}else{
			CertObj = iter->second;
		}

		// set context
		if(!pctx->set(true, server_cert, CertObj->CERT_obj, CertObj->PKEY_obj)){
			ERR_CHMPRN("Failed to set SSL context for server to data structure");
			PR_Unlock(ChmSecureSock::CertMapLock);
			CHM_Delete(pctx);
			return NULL;
		}
	}

	// check existed slave cert, and create if not exist
	if(!CHMEMPTYSTR(slave_cert)){
		pk11CertObj						= NULL;
		pk11PKeyObj						= NULL;
		pCert							= NULL;
		pPrivateKey						= NULL;

		if(!ChmSecureSock::CheckCertNicknameType(slave_cert, slave_prikey, strNickname, strKey, isExistFile)){
			ERR_CHMPRN("Failed to check cert type and nickname, because slave cert path or nickname is something wrong.");
			PR_Unlock(ChmSecureSock::CertMapLock);
			CHM_Delete(pctx);
			return NULL;
		}

		chmsscertmap_t::iterator	iter= ChmSecureSock::GetCertMap().find(strKey);
		if(ChmSecureSock::GetCertMap().end() == iter){
			// load
			if(!ChmSecureSock::LoadCertPrivateKey(slave_cert, slave_prikey, &pk11CertObj, &pk11PKeyObj, &pCert, &pPrivateKey)){
				ERR_CHMPRN("Failed to create SSL context for server");
				PR_Unlock(ChmSecureSock::CertMapLock);
				CHM_Delete(pctx);
				return NULL;
			}
			// create new
			CertObj = new ChmSSCertEnt();
			if(!CertObj->set(slave_cert, slave_prikey, pk11CertObj, pk11PKeyObj, pCert, pPrivateKey)){
				ERR_CHMPRN("Could not create Cert object");
				if(pk11CertObj){
					PK11_DestroyGenericObject(pk11CertObj);
				}
				if(pk11PKeyObj){
					PK11_DestroyGenericObject(pk11PKeyObj);
				}
				if(pCert){
					CERT_DestroyCertificate(pCert);
				}
				if(pPrivateKey){
					SECKEY_DestroyPrivateKey(pPrivateKey);
				}
				PR_Unlock(ChmSecureSock::CertMapLock);
				CHM_Delete(pctx);
				CHM_Delete(CertObj);
				return NULL;
			}
			ChmSecureSock::GetCertMap()[strKey] = CertObj;
		}else{
			CertObj = iter->second;
		}

		// set context
		if(!pctx->set(false, slave_cert, CertObj->CERT_obj, CertObj->PKEY_obj)){
			ERR_CHMPRN("Failed to set SSL context for server to data structure");
			PR_Unlock(ChmSecureSock::CertMapLock);
			CHM_Delete(pctx);
			return NULL;
		}
	}
	PR_Unlock(ChmSecureSock::CertMapLock);

	return pctx;
}

void ChmSecureSock::FreeSSLContextEx(ChmSSCtx ctx)
{
	if(ctx){
		ChmSSCtxEnt*	pCtx = reinterpret_cast<ChmSSCtxEnt*>(ctx);
		CHM_Delete(pCtx);
	}
}

void ChmSecureSock::FreeSSLSessionEx(ChmSSSession sslsession)
{
	if(sslsession){
		ChmSSSessionEnt*	session = reinterpret_cast<ChmSSSessionEnt*>(sslsession);
		CHM_Delete(session);
	}
}

//------------------------------------------------------
// Methods
//------------------------------------------------------
// cppcheck-suppress unmatchedSuppression
// cppcheck-suppress uninitMemberVar
ChmSecureSock::ChmSecureSock(const char* CApath, const char* CAfile, bool is_verify_peer) : nss_ctx(NULL)
{
	if(!ChmSecureSock::InitLibrary(&nss_ctx, CApath, CAfile, is_verify_peer)){
		ERR_CHMPRN("Failed initializing NSS context");
	}
}

ChmSecureSock::~ChmSecureSock(void)
{
	ChmSecureSock::FreeLibrary(nss_ctx);
	nss_ctx = NULL;
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
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
