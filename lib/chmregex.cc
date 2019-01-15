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
#include <string.h>
#include <regex.h>
#include <string>

#include "chmcommon.h"
#include "chmutil.h"
#include "chmdbg.h"
#include "chmregex.h"
#include "chmnetdb.h"

using namespace	std;

//---------------------------------------------------------
// Simple regex
//---------------------------------------------------------
// Chmpx configuration allows FQDN and custom simple regex
// for hostname.
// FQDN must be listed by DNS(or /etc/hosts) and do not have
// to specify full FQDN.
// Custom simple regex is following:
// 	server[xx,yy].yahoo.co.jp	- Many strings
// 	server[0-9].yahoo.co.jp		- Number range
// 	server[A-K].yahoo.co.jp		- Alphabetical range, Must be a-z or A-Z.
// 	s[A-K][0-9].yahoo.co.jp		- Mixed
// And can specify own server by 'localhost'.
// Last, IP address(IPv4 or IPv6) can be specified. But if it
// is specified in server list, that IP address must be reverse
// to FQDN by DNS(or /etc/hosts).
// If it is in slave list, not need to reverse.
// 
// The chmpx process checks hostname list when the other chmpx
// accesses to them.
// 
// If the slave chmpx accesses to server when chmpx runs as
// server mode, the server chmpx checks only IP address against
// hostname(FQDN, IP..) list from configuration file.
// Another if the server mode chmpx accesses to another server
// chmpx, the server checks FQDN converted from IP address.
// 
// The chmpx makes RING by consistent hashing, they need to
// make order for servers, so that the server chmpx must have
// FQDN. But the slave chmpx does not have FQDN, only need
// to list in configuration file.
// 
// Then following functions are for simple regex.
// 
static bool expand_simple_regex_string(const string& str_part_regex, strlst_t& expand_lst)
{
	string				strtarget;
	strlst_t			sep_commma_lst;
	string::size_type	pos;

	expand_lst.clear();

	// parse ','
	for(strtarget = trim(str_part_regex); strtarget.length(); strtarget = trim(strtarget)){
		if(string::npos == (pos = strtarget.find(","))){
			sep_commma_lst.push_back(strtarget);
			strtarget = "";
		}else{
			string 	tmp = strtarget.substr(0, pos);
			tmp = trim(tmp);
			if(tmp.length()){
				sep_commma_lst.push_back(tmp);
			}
			strtarget = strtarget.substr(pos + 1);
		}
	}

	// parse '-' in comma separated array
	for(strlst_t::const_iterator iter = sep_commma_lst.begin(); iter != sep_commma_lst.end(); ++iter){
		if(string::npos == (pos = iter->find("-"))){
			expand_lst.push_back(*iter);
		}else{
			// found '-'
			string 	tmp1 = iter->substr(0, pos);
			string 	tmp2 = iter->substr(pos + 1);
			tmp1 = trim(tmp1);
			tmp2 = trim(tmp2);
			if(0 == tmp1.length() || 0 == tmp2.length()){
				MSG_CHMPRN("Area strings separated are empty.");
				return false;
			}
			if(string::npos != tmp2.find("-")){
				MSG_CHMPRN("Found many area separator.");
				return false;
			}

			if(is_string_number(tmp1.c_str()) && is_string_number(tmp2.c_str())){
				// Number
				int	num1 = atoi(tmp1.c_str());
				int	num2 = atoi(tmp2.c_str());
				if(num2 < num1){
					MSG_CHMPRN("Number range are wrong.");
					return false;
				}
				for(; num1 <= num2; num1++){
					expand_lst.push_back(to_string(num1));
				}
			}else{
				// Alpha
				if(1 != tmp1.length() || 1 != tmp2.length()){
					MSG_CHMPRN("Character range must be specified by one character.");
					return false;
				}
				char	cTmp1 = tmp1[0];
				char	cTmp2 = tmp2[0];
				if(!(('A' <= cTmp1 && cTmp1 <= 'Z') || ('a' <= cTmp1 && cTmp1 <= 'z')) || !(('A' <= cTmp2 && cTmp2 <= 'Z') || ('a' <= cTmp2 && cTmp2 <= 'z'))){
					MSG_CHMPRN("Character range must be specified by a-z or A-Z.");
					return false;
				}
				if(cTmp2 < cTmp1 || !(('a' <= cTmp1 && 'a' <= cTmp2) || (cTmp1 <= 'Z' && cTmp2 <= 'Z'))){
					MSG_CHMPRN("Both character word does not same range.");
					return false;
				}
				for(; cTmp1 <= cTmp2; cTmp1++){
					expand_lst.push_back(string(1, cTmp1));
				}
			}
		}
	}
	return true;
}

