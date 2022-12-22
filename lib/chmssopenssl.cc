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

#include "chmcommon.h"
#include "chmssopenssl.h"
#include "chmutil.h"
#include "chmdbg.h"

using namespace	std;

//------------------------------------------------------
// Symbols
//------------------------------------------------------
#define	CHMEVENTSOCK_SSL_VP_DEPTH					3
#define	CHMEVENTSOCK_VCB_INDEX_STR					"verify_cb_data_index"

//------------------------------------------------------
// Class Variables
//------------------------------------------------------
int			ChmSecureSock::CHM_SSL_VERIFY_DEPTH		= CHMEVENTSOCK_SSL_VP_DEPTH;
const char*	ChmSecureSock::strVerifyCBDataIndex		= CHMEVENTSOCK_VCB_INDEX_STR;
int			ChmSecureSock::verify_cb_data_index		= -1;
int			ChmSecureSock::ssl_session_id			= static_cast<int>(getpid());
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
	// Now(after 1.1.0) it does not need to call OPENSSL_init_ssl()/SSL_library_init() etc.
	// But we call old type functions for under 1.1.0.
	//
	SSL_load_error_strings();
	SSL_library_init();

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
	// Now(after 1.1.0) it does not need to call ERR_free_strings() etc.
	// But we call old type functions for under 1.1.0.
	//
	ERR_free_strings();

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
	return "OpenSSL";
}

bool ChmSecureSock::IsCAPathAllowFile(void)
{
	return true;
}

