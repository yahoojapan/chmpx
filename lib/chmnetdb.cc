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
 * CREATE:   Tue July 1 2014
 * REVISION:
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <string>

#include "chmcommon.h"
#include "chmutil.h"
#include "chmdbg.h"
#include "chmnetdb.h"

using namespace	std;

//---------------------------------------------------------
// Class variables
//---------------------------------------------------------
const time_t	ChmNetDb::ALIVE_TIME;
int				ChmNetDb::lockval = FLCK_NOSHARED_MUTEX_VAL_UNLOCKED;

//---------------------------------------------------------
// Class methods
//---------------------------------------------------------
// [NOTE]
// To avoid static object initialization order problem(SIOF)
//
ChmNetDb* ChmNetDb::Get(void)
{
	static ChmNetDb		netdb;				// singleton
	return &netdb;
}

time_t ChmNetDb::SetTimeout(time_t value)
{
	time_t	old = ChmNetDb::Get()->timeout;
	ChmNetDb::Get()->timeout = value;
	return old;
}


bool ChmNetDb::Clear(void)
{
	return ChmNetDb::Get()->ClearEx();
}

bool ChmNetDb::CacheOut(void)
{
	return ChmNetDb::Get()->CacheOutEx();
}

bool ChmNetDb::GetLocalHostname(string& hostname)
{
	if(!ChmNetDb::Get()->GetHostname("localhost", hostname, true)){
		MSG_CHMPRN("Could not get localhost to global hostname.");
		return false;
	}
	return true;
}

