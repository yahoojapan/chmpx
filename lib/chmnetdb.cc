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
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>

#include "chmcommon.h"
#include "chmutil.h"
#include "chmdbg.h"
#include "chmnetdb.h"

using namespace	std;

//---------------------------------------------------------
// Utilities
//---------------------------------------------------------
static void AddUniqueStringToList(const string& str, strlst_t& list, bool is_clear)
{
	if(is_clear){
		list.clear();
	}
	if(str.empty()){
		return;
	}
	for(strlst_t::const_iterator iter = list.begin(); iter != list.end(); ++iter){
		if((*iter) == str){
			// already has same string.
			return;
		}
	}
	list.push_back(str);
}

static void AddUniqueStringListToList(const strlst_t& addlist, strlst_t& list, bool is_clear)
{
	if(is_clear){
		list.clear();
	}
	for(strlst_t::const_iterator iter1 = addlist.begin(); iter1 != addlist.end(); ++iter1){
		if(iter1->empty()){
			continue;
		}
		bool	found = false;
		for(strlst_t::const_iterator iter2 = list.begin(); iter2 != list.end(); ++iter2){
			if((*iter1) == (*iter2)){
				found = true;
				break;
			}
		}
		if(!found){
			list.push_back(*iter1);
		}
	}
}

// [NOTE]
// The IPv6 address string may include zone index(ex. "fe80::a00:27ff:fe51:2336%eth1").
// This method inserts the address string from which zone index is deleted at the detected position.
// This is necessary when matching.
// 
static void ExpandZoneIndexInList(strlst_t& list)
{
	for(strlst_t::iterator iter = list.begin(); iter != list.end(); ++iter){
		if(string::npos != iter->find('%')){
			string	nozoneindex = ChmNetDb::GetNoZoneIndexIpAddress(*iter);
			++iter;
			if(list.end() == iter){
				// insert to end of list
				list.push_back(nozoneindex);
				break;
			}else{
				iter = list.insert(iter, nozoneindex);
			}
		}
	}
}

static void RemoveZoneIndexInList(strlst_t& list)
{
	for(strlst_t::iterator iter = list.begin(); iter != list.end(); ){
		if(string::npos != iter->find('%')){
			string	nozi = iter->substr(0, iter->find('%'));
			iter = list.erase(iter);
			iter = list.insert(iter, nozi);
		}else{
			++iter;
		}
	}
}

static void RemoveLocalhostKeys(strlst_t& list)
{
	for(strlst_t::iterator iter = list.begin(); iter != list.end(); ){
		if(ChmNetDb::IsLocalhostKeyword(iter->c_str())){
			iter = list.erase(iter);
		}else{
			++iter;
		}
	}
}

static bool RemoveLocalhostInCache(CHMNDBCACHE& data)
{
	// check localhost(or 127.0.0.1 or ::1) and remove it.
	strlst_t::iterator	iter;
	bool				found = false;
	for(iter = data.hostnames.begin(); data.hostnames.end() != iter; ){
		if((*iter) == "localhost"){
			found	= true;
			iter	= data.hostnames.erase(iter);
		}else{
			++iter;
		}
	}
	for(iter = data.ipaddresses.begin(); data.ipaddresses.end() != iter; ){
		if(ChmNetDb::IsLocalhostKeyword(iter->c_str())){
			found	= true;
			iter	= data.ipaddresses.erase(iter);
		}else{
			++iter;
		}
	}
	return found;
}

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
	// [NOTE]
	// calling GetHostname with "localhost" always returns full local hostname.
	//
	if(!ChmNetDb::Get()->GetHostname("localhost", hostname, true)){
		MSG_CHMPRN("Could not get localhost to global hostname.");
		return false;
	}
	return true;
}

bool ChmNetDb::GetLocalHostnameList(strlst_t& hostnames)
{
	if(!ChmNetDb::Get()->GetHostnameList("localhost", hostnames, true)){
		MSG_CHMPRN("Could not get localhost to hostname list.");
		return false;
	}
	return true;
}