static bool expand_simple_regex(const string& simple_regex, strlst_t& expand_lst)
{
	strlst_t	simple_regex_lst;
	string		one_simple_regex;

	// cppcheck-suppress stlSize
	for(simple_regex_lst.push_back(trim(simple_regex)); 0 < simple_regex_lst.size(); ){
		one_simple_regex = simple_regex_lst.front();
		simple_regex_lst.pop_front();

		string::size_type	pos;
		string::size_type	pos2 = one_simple_regex.find("]");

		if(string::npos == (pos = one_simple_regex.find("["))){
			if(string::npos != pos2){
				MSG_CHMPRN("Found \']\' separator word without \'[\' word.");
				return false;
			}
			expand_lst.push_back(one_simple_regex);

		}else{
			// found '['
			string	prefix_str	= one_simple_regex.substr(0, pos);
			one_simple_regex	= one_simple_regex.substr(pos + 1);

			if(string::npos != pos2 && pos2 < pos){
				MSG_CHMPRN("Found \']\' separator word without \'[\' word.");
				return false;
			}

			if(string::npos == (pos = one_simple_regex.find("]"))){
				MSG_CHMPRN("Not found \']\' separator word.");
				return false;
			}
			string	str_part_regex 	= one_simple_regex.substr(0, pos);
			string	suffix_str		= one_simple_regex.substr(pos + 1);

			str_part_regex = trim(str_part_regex);
			if(0 == str_part_regex.length()){
				MSG_CHMPRN("There is no string in \'[\' to \']\' area.");
				return false;
			}

			if(string::npos != str_part_regex.find("[")){
				MSG_CHMPRN("Found many \'[\' separator word.");
				return false;
			}

			// parse [...] to string array
			strlst_t	expandarea;
			if(!expand_simple_regex_string(str_part_regex, expandarea)){
				MSG_CHMPRN("Could not expand simple regex, maybe string is wrong.");
				return false;
			}

			// push target array for recheck
			for(strlst_t::const_iterator iter = expandarea.begin(); iter != expandarea.end(); ++iter){
				string	tmp;
				tmp = prefix_str;
				tmp += *iter;
				tmp += suffix_str;
				simple_regex_lst.push_back(tmp);
			}
		}
	}
	return true;
}

//---------------------------------------------------------
// Utilities
//---------------------------------------------------------
// For server hostname
//
// This function expands hostname list from hostname which
// has simple regex rule.
// If is_cvt_fqdn is true, all hostname is checked by NetDB.
// The other does not check.
// If is_cvt_localhost is true, hostname which is "localhost"
// or "127.0.0.1" or "::1" is changed FQDN.
//
bool ExpandSimpleRegxHostname(const char* hostname, strlst_t& expand_lst, bool is_cvt_localhost, bool is_cvt_fqdn, bool is_strict)
{
	if(CHMEMPTYSTR(hostname)){
		ERR_CHMPRN("Parameter is NULL.");
		return false;
	}
	string	strhost = hostname;

	expand_lst.clear();
	// cppcheck-suppress stlSize
	if(!expand_simple_regex(strhost, expand_lst) || 0 == expand_lst.size()){
		ERR_CHMPRN("Failed to expand simple regex.");
		return false;
	}

	if(is_cvt_fqdn){
		// check & convert IP address or localhost to FQDN.
		strlst_t	tmp_lst = expand_lst;
		expand_lst.clear();
		for(strlst_t::const_iterator iter = tmp_lst.begin(); iter != tmp_lst.end(); ++iter){
			string	fqdn;
			if(!ChmNetDb::Get()->GetHostname(iter->c_str(), fqdn, is_cvt_localhost)){
				if(is_strict){
					ERR_CHMPRN("Failed to convert FQDN from %s, so break because this function strict mode", iter->c_str());
					return false;
				}else{
					//MSG_CHMPRN("Failed to convert FQDN from %s, but skip it and continue...", iter->c_str());
					continue;
				}
			}
			expand_lst.push_back(fqdn);
		}
	}
	return true;
}