bool ChmNetDb::GetAnyAddrInfo(short port, struct addrinfo** ppaddrinfo, bool is_inetv6)
{
	if(!ppaddrinfo){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	*ppaddrinfo = NULL;

	struct addrinfo	hints;
	int				result;
	string			strPort = to_string(port);

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags		= AI_PASSIVE;
	hints.ai_family		= is_inetv6 ? AF_INET6 : AF_INET;
	hints.ai_socktype	= SOCK_STREAM;

	// addrinfo
	if(0 != (result = getaddrinfo(NULL, strPort.c_str(), &hints, ppaddrinfo)) || !(*ppaddrinfo)){
		MSG_CHMPRN("Could not get %s addrinfo for %s, errno=%d.", (is_inetv6 ? "IN6ADDR_ANY_INIT" : "INADDR_ANY"), (is_inetv6 ? "AF_INET6" : "AF_INET"), result);
		return false;
	}
	return true;
}

bool ChmNetDb::CvtAddrInfoToIpAddress(const struct sockaddr_storage* info, socklen_t infolen, string& stripaddress)
{
	char	ipaddress[NI_MAXHOST];
	int		result;

	if(!info){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}

	// addrinfo -> normalized ipaddress
	if(0 != (result = getnameinfo(reinterpret_cast<const struct sockaddr*>(info), infolen, ipaddress, sizeof(ipaddress), NULL, 0, NI_NUMERICHOST | NI_NUMERICSERV))){
		MSG_CHMPRN("Could not convert addrinfo to normalized ipaddress, errno=%d.", result);
		return false;
	}
	stripaddress = ipaddress;

	return true;
}

bool ChmNetDb::CvtSockToLocalPort(int sock, short& port)
{
	if(CHM_INVALID_SOCK == sock){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}

	struct sockaddr_storage info;
	socklen_t               infolen = (socklen_t)sizeof(struct sockaddr_storage);
	if(!getsockname(sock, reinterpret_cast<struct sockaddr*>(&info), &infolen)){
		ERR_CHMPRN("Failed to get sock info, errno=%d", errno);
		return false;
	}

	char	szport[NI_MAXHOST];
	int		result;
	if(0 != (result = getnameinfo(reinterpret_cast<struct sockaddr*>(&info), infolen, NULL, 0, szport, sizeof(szport), NI_NUMERICHOST | NI_NUMERICSERV))){
		MSG_CHMPRN("Could not convert addrinfo to port number, errno=%d.", result);
		return false;
	}
	port = static_cast<short>(atoi(szport));

	return true;
}

bool ChmNetDb::CvtSockToPeerPort(int sock, short& port)
{
	if(CHM_INVALID_SOCK == sock){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}

	struct sockaddr_storage info;
	socklen_t               infolen = (socklen_t)sizeof(struct sockaddr_storage);
	if(!getpeername(sock, reinterpret_cast<struct sockaddr*>(&info), &infolen)){
		ERR_CHMPRN("Failed to get sock info, errno=%d", errno);
		return false;
	}

	char	szport[NI_MAXHOST];
	int		result;
	if(0 != (result = getnameinfo(reinterpret_cast<struct sockaddr*>(&info), infolen, NULL, 0, szport, sizeof(szport), NI_NUMERICHOST | NI_NUMERICSERV))){
		MSG_CHMPRN("Could not convert addrinfo to port number, errno=%d.", result);
		return false;
	}
	port = static_cast<short>(atoi(szport));

	return true;
}

bool ChmNetDb::CvtV4MappedAddrInfo(struct sockaddr_storage* info, socklen_t& addrlen)
{
	if(!info){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(AF_INET6 != info->ss_family){
		// nothing to do
		return true;
	}

	struct sockaddr_in6*	in6 = reinterpret_cast<struct sockaddr_in6*>(info);
	if(IN6_IS_ADDR_V4MAPPED(&(in6->sin6_addr))){
		struct sockaddr_in	in4;

		memset(&in4, 0, sizeof(struct sockaddr_in));
		in4.sin_family	= AF_INET;
		in4.sin_port	= in6->sin6_port;

		memcpy(&(in4.sin_addr.s_addr), in6->sin6_addr.s6_addr + 12, sizeof(in4.sin_addr.s_addr));
		memcpy(info, &in4, sizeof(struct sockaddr_in));

		addrlen = sizeof(struct sockaddr_in);
	}
	return true;
}

//---------------------------------------------------------
// Methods
//---------------------------------------------------------
ChmNetDb::ChmNetDb() : timeout(ChmNetDb::ALIVE_TIME), fulllocalname(""), localname("")
{
	static ChmNetDb*	pnetdb = NULL;		// for checking initializing
	if(!pnetdb){
		pnetdb = this;
		InitializeLocalHostName();			// initializing
	}
}

ChmNetDb::~ChmNetDb()
{
	ClearEx();
}

bool ChmNetDb::InitializeLocalHostName(void)
{
	struct addrinfo		hints;
	struct addrinfo*	res_info = NULL;
	struct addrinfo*	tmpaddrinfo;
	char				hostname[NI_MAXHOST];
	int					result;
	struct utsname		buf;

	// Get local hostname by uname
	if(-1 == uname(&buf)){
		ERR_CHMPRN("Failed to get own host(node) name, errno=%d", errno);
		return false;
	}
	fulllocalname = buf.nodename;

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags		= AI_CANONNAME;
	hints.ai_family		= AF_UNSPEC;
	hints.ai_socktype	= SOCK_STREAM;

	// local hostname -> addrinfo
	if(0 != (result = getaddrinfo(buf.nodename, NULL, &hints, &res_info)) || !res_info){				// port is NULL
		MSG_CHMPRN("Could not get addrinfo from %s, errno=%d.", buf.nodename, result);
		if(res_info){
			freeaddrinfo(res_info);
		}
		return false;
	}

	// addrinfo(list) -> hostname
	for(tmpaddrinfo = res_info; tmpaddrinfo; tmpaddrinfo = tmpaddrinfo->ai_next){
		if(0 != (result = getnameinfo(tmpaddrinfo->ai_addr, tmpaddrinfo->ai_addrlen, hostname, sizeof(hostname), NULL, 0, NI_NAMEREQD | NI_NUMERICSERV))){
			MSG_CHMPRN("Could not get hostname %s, errno=%d.", buf.nodename, result);
		}else{
			break;
		}
	}
	if(!tmpaddrinfo){
		MSG_CHMPRN("Could not get hostname %s.", buf.nodename);
		freeaddrinfo(res_info);
		return false;
	}

	// When local hostname without domain name is set in /ec/hosts,
	// "hostname" is short name.
	// (if other server name is set, this class do not care it.)
	//
	localname = hostname;

	freeaddrinfo(res_info);

	return true;
}

bool ChmNetDb::ClearEx(void)
{
	while(!fullock::flck_trylock_noshared_mutex(&ChmNetDb::lockval));
	cachemap.clear();
	fullock::flck_unlock_noshared_mutex(&ChmNetDb::lockval);

	return true;
}

bool ChmNetDb::CacheOutEx(void)
{
	time_t	timeouted = time(NULL) + timeout;

	while(!fullock::flck_trylock_noshared_mutex(&ChmNetDb::lockval));

	for(chmndbmap_t::iterator iter = cachemap.begin(); iter != cachemap.end(); ){
		if(timeouted < iter->second.cached_time){
			cachemap.erase(iter++);
		}else{
			++iter;
		}
	}
	fullock::flck_unlock_noshared_mutex(&ChmNetDb::lockval);

	return true;
}

// [TODO]
// If one FQDN has many address(ex. it specifies in /etc/hosts), this method returns one address.
// But this should return all names and addresses.
// If it is needed, should use res_query etc for checking DNS entry.
//
bool ChmNetDb::GetHostAddressInfo(const char* target, string& strhostname, string& stripaddress)
{
	struct addrinfo		hints;
	struct addrinfo*	res_info = NULL;
	struct addrinfo*	tmpaddrinfo;
	char				hostname[NI_MAXHOST];
	char				ipaddress[NI_MAXHOST];
	int					result;

	if(CHMEMPTYSTR(target)){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags		= AI_CANONNAME;
	hints.ai_family		= AF_UNSPEC;
	hints.ai_socktype	= SOCK_STREAM;

	// target -> addrinfo
	if(0 != (result = getaddrinfo(target, NULL, &hints, &res_info)) || !res_info){				// port is NULL
		//MSG_CHMPRN("Could not get addrinfo from %s, errno=%d.", target, result);
		return false;
	}

	// addrinfo(list) -> hostname
	for(tmpaddrinfo = res_info; tmpaddrinfo; tmpaddrinfo = tmpaddrinfo->ai_next){
		if(0 != (result = getnameinfo(tmpaddrinfo->ai_addr, tmpaddrinfo->ai_addrlen, hostname, sizeof(hostname), NULL, 0, NI_NAMEREQD | NI_NUMERICSERV))){
			MSG_CHMPRN("Could not get hostname %s, errno=%d.", target, result);
		}else{
			break;
		}
	}
	if(!tmpaddrinfo){
		MSG_CHMPRN("Could not get hostname %s.", target);
		freeaddrinfo(res_info);
		return false;
	}

	// addrinfo -> normalized ipaddress
	for(tmpaddrinfo = res_info; tmpaddrinfo; tmpaddrinfo = tmpaddrinfo->ai_next){
		if(0 != (result = getnameinfo(tmpaddrinfo->ai_addr, tmpaddrinfo->ai_addrlen, ipaddress, sizeof(ipaddress), NULL, 0, NI_NUMERICHOST | NI_NUMERICSERV))){
			MSG_CHMPRN("Could not convert normalized ipaddress  %s, errno=%d.", target, result);
		}else{
			break;
		}
	}
	freeaddrinfo(res_info);
	if(!tmpaddrinfo){
		MSG_CHMPRN("Could not get ipaddress %s.", target);
		return false;
	}

	// if short local hostname, returns fully hostname.
	if(hostname == localname){
		strhostname	= fulllocalname;
	}else{
		strhostname	= hostname;
	}
	stripaddress= ipaddress;

	return true;
}

bool ChmNetDb::SearchCache(const char* target, CHMNDBCACHE& data)
{
	if(CHMEMPTYSTR(target)){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	bool	result = false;

	while(!fullock::flck_trylock_noshared_mutex(&ChmNetDb::lockval));

	string	strtarget(target);
	if(cachemap.end() != cachemap.find(strtarget)){
		if(0 != timeout && timeout < (time(NULL) - cachemap[strtarget].cached_time)){
			MSG_CHMPRN("find cache but it is old, do removing cache.");

			string	tmpipaddress= cachemap[strtarget].ipaddress;
			string	tmphostname	= cachemap[strtarget].hostname;
			if(cachemap.end() != cachemap.find(tmpipaddress)){
				cachemap.erase(tmpipaddress);
			}
			if(cachemap.end() != cachemap.find(tmphostname)){
				cachemap.erase(tmphostname);
			}
			if(cachemap.end() != cachemap.find(strtarget)){
				cachemap.erase(strtarget);
			}
		}else{
			data.ipaddress		= cachemap[strtarget].ipaddress;
			data.hostname		= cachemap[strtarget].hostname;
			data.cached_time	= cachemap[strtarget].cached_time;
			result				= true;
		}
	}
	fullock::flck_unlock_noshared_mutex(&ChmNetDb::lockval);

	return result;
}

bool ChmNetDb::Search(const char* target, CHMNDBCACHE& data, bool is_cvt_localhost)
{
	if(CHMEMPTYSTR(target)){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(SearchCache(target, data)){
		return true;		// Hit cache
	}

	string	hostname;
	string	ipaddress;
	if(!GetHostAddressInfo(target, hostname, ipaddress)){
		//MSG_CHMPRN("Could not find hostname or IP address by %s.", target);
		return false;
	}

	// If localhost, using global name, ip.
	if(is_cvt_localhost && (hostname == "localhost" || ipaddress == "127.0.0.1" || ipaddress == "::1")){
		char	localname[NI_MAXHOST];
		if(0 != gethostname(localname, sizeof(localname))){
			MSG_CHMPRN("Could not get localhost name, errno=%d", errno);
			return false;
		}
		// retry
		if(!GetHostAddressInfo(localname, hostname, ipaddress)){
			MSG_CHMPRN("Could not find hostname or IP address by %s.", localname);
			return false;
		}
	}
	data.ipaddress		= ipaddress;
	data.hostname		= hostname;
	data.cached_time	= time(NULL);

	// set cache
	while(!fullock::flck_trylock_noshared_mutex(&ChmNetDb::lockval));

	cachemap[ipaddress] = data;
	cachemap[hostname] = data;

	fullock::flck_unlock_noshared_mutex(&ChmNetDb::lockval);

	return true;
}

bool ChmNetDb::GetAddrInfo(const char* target, short port, struct addrinfo** ppaddrinfo, bool is_cvt_localhost)
{
	if(CHMEMPTYSTR(target) || !ppaddrinfo){
		ERR_CHMPRN("Parameter are wrong.");
		return false;
	}
	*ppaddrinfo = NULL;

	CHMNDBCACHE	data;
	if(!Search(target, data, is_cvt_localhost)){
		MSG_CHMPRN("Failed to convert %s to addrinfo structure.", target);
		return false;
	}

	struct addrinfo	hints;
	int				result;
	string			strPort = to_string(port);

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags		= AI_CANONNAME;
	hints.ai_family		= AF_UNSPEC;
	hints.ai_socktype	= SOCK_STREAM;

	// addrinfo
	if(0 != (result = getaddrinfo(data.ipaddress.c_str(), strPort.c_str(), &hints, ppaddrinfo)) || !(*ppaddrinfo)){
		MSG_CHMPRN("Could not get addrinfo from %s[%d], errno=%d.", target, port, result);
		return false;
	}
	return true;
}

bool ChmNetDb::GetHostname(const char* target, string& hostname, bool is_cvt_localhost)
{
	if(CHMEMPTYSTR(target)){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}

	CHMNDBCACHE	data;
	if(!Search(target, data, is_cvt_localhost)){
		//MSG_CHMPRN("Failed to convert %s to addrinfo structure.", target);
		return false;
	}
	hostname = data.hostname;

	return true;
}

bool ChmNetDb::GetIpAddressString(const char* target, string& ipaddress, bool is_cvt_localhost)
{
	if(CHMEMPTYSTR(target)){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}

	CHMNDBCACHE	data;
	if(!Search(target, data, is_cvt_localhost)){
		MSG_CHMPRN("Failed to convert %s to addrinfo structure.", target);
		return false;
	}
	ipaddress = data.ipaddress;

	return true;
}

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