bool ChmNetDb::GetLocalHostList(strlst_t& hostinfo, bool remove_localhost)
{
	if(!ChmNetDb::Get()->GetAllHostList("localhost", hostinfo, true)){
		WAN_CHMPRN("Could not get localhost to hostname and ip address list.");
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

bool ChmNetDb::CvtAddrInfoToIpAddress(struct sockaddr_storage* info, socklen_t infolen, string& stripaddress)
{
	char	ipaddress[NI_MAXHOST];
	int		result;

	if(!info){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}

	// addrinfo -> normalized ipaddress
	memset(&ipaddress, 0, sizeof(ipaddress));
	if(0 != (result = getnameinfo(reinterpret_cast<const struct sockaddr*>(info), infolen, ipaddress, sizeof(ipaddress), NULL, 0, NI_NUMERICHOST | NI_NUMERICSERV))){
		MSG_CHMPRN("Could not convert addrinfo to normalized ipaddress(errno=%d), but retry IPv4 address if address is IPv4 mapped IPv6.", result);

		// If the address is IPv4 mapped IPv6, try again with the IPv4 address.
		socklen_t	tmp_infolen = infolen;
		if(ChmNetDb::CvtV4MappedAddrInfo(info, infolen) && tmp_infolen != infolen){
			if(0 != (result = getnameinfo(reinterpret_cast<const struct sockaddr*>(info), tmp_infolen, ipaddress, sizeof(ipaddress), NULL, 0, NI_NUMERICHOST | NI_NUMERICSERV))){
				MSG_CHMPRN("Could not convert addrinfo to normalized ipaddress(errno=%d) from IPv4 mapped IPv6.", result);
				return false;
			}
		}else{
			return false;
		}
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
	memset(&szport, 0, sizeof(szport));
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
	memset(&szport, 0, sizeof(szport));
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

// [NOTE] IPv4-Mapped IPv6 Address(RFC 4291)
// This function checks if the IP address string is IPv4-Mapped IPv6 Address(RFC 4291)
// and returns an IPv4 address string if applicable.
//
// ex1)
//	IPv4 address				-> 192.168.0.0
//	IPv4-Mapped IPv6 Address	-> ::ffff:192.168.0.0 or ::ffff:c0a8:0
// ex2)
//	IPv4 address				-> 192.168.0.1
//	IPv4-Mapped IPv6 Address	-> ::ffff:192.168.0.1 or ::ffff:c0a8:0001
//
bool ChmNetDb::GetIPv4MappedIPv6Address(const char* target, string& stripv4)
{
	if(CHMEMPTYSTR(target)){
		return false;
	}
	if(0 != strncmp(target, "::ffff:", 7)){
		return false;
	}

	// get address
	unsigned char	buf[sizeof(struct in6_addr)];
	int				result;
	if(1 != (result = inet_pton(AF_INET6, target, &buf))){
		if(0 == result){
			MSG_CHMPRN("Could not convert %s by wrong ip address string\n", target);
		}else{
			MSG_CHMPRN("Could not convert %s by errno(%d)\n", target, errno);
		}
		return false;
	}

	// do convert
	char			ipv4buf[32];									// A maximum of 16 bytes("xxx.xxx.xxx.xxx") is enough, but declare with 32 bytes
	struct in_addr*	ipv4	= reinterpret_cast<struct in_addr*>(&buf[12]);
	memset(ipv4buf, 0, sizeof(ipv4buf));
	strncpy(ipv4buf, inet_ntoa(*ipv4), sizeof(ipv4buf) - 1);		// inet_ntoa() is not thread safe

	stripv4 = ipv4buf;
	return true;
}

string ChmNetDb::CvtIPv4MappedIPv6Address(const string& target)
{
	string	result;
	if(!ChmNetDb::GetIPv4MappedIPv6Address(target.c_str(), result)){
		result = target;
	}
	return result;
}

void ChmNetDb::FreeAddrInfoList(addrinfolist_t& infolist)
{
	for(addrinfolist_t::const_iterator iter = infolist.begin(); iter != infolist.end(); ++iter){
		struct addrinfo*	tmp = *iter;
		freeaddrinfo(tmp);
	}
	infolist.clear();
}

string ChmNetDb::GetNoZoneIndexIpAddress(const string& ipaddr)
{
	// if IP address is IPv6 with zone index, we set both to cache.
	string::size_type	pos;
	if(string::npos != (pos = ipaddr.find('%'))){
		return ipaddr.substr(0, pos);
	}
	return ipaddr;
}

bool ChmNetDb::IsLocalhostKeyword(const char* host)
{
	if(CHMEMPTYSTR(host)){
		return false;
	}
	if(	0 == strcmp(host, "localhost")	||
		0 == strcmp(host, "127.0.0.1")	||
		0 == strcmp(host, "::1")		||
		0 == strncmp(host, "::1%", 4)	)
	{
		return true;
	}
	return false;
}

//---------------------------------------------------------
// Methods
//---------------------------------------------------------
ChmNetDb::ChmNetDb() : timeout(ChmNetDb::ALIVE_TIME)
{
	static ChmNetDb*	pnetdb = NULL;		// for checking initializing
	if(!pnetdb){
		pnetdb = this;
		InitializeLocalHostInfo();			// initializing
	}
}

ChmNetDb::~ChmNetDb()
{
	ClearEx();
}

bool ChmNetDb::InitializeLocalHostInfo(void)
{
	ClearEx();								// clear all cache
	fulllocalname.erase();
	localaddrs.clear();
	localnames.clear();

	if(!InitializeLocalHostIpAddresses()){
		WAN_CHMPRN("Obtaining the IP address of the local interfaces may have failed, but continue...");
	}
	if(!InitializeLocalHostnames()){
		WAN_CHMPRN("Obtaining the local hostnames may have failed, but continue...");
	}
	return true;
}

bool ChmNetDb::InitializeLocalHostIpAddresses()
{
	struct ifaddrs*	ifaddr;
	char			ipaddr[NI_MAXHOST];

	// get ip addresses on interface
	memset(&ipaddr, 0, sizeof(ipaddr));
	if(-1 == getifaddrs(&ifaddr)){
		ERR_CHMPRN("Failed to get local interface addresses by getifaddrs : errno=%d", errno);
		return false;
	}

	// get all ip addresses
	for(struct ifaddrs* tmp_ifaddr = ifaddr; NULL != tmp_ifaddr; tmp_ifaddr = tmp_ifaddr->ifa_next){
		if(NULL == tmp_ifaddr->ifa_addr){
			continue;
		}
		if(AF_INET == tmp_ifaddr->ifa_addr->sa_family || AF_INET6 == tmp_ifaddr->ifa_addr->sa_family){
			memset(ipaddr, 0, sizeof(ipaddr));
			socklen_t	salen	= (AF_INET == tmp_ifaddr->ifa_addr->sa_family) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);
			int			result	= getnameinfo(tmp_ifaddr->ifa_addr, salen, ipaddr, sizeof(ipaddr), NULL, 0, NI_NUMERICHOST);
			if(0 != result){
				// If the address is IPv4 mapped IPv6, try again with the IPv4 address.
				socklen_t	tmp_salen = salen;
				if(ChmNetDb::CvtV4MappedAddrInfo(reinterpret_cast<struct sockaddr_storage*>(tmp_ifaddr->ifa_addr), tmp_salen) && tmp_salen != salen){
					result	= getnameinfo(tmp_ifaddr->ifa_addr, tmp_salen, ipaddr, sizeof(ipaddr), NULL, 0, NI_NUMERICHOST);
				}
			}
			if(0 == result){
				if(!CHMEMPTYSTR(ipaddr)){
					MSG_CHMPRN("Found local interface IP address : %s", ipaddr);

					AddUniqueStringToList(string(ipaddr), localaddrs, false);

					// add cache without hostnames
					IpAddressAddCache(string(ipaddr), string(""), true);

					// check IPv4 mapped IPv6
					string	stripv4;
					if(ChmNetDb::GetIPv4MappedIPv6Address(ipaddr, stripv4)){
						MSG_CHMPRN("Found local interface IP address : %s", stripv4.c_str());

						AddUniqueStringToList(stripv4, localaddrs, false);

						// add cache without hostnames
						IpAddressAddCache(stripv4, string(""), true);
					}
				}else{
					WAN_CHMPRN("Found local interface IP address, but it is empty.");
				}
			}else{
				WAN_CHMPRN("Failed to get local interface IP address by getnameinfo : %s", gai_strerror(result));
			}
		}
	}
	freeifaddrs(ifaddr);

	return true;
}

bool ChmNetDb::InitializeLocalHostnames()
{
	struct addrinfo		hints;
	struct addrinfo*	res_info = NULL;
	struct addrinfo*	tmpaddrinfo;
	struct utsname		buf;
	char				hostname[NI_MAXHOST];
	char				ipaddr[NI_MAXHOST];
	int					result;

	// Get local hostname by uname
	if(-1 == uname(&buf)){
		ERR_CHMPRN("Failed to get own host(node) name, errno=%d", errno);
		return false;
	}
	if(CHMEMPTYSTR(buf.nodename)){
		ERR_CHMPRN("Got own host(node) name, but it is empty.");
		return false;
	}
	fulllocalname = buf.nodename;

	// add cache without ip addresses
	HostnammeAddCache(fulllocalname, string(""), true);

	// local hostname -> addrinfo
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags		= AI_CANONNAME;
	hints.ai_family		= AF_UNSPEC;
	hints.ai_socktype	= SOCK_STREAM;
	if(0 != (result = getaddrinfo(buf.nodename, NULL, &hints, &res_info)) || !res_info){				// port is NULL
		MSG_CHMPRN("Could not get addrinfo from %s, errno=%d.", buf.nodename, result);
		if(res_info){
			freeaddrinfo(res_info);
		}
		// already set full local hostname, then returns true.
		return true;
	}

	// addrinfo(list) -> hostname
	bool	is_fulllocalname;
	for(tmpaddrinfo = res_info; tmpaddrinfo; tmpaddrinfo = tmpaddrinfo->ai_next){
		memset(hostname, 0, sizeof(hostname));
		if(0 == (result = getnameinfo(tmpaddrinfo->ai_addr, tmpaddrinfo->ai_addrlen, hostname, sizeof(hostname), NULL, 0, NI_NAMEREQD | NI_NUMERICSERV))){
			if(!CHMEMPTYSTR(hostname)){
				// When local hostname without domain name is set in /etc/hosts, "hostname" is short name.
				// (if other server name is set, this class do not care it.)
				//
				MSG_CHMPRN("Found another local hostname : %s", hostname);

				if(0 != strcmp(fulllocalname.c_str(), hostname)){
					is_fulllocalname = false;
				}else{
					is_fulllocalname = true;
				}
				AddUniqueStringToList(string(hostname), localnames, false);

				// add cache
				memset(&ipaddr, 0, sizeof(ipaddr));
				if(0 == (result = getnameinfo(tmpaddrinfo->ai_addr, tmpaddrinfo->ai_addrlen, ipaddr, sizeof(ipaddr), NULL, 0, NI_NUMERICHOST))){
					HostnammeAddCache(string(hostname), string(ipaddr), true);

					if(!is_fulllocalname){
						HostnammeAddCache(fulllocalname, string(ipaddr), true);
					}
				}
			}else{
				WAN_CHMPRN("Found another local hostname, but it is empty.");
			}
		}else{
			MSG_CHMPRN("Failed to get another local hostname %s, errno=%d.", buf.nodename, result);
		}
	}
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
	time_t	now = time(NULL);

	while(!fullock::flck_trylock_noshared_mutex(&ChmNetDb::lockval));

	for(chmndbmap_t::iterator iter = cachemap.begin(); iter != cachemap.end(); ){
		if(0 != iter->second.cached_time && (iter->second.cached_time + timeout) < now){
			cachemap.erase(iter++);
		}else{
			++iter;
		}
	}
	fullock::flck_unlock_noshared_mutex(&ChmNetDb::lockval);

	return true;
}

// [NOTE]
// This method is reentrant.
// If called as re-entry, it will not be re-entered.
//
bool ChmNetDb::RawAddCache(const string& target, const strlst_t& addlist, bool is_noexp, bool is_hostname, bool is_reentrant)
{
	if(!is_reentrant){
		while(!fullock::flck_trylock_noshared_mutex(&ChmNetDb::lockval));
	}
	chmndbmap_t::iterator	iter	= cachemap.find(target);
	bool					result	= true;
	bool					found;

	if(is_hostname){
		strlst_t	newipaddrs;
		if(cachemap.end() != iter){
			// found in cache
			// merge ip addresses
			for(strlst_t::const_iterator additer = addlist.begin(); addlist.end() != additer; ++additer){
				found = false;
				if(additer->empty()){
					continue;
				}
				for(strlst_t::const_iterator ipiter = iter->second.ipaddresses.begin(); iter->second.ipaddresses.end() != ipiter; ++ipiter){
					if((*ipiter) == (*additer)){
						found = true;
						break;
					}
				}
				if(!found){
					AddUniqueStringToList(*additer, iter->second.ipaddresses, false);
					AddUniqueStringToList(*additer, newipaddrs, false);
				}
			}
			// add hostanme
			found = false;
			for(strlst_t::const_iterator hostiter = iter->second.hostnames.begin(); iter->second.hostnames.end() != hostiter; ++hostiter){
				if((*hostiter) == target){
					found = true;
					break;
				}
			}
			if(!found){
				AddUniqueStringToList(target, iter->second.hostnames, false);
			}
			// update timeout
			iter->second.cached_time = (is_noexp && 0 == iter->second.cached_time) ? 0 : time(NULL);

		}else{
			// not found, add new cache
			CHMNDBCACHE	newcache;
			AddUniqueStringListToList(addlist, newcache.ipaddresses, true);
			AddUniqueStringToList(target, newcache.hostnames, true);
			newcache.cached_time= is_noexp ? 0 : time(NULL);

			cachemap[target]	= newcache;

			AddUniqueStringListToList(addlist, newipaddrs, false);
		}

		// add ip addresses(re-entrant)
		if(!is_reentrant){
			strlst_t	addhostnames;
			addhostnames.push_back(target);
			for(strlst_t::const_iterator ipiter2 = newipaddrs.begin(); newipaddrs.end() != ipiter2; ++ipiter2){
				if(!RawAddCache(*ipiter2, addhostnames, is_noexp, false, true)){
					result = false;
				}
				// if IP address is IPv6 with zone index, we set both to cache.
				if(string::npos != ipiter2->find('%')){
					string	nozi = ChmNetDb::GetNoZoneIndexIpAddress(*ipiter2);
					if(!RawAddCache(nozi, addhostnames, is_noexp, false, true)){
						result = false;
					}
				}
			}
		}
	}else{
		strlst_t	newhostnames;
		if(cachemap.end() != iter){
			// found in cache
			// merge hostnames
			for(strlst_t::const_iterator additer = addlist.begin(); addlist.end() != additer; ++additer){
				found = false;
				if(additer->empty()){
					continue;
				}
				for(strlst_t::const_iterator hostiter = iter->second.hostnames.begin(); iter->second.hostnames.end() != hostiter; ++hostiter){
					if((*hostiter) == (*additer)){
						found = true;
						break;
					}
				}
				if(!found){
					AddUniqueStringToList(*additer, iter->second.hostnames, false);
					AddUniqueStringToList(*additer, newhostnames, false);
				}
			}
			// add ip address
			found = false;
			for(strlst_t::const_iterator ipiter = iter->second.ipaddresses.begin(); iter->second.ipaddresses.end() != ipiter; ++ipiter){
				if((*ipiter) == target){
					found = true;
					break;
				}
			}
			if(!found){
				AddUniqueStringToList(target, iter->second.ipaddresses, false);
			}
			// update timeout
			iter->second.cached_time = (is_noexp && 0 == iter->second.cached_time) ? 0 : time(NULL);

		}else{
			// not found, add new cache
			CHMNDBCACHE	newcache;
			AddUniqueStringToList(target, newcache.ipaddresses, true);
			AddUniqueStringListToList(addlist, newcache.hostnames, true);
			newcache.cached_time = is_noexp ? 0 : time(NULL);

			cachemap[target] = newcache;

			AddUniqueStringListToList(addlist, newhostnames, false);
		}

		// add hostnames(re-entrant)
		if(!is_reentrant){
			strlst_t	addipaddrs;
			addipaddrs.push_back(target);
			for(strlst_t::const_iterator hostiter2 = newhostnames.begin(); newhostnames.end() != hostiter2; ++hostiter2){
				if(!RawAddCache(*hostiter2, addipaddrs, is_noexp, true, true)){
					result = false;
				}
			}
		}
	}
	if(!is_reentrant){
		fullock::flck_unlock_noshared_mutex(&ChmNetDb::lockval);
	}
	return result;
}

bool ChmNetDb::HostnammeAddCache(const string& hostname, const string& ipaddr, bool is_noexp)
{
	strlst_t	ipaddrs;
	ipaddrs.push_back(ipaddr);
	return RawAddCache(hostname, ipaddrs, is_noexp, true);
}

bool ChmNetDb::HostnammeAddCache(const string& hostname, const strlst_t& ipaddrs, bool is_noexp)
{
	return RawAddCache(hostname, ipaddrs, is_noexp, true);
}

bool ChmNetDb::IpAddressAddCache(const string& ipaddr, const string& hostname, bool is_noexp)
{
	bool		result = true;
	strlst_t	hostnames;
	hostnames.push_back(hostname);

	// if ipaddr is IPv6 with zone index, we set both to cache.
	if(string::npos != ipaddr.find('%')){
		string	nozi= ChmNetDb::GetNoZoneIndexIpAddress(ipaddr);
		result		= RawAddCache(nozi, hostnames, is_noexp, false);
	}
	if(!RawAddCache(ipaddr, hostnames, is_noexp, false)){
		result		= false;
	}
	return result;
}

bool ChmNetDb::IpAddressAddCache(const string& ipaddr, const strlst_t& hostnames, bool is_noexp)
{
	bool		result = true;

	// if ipaddr is IPv6 with zone index, we set both to cache.
	if(string::npos != ipaddr.find('%')){
		string	nozi= ChmNetDb::GetNoZoneIndexIpAddress(ipaddr);
		result		= RawAddCache(nozi, hostnames, is_noexp, false);
	}
	if(!RawAddCache(ipaddr, hostnames, is_noexp, false)){
		result		= false;
	}
	return result;
}

bool ChmNetDb::SearchCache(const char* target, CHMNDBCACHE& data)
{
	if(CHMEMPTYSTR(target)){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	while(!fullock::flck_trylock_noshared_mutex(&ChmNetDb::lockval));

	string					strtarget(target);
	bool					result	= false;
	chmndbmap_t::iterator	iter	= cachemap.find(strtarget);
	if(cachemap.end() == iter){
		// if target is IPv6 with zone index, try to check no zone index.
		if(string::npos != strtarget.find('%')){
			string	nozi= ChmNetDb::GetNoZoneIndexIpAddress(strtarget);
			iter		= cachemap.find(nozi);
		}
	}
	if(cachemap.end() != iter){
		if(0 != timeout && time(NULL) < (iter->second.cached_time + timeout)){
			MSG_CHMPRN("find cache but it is old, do removing cache.");
			cachemap.erase(iter);
		}else{
			AddUniqueStringListToList(iter->second.ipaddresses, data.ipaddresses, true);
			AddUniqueStringListToList(iter->second.hostnames, data.hostnames, true);
			data.cached_time	= time(NULL);				// always now, because this value is not used outside.
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

	// search in chache
	if(!SearchCache(target, data)){
		// not found in cache
		if(!GetHostAddressInfo(target, data)){
			//MSG_CHMPRN("Could not find hostname or IP address by %s.", target);
			return false;
		}
	}

	// If localhost, using global name, ip.
	if(is_cvt_localhost){
		// check localhost(or 127.0.0.1 or ::1) and remove it.
		if(RemoveLocalhostInCache(data)){
			// found, then get address info by full local hostname
			if(!GetHostAddressInfo(fulllocalname.c_str(), data)){
				MSG_CHMPRN("Could not find full local hostname or IP address by %s.", fulllocalname.c_str());
			}else{
				// remove (only) localhost
				RemoveLocalhostInCache(data);
			}
		}
	}
	return true;
}

// [NOTE]
// If one FQDN has many address(ex. it specifies in /etc/hosts), this method returns
// all hostnames and ip addresses.
//
bool ChmNetDb::GetHostAddressInfo(const char* target, CHMNDBCACHE& data)
{
	// [NOTE]
	// do not clear data in this method.

	struct addrinfo		hints;
	struct addrinfo*	res_info = NULL;
	struct addrinfo*	tmpaddrinfo;
	char				hostname[NI_MAXHOST];
	char				ipaddr[NI_MAXHOST];
	int					result;

	if(CHMEMPTYSTR(target)){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}

	// at first, check IPv4 mapped IPv6
	string	stripv4;
	if(ChmNetDb::GetIPv4MappedIPv6Address(target, stripv4)){
		if(!ChmNetDb::GetHostAddressInfo(stripv4.c_str(), data)){
			MSG_CHMPRN("Could not get (first checking)host address information %s which is IPv4 mapped IPv6, but continue by target(%s)", stripv4.c_str(), target);
		}
	}

	// target -> addrinfo
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags		= AI_CANONNAME;
	hints.ai_family		= AF_UNSPEC;
	hints.ai_socktype	= SOCK_STREAM;
	if(0 != getaddrinfo(target, NULL, &hints, &res_info) || !res_info){				// port is NULL
		return false;
	}

	// addrinfo(list) -> hostname
	for(tmpaddrinfo = res_info; tmpaddrinfo; tmpaddrinfo = tmpaddrinfo->ai_next){
		memset(&hostname, 0, sizeof(hostname));
		if(0 != (result = getnameinfo(tmpaddrinfo->ai_addr, tmpaddrinfo->ai_addrlen, hostname, sizeof(hostname), NULL, 0, NI_NAMEREQD | NI_NUMERICSERV))){
			MSG_CHMPRN("Could not get hostname %s, errno=%d.", target, result);
		}else{
			// add hostname
			AddUniqueStringToList(string(hostname), data.hostnames, false);

			// add cache
			if(0 != (result = getnameinfo(tmpaddrinfo->ai_addr, tmpaddrinfo->ai_addrlen, ipaddr, sizeof(ipaddr), NULL, 0, NI_NUMERICHOST | NI_NUMERICSERV))){
				HostnammeAddCache(string(hostname), string(ipaddr));
			}
		}
	}

	// addrinfo -> normalized ipaddress
	for(tmpaddrinfo = res_info; tmpaddrinfo; tmpaddrinfo = tmpaddrinfo->ai_next){
		memset(&ipaddr, 0, sizeof(ipaddr));
		if(0 != (result = getnameinfo(tmpaddrinfo->ai_addr, tmpaddrinfo->ai_addrlen, ipaddr, sizeof(ipaddr), NULL, 0, NI_NUMERICHOST | NI_NUMERICSERV))){
			MSG_CHMPRN("Could not convert normalized ipaddress  %s, errno=%d.", target, result);
		}else{
			// add ip address
			AddUniqueStringToList(string(ipaddr), data.ipaddresses, false);

			// add cache
			bool	is_hostname = false;
			if(0 != (result = getnameinfo(tmpaddrinfo->ai_addr, tmpaddrinfo->ai_addrlen, hostname, sizeof(hostname), NULL, 0, NI_NAMEREQD | NI_NUMERICSERV))){
				is_hostname = true;
				IpAddressAddCache(string(ipaddr), string(hostname));
			}

			// check IPv4 mapped IPv6
			stripv4.erase();
			if(ChmNetDb::GetIPv4MappedIPv6Address(ipaddr, stripv4)){
				// add ip address
				AddUniqueStringToList(stripv4, data.ipaddresses, false);

				// add cache
				if(is_hostname){
					IpAddressAddCache(stripv4, string(hostname));
				}
			}
		}
	}
	freeaddrinfo(res_info);

	// if short local hostname, adds fully hostname.
	bool	found = false;
	for(strlst_t::const_iterator iter1 = data.hostnames.begin(); data.hostnames.end() != iter1; ++iter1){
		if((*iter1) == fulllocalname){
			// already has full local hostname
			found = true;
			break;
		}
	}
	if(!found){
		// if hostnames has one of localnames, add full local hostname.
		for(strlst_t::const_iterator iter2 = data.hostnames.begin(); data.hostnames.end() != iter2; ++iter2){
			found = false;
			for(strlst_t::const_iterator iter3 = localnames.begin(); localnames.end() != iter3; ++iter3){
				if((*iter2) == (*iter3)){
					found = true;
					AddUniqueStringToList(fulllocalname, data.hostnames, false);
					break;
				}
			}
			if(found){
				break;
			}
		}
	}
	data.cached_time = time(NULL);

	return true;
}

bool ChmNetDb::GetAddrInfo(const char* target, short port, struct addrinfo** ppaddrinfo, bool is_cvt_localhost)
{
	if(CHMEMPTYSTR(target) || !ppaddrinfo){
		ERR_CHMPRN("Parameter are wrong.");
		return false;
	}
	*ppaddrinfo = NULL;

	addrinfolist_t	infolist;
	if(!GetAddrInfoList(target, port, infolist, is_cvt_localhost) || infolist.empty()){
		ChmNetDb::FreeAddrInfoList(infolist);
		return false;
	}
	*ppaddrinfo = infolist.front();
	infolist.pop_front();
	ChmNetDb::FreeAddrInfoList(infolist);

	return true;
}

bool ChmNetDb::GetAddrInfoList(const char* target, short port, addrinfolist_t& infolist, bool is_cvt_localhost)
{
	if(CHMEMPTYSTR(target)){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	ChmNetDb::FreeAddrInfoList(infolist);

	CHMNDBCACHE	data;
	if(!Search(target, data, is_cvt_localhost)){
		MSG_CHMPRN("Failed to convert %s to addrinfo structure.", target);
		return false;
	}

	// get address information
	string				strPort = to_string(port);
	struct addrinfo*	paddrinfo;
	struct addrinfo		hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags		= AI_CANONNAME;
	hints.ai_family		= AF_UNSPEC;
	hints.ai_socktype	= SOCK_STREAM;

	// ip addresses
	int							result;
	strlst_t::const_iterator	iter;
	for(iter = data.ipaddresses.begin(); data.ipaddresses.end() != iter; ++iter){
		paddrinfo = NULL;
		if(0 != (result = getaddrinfo(iter->c_str(), strPort.c_str(), &hints, &paddrinfo)) || !paddrinfo){
			MSG_CHMPRN("Could not get addrinfo from %s[%d], errno=%d.", target, port, result);
		}else{
			infolist.push_back(paddrinfo);
		}
	}
	// hostnames
	for(iter = data.hostnames.begin(); data.hostnames.end() != iter; ++iter){
		paddrinfo = NULL;
		if(0 != (result = getaddrinfo(iter->c_str(), strPort.c_str(), &hints, &paddrinfo)) || !paddrinfo){
			MSG_CHMPRN("Could not get addrinfo from %s[%d], errno=%d.", target, port, result);
		}else{
			infolist.push_back(paddrinfo);
		}
	}
	if(infolist.empty()){
		MSG_CHMPRN("Could not get any addrinfo from all ipaddresses and fqdns from %s[%d].", target, port);
		return false;
	}
	return true;
}

bool ChmNetDb::GetHostname(const char* target, string& hostname, bool is_cvt_localhost)
{
	strlst_t	hostnames;
	if(!GetHostnameList(target, hostnames, is_cvt_localhost)){
		return false;
	}
	if(hostnames.empty()){
		MSG_CHMPRN("No hostname is found from %s", target);
		return false;
	}
	hostname = hostnames.front();	// set from first position

	return true;
}

bool ChmNetDb::GetHostnameList(const char* target, strlst_t& hostnames, bool is_cvt_localhost)
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
	AddUniqueStringListToList(data.hostnames, hostnames, true);
	AddFullLocalHostname(hostnames);

	return true;
}

// {NOTE]
// If the same hostname as localhost is detected, add full local hostname to the beginning.
// (If it exists in the middle of the list, it moves to the top)
//
void ChmNetDb::AddFullLocalHostname(strlst_t& hostnames)
{
	bool	found = false;
	for(strlst_t::iterator iter = hostnames.begin(); hostnames.end() != iter; ){
		if((*iter) == fulllocalname){
			iter	= hostnames.erase(iter);
			found	= true;
			break;
		}
		for(strlst_t::const_iterator liter = localnames.begin(); localnames.end() != liter; ++liter){
			if((*iter) == (*liter)){
				found = true;
				break;
			}
		}
		++iter;
	}
	if(found){
		hostnames.push_front(fulllocalname);
	}
}

bool ChmNetDb::GetIpAddressString(const char* target, string& ipaddress, bool is_cvt_localhost)
{
	strlst_t	ipaddrs;
	if(!GetIpAddressStringList(target, ipaddrs, is_cvt_localhost)){
		return false;
	}
	if(ipaddrs.empty()){
		MSG_CHMPRN("No ip address is found from %s", target);
		return false;
	}
	ipaddress = ipaddrs.front();	// set from first position

	return true;
}

bool ChmNetDb::GetIpAddressStringList(const char* target, strlst_t& ipaddrs, bool is_cvt_localhost)
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
	AddUniqueStringListToList(data.ipaddresses, ipaddrs, true);
	ExpandZoneIndexInList(ipaddrs);

	return true;
}

// [NOTE]
// Returns all hostname and IP addresses which are ordered by below.
// full hostname -> hostanmes -> IP addresses (-> localhost) -> target(hostname)
//
bool ChmNetDb::GetAllHostList(const char* target, strlst_t& expandlist, bool is_cvt_localhost)
{
	// [NOTE]
	// expandlist is not initialized.
	//
	if(CHMEMPTYSTR(target)){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}

	if(ChmNetDb::IsLocalhostKeyword(target)){
		// case localhost
		AddUniqueStringListToList(localnames, expandlist, false);
		AddUniqueStringListToList(localaddrs, expandlist, false);

		if(is_cvt_localhost){
			RemoveLocalhostKeys(expandlist);
		}
	}else{
		strlst_t	tmplist;
		GetHostnameList(target, tmplist, true);						// without localhost
		AddUniqueStringListToList(tmplist, expandlist, false);

		tmplist.clear();
		GetIpAddressStringList(target, tmplist, true);				// without localhost
		AddUniqueStringListToList(tmplist, expandlist, false);

		if(!is_cvt_localhost){
			tmplist.clear();
			GetHostnameList(target, tmplist, false);				// with localhost
			AddUniqueStringListToList(tmplist, expandlist, false);

			tmplist.clear();
			GetIpAddressStringList(target, tmplist, false);			// with localhost
			AddUniqueStringListToList(tmplist, expandlist, false);
		}
		AddUniqueStringToList(string(target), expandlist, false);	// last
	}
	// cut zone index
	RemoveZoneIndexInList(expandlist);

	return true;
}

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
