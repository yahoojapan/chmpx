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
#ifndef	CHMREGEX_H
#define	CHMREGEX_H

//---------------------------------------------------------
// Utilities
//---------------------------------------------------------
bool ExpandSimpleRegxHostname(const char* hostname, strlst_t& expand_lst, bool is_cvt_localhost, bool is_cvt_fqdn = true, bool is_strict = false);
bool IsInHostnameList(const char* hostname, strlst_t& hostname_lst, std::string& matchhostname);
bool IsMatchHostname(const char* hostname, strlst_t& regex_lst, std::string& matchhostname);

#endif	// CHMREGEX_H

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