// For server hostname
//
// This function is checking hostname in expanded hostname list
// for server list. The hostname_lst should be expanded by 
// ExpandSimpleRegxHostname() with is_cvt_localhost = true and 
// is_cvt_fqdn = true.
// If the hostname matches in array, matchhostname is set as
// matched hostname(FQDN or localhost or IP address).
//
bool IsInHostnameList(const char* hostname, strlst_t& hostname_lst, string& matchhostname)
{
	if(CHMEMPTYSTR(hostname)){
		ERR_CHMPRN("Parameter is NULL.");
		return false;
	}

	if(!ChmNetDb::Get()->GetHostname(hostname, matchhostname, true)){		// If "localhost", convert local host name
		ERR_CHMPRN("Failed to convert FQDN from %s.", hostname);
		return false;
	}
	for(strlst_t::const_iterator iter = hostname_lst.begin(); iter != hostname_lst.end(); ++iter){
		if(matchhostname == (*iter)){
			// found!
			return true;
		}
	}
	return false;
}

// For slave hostname
//
// This function is checking hostname in hostname array which 
// are wrote regex.
// If the hostname matches in array, matchhostname is set as
// matched hostname(FQDN or localhost or IP address).
//
bool IsMatchHostname(const char* hostname, strlst_t& regex_lst, string& matchhostname)
{
	if(CHMEMPTYSTR(hostname)){
		ERR_CHMPRN("Parameter is NULL.");
		return false;
	}
	string		fqdn;
	string		ipaddress;
	string		if_ipaddress;
	strlst_t	target_lst;			// order by global hostname, ip address, i/f ip address, localhost

	// make from hostname to FQDN & IP address list.
	//
	if(!ChmNetDb::Get()->GetHostname(hostname, fqdn, true)){					// If "localhost", convert FQDN host name
		ERR_CHMPRN("Failed to convert FQDN from %s.", hostname);
		return false;
	}
	target_lst.push_back(fqdn);													// global name is added at first of list

	if(!ChmNetDb::Get()->GetIpAddressString(fqdn.c_str(), ipaddress, false)){	// If "localhost", not convert
		ERR_CHMPRN("Failed to convert IP address from %s.", hostname);
		return false;
	}
	if(fqdn != ipaddress){
		target_lst.push_back(ipaddress);										// if hostname is not ipaddress, set ipaddress
	}
	if(!ChmNetDb::Get()->GetIpAddressString(fqdn.c_str(), if_ipaddress, true)){	// If "localhost", convert I/F ip address
		ERR_CHMPRN("Failed to convert I/F IP address from %s.", hostname);
		return false;
	}
	if(if_ipaddress != ipaddress){
		target_lst.push_back(if_ipaddress);
	}
	if(0 != strcmp(hostname, fqdn.c_str())){
		target_lst.push_back(string(hostname));									// if hostname is not same as globalname, add to end of list.
	}

	// Matching check
	for(strlst_t::const_iterator iter = regex_lst.begin(); iter != regex_lst.end(); ++iter){
		regex_t	regex_obj;
		int		result;
		if(0 != (result = regcomp(&regex_obj, iter->c_str(), REG_EXTENDED | REG_NOSUB))){
			ERR_CHMPRN("Failed to compile regex for %s.", iter->c_str());
			return false;
		}

		for(strlst_t::const_iterator titer = target_lst.begin(); titer != target_lst.end(); ++titer){
			if(0 == regexec(&regex_obj, titer->c_str(), 0, NULL, 0)){
				// match!
				matchhostname = (*titer);
				regfree(&regex_obj);
				return true;
			}
		}
		regfree(&regex_obj);
	}
	return false;
}

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