bool ChmSecureSock::IsCertAllowName(void)
{
	return false;
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


//
// [NOTE][FIXME]
// In OpenSSL 1.1.0 and later, the use of SSL_CTX_set_min_proto_version() is recommended
// and should be used.
// However, before OpenSSL 1.1.0, since SSL_CTX_set_min_proto_version function do not exist,
// we will use following in future instead of SSL_CTX_set_options().
//
bool ChmSecureSock::SetMinVersion(SSL_CTX* ctx)
{
	long	options = 0L;

	switch(ChmSecureSock::ssl_min_ver){
		case	CHM_SSLTLS_VER_TLSV1_3:
			options |= SSL_OP_NO_TLSv1_2;

		case	CHM_SSLTLS_VER_TLSV1_2:
			options |= SSL_OP_NO_TLSv1_1;

		case	CHM_SSLTLS_VER_TLSV1_1:
			options |= SSL_OP_NO_TLSv1;

		case	CHM_SSLTLS_VER_TLSV1_0:
		case	CHM_SSLTLS_VER_DEFAULT:		// Default is after TLS v1.0
			options |= SSL_OP_NO_SSLv3;

		case	CHM_SSLTLS_VER_SSLV3:
			options |= SSL_OP_NO_SSLv2;
			break;

		case	CHM_SSLTLS_VER_SSLV2:
			// Nothing to do
			MSG_CHMPRN("NOTICE - SSLv2 is allowed.");
			return true;

		default:
			ERR_CHMPRN("SSL/TLS minimum version value(%s) is something wrong, thus we use %s setting as default", CHM_GET_STR_SSLTLS_VERSION(ChmSecureSock::ssl_min_ver), CHM_GET_STR_SSLTLS_VERSION(CHM_SSLTLS_VER_SSLV3));
			options |= SSL_OP_NO_SSLv3;
			options |= SSL_OP_NO_SSLv2;
			break;
	}
	SSL_CTX_set_options(ctx, options);

	return true;
}

string ChmSecureSock::GetSessionInfo(SSL* ssl)
{
	string	strInfo("(error)");
	if(!ssl){
		ERR_CHMPRN("Parameter is wrong.");
		return strInfo;
	}
	const char*	pVersion	= SSL_get_version(ssl);
	const char*	pCipher		= SSL_get_cipher(ssl);

	strInfo	 = CHMEMPTYSTR(pVersion)? "unknown SSL/TLS version"	: pVersion;
	strInfo += " - ";
	strInfo += CHMEMPTYSTR(pCipher)	? "unknown cipher suite"	: pCipher;

	return strInfo;
}

int ChmSecureSock::VerifyCallBackSSL(int preverify_ok, X509_STORE_CTX* store_ctx)
{
	char			strerr[256];
	X509*			err_cert	= X509_STORE_CTX_get_current_cert(store_ctx);
	int				err_code	= X509_STORE_CTX_get_error(store_ctx);
	int				depth		= X509_STORE_CTX_get_error_depth(store_ctx);
	SSL*			ssl			= reinterpret_cast<SSL*>(X509_STORE_CTX_get_ex_data(store_ctx, SSL_get_ex_data_X509_STORE_CTX_idx()));
	SSL_CTX*		ssl_ctx		= SSL_get_SSL_CTX(ssl);
	const int*		pchm_depth	= reinterpret_cast<const int*>(SSL_CTX_get_ex_data(ssl_ctx, ChmSecureSock::verify_cb_data_index));

	// error string
	X509_NAME_oneline(X509_get_subject_name(err_cert), strerr, sizeof(strerr));

	// depth
	if(!pchm_depth || *pchm_depth < depth){
		// Force changing error code.
		preverify_ok= 0;
		err_code	= X509_V_ERR_CERT_CHAIN_TOO_LONG;
		X509_STORE_CTX_set_error(store_ctx, err_code);
	}

	// Message
	if(!preverify_ok){
		ERR_CHMPRN("VERIFY ERROR: depth=%d, errnum=%d(%s), string=%s", depth, err_code, X509_verify_cert_error_string(err_code), strerr);
	}else{
		MSG_CHMPRN("VERIFY OK: depth=%d, string=%s", depth, strerr);
	}

	// check error code
	switch(err_code){
		case	X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
			X509_NAME_oneline(X509_get_issuer_name(err_cert), strerr, 256);
			ERR_CHMPRN("DETAIL: X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT: issuer=%s", strerr);
			preverify_ok = 0;
			break;

		case	X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
			if(ChmSecureSock::is_self_sigined){
				// For DEBUG
				WAN_CHMPRN("SKIP ERROR(DEBUG): X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT: self signed certificate is ERROR.");
				preverify_ok = 1;
			}else{
				ERR_CHMPRN("DETAIL: X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT: self signed certificate is ERROR.");
				preverify_ok = 0;
			}
			break;

		case	X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
			X509_STORE_CTX_set_error(store_ctx, X509_V_OK);
			WAN_CHMPRN("SKIP ERROR: X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN: Verified with no error in self signed certificate chain.");
			preverify_ok = 1;
			break;

		case	X509_V_ERR_CERT_NOT_YET_VALID:
			ERR_CHMPRN("DETAIL: X509_V_ERR_CERT_NOT_YET_VALID: certificate is not yet valid(date is after the current time).");
			preverify_ok = 0;
			break;

		case	X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD:
			ERR_CHMPRN("DETAIL: X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD: certificate not before field contains an invalid time.");
			preverify_ok = 0;
			break;

		case	X509_V_ERR_CERT_HAS_EXPIRED:
			ERR_CHMPRN("DETAIL: X509_V_ERR_CERT_HAS_EXPIRED: certificate has expired.");
			preverify_ok = 0;
			break;

		case	X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD:
			ERR_CHMPRN("DETAIL: X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD: certificate not after field contains an invalid time.");
			preverify_ok = 0;
			break;

		default:
			break;
	}
	return preverify_ok;
}

bool ChmSecureSock::CheckResultSSL(int sock, ChmSSSession sslsession, long action_result, int type, bool& is_retry, bool& is_close, int retrycnt, suseconds_t waittime)
{
	if(CHM_INVALID_SOCK == sock || !sslsession || !IS_SAFE_CHKRESULTSSL_TYPE(type)){
		ERR_CHMPRN("Parameters are wrong.");
		is_retry = false;
		return false;
	}
	SSL*	ssl = reinterpret_cast<SSL*>(sslsession);

	if(CHMEVENTSOCK_RETRY_DEFAULT == retrycnt){
		retrycnt = ChmEventSock::DEFAULT_RETRYCNT;
		waittime = ChmEventSock::DEFAULT_WAIT_SOCKET;
	}
	is_retry = true;
	is_close = false;

	bool	result = true;
	if(action_result <= 0){
		int	werr;
		int	ssl_result = SSL_get_error(ssl, action_result);
		switch(ssl_result){
			case	SSL_ERROR_NONE:
				// Succeed.
				MSG_CHMPRN("SSL action result(%ld): ssl result(%d: %s), succeed.", action_result, ssl_result, ERR_error_string(ssl_result, NULL));
				break;

			case	SSL_ERROR_SSL:
				if(CHKRESULTSSL_TYPE_SD == type){
					WAN_CHMPRN("SSL action result(%ld): ssl result(%d: %s). not retry to shutdown.", action_result, ssl_result, ERR_error_string(ssl_result, NULL));
					is_retry= false;
					result	= false;
				}else{
					ERR_CHMPRN("SSL action result(%ld): ssl result(%d: %s), so something error occured(errno=%d).", action_result, ssl_result, ERR_error_string(ssl_result, NULL), errno);
					is_retry= false;
					is_close= true;
					result	= false;
				}
				break;

			case	SSL_ERROR_WANT_WRITE:
				// Wait for up
				if(0 != (werr = ChmEventSock::WaitForReady(sock, WAIT_WRITE_FD, retrycnt, false, waittime))){		// not check SO_ERROR
					ERR_CHMPRN("SSL action result(%ld): ssl result(%d: %s), and Failed to wait write.", action_result, ssl_result, ERR_error_string(ssl_result, NULL));
					is_retry = false;
					if(ETIMEDOUT != werr){
						is_close= true;
					}
				}else{
					//MSG_CHMPRN("SSL action result(%ld): ssl result(%d: %s), and Succeed to wait write.", action_result, ssl_result, ERR_error_string(ssl_result, NULL));
				}
				result = false;
				break;

			case	SSL_ERROR_WANT_READ:
				// Wait for up
				if(0 != (werr = ChmEventSock::WaitForReady(sock, WAIT_READ_FD, retrycnt, false, waittime))){		// not check SO_ERROR
					ERR_CHMPRN("SSL action result(%ld): ssl result(%d: %s), and Failed to wait read.", action_result, ssl_result, ERR_error_string(ssl_result, NULL));
					is_retry = false;
					if(ETIMEDOUT != werr){
						is_close= true;
					}
				}else{
					//MSG_CHMPRN("SSL action result(%ld): ssl result(%d: %s), and Succeed to wait read.", action_result, ssl_result, ERR_error_string(ssl_result, NULL));
				}
				result = false;
				break;

			case	SSL_ERROR_SYSCALL:
				if(action_result < 0){
					ERR_CHMPRN("SSL action result(%ld): ssl result(%d: %s), errno(%d).", action_result, ssl_result, ERR_error_string(ssl_result, NULL), errno);
					is_retry= false;
					result	= false;
				}else{	// action_result == 0
					if(CHKRESULTSSL_TYPE_CON == type){
						MSG_CHMPRN("SSL action result(%ld): ssl result(%d: %s), so this case is received illegal EOF after calling connect/accept, but no error(no=%d).", action_result, ssl_result, ERR_error_string(ssl_result, NULL), errno);
						result	= true;
					}else if(CHKRESULTSSL_TYPE_SD == type){
						WAN_CHMPRN("SSL action result(%ld): ssl result(%d: %s). not retry to shutdown.", action_result, ssl_result, ERR_error_string(ssl_result, NULL));
						is_retry= false;
						result	= false;
					}else{
						ERR_CHMPRN("SSL action result(%ld): ssl result(%d: %s).", action_result, ssl_result, ERR_error_string(ssl_result, NULL));
						is_retry= false;
						is_close= true;
						result	= false;
					}
				}
				break;

			case	SSL_ERROR_ZERO_RETURN:
				if(CHKRESULTSSL_TYPE_SD == type){
					MSG_CHMPRN("SSL action result(%ld): ssl result(%d: %s). so the peer is closed then not retry to shutdown.", action_result, ssl_result, ERR_error_string(ssl_result, NULL));
					is_retry= false;
					is_close= true;
					result	= false;
				}else{
					ERR_CHMPRN("SSL action result(%ld): ssl result(%d: %s), so the peer is closed.", action_result, ssl_result, ERR_error_string(ssl_result, NULL));
					is_retry= false;
					is_close= true;
					result	= false;
				}
				break;

			default:
				if(CHKRESULTSSL_TYPE_SD == type){
					WAN_CHMPRN("SSL action result(%ld): ssl result(%d: %s). not retry to shutdown.", action_result, ssl_result, ERR_error_string(ssl_result, NULL));
					is_retry= false;
					result	= false;
				}else{
					ERR_CHMPRN("SSL action result(%ld): ssl result(%d: %s).", action_result, ssl_result, ERR_error_string(ssl_result, NULL));
					is_retry= false;
					result	= false;
				}
				break;
		}
	}else{
		// Result is Success!
		//MSG_CHMPRN("SSL action result(%ld): succeed.", action_result);
	}
	return result;
}

ChmSSCtx ChmSecureSock::MakeSSLContext(const char* server_cert, const char* server_prikey, const char* slave_cert, const char* slave_prikey)
{
	if((CHMEMPTYSTR(server_cert) || CHMEMPTYSTR(server_prikey)) && (ChmSecureSock::is_verify_peer && (CHMEMPTYSTR(slave_cert) || CHMEMPTYSTR(slave_prikey)))){
		ERR_CHMPRN("Parameters are wrong.");
		return NULL;
	}

	// Make context data index
	if(-1 == ChmSecureSock::verify_cb_data_index){
		// make new index
		ChmSecureSock::verify_cb_data_index = SSL_CTX_get_ex_new_index(0, const_cast<char*>(ChmSecureSock::strVerifyCBDataIndex), NULL, NULL, NULL);
	}

	SSL_CTX*	ctx;
	bool		verify_peer	= ChmSecureSock::is_verify_peer;

	// Make context
	//
	// [NOTE][FIXME]
	// In OpenSSL 1.1.0 and later, the use of TLS_method() is recommended and should be used.
	// However, before OpenSSL 1.1.0, since these TLS_*** functions do not exist, we will keep
	// following in future.
	//
	if(NULL == (ctx = SSL_CTX_new(SSLv23_method()))){
		ERR_CHMPRN("Could not make SSL Context.");
		return NULL;
	}
	// Restrict the protocol according to the setting.
	if(!ChmSecureSock::SetMinVersion(ctx)){
		ERR_CHMPRN("Setting of minimum SSL/TLS version failed.");
		SSL_CTX_free(ctx);
		return NULL;
	}

	// Options/Modes
	SSL_CTX_set_options(ctx, SSL_OP_ALL);
	SSL_CTX_set_mode(ctx, SSL_MODE_ENABLE_PARTIAL_WRITE);			// Non blocking
	SSL_CTX_set_mode(ctx, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);		// Non blocking

	// Load CA cert
	const char*	CApath = ChmSecureSock::GetCAPath().empty() ? NULL : ChmSecureSock::GetCAPath().c_str();
	const char*	CAfile = ChmSecureSock::GetCAFile().empty() ? NULL : ChmSecureSock::GetCAFile().c_str();
	if((CHMEMPTYSTR(CAfile) && CHMEMPTYSTR(CApath)) || 1 != SSL_CTX_load_verify_locations(ctx, CAfile, CApath)){
		if(!CHMEMPTYSTR(CAfile) || !CHMEMPTYSTR(CApath)){
			// Failed loading -> try to default CA
			WAN_CHMPRN("Failed to load CA certs, CApath=%s, CAfile=%s", CApath, CAfile);
		}
		// Load default CA
		if(1 != SSL_CTX_set_default_verify_paths(ctx)){
			ERR_CHMPRN("Failed to load default certs.");
			SSL_CTX_free(ctx);
			return NULL;
		}
	}

	// Set cert
	if(!CHMEMPTYSTR(server_cert) && !CHMEMPTYSTR(server_prikey)){
		// Set server cert
		if(1 != SSL_CTX_use_certificate_chain_file(ctx, server_cert)){
			// Failed loading server cert
			ERR_CHMPRN("Failed to set server cert(%s)", server_cert);
			SSL_CTX_free(ctx);
			return NULL;
		}
		// Set server private keys(no pass)
		if(1 != SSL_CTX_use_PrivateKey_file(ctx, server_prikey, SSL_FILETYPE_PEM)){		// **** Not use following functions for passwd and RSA ****
			// Failed loading server private key
			ERR_CHMPRN("Failed to load private key file(%s)", server_prikey);
			SSL_CTX_free(ctx);
			return NULL;
		}
		// Verify cert
		if(1 != SSL_CTX_check_private_key(ctx)){
			// Not success to verify private key.
			ERR_CHMPRN("Failed to verify server cert(%s) & server private key(%s)", server_cert, server_prikey);
			SSL_CTX_free(ctx);
			return NULL;
		}

		// Set session id to context
		SSL_CTX_set_session_id_context(ctx, reinterpret_cast<const unsigned char*>(&ChmSecureSock::ssl_session_id), sizeof(ChmSecureSock::ssl_session_id));

		// Set CA list for client(slave)
		if(!CHMEMPTYSTR(CAfile)){
			STACK_OF(X509_NAME)*	cert_names;
			if(NULL != (cert_names = SSL_load_client_CA_file(CAfile))){
				SSL_CTX_set_client_CA_list(ctx, cert_names);
			}else{
				WAN_CHMPRN("Failed to load client(slave) CA certs(%s)", CAfile);
			}
		}

	}else if(!CHMEMPTYSTR(slave_cert) && !CHMEMPTYSTR(slave_prikey)){
		// Set slave cert
		if(1 != SSL_CTX_use_certificate_chain_file(ctx, slave_cert)){
			// Failed loading slave cert
			ERR_CHMPRN("Failed to set slave cert(%s)", slave_cert);
			SSL_CTX_free(ctx);
			return NULL;
		}
		// Set slave private keys(no pass)
		if(1 != SSL_CTX_use_PrivateKey_file(ctx, slave_prikey, SSL_FILETYPE_PEM)){		// **** Not use following functions for passwd and RSA ****
			// Failed loading slave private key
			ERR_CHMPRN("Failed to load private key file(%s)", slave_prikey);
			SSL_CTX_free(ctx);
			return NULL;
		}
		// Verify cert
		if(1 != SSL_CTX_check_private_key(ctx)){
			// Not success to verify private key.
			ERR_CHMPRN("Failed to verify slave cert(%s) & slave private key(%s)", slave_cert, slave_prikey);
			SSL_CTX_free(ctx);
			return NULL;
		}

		// slave SSL context does not need verify flag.
		verify_peer = false;
	}

	// Make verify peer mode
	int	verify_mode = verify_peer ? (SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT | SSL_VERIFY_CLIENT_ONCE) : SSL_VERIFY_NONE;

	// Set callback
	SSL_CTX_set_verify(ctx, verify_mode, ChmSecureSock::VerifyCallBackSSL);									// Set verify callback
	SSL_CTX_set_verify_depth(ctx, ChmSecureSock::CHM_SSL_VERIFY_DEPTH + 1);									// Verify depth
	SSL_CTX_set_ex_data(ctx, ChmSecureSock::verify_cb_data_index, &ChmSecureSock::CHM_SSL_VERIFY_DEPTH);	// Set external data

	return ctx;
}

ChmSSSession ChmSecureSock::HandshakeSSL(ChmSSCtx ctx, int sock, bool is_accept, int con_retrycnt, suseconds_t con_waittime)
{
	if(!ctx || CHM_INVALID_SOCK == sock){
		ERR_CHMPRN("Parameters are wrong.");
		return NULL;
	}
	if(CHMEVENTSOCK_RETRY_DEFAULT == con_retrycnt){
		con_retrycnt = ChmEventSock::DEFAULT_RETRYCNT_CONNECT;
		con_waittime = ChmEventSock::DEFAULT_WAIT_CONNECT;
	}

	// make SSL object
	SSL*	ssl = SSL_new(ctx);
	if(!ssl){
		ERR_CHMPRN("Could not make SSL object from context.");
		return NULL;
	}
	if(is_accept){
		SSL_set_accept_state(ssl);
	}else{
	    SSL_set_connect_state(ssl);
	}
	SSL_set_fd(ssl, sock);

	// accept/connect
	bool	is_close = false;	// Not used this value.
	long	action_result;
	for(int tmp_retrycnt = con_retrycnt; 0 < tmp_retrycnt; --tmp_retrycnt){
		bool	is_retry;

		// accept
		if(is_accept){
			action_result = SSL_accept(ssl);
		}else{
			action_result = SSL_connect(ssl);
		}

		is_retry = true;
		if(ChmSecureSock::CheckResultSSL(sock, ssl, action_result, CHKRESULTSSL_TYPE_CON, is_retry, is_close, con_retrycnt, con_waittime)){
			// success
			MSG_CHMPRN("Connected session(handshaked) : %s", ChmSecureSock::GetSessionInfo(ssl).c_str());
			return reinterpret_cast<ChmSSSession>(ssl);
		}
		if(!is_retry){
			break;
		}
	}
	ERR_CHMPRN("Failed to %s SSL.", (is_accept ? "accept" : "connect"));

	// shutdown SSL
	if(!ChmSecureSock::ShutdownSSL(sock, ssl, con_retrycnt, con_waittime)){
		ERR_CHMPRN("Failed to shutdown SSL, but continue...");
	}

	return NULL;
}

ChmSSSession ChmSecureSock::AcceptSSL(ChmSSCtx ctx, int sock, int con_retrycnt, suseconds_t con_waittime)
{
	return ChmSecureSock::HandshakeSSL(ctx, sock, true, con_retrycnt, con_waittime);
}

ChmSSSession ChmSecureSock::ConnectSSL(ChmSSCtx ctx, int sock, int con_retrycnt, suseconds_t con_waittime)
{
	return ChmSecureSock::HandshakeSSL(ctx, sock, false, con_retrycnt, con_waittime);
}

bool ChmSecureSock::ShutdownSSL(int sock, ChmSSSession sslsession, int con_retrycnt, suseconds_t con_waittime)
{
	if(CHM_INVALID_SOCK == sock || !sslsession){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}
	SSL*	ssl = reinterpret_cast<SSL*>(sslsession);

	if(CHMEVENTSOCK_RETRY_DEFAULT == con_retrycnt){
		con_retrycnt = ChmEventSock::DEFAULT_RETRYCNT_CONNECT;
		con_waittime = ChmEventSock::DEFAULT_WAIT_CONNECT;
	}

	bool	is_close = false;		// Not used this value.
	for(int tmp_retrycnt = con_retrycnt; 0 < tmp_retrycnt; --tmp_retrycnt){
		bool	is_retry;
		long	action_result;

		// accept
		action_result	= SSL_shutdown(ssl);
		is_retry		= true;

		if(ChmSecureSock::CheckResultSSL(sock, ssl, action_result, CHKRESULTSSL_TYPE_SD, is_retry, is_close, con_retrycnt, con_waittime)){
			// success
			SSL_free(ssl);
			return true;
		}
		if(!is_retry){
			break;
		}
	}
	ERR_CHMPRN("Failed to shutdown SSL for sock(%d), but continue to free ssl session.", sock);
	SSL_free(ssl);

	return false;
}

int ChmSecureSock::Read(ChmSSSession sslsession, void* pbuf, int length)
{
	SSL*	ssl	= reinterpret_cast<SSL*>(sslsession);
	return SSL_read(ssl, pbuf, length);
}

int ChmSecureSock::Write(ChmSSSession sslsession, const void* pbuf, int length)
{
	SSL*	ssl	= reinterpret_cast<SSL*>(sslsession);
	return SSL_write(ssl, pbuf, length);
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
ChmSecureSock::ChmSecureSock(const char* CApath, const char* CAfile, bool is_verify_peer) : svr_sslctx(NULL), slv_sslctx(NULL)
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
	// Clean SSL contexts
	if(svr_sslctx){
		SSL_CTX_free(svr_sslctx);
		svr_sslctx = NULL;
	}
	if(slv_sslctx){
		SSL_CTX_free(slv_sslctx);
		slv_sslctx = NULL;
	}
	return true;
}

bool ChmSecureSock::IsSafeSSLContext(const char* server_cert, const char* server_prikey, const char* slave_cert, const char* slave_prikey)
{
	return (NULL != GetSSLContext(server_cert, server_prikey, slave_cert, slave_prikey));
}

ChmSSCtx ChmSecureSock::GetSSLContext(const char* server_cert, const char* server_prikey, const char* slave_cert, const char* slave_prikey)
{
	// Which ctx is needed?
	SSL_CTX*	pctx = NULL;
	if(!CHMEMPTYSTR(server_cert) && !CHMEMPTYSTR(server_prikey)){
		pctx = svr_sslctx;
	}else{
		pctx = slv_sslctx;
	}

	if(!pctx){
		// Make context
		if(NULL == (pctx = ChmSecureSock::MakeSSLContext(CVT_ESTR_NULL(server_cert), CVT_ESTR_NULL(server_prikey), CVT_ESTR_NULL(slave_cert), CVT_ESTR_NULL(slave_prikey)))){
			ERR_CHMPRN("Failed to make SSL context.");
			return NULL;
		}

		// register
		if(!CHMEMPTYSTR(server_cert) && !CHMEMPTYSTR(server_prikey)){
			svr_sslctx = pctx;
		}else{
			slv_sslctx = pctx;
		}
	}else{
		// already has ctx.
	}
	return pctx;
}

void ChmSecureSock::FreeSSLContext(ChmSSCtx ctx)
{
	// Nothing to do
	return;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noexpandtab sw=4 ts=4 fdm=marker
 * vim<600: noexpandtab sw=4 ts=4
 */
