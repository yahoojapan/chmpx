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
#ifndef	CHMNETDB_H
#define	CHMNETDB_H

#include <fullock/flckstructure.h>
#include <fullock/flckbaselist.tcc>

#include "chmcommon.h"
#include "chmutil.h"

//---------------------------------------------------------
// Structure
//---------------------------------------------------------
typedef struct chm_netdb_cache{
	strlst_t		ipaddresses;
	strlst_t		hostnames;
	time_t			cached_time;							// set 0 if not cashed out.

	chm_netdb_cache() : cached_time(0) {}
}CHMNDBCACHE, *PCHMNDBCACHE;

typedef std::map<std::string, CHMNDBCACHE>	chmndbmap_t;	// Key is ipaddress or hostname
typedef std::list<struct addrinfo*>			addrinfolist_t;

//---------------------------------------------------------
// Class ChmNetDb
//---------------------------------------------------------
class ChmNetDb
{
	protected:
		static const time_t	ALIVE_TIME = 60;	// default 60s
		static int			lockval;			// like mutex

		chmndbmap_t			cachemap;
		time_t				timeout;			// 0 means no timeout
		std::string			fulllocalname;		// local hostname from uname(gethostname), this is Full FQDN.
		strlst_t			localaddrs;			// local ip addresses
		strlst_t			localnames;			// local hostname from getnameinfo, sometimes this name is without domain name if set in /etc/hosts.

	protected:
		ChmNetDb();
		virtual ~ChmNetDb();

		bool InitializeLocalHostInfo(void);
		bool InitializeLocalHostIpAddresses(void);
		bool InitializeLocalHostnames(void);
		bool ClearEx(void);
		bool CacheOutEx(void);
		bool RawAddCache(const std::string& target, const strlst_t& addlist, bool is_noexp, bool is_hostname, bool is_reentrant = false);
		bool HostnammeAddCache(const std::string& hostname, const std::string& ipaddr, bool is_noexp = false);
		bool HostnammeAddCache(const std::string& hostname, const strlst_t& ipaddrs, bool is_noexp = false);
		bool IpAddressAddCache(const std::string& ipaddr, const std::string& hostname, bool is_noexp = false);
		bool IpAddressAddCache(const std::string& ipaddr, const strlst_t& hostnames, bool is_noexp = false);
		bool SearchCache(const char* target, CHMNDBCACHE& data);
		bool Search(const char* target, CHMNDBCACHE& data, bool is_cvt_localhost);
		bool GetHostAddressInfo(const char* target, CHMNDBCACHE& data);
		void AddFullLocalHostname(strlst_t& hostnames);

	public:
		static ChmNetDb* Get(void);
		static time_t SetTimeout(time_t value);
		static bool Clear(void);
		static bool CacheOut(void);
		static bool GetLocalHostname(std::string& hostname);
		static bool GetLocalHostnameList(strlst_t& hostnames);
		static bool GetLocalHostList(strlst_t& hostinfo, bool remove_localhost = false);
		static bool GetAnyAddrInfo(short port, struct addrinfo** ppaddrinfo, bool is_inetv6);
		static bool CvtAddrInfoToIpAddress(const struct sockaddr_storage* info, socklen_t infolen, std::string& stripaddress);
		static bool CvtSockToLocalPort(int sock, short& port);
		static bool CvtSockToPeerPort(int sock, short& port);
		static bool CvtV4MappedAddrInfo(struct sockaddr_storage* info, socklen_t& addrlen);
		static void FreeAddrInfoList(addrinfolist_t& infolist);
		static std::string GetNoZoneIndexIpAddress(const std::string& ipaddr);
		static bool IsLocalhostKeyword(const char* host);

		bool GetAddrInfo(const char* target, short port, struct addrinfo** ppaddrinfo, bool is_cvt_localhost);	// Must freeaddrinfo for *ppaddrinfo
		bool GetAddrInfoList(const char* target, short port, addrinfolist_t& infolist, bool is_cvt_localhost);	// Must FreeAddrInfoList for clear
		bool GetHostname(const char* target, std::string& hostname, bool is_cvt_localhost);
		bool GetHostnameList(const char* target, strlst_t& hostnames, bool is_cvt_localhost);
		bool GetIpAddressString(const char* target, std::string& ipaddress, bool is_cvt_localhost);
		bool GetIpAddressStringList(const char* target, strlst_t& ipaddrs, bool is_cvt_localhost);
		bool GetAllHostList(const char* target, strlst_t& expandlist, bool is_cvt_localhost);
};

#endif	// CHMNETDB_H

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
