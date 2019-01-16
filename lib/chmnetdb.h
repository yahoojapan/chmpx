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
	std::string		ipaddress;
	std::string		hostname;
	time_t			cached_time;
}CHMNDBCACHE, *PCHMNDBCACHE;

typedef std::map<std::string, CHMNDBCACHE>	chmndbmap_t;	// Key is ipaddress or hostname

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
		std::string			localname;			// local hostname from getnameinfo, sometimes this name is without domain name if set in /etc/hosts.

	protected:
		ChmNetDb();
		virtual ~ChmNetDb();

		bool InitializeLocalHostName(void);
		bool ClearEx(void);
		bool CacheOutEx(void);
		bool GetHostAddressInfo(const char* target, std::string& strhostname, std::string& stripaddress);
		bool SearchCache(const char* target, CHMNDBCACHE& data);
		bool Search(const char* target, CHMNDBCACHE& data, bool is_cvt_localhost);

	public:
		static ChmNetDb* Get(void);
		static time_t SetTimeout(time_t value);
		static bool Clear(void);
		static bool CacheOut(void);
		static bool GetLocalHostname(std::string& hostname);
		static bool GetAnyAddrInfo(short port, struct addrinfo** ppaddrinfo, bool is_inetv6);
		static bool CvtAddrInfoToIpAddress(const struct sockaddr_storage* info, socklen_t infolen, std::string& stripaddress);
		static bool CvtSockToLocalPort(int sock, short& port);
		static bool CvtSockToPeerPort(int sock, short& port);
		static bool CvtV4MappedAddrInfo(struct sockaddr_storage* info, socklen_t& addrlen);

		bool GetAddrInfo(const char* target, short port, struct addrinfo** ppaddrinfo, bool is_cvt_localhost);	// Must freeaddrinfo for *ppaddrinfo
		bool GetHostname(const char* target, std::string& hostname, bool is_cvt_localhost);
		bool GetIpAddressString(const char* target, std::string& ipaddress, bool is_cvt_localhost);
};

#endif	// CHMNETDB_H

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
