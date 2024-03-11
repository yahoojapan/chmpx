/*
 * CHMPX
 *
 * Copyright 2018 Yahoo Japan Corporation.
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
 * CREATE:   Fri Mar 9 2018
 * REVISION:
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <libgen.h>
#include <unistd.h>
#include <termios.h>
#include <signal.h>
#include <errno.h>
#include <sys/utsname.h>
#include <ifaddrs.h>
#include <map>
#include <list>

#include <k2hash/k2hutil.h>

#include "chmcommon.h"
#include "chmconfutil.h"
#include "chmcntrl.h"
#include "chmregex.h"
#include "chmnetdb.h"
#include "chmutil.h"

using namespace std;

//---------------------------------------------------------
// Utilities for debugging
//---------------------------------------------------------
static inline void PRN(const char* format, ...)
{
	if(format){
		va_list ap;
		va_start(ap, format);
		vfprintf(stdout, format, ap); 
		va_end(ap);
	}
	fprintf(stdout, "\n");
}

static inline void CHMPXCTRLTOOL_PRINT(const char* prefix, const char* format, ...)
{
	if(!CHMEMPTYSTR(prefix)){
		fprintf(stderr, "%s ", prefix);
	}
	if(format){
		va_list ap;
		va_start(ap, format);
		vfprintf(stderr, format, ap); 
		va_end(ap);
	}
	fprintf(stderr, "\n");
}

// Print message mode
static bool is_print_dmp = false;
static bool is_print_msg = false;
static bool is_print_wan = false;
static bool is_print_err = true;		// default
static bool is_chmpx_dbg = false;

#define	MSG(...)		if(is_print_msg){ CHMPXCTRLTOOL_PRINT("[CCT-MSG]", __VA_ARGS__); }
#define	WAN(...)		if(is_print_wan){ CHMPXCTRLTOOL_PRINT("[CCT-WAN]", __VA_ARGS__); }
#define	ERR(...)		if(is_print_err){ CHMPXCTRLTOOL_PRINT("[CCT-ERR]", __VA_ARGS__); }

//---------------------------------------------------------
// Common Variables
//---------------------------------------------------------
// Input option & parameter value
static bool					isColorDisplay		= true;
static string				strInitialConfig("");
static string				strInitialHostname("");
static short				nInitialCtrlPort	= CHM_INVALID_PORT;
static string				strInitialCuk("");
static string				strInitialCtlEPS("");
static string				strInitialCustomSeed("");
static bool					isOneHostTarget		= false;
static bool					isInitialServerMode	= false;
static int					nThreadCount		= 0;

//---------------------------------------------------------
// Hostname and list information
//---------------------------------------------------------
typedef struct confirmed_hostinfo{
	string	hostname;							// hostname
	bool	confirmed;							// FQDN is true
}CHOSTINFO, *PCHOSTINFO;

typedef std::list<CHOSTINFO>					chostinfolist_t;
typedef std::list<chostinfolist_t*>				hostlistptrs_t;
typedef std::map<std::string, chostinfolist_t*>	hostlistmap_t;

//
// Utilities
//
static inline bool cvt_chostinfolist(const chostinfolist_t& baselist, strlst_t& hostnames, bool only_confirmed)
{
	hostnames.clear();
	for(chostinfolist_t::const_iterator iter = baselist.begin(); iter != baselist.end(); ++iter){
		if(!only_confirmed || iter->confirmed){
			hostnames.push_back(iter->hostname);
		}
	}
	return true;
}

//
// Utilities
//
static inline bool merge_chostinfolist(chostinfolist_t& baselist, const chostinfolist_t& addlist)
{
	for(chostinfolist_t::const_iterator aiter = addlist.begin(); aiter != addlist.end(); ++aiter){
		bool	found;
		found = false;
		for(chostinfolist_t::iterator biter = baselist.begin(); biter != baselist.end(); ++biter){
			// cppcheck-suppress unmatchedSuppression
			// cppcheck-suppress useStlAlgorithm
			if(biter->hostname == aiter->hostname){
				found = true;
				if(!biter->confirmed && aiter->confirmed){
					biter->confirmed = true;
				}
				break;
			}
		}
		if(!found){
			baselist.push_back(*aiter);
		}
	}
	return true;
}

//
// Utilities
//
static inline bool add_chostinfolist(chostinfolist_t& baselist, const string& hostname, bool confirmed)
{
	bool	found = false;
	for(chostinfolist_t::iterator biter = baselist.begin(); biter != baselist.end(); ++biter){
		// cppcheck-suppress unmatchedSuppression
		// cppcheck-suppress useStlAlgorithm
		if(biter->hostname == hostname){
			found = true;
			if(!biter->confirmed && confirmed){
				biter->confirmed = true;
			}
			break;
		}
	}
	if(!found){
		CHOSTINFO	chostinfo;
		chostinfo.hostname	= hostname;
		chostinfo.confirmed	= confirmed;
		baselist.push_back(chostinfo);
	}
	return true;
}

//
// Utilities
//
static inline bool add_chostinfolist(chostinfolist_t& baselist, const CHOSTINFO& chostinfo)
{
	return add_chostinfolist(baselist, chostinfo.hostname, chostinfo.confirmed);
}

//
// Utilities
//
static inline bool add_chostinfolist(chostinfolist_t& baselist, const strlst_t& hostnames, bool confirmed)
{
	for(strlst_t::const_iterator iter = hostnames.begin(); iter != hostnames.end(); ++iter){
		add_chostinfolist(baselist, *iter, confirmed);
	}
	return true;
}

//---------------------------------------------------------
// Hostname and list Utility Class (for cache)
//---------------------------------------------------------
class HostnameMap
{
	protected:
		static hostlistptrs_t	listptrs;
		static hostlistmap_t	listmaps;

	protected:
		static chostinfolist_t* FindHostnamesPtr(const char* target);
		static bool MakeHostnamesMap(const strlst_t& hostnames, bool confirmed);

		explicit HostnameMap(void);
		virtual ~HostnameMap(void);

	public:
		static bool Initialize(void);
		static bool Clear(void);

		static bool GetHostnames(const char* target, chostinfolist_t& hostnames);
		static bool GetHostnames(const char* target, strlst_t& hostnames, bool only_confirmed = false);
		static std::string GetFirstHostname(const char* target);
		static bool UpdateHostnameConfirm(const char* target);

		static bool DumpMap(void);
};

hostlistptrs_t	HostnameMap::listptrs;
hostlistmap_t	HostnameMap::listmaps;

bool HostnameMap::Initialize(void)
{
	HostnameMap::listptrs.clear();
	HostnameMap::listmaps.clear();
	return true;
}

bool HostnameMap::Clear(void)
{
	HostnameMap::listmaps.clear();
	for(hostlistptrs_t::iterator iter = HostnameMap::listptrs.begin(); iter != HostnameMap::listptrs.end(); ++iter){
		delete (*iter);
	}
	HostnameMap::listptrs.clear();
	return true;
}

chostinfolist_t* HostnameMap::FindHostnamesPtr(const char* target)
{
	if(CHMEMPTYSTR(target)){
		return NULL;
	}
	hostlistmap_t::iterator	iter = HostnameMap::listmaps.find(string(target));
	if(iter == HostnameMap::listmaps.end()){
		return NULL;
	}
	if(!(iter->second)){
		// Bad case, do recover
		HostnameMap::listmaps.erase(iter);
		return NULL;
	}
	return (iter->second);
}

bool HostnameMap::MakeHostnamesMap(const strlst_t& hostnames, bool confirmed)
{
	strlst_t::const_iterator	iter;
	chostinfolist_t::iterator	chiter;
	hostlistptrs_t::iterator	hpiter;
	hostlistmap_t::iterator		miter;

	//-----------------------------------------------------
	// Check the mapping of each host in hostnames
	//-----------------------------------------------------
	// Check if the same hostname exists in the existing map.
	// If it exists, collect the pointer of the associated hostname list.
	//
	hostlistptrs_t	existedhostlist;			// found hostname list pointers.
	for(iter = hostnames.begin(); iter != hostnames.end(); ++iter){
		string				onehost	= *iter;
		chostinfolist_t*	plist	= HostnameMap::FindHostnamesPtr(onehost.c_str());
		if(plist){
			bool	found	= false;
			for(hpiter = existedhostlist.begin(); hpiter != existedhostlist.end(); ++hpiter){
				// cppcheck-suppress unmatchedSuppression
				// cppcheck-suppress useStlAlgorithm
				if(plist == (*hpiter)){
					found	= true;
					break;
				}
			}
			if(!found){
				existedhostlist.push_back(plist);
			}
		}
	}

	// make/remake mapping and hostname lists
	if(1 < existedhostlist.size()){
		//-----------------------------------------------------
		// make(merge) new hostname list pointer and set it to all hostname in mapping
		//-----------------------------------------------------
		// When multiple hostname list pointers are detected.
		// In this case, the hostname list pointer found is merged with the hostname
		// list passed in the argument to create a new hostname list pointer.
		// Then map this new hostname list pointer to each hostname in the merged
		// hostname list.
		// The detected hostname list pointer is no longer used and is discarded.
		//

		// 
		// Create a unique host name list from the detected host name list pointers.
		// It also merges the hostname list passed in as an argument.
		//
		chostinfolist_t*	pnewlist = new chostinfolist_t;				// new hostname list
		for(hpiter = existedhostlist.begin(); hpiter != existedhostlist.end(); ++hpiter){
			for(miter = HostnameMap::listmaps.begin(); miter != HostnameMap::listmaps.end(); ++miter){
				if((*hpiter) == miter->second){
					chostinfolist_t*	pchostlist = miter->second;
					for(chiter = pchostlist->begin(); chiter != pchostlist->end(); ++chiter){
						add_chostinfolist(*pnewlist, chiter->hostname, chiter->confirmed);
					}
				}
			}
		}
		add_chostinfolist(*pnewlist, hostnames, confirmed);

		//
		// Map this new list to each hostname in the unique hostname list.
		//
		for(chiter = pnewlist->begin(); chiter != pnewlist->end(); ++chiter){
			HostnameMap::listmaps[chiter->hostname] = pnewlist;
		}

		//
		// remove old hostname list pointers
		//
		for(hpiter = existedhostlist.begin(); hpiter != existedhostlist.end(); ++hpiter){
			for(hostlistptrs_t::iterator hpiter2 = HostnameMap::listptrs.begin(); hpiter2 != HostnameMap::listptrs.end(); ){
				if((*hpiter2) == (*hpiter)){
					hpiter2 = HostnameMap::listptrs.erase(hpiter2);
				}else{
					++hpiter2;
				}
			}
			delete (*hpiter);
		}

	}else if(1 == existedhostlist.size()){
		//-----------------------------------------------------
		// found only one hostname list pointer
		//-----------------------------------------------------
		// add hostnames to existed pointer
		hpiter = existedhostlist.begin();
		add_chostinfolist(*(*hpiter), hostnames, confirmed);

		// set existed pointer(over write if same existed hostname)
		for(iter = hostnames.begin(); iter != hostnames.end(); ++iter){
			HostnameMap::listmaps[(*iter)] = (*hpiter);
		}

	}else{	// 0 == existedhostlist.size()
		//-----------------------------------------------------
		// not found hostname list pointer
		//-----------------------------------------------------
		// make new hostname list pointer
		chostinfolist_t*	pnewlist = new chostinfolist_t;
		add_chostinfolist(*pnewlist, hostnames, confirmed);

		// add mapping and list
		HostnameMap::listptrs.push_back(pnewlist);
		for(iter = hostnames.begin(); iter != hostnames.end(); ++iter){
			HostnameMap::listmaps[(*iter)] = pnewlist;
		}
	}
	return true;
}

bool HostnameMap::GetHostnames(const char* target, chostinfolist_t& chostinfolist)
{
	if(CHMEMPTYSTR(target)){
		return false;
	}
	// search cache
	const chostinfolist_t*	pchostinfolist = HostnameMap::FindHostnamesPtr(target);
	if(pchostinfolist){
		chostinfolist = *pchostinfolist;
	}else{
		// get hostname list without cache
		strlst_t	hostnames;
		if(!ChmNetDb::Get()->GetHostnameList(target, hostnames, true) || hostnames.empty()){
			return false;
		}
		// add cache(mapping)
		HostnameMap::MakeHostnamesMap(hostnames, true);		// confirmed

		// re-search cache(after merging)
		pchostinfolist = HostnameMap::FindHostnamesPtr(target);
		if(!pchostinfolist){
			return false;
		}
		chostinfolist = *pchostinfolist;
	}
	return true;
}

bool HostnameMap::GetHostnames(const char* target, strlst_t& hostnames, bool only_confirmed)
{
	chostinfolist_t	chostinfolist;
	if(!HostnameMap::GetHostnames(target, chostinfolist)){
		return false;
	}
	return cvt_chostinfolist(chostinfolist, hostnames, only_confirmed);
}

std::string HostnameMap::GetFirstHostname(const char* target)
{
	strlst_t	hostnames;
	string		hostname;
	if(HostnameMap::GetHostnames(target, hostnames, true)){
		hostname = hostnames.front();
	}else{
		hostname = target;
	}
	return hostname;
}

bool HostnameMap::UpdateHostnameConfirm(const char* target)
{
	if(CHMEMPTYSTR(target)){
		return false;
	}
	// get hostname list without cache
	strlst_t	hostnames;
	if(!ChmNetDb::Get()->GetHostnameList(target, hostnames, true) || hostnames.empty()){
		return false;
	}
	// add cache(mapping)
	HostnameMap::MakeHostnamesMap(hostnames, true);		// confirmed
	return true;
}

bool HostnameMap::DumpMap(void)
{
	PRN("DUMP : HostnameMap::listmaps {");

	hostlistmap_t::iterator	miter;
	int						count;
	for(count = 0, miter = HostnameMap::listmaps.begin(); miter != HostnameMap::listmaps.end(); ++count, ++miter){
		PRN("    [%d] %s = {", count, miter->first.c_str());

		if(miter->second){
			for(chostinfolist_t::iterator hpiter = miter->second->begin(); hpiter != miter->second->end(); ++hpiter){
				PRN("               %s (%s)", hpiter->hostname.c_str(), hpiter->confirmed ? "comfirmed" : "not confirmed");
			}
		}else{
			PRN("               NULL");
		}
		PRN("    }");
	}
	PRN("}");

	return true;
}

HostnameMap::HostnameMap()
{
}

HostnameMap::~HostnameMap()
{
}

//---------------------------------------------------------
// Chmpx Node information
//---------------------------------------------------------
class NodeCtrlInfo
{
	protected:
		static CHMPXID_SEED_TYPE	chmpxidType;		// common seed type for creating chmpxid
		static string				groupName;			// common chmpx group name

		chmpxid_t					chmpxid;
		string						hostname;			// The hostname specified when it was built
		short						ctrlport;
		string						cuk;
		string						ctlendpoints;
		string						custom_seed;
		bool						is_server;

	protected:
		bool GetMaximumChmpxidList(chmpxidlist_t& chmpxidlist) const;

	public:
		static CHMPXID_SEED_TYPE GetType(void) { return chmpxidType; }
		static string GetGroup(void) { return groupName; }
		static bool SetType(const char* ptype);
		static void SetType(CHMPXID_SEED_TYPE type);
		static bool SetGroup(const char* group);

		explicit NodeCtrlInfo(void);
		explicit NodeCtrlInfo(const NodeCtrlInfo& other);
		explicit NodeCtrlInfo(chmpxid_t set_chmpxid, const char* target = NULL, short port = 0, const char* pcuk = NULL, const char* pendpoints = NULL, const char* pseed = NULL, bool is_server_mode = false);
		explicit NodeCtrlInfo(const char* target, short port, const char* pcuk = NULL, const char* pendpoints = NULL, const char* pseed = NULL, bool is_server_mode = false);
		virtual ~NodeCtrlInfo(void);


		bool Set(chmpxid_t set_chmpxid, const char* target = NULL, short port = 0, const char* pcuk = NULL, const char* pendpoints = NULL, const char* pseed = NULL, bool is_server_mode = false);
		bool Set(const char* target, short port, const char* pcuk = NULL, const char* pendpoints = NULL, const char* pseed = NULL, bool is_server_mode = false);

		chmpxid_t GetChmpxId(void) const { return chmpxid; }
		string GetHostname(void) const { return hostname; }
		short GetCtrlport(void) const { return ctrlport; }
		string GetCuk(void) const { return cuk; }
		string GetCtlendpoints(void) const { return ctlendpoints; }
		string GetCusomSeed(void) const { return custom_seed; }
		bool IsServerNode(void) const { return is_server; }
		bool IsSlaveNode(void) const { return !is_server; }

		int compare(const NodeCtrlInfo& other) const;
		bool operator==(const NodeCtrlInfo& other) const
		{
			return (0 == compare(other));
		}
		bool operator!=(const NodeCtrlInfo& other) const
		{
			return (0 != compare(other));
		}
		NodeCtrlInfo& operator=(const NodeCtrlInfo& other)
		{
			chmpxid		= other.chmpxid;
			hostname	= other.hostname;
			ctrlport	= other.ctrlport;
			cuk			= other.cuk;
			ctlendpoints= other.ctlendpoints;
			custom_seed	= other.custom_seed;
			is_server	= other.is_server;
			return *this;
		}
		bool IsConfirmHostname(void) const;
		bool UpdateConfirmHostname(void);
};

//
// Class variables
//
CHMPXID_SEED_TYPE	NodeCtrlInfo::chmpxidType = CHMPXID_SEED_NAME;
string				NodeCtrlInfo::groupName("");

//
// Class methods
//
bool NodeCtrlInfo::SetType(const char* ptype)
{
	if(CHMEMPTYSTR(ptype)){
		return false;
	}
	if(0 == strcasecmp(ptype, "name")){
		NodeCtrlInfo::chmpxidType	= CHMPXID_SEED_NAME;
	}else if(0 == strcasecmp(ptype, "cuk")){
		NodeCtrlInfo::chmpxidType	= CHMPXID_SEED_CUK;
	}else if(0 == strcasecmp(ptype, "endpoint")){
		NodeCtrlInfo::chmpxidType	= CHMPXID_SEED_CTLENDPOINTS;
	}else if(0 == strcasecmp(ptype, "seed")){
		NodeCtrlInfo::chmpxidType	= CHMPXID_SEED_CUSTOM;
	}else{
		return false;
	}
	return true;
}

void NodeCtrlInfo::SetType(CHMPXID_SEED_TYPE type)
{
	NodeCtrlInfo::chmpxidType = type;
}

bool NodeCtrlInfo::SetGroup(const char* group)
{
	if(CHMEMPTYSTR(group)){
		return false;
	}
	NodeCtrlInfo::groupName = group;
	return true;
}

//
// Methods
//
NodeCtrlInfo::NodeCtrlInfo(void) : chmpxid(CHM_INVALID_CHMPXID), hostname(""), ctrlport(0), cuk(""), ctlendpoints(""), custom_seed(""), is_server(false)
{
}

NodeCtrlInfo::NodeCtrlInfo(const NodeCtrlInfo& other) : chmpxid(other.chmpxid), hostname(other.hostname), ctrlport(other.ctrlport), cuk(other.cuk), ctlendpoints(other.ctlendpoints), custom_seed(other.custom_seed), is_server(other.is_server)
{
}

NodeCtrlInfo::NodeCtrlInfo(chmpxid_t set_chmpxid, const char* target, short port, const char* pcuk, const char* pendpoints, const char* pseed, bool is_server_mode)
{
	Set(set_chmpxid, target, port, pcuk, pendpoints, pseed, is_server_mode);
}

NodeCtrlInfo::NodeCtrlInfo(const char* target, short port, const char* pcuk, const char* pendpoints, const char* pseed, bool is_server_mode)
{
	Set(target, port, pcuk, pendpoints, pseed, is_server_mode);
}

NodeCtrlInfo::~NodeCtrlInfo(void)
{
}

bool NodeCtrlInfo::Set(chmpxid_t set_chmpxid, const char* target, short port, const char* pcuk, const char* pendpoints, const char* pseed, bool is_server_mode)
{
	chmpxid		= set_chmpxid;
	hostname	= CHMEMPTYSTR(target) ? "" : target;
	ctrlport	= port;
	cuk			= CHMEMPTYSTR(pcuk) ? "" : pcuk;
	ctlendpoints= CHMEMPTYSTR(pendpoints) ? "" : pendpoints;
	custom_seed	= CHMEMPTYSTR(pseed) ? "" : pseed;
	is_server	= is_server_mode;

	return true;
}

bool NodeCtrlInfo::Set(const char* target, short port, const char* pcuk, const char* pendpoints, const char* pseed, bool is_server_mode)
{
	if(CHMEMPTYSTR(target)){
		chmpxid		= CHM_INVALID_CHMPXID;
		ctrlport	= 0;
		is_server	= false;
		hostname.erase();
		cuk.erase();
		ctlendpoints.erase();
		custom_seed.erase();
	}else{
		hostname	= target;
		ctrlport	= port;
		cuk			= CHMEMPTYSTR(pcuk) ? "" : pcuk;
		ctlendpoints= CHMEMPTYSTR(pendpoints) ? "" : pendpoints;
		custom_seed	= CHMEMPTYSTR(pseed) ? "" : pseed;
		is_server	= is_server_mode;

		// chmpxid with initial parameters.
		chmpxid		= MakeChmpxId(NodeCtrlInfo::groupName.c_str(), NodeCtrlInfo::chmpxidType, hostname.c_str(), ctrlport, cuk.c_str(), ctlendpoints.c_str(), custom_seed.c_str());
	}
	return true;
}

bool NodeCtrlInfo::GetMaximumChmpxidList(chmpxidlist_t& chmpxidlist) const
{
	chmpxidlist.clear();
	if(CHM_INVALID_CHMPXID != chmpxid){
		chmpxidlist.push_back(chmpxid);
	}

	strlst_t::iterator	siter;
	strlst_t			hostnames;
	if(HostnameMap::GetHostnames(hostname.c_str(), hostnames) && !hostnames.empty()){
		for(siter = hostnames.begin(); siter != hostnames.end(); ++siter){
			chmpxid_t	tmpchmpxid = MakeChmpxId(NodeCtrlInfo::groupName.c_str(), NodeCtrlInfo::chmpxidType, siter->c_str(), ctrlport, cuk.c_str(), ctlendpoints.c_str(), custom_seed.c_str());
			if(CHM_INVALID_CHMPXID != tmpchmpxid){
				bool	found = false;
				for(chmpxidlist_t::const_iterator iter = chmpxidlist.begin(); iter != chmpxidlist.end(); ++iter){
					// cppcheck-suppress unmatchedSuppression
					// cppcheck-suppress useStlAlgorithm
					if(*iter == tmpchmpxid){
						found = true;
						break;
					}
				}
				if(!found){
					chmpxidlist.push_back(tmpchmpxid);
				}
			}
		}
	}
	return true;
}

int NodeCtrlInfo::compare(const NodeCtrlInfo& other) const
{
	// [NOTE]
	// Not care for server/slave mode
	//
	chmpxidlist_t	baselist;
	chmpxidlist_t	otherlist;

	// get both all chmpxid
	GetMaximumChmpxidList(baselist);
	other.GetMaximumChmpxidList(otherlist);

	// compare chmpxid
	for(chmpxidlist_t::const_iterator biter = baselist.begin(); biter != baselist.end(); ++biter){
		for(chmpxidlist_t::const_iterator oiter = otherlist.begin(); oiter != otherlist.end(); ++oiter){
			// cppcheck-suppress unmatchedSuppression
			// cppcheck-suppress useStlAlgorithm
			if((*biter) == (*oiter)){
				// found
				return 0;
			}
		}
	}
	if(CHM_INVALID_CHMPXID != chmpxid && CHM_INVALID_CHMPXID != other.chmpxid){
		return (chmpxid < other.chmpxid ? 1 : -1);
	}else if(CHM_INVALID_CHMPXID != chmpxid){
		return 1;
	}else if(CHM_INVALID_CHMPXID != other.chmpxid){
		return -1;
	}

	int result;
	if(0 == (result = strcmp(hostname.c_str(), other.hostname.c_str()))){
		if(ctrlport == other.ctrlport){
			if(0 == (result = strcmp(cuk.c_str(), other.cuk.c_str()))){
				if(0 == (result = strcmp(ctlendpoints.c_str(), other.ctlendpoints.c_str()))){
					result = strcmp(custom_seed.c_str(), other.custom_seed.c_str());
				}
			}
		}else if(ctrlport < other.ctrlport){
			result = 1;
		}else{	// ctrlport > other.ctrlport
			result = -1;
		}
	}
	return result;
}

bool NodeCtrlInfo::IsConfirmHostname(void) const
{
	strlst_t	hostnames;
	if(!HostnameMap::GetHostnames(hostname.c_str(), hostnames, true) || hostnames.empty()){
		return false;
	}
	for(strlst_t::iterator siter = hostnames.begin(); siter != hostnames.end(); ++siter){
		// cppcheck-suppress unmatchedSuppression
		// cppcheck-suppress useStlAlgorithm
		if((*siter) == hostname){
			// found hostname in confirm hostname list
			return true;
		}
	}
	return false;
}

bool NodeCtrlInfo::UpdateConfirmHostname(void)
{
	if(NodeCtrlInfo::IsConfirmHostname()){
		// nothing to do
		return true;
	}
	strlst_t	hostnames;
	if(!HostnameMap::GetHostnames(hostname.c_str(), hostnames, true) || hostnames.empty()){
		return false;
	}
	hostname = hostnames.front();
	return true;
}

//---------------------------------------------------------
// Chmpx Node List
//---------------------------------------------------------
typedef std::list<NodeCtrlInfo>		nodectrllist_t;

struct node_ctrl_info_sort
{
	bool operator()(const NodeCtrlInfo& lnodectrlinfo, const NodeCtrlInfo& rnodectrlinfo) const
    {
		return (0 <= lnodectrlinfo.compare(rnodectrlinfo));
    }
};

struct node_ctrl_info_same
{
	bool operator()(const NodeCtrlInfo& lnodectrlinfo, const NodeCtrlInfo& rnodectrlinfo) const
    {
		return (0 == lnodectrlinfo.compare(rnodectrlinfo));
    }
};

static bool nodectrllist_uniqsort(nodectrllist_t& nodes)
{
	// sort
	nodes.sort(node_ctrl_info_sort());

	// uniq by chmpxid with confirmed_hostname
	for(nodectrllist_t::iterator iter = nodes.begin(); iter != nodes.end(); ){
		nodectrllist_t::iterator cur_iter = iter;
		++iter;										// next iter
		if(iter != nodes.end()){
			if((*cur_iter) == (*iter)){
				if(cur_iter->IsConfirmHostname()){
					// leave cur_iter and remove iter
					nodes.erase(iter);
					iter = cur_iter;
				}else if(iter->IsConfirmHostname()){
					// leave iter and remove cur_iter
					iter = nodes.erase(cur_iter);
				}else{
					// [RETRY]
					// update hostname both
					cur_iter->UpdateConfirmHostname();
					iter->UpdateConfirmHostname();

					if(cur_iter->IsConfirmHostname()){
						// leave cur_iter and remove iter
						nodes.erase(iter);
						iter = cur_iter;
					}else if(iter->IsConfirmHostname()){
						// leave iter and remove cur_iter
						iter = nodes.erase(cur_iter);
					}else{
						// leave cur_iter and remove iter
						nodes.erase(iter);
						iter = cur_iter;
					}
				}
			}
		}
	}
	return true;
}

static bool load_initial_chmpx_nodes(nodectrllist_t& nodes, const string& strConfig, short port, const char* cuk)
{
	// Check configuration file(string) options without env
	if(strConfig.empty()){
		ERR("configuration file or string is empty.");
		return false;
	}
	CHMCONFTYPE	conftype = check_chmconf_type(strConfig.c_str());
	if(CHMCONF_TYPE_UNKNOWN == conftype || CHMCONF_TYPE_NULL == conftype){
		ERR("configuration file or string is something wrong, you can check it by \"chmpxconftest\" tool.");
		return false;
	}

	// clear node information
	nodes.clear();

	// Attach SHM
	ChmCntrl		chmobj;
	NodeCtrlInfo	newnode;
	if(chmobj.OnlyAttachInitialize(strConfig.c_str(), port, cuk)){
		MSG("Attached local chmpx shared memory, then loading all chmpx information from local SHM.");

		// Get chmpx nodes information from SHM
		PCHMINFOEX	pInfo = chmobj.DupAllChmInfo();
		if(!pInfo){
			ERR("Something error occurred in getting chmpx nodes information from SHM.");
			return false;
		}
		if(!pInfo->pchminfo){
			ERR("Something error occurred in getting chmpx nodes information from SHM.");
			ChmCntrl::FreeDupAllChmInfo(pInfo);
			return false;
		}

		// Backup for type/group
		NodeCtrlInfo::SetType(pInfo->pchminfo->chmpxid_type);
		NodeCtrlInfo::SetGroup(pInfo->pchminfo->chmpx_man.group);

		//--------------------------------------
		// extract node information from SHM
		//--------------------------------------
		int				counter;
		PCHMPXLIST		pchmpxlist;
		// first, get self node information.
		// because if slave node is up without servers, we get node information only self structure.
		//
		if(pInfo->pchminfo->chmpx_man.chmpx_self){
			HostnameMap::UpdateHostnameConfirm(pInfo->pchminfo->chmpx_man.chmpx_self->chmpx.name);

			newnode.Set(pInfo->pchminfo->chmpx_man.chmpx_self->chmpx.chmpxid,
						pInfo->pchminfo->chmpx_man.chmpx_self->chmpx.name,
						pInfo->pchminfo->chmpx_man.chmpx_self->chmpx.ctlport,
						pInfo->pchminfo->chmpx_man.chmpx_self->chmpx.cuk,
						get_hostport_pairs_string(pInfo->pchminfo->chmpx_man.chmpx_self->chmpx.ctlendpoints, EXTERNAL_EP_MAX).c_str(),
						pInfo->pchminfo->chmpx_man.chmpx_self->chmpx.custom_seed,
						(CHMPX_SERVER == pInfo->pchminfo->chmpx_man.chmpx_self->chmpx.mode));
			nodes.push_back(newnode);
		}
		// loop to get all server nodes
		for(counter = 0, pchmpxlist = pInfo->pchminfo->chmpx_man.chmpx_servers; pchmpxlist; pchmpxlist = pchmpxlist->next, ++counter){
			HostnameMap::UpdateHostnameConfirm(pchmpxlist->chmpx.name);

			newnode.Set(pchmpxlist->chmpx.chmpxid,
						pchmpxlist->chmpx.name,
						pchmpxlist->chmpx.ctlport,
						pchmpxlist->chmpx.cuk,
						get_hostport_pairs_string(pchmpxlist->chmpx.ctlendpoints, EXTERNAL_EP_MAX).c_str(),
						pchmpxlist->chmpx.custom_seed,
						true);
			nodes.push_back(newnode);
		}
		// loop to get all slave nodes
		for(counter = 0, pchmpxlist = pInfo->pchminfo->chmpx_man.chmpx_slaves; pchmpxlist; pchmpxlist = pchmpxlist->next, ++counter){
			HostnameMap::UpdateHostnameConfirm(pchmpxlist->chmpx.name);

			newnode.Set(pchmpxlist->chmpx.chmpxid,
						pchmpxlist->chmpx.name,
						pchmpxlist->chmpx.ctlport,
						pchmpxlist->chmpx.cuk,
						get_hostport_pairs_string(pchmpxlist->chmpx.ctlendpoints, EXTERNAL_EP_MAX).c_str(),
						pchmpxlist->chmpx.custom_seed,
						false);
			nodes.push_back(newnode);
		}

		ChmCntrl::FreeDupAllChmInfo(pInfo);

	}else{
		MSG("Could not attach local chmpx shared memory, then loading all chmpx information from configuration.");

		// Load configuration without env
		CHMConf*	pConfObj;
		if(NULL == (pConfObj = CHMConf::GetCHMConf(CHM_INVALID_HANDLE, NULL, strConfig.c_str(), port, cuk, false, NULL))){
			ERR_CHMPRN("Failed to make configuration object from configuration(%s)", strConfig.c_str());
			PRN("You can see detail about error, execute this program with \"-d\"(\"-g\") option.");
			return false;
		}
		CHMCFGINFO	chmcfg;
		if(!pConfObj->GetConfiguration(chmcfg, true)){
			ERR("Something error occurred in getting chmpx nodes information from configuration(%s).", strConfig.c_str());
			pConfObj->Clean();
			CHM_Delete(pConfObj);
			return false;
		}

		// Backup for type/group
		NodeCtrlInfo::SetType(chmcfg.chmpxid_type);
		NodeCtrlInfo::SetGroup(chmcfg.groupname.c_str());

		//--------------------------------------
		// extract node information from configuration
		//--------------------------------------
		chmnode_cfginfos_t::const_iterator	iter;

		// [NOTE]
		// At this point the hostname is still untrustworthy.
		// (May not be registered in DNS)
		//

		// loop to get all server nodes
		for(iter = chmcfg.servers.begin(); iter != chmcfg.servers.end(); ++iter){
			HostnameMap::UpdateHostnameConfirm(iter->name.c_str());

			newnode.Set(iter->name.c_str(),
						iter->ctlport,
						iter->cuk.c_str(),
						get_hostports_string(iter->ctlendpoints).c_str(),
						iter->custom_seed.c_str(),
						true);
			nodes.push_back(newnode);
		}
		// loop to get all slave nodes
		for(iter = chmcfg.slaves.begin(); iter != chmcfg.slaves.end(); ++iter){
			HostnameMap::UpdateHostnameConfirm(iter->name.c_str());

			newnode.Set(iter->name.c_str(),
						iter->ctlport,
						iter->cuk.c_str(),
						get_hostports_string(iter->ctlendpoints).c_str(),
						iter->custom_seed.c_str(),
						false);
			nodes.push_back(newnode);
		}
		pConfObj->Clean();
		CHM_Delete(pConfObj);
	}

	// uniq & sort(by chmpxid)
	nodectrllist_uniqsort(nodes);

	return true;
}

static bool add_chmpx_node(nodectrllist_t& nodes, chmpxid_t chmpxid, const string& host, short port, const string& cuk, const string& ctlendpoints, const string& custom_seed, bool is_server = false, bool is_clear = false)
{
	if(is_clear){
		nodes.clear();
	}
	HostnameMap::UpdateHostnameConfirm(host.c_str());

	size_t			nodes_count	= nodes.size();
	NodeCtrlInfo	newnode;
	if(CHM_INVALID_CHMPXID != chmpxid){
		newnode.Set(chmpxid,
					host.c_str(),
					port,
					cuk.c_str(),
					ctlendpoints.c_str(),
					custom_seed.c_str(),
					is_server);
	}else{
		newnode.Set(host.c_str(),
					port,
					cuk.c_str(),
					ctlendpoints.c_str(),
					custom_seed.c_str(),
					is_server);
	}
	nodes.push_back(newnode);

	// uniq & sort(by chmpxid)
	nodectrllist_uniqsort(nodes);

	if(nodes_count == nodes.size()){
		//MSG("%s:%d chmpx node is already in chmpx node list.", host.c_str(), port);
		return true;								// result is success
	}
	return true;
}

static size_t get_chmpx_nodes_count(const nodectrllist_t& nodes, bool is_server)
{
	size_t	rescnt = 0;
	for(nodectrllist_t::const_iterator iter = nodes.begin(); iter != nodes.end(); ++iter){
		if(is_server == iter->IsServerNode()){
			// cppcheck-suppress unmatchedSuppression
			// cppcheck-suppress useStlAlgorithm
			++rescnt;
		}
	}
	return rescnt;
}

static size_t get_chmpx_nodes(const nodectrllist_t& nodes, nodectrllist_t& tgnodes, bool is_server)
{
	tgnodes.clear();
	for(nodectrllist_t::const_iterator iter = nodes.begin(); iter != nodes.end(); ++iter){
		if(is_server == iter->IsServerNode()){
			add_chmpx_node(tgnodes, iter->GetChmpxId(), iter->GetHostname(), iter->GetCtrlport(), iter->GetCuk(), iter->GetCtlendpoints(), iter->GetCusomSeed(), is_server, false);
		}
	}
	return tgnodes.size();
}

/*
* [NOTE]
* This function is not used now.
*
static bool find_chmpx_node_by_hostname(const nodectrllist_t& nodes, chmpxid_t chmpxid, NodeCtrlInfo& node)
{
	for(nodectrllist_t::const_iterator iter = nodes.begin(); iter != nodes.end(); ++iter){
		if(chmpxid == iter->GetChmpxId()){
			node = *iter;
			return true;
		}
	}
	return false;
}
*/

/*
* [NOTE]
* This function is not used now.
*
static bool find_chmpx_node_by_hostname(const nodectrllist_t& nodes, const char* strchmpxid, NodeCtrlInfo& node)
{
	if(CHMEMPTYSTR(strchmpxid)){
		return false;
	}
	chmpxid_t	chmpxid = cvt_string_to_number(strchmpxid, false);

	return find_chmpx_node_by_hostname(nodes, chmpxid, node);
}
*/

static bool find_chmpx_node_by_hostname(const nodectrllist_t& nodes, const string& hostname, short port, const string& cuk, const string& ctlendpoints, const string& custom_seed, NodeCtrlInfo& node)
{
	node.Set(hostname.c_str(),
			port,
			cuk.c_str(),
			ctlendpoints.c_str(),
			custom_seed.c_str(),
			true);				// do not care this value, because compare does not look this.

	for(nodectrllist_t::const_iterator iter = nodes.begin(); iter != nodes.end(); ++iter){
		// cppcheck-suppress unmatchedSuppression
		// cppcheck-suppress useStlAlgorithm
		if(0 == iter->compare(node)){
			// found
			node = *iter;
			return true;
		}
	}
	return false;
}

static void print_chmpx_nodes_by_type(const nodectrllist_t& nodes, bool is_server)
{
	int	prncnt = 0;
	for(nodectrllist_t::const_iterator iter = nodes.begin(); iter != nodes.end(); ++iter){
		if(is_server == iter->IsServerNode()){
			PRN("    [%d] = {",										prncnt);
			PRN("        Chmpxid                 : 0x%016" PRIx64,	iter->GetChmpxId());
			PRN("        Hostname                : %s",				iter->GetHostname().c_str());
			PRN("        Control Port            : %d",				iter->GetCtrlport());
			PRN("        CUK                     : %s",				iter->GetCuk().c_str());
			PRN("        Control Endpoints       : %s",				iter->GetCtlendpoints().c_str());
			PRN("        Custom ID Seed          : %s",				iter->GetCusomSeed().c_str());
			PRN("    }");
			++prncnt;
		}
	}
}

static void print_chmpx_all_nodes(const nodectrllist_t& nodes)
{
	size_t	svrcnt = get_chmpx_nodes_count(nodes, true);
	size_t	slvcnt = get_chmpx_nodes_count(nodes, false);

	PRN(" Chmpx server nodes             : %zd", svrcnt);
	if(0 < svrcnt){
		PRN(" {");
		print_chmpx_nodes_by_type(nodes, true);
		PRN(" }");
	}
	PRN(" Chmpx slave nodes              : %zd", slvcnt);
	if(0 < slvcnt){
		PRN(" {");
		print_chmpx_nodes_by_type(nodes, false);
		PRN(" }");
	}
}

//---------------------------------------------------------
// Common Variables
//---------------------------------------------------------
// [NOTE]
// These lists are listed with the server node first.
// That is, they are pre-sorted by node_ctrl_info_sort.
// Nodes with the same host name, control port, etc. do not exist as servers and
// slaves from the command line, but even if there are duplicates, it is safe to
// detect the server node first.
//
static nodectrllist_t		InitialAllNodes;			// all chmpx node information at initializing
static nodectrllist_t		TargetNodes;				// target all chmpx nodes as dynamically

//---------------------------------------------------------
// Typedefs for Command option
//---------------------------------------------------------
typedef strarr_t						params_t;
typedef map<string, params_t>			option_t;

typedef struct option_type{
	const char*	option;
	const char*	norm_option;
	int			min_param_cnt;
	int			max_param_cnt;
}OPTTYPE, *POPTTYPE;

typedef	const struct option_type		*CPOPTTYPE;
typedef	const char						*const_pchar;

//---------------------------------------------------------
// Class LapTime
//---------------------------------------------------------
class LapTime
{
	private:
		static bool	isEnable;

	private:
		static bool Set(bool enable);

		struct timeval	start;

	public:
		static bool Toggle(void) { return Set(!LapTime::isEnable); }
		static bool Enable(void) { return Set(true); }
		static bool Disable(void) { return Set(false); }
		static bool IsEnable(void) { return isEnable; }

		LapTime();
		virtual ~LapTime();
};

bool LapTime::isEnable = false;

bool LapTime::Set(bool enable)
{
	bool	old = LapTime::isEnable;
	LapTime::isEnable = enable;
	return old;
}

LapTime::LapTime()
{
	memset(&start, 0, sizeof(struct timeval));
	gettimeofday(&start, NULL);
}

LapTime::~LapTime()
{
	if(LapTime::isEnable){
		struct timeval	end;
		struct timeval	lap;

		memset(&end, 0, sizeof(struct timeval));
		gettimeofday(&end, NULL);

		memset(&lap, 0, sizeof(struct timeval));
		timersub(&end, &start, &lap);

		time_t	hour, min, sec, msec, usec;

		sec	 = lap.tv_sec % 60;
		min	 = (lap.tv_sec / 60) % 60;
		hour = (lap.tv_sec / 60) / 60;
		msec = lap.tv_usec / 1000;
		usec = lap.tv_usec % 1000;

		PRN(NULL);
		PRN("Lap time: %jdh %jdm %jds %jdms %jdus(%jds %jdus)\n",
			static_cast<intmax_t>(hour),
			static_cast<intmax_t>(min),
			static_cast<intmax_t>(sec),
			static_cast<intmax_t>(msec),
			static_cast<intmax_t>(usec),
			static_cast<intmax_t>(lap.tv_sec),
			static_cast<intmax_t>(lap.tv_usec));
	}
}

//---------------------------------------------------------
// Utility : Signal
//---------------------------------------------------------
static bool	IsBreakLoop	= false;

//
// Signal for blocking signal
//
static bool BlockSignal(int sig)
{
	sigset_t	blockmask;
	sigemptyset(&blockmask);
	sigaddset(&blockmask, sig);
	if(-1 == sigprocmask(SIG_BLOCK, &blockmask, NULL)){
		ERR("Could not block signal(%d) by errno(%d)", sig, errno);
		return false;
	}
	return true;
}

//
// SIGINT handler
//
static void SigIntHandler(int sig)
{
	if(SIGINT != sig){
		return;
	}
	IsBreakLoop = true;
}

static bool SetSigIntHandler(void)
{
	// set signal handler
	struct sigaction	sa;
	sigemptyset(&sa.sa_mask);
	sigaddset(&sa.sa_mask, SIGINT);
	sa.sa_flags		= 0;
	sa.sa_handler	= SigIntHandler;
	if(-1 == sigaction(SIGINT, &sa, NULL)){
		ERR("Could not set signal SIGINT handler. errno = %d", errno);
		return false;
	}
	return true;
}

//---------------------------------------------------------
// Class ConsoleInput
//---------------------------------------------------------
class ConsoleInput
{
	protected:
		static const int	DEFAULT_HISTORY_MAX	= 500;

		size_t				history_max;
		string				prompt;
		strarr_t			history;
		ssize_t				history_pos;
		string				input;
		size_t				input_pos;	// == cursor pos
		struct termios		tty_backup;
		bool				is_set_terminal;
		int					last_errno;

	protected:
		bool SetTerminal(void);
		bool UnsetTerminal(void);
		bool ReadByte(char& cInput);
		void ClearInput(void);
		void ClearLine(void);

	public:
		size_t SetMax(size_t max);
		size_t GetMax(void) const { return history_max; }
		bool SetPrompt(const char* pprompt);
		const char* GetPrompt(void) const { return prompt.c_str(); }

		ConsoleInput();
		virtual ~ConsoleInput();

		bool Clean(void);
		bool GetCommand(void);
		bool PutHistory(const char* pCommand);
		bool RemoveLastHistory(void);
		int LastErrno(void) const { return last_errno; }
		const string& str(void) const { return input; }
		const char* c_str(void) const { return input.c_str(); }
		const strarr_t& GetAllHistory(void) const { return history; }
};

//
// Class ConsoleInput::Methods
//
ConsoleInput::ConsoleInput() : history_max(DEFAULT_HISTORY_MAX), prompt("PROMPT> "), history_pos(-1L), input(""), input_pos(0UL), is_set_terminal(false), last_errno(0)
{
}

ConsoleInput::~ConsoleInput()
{
	UnsetTerminal();
	Clean();
}

bool ConsoleInput::Clean(void)
{
	history.clear();
	prompt.clear();
	input.clear();
	return true;
}

size_t ConsoleInput::SetMax(size_t max)
{
	size_t	old = history_max;
	if(0 != max){
		history_max = max;
	}
	return old;
}

bool ConsoleInput::SetPrompt(const char* pprompt)
{
	if(CHMEMPTYSTR(pprompt)){
		return false;
	}
	prompt = pprompt;
	return true;
}

bool ConsoleInput::SetTerminal(void)
{
	if(is_set_terminal){
		// already set
		return true;
	}

	struct termios tty_change;

	// backup
	tcgetattr(0, &tty_backup);
	tty_change				= tty_backup;
	tty_change.c_lflag		&= ~(ECHO | ICANON);
	tty_change.c_cc[VMIN]	= 0;
	tty_change.c_cc[VTIME]	= 1;

	// set
	tcsetattr(0, TCSAFLUSH, &tty_change);
	is_set_terminal = true;

	return true;
}

bool ConsoleInput::UnsetTerminal(void)
{
	if(!is_set_terminal){
		// already unset
		return true;
	}

	// unset
	tcsetattr(0, TCSAFLUSH, &tty_backup);
	is_set_terminal = false;

	return true;
}

//
// If error occurred, return 0x00
//
bool ConsoleInput::ReadByte(char& cInput)
{
	cInput = '\0';
	if(-1 == read(0, &cInput, sizeof(char))){
		last_errno = errno;
		return false;
	}
	last_errno = 0;
	return true;
}

void ConsoleInput::ClearInput(void)
{
	history_pos	= -1L;
	input_pos	= 0UL;
	last_errno	= 0;
	input.erase();
}

void ConsoleInput::ClearLine(void)
{
	for(size_t Count = 0; Count < input_pos; Count++){		// cursor to head
		putchar('\x08');
	}
	for(size_t Count = 0; Count < input.length(); Count++){	// clear by space
		putchar(' ');
	}
	for(size_t Count = 0; Count < input.length(); Count++){	// rewind cursor to head
		putchar('\x08');
	}
	fflush(stdout);
}

// 
// [Input key value]
//	0x1b 0x5b 0x41			Up
//	0x1b 0x5b 0x42			Down
//	0x1b 0x5b 0x43			Right
//	0x1b 0x5b 0x44			Left
//	0x7f					Delete
//	0x08					backSpace
//	0x01					CTRL-A
//	0x05					CTRL-E
//	0x1b 0x5b 0x31 0x7e		HOME
//	0x1b 0x5b 0x34 0x7e		END
// 
bool ConsoleInput::GetCommand(void)
{
	ClearInput();
	SetTerminal();

	// prompt
	printf("%s", ConsoleInput::prompt.c_str());
	fflush(stdout);

	char	input_char;
	while(!IsBreakLoop){
		// read one character
		if(!ReadByte(input_char)){
			if(EINTR == last_errno){
				last_errno = 0;
				continue;
			}
			break;
		}
		if('\n' == input_char){
			// finish input one line
			putchar('\n');
			fflush(stdout);
			PutHistory(input.c_str());
			break;

		}else if('\x1b' == input_char){
			// escape character --> next byte read
			if(!ReadByte(input_char)){
				break;
			}
			if('\x5b' == input_char){
				// read more character
				if(!ReadByte(input_char)){
					break;
				}
				if('\x41' == input_char){
					// Up key
					if(0 != history_pos && 0 < history.size()){
						ClearLine();	// line clear

						if(-1L == history_pos){
							history_pos = static_cast<ssize_t>(history.size() - 1UL);
						}else if(0 != history_pos){
							history_pos--;
						}
						input = history[history_pos];

						for(input_pos = 0UL; input_pos < input.length(); input_pos++){
							putchar(input[input_pos]);
						}
						fflush(stdout);
					}

				}else if('\x42' == input_char){
					// Down key
					if(-1L != history_pos && static_cast<size_t>(history_pos) < history.size()){
						ClearLine();	// line clear

						if(history.size() <= static_cast<size_t>(history_pos) + 1UL){
							history_pos = -1L;
							input.erase();
							input_pos = 0UL;
						}else{
							history_pos++;
							input = history[history_pos];
							input_pos = input.length();

							for(input_pos = 0UL; input_pos < input.length(); input_pos++){
								putchar(input[input_pos]);
							}
							fflush(stdout);
						}
					}

				}else if('\x43' == input_char){
					// Right key
					if(input_pos < input.length()){
						putchar(input[input_pos]);
						fflush(stdout);
						input_pos++;
					}

				}else if('\x44' == input_char){
					// Left key
					if(0 < input_pos){
						input_pos--;
						putchar('\x08');
						fflush(stdout);
					}

				}else if('\x31' == input_char){
					// read more character
					if(!ReadByte(input_char)){
						break;
					}
					if('\x7e' == input_char){
						// Home key
						for(size_t Count = 0; Count < input_pos; Count++){
							putchar('\x08');
						}
						input_pos = 0UL;
						fflush(stdout);
					}

				}else if('\x34' == input_char){
					// read more character
					if(!ReadByte(input_char)){
						break;
					}
					if('\x7e' == input_char){
						// End key
						for(size_t Count = input_pos; Count < input.length(); Count++){
							putchar(input[Count]);
						}
						input_pos = input.length();
						fflush(stdout);
					}

				}else if('\x33' == input_char){
					// read more character
					if(!ReadByte(input_char)){
						break;
					}
					if('\x7e' == input_char){
						// BackSpace key on OSX
						if(0 < input_pos){
							input.erase((input_pos - 1), 1);
							input_pos--;
							putchar('\x08');
							for(size_t Count = input_pos; Count < input.length(); Count++){
								putchar(input[Count]);
							}
							putchar(' ');
							for(size_t Count = input_pos; Count < input.length(); Count++){
								putchar('\x08');
							}
							putchar('\x08');
							fflush(stdout);
						}
					}
				}
			}

		}else if('\x7f' == input_char){
			// Delete
			if(0 < input.length()){
				input.erase(input_pos, 1);

				for(size_t Count = input_pos; Count < input.length(); Count++){
					putchar(input[Count]);
				}
				putchar(' ');
				for(size_t Count = input_pos; Count < input.length(); Count++){
					putchar('\x08');
				}
				putchar('\x08');
				fflush(stdout);
			}

		}else if('\x08' == input_char){
			// BackSpace
			if(0 < input_pos){
				input.erase((input_pos - 1), 1);
				input_pos--;
				putchar('\x08');
				for(size_t Count = input_pos; Count < input.length(); Count++){
					putchar(input[Count]);
				}
				putchar(' ');
				for(size_t Count = input_pos; Count < input.length(); Count++){
					putchar('\x08');
				}
				putchar('\x08');
				fflush(stdout);
			}

		}else if('\x01' == input_char){
			// ctrl-A
			for(size_t Count = 0; Count < input_pos; Count++){
				putchar('\x08');
			}
			input_pos = 0;
			fflush(stdout);

		}else if('\x05' == input_char){
			// ctrl-E
			for(size_t Count = input_pos; Count < input.length(); Count++){
				putchar(input[Count]);
			}
			input_pos = input.length();
			fflush(stdout);

		}else if(isprint(input_char)){
			// normal character
			input.insert(input_pos, 1, input_char);
			for(size_t Count = input_pos; Count < input.length(); Count++){
				putchar(input[Count]);
			}
			input_pos++;
			for(size_t Count = input_pos; Count < input.length(); Count++){
				putchar('\x08');
			}
			fflush(stdout);
		}
	}
	UnsetTerminal();

	if(0 != last_errno){
		return false;
	}
	return true;
}

bool ConsoleInput::PutHistory(const char* pCommand)
{
	if(CHMEMPTYSTR(pCommand)){
		return false;
	}
	history.push_back(string(pCommand));
	if(ConsoleInput::history_max < history.size()){
		history.erase(history.begin());
	}
	return true;
}

bool ConsoleInput::RemoveLastHistory(void)
{
	if(0 < history.size()){
		history.pop_back();
	}
	return true;
}

//---------------------------------------------------------
// Utilities: Help
//---------------------------------------------------------
// 
// -help(h)                         help display
// -conf <filename>                 chmpx configuration file path(.ini .yaml .json) when run on chmpx node host
// -json <string>                   chmpx configuration by json string when run on chmpx node host
// -group <groupname>               group name for chmpx cluster, a required option if you do not specify a conf file
// -host <hostname>                 hostname for chmpx node, if not specified, using localhost
// -type <...>                      specify CHMPXIDTYPE: name(default), cuk, endpoint, seed
// -ctrlport <port>                 chmpx node control port, if host option is specified, this option can be be specified.
// -cuk <cuk>                       chmpx node cuk, if host option is specified, this option can be specified.
// -ctlendpoints <host:port,...>    chmpx node ctlendpoints, if host option is specified, this option can be specified.
// -custom_seed <seed>              chmpx node custom_seed, if host option is specified, this option can be specified.
// -server                          chmpx node server type for hostname specified
// -slave                           chmpx node server type for hostname specified(default)
// -threadcnt <count>               thread count for DUMP command(default is 0)
// -check <second>                  check and print all nodes status/hash/socket connection count after startup.
// -status <second>                 print all nodes status by SELFSTATUS or ALLSTATUS after startup.
// -nocolor                         common option, print without no escape sequence(no color)
// -lap                             common option, print lap time after line command
// -d <debug level>                 common option, print debugging message mode: SILENT(SLT)/ERROR(ERR)/WARNING(WAN)/INFO(MSG)
// -dchmpx                          common option, print debugging message from chmpx library when valid -d option is specified.
// -his <count>                     common option, set history count(default 500)
// -run <file path>                 common option, run command(history) file.
// 
static void Help(const char* progname)
{
	PRN("Usage:");
	PRN("       %s -help", progname ? progname : "program");
	PRN("Usage: set environment(%s or %s)", CHM_CONFFILE_ENV_NAME, CHM_JSONCONF_ENV_NAME);
	PRN("       %s", progname ? progname : "program");
	PRN("Usage: specify configuration file path");
	PRN("       %s -conf <file> [-ctrlport <port>] [-cuk <cuk>] [-ctlendpoints <host:port,...>] [-custom_seed <seed>] [options...]", progname ? progname : "program");
	PRN("Usage: specify json configuration string");
	PRN("       %s -json <string> [-ctrlport <port>] [-cuk <cuk>] [-ctlendpoints <host:port,...>] [-custom_seed <seed>] [options...]", progname ? progname : "program");
	PRN("Usage: specify hostname and port");
	PRN("       %s -group <groupname> -host <hostname> [-type <...>] [-ctrlport <port>] [-cuk <cuk>] [-ctlendpoints <host:port,...>] [-custom_seed <seed>] {-server | -slave} [options...]", progname ? progname : "program");
	PRN(NULL);
	PRN("Options:");
	PRN("  -help(h)                      help display");
	PRN("  -conf <filename>              chmpx configuration file path(.ini .yaml .json) when run on chmpx node host");
	PRN("  -json <string>                chmpx configuration by json string when run on chmpx node host");
	PRN("  -group <groupname>            group name for chmpx cluster, a required option if you do not specify a conf file.");
	PRN("  -host <hostname>              hostname for chmpx node, if not specified, using localhost");
	PRN("  -type <...>                   specify CHMPXIDTYPE: name(default), cuk, endpoint, seed");
	PRN("  -ctrlport <port>              chmpx node control port, if host option is specified, this option can be be specified.");
	PRN("  -cuk <cuk>                    chmpx node cuk, if host option is specified, this option can be specified.");
	PRN("  -ctlendpoints <host:port,...> chmpx node ctlendpoints, if host option is specified, this option can be specified.");
	PRN("  -custom_seed <seed>           chmpx node custom_seed, if host option is specified, this option can be specified.");
	PRN("  -server                       chmpx node server type for hostname specified");
	PRN("  -slave                        chmpx node server type for hostname specified(default)");
	PRN("  -threadcnt <count>            thread count for DUMP command(default is 0)");
	PRN("  -check <second>               check and print all nodes status/hash/socket connection count after startup.");
	PRN("  -status <second>              print all nodes status by SELFSTATUS or ALLSTATUS after startup.");
	PRN("  -nocolor                      common option, print without no escape sequence(no color)");
	PRN("  -lap                          common option, print lap time after line command");
	PRN("  -d <debug level>              common option, print debugging message mode: SILENT(SLT)/ERROR(ERR)/WARNING(WAN)/INFO(MSG)/DUMP(DMP)");
	PRN("  -dchmpx                       common option, print debugging message from chmpx library when valid -d option is specified.");
	PRN("  -his <count>                  common option, set history count(default 500)");
	PRN("  -run <file path>              common option, run command(history) file.");
	PRN("Environments:");
	PRN("  %s        can use configuration file path if -conf/-json/-host is not specified.", CHM_CONFFILE_ENV_NAME);
	PRN("  %s        can use json configuration string  if -conf/-json/-host is not specified..", CHM_JSONCONF_ENV_NAME);
	PRN(NULL);
}

// 
// Command: [command] [parameters...]
// 
// help(h)                              print help
// quit(q)/exit                         quit
// update                               update dynamic target chmpx nodes
// nodes [nodyna | noupdate] [server | slave]
//                                      print all/server/slave chmpx nodes.
//                                      if noupdate parameter is specified, do not update before doing.
//                                      if nodyna parameter is specified, only initially chmpx nodes.
// status [self | all] [host(*1)]       print target node status by SELFSTATUS or ALLSTATUS
//                                      if tool runs with host option, target node is specified host.
//                                      if tool runs with conf option, must specify host and control
//                                      port in nodes list.
//                                      self option means printing result of SELFSTATUS control command
//                                      to node.
//                                      all means ALLSTATUS command.
// check [noupdate] [all | host(*1)]    check and print all nodes status/hash/socket connection count.
//                                      if host and control port is specified, check only that host
//                                      and print target node status/etc which are looked by other nodes.
//                                      if noupdate parameter is specified, do not update before doing.
// statusupdate [noupdate] [all | host(*1)]
//                                      push status of all/one node(s) to other nodes.
//                                      if host and control port is specified, push only that host. 
//                                      if noupdate parameter is specified, do not update before doing.
// servicein [noupdate] [host(*1)]      service in node to RING by SERVICEIN
//                                      if tool runs with host option, target node is specified host.
//										if noupdate parameter is specified, do not update before doing.
// serviceout [noupdate] [host(*1)] {[target host(*1)]}
//									     service out node to RING by SERVICEOUT
//                                      if tool runs with host option, target node is specified host.
//										if noupdate parameter is specified, do not update before doing.
// merge [noupdate] [start | abort | complete]
//                                      control merging, start/stop/complete(finish) to RING by MERGE/
//                                      ABORTMERGE/COMPMERGE
// suspend [noupdate]                   suspend auto merging to RING by SUSPENDMERGE
// nosuspend [noupdate]                 not suspend auto merging to RING by NOSUSPENDMERGE
// dump [noupdate] [host(*1)]           print target node all information by DUMP
//                                      if tool runs with host option, target node is specified host.
//                                      if tool runs with conf option, must specify host and control
//                                      port in nodes list.
//										if noupdate parameter is specified, do not update before doing.
// version [nodyna | noupdate]          print all chmpx nodes version.
//                                      if noupdate parameter is specified, do not update before doing.
//                                      if nodyna parameter is specified, only initially chmpx nodes.
// loop [second] [loop limit count]     loop command input specified with interval second.
//                                      enter a series of commands to be executed continuously after
//                                      this command.
//                                      to end the input, enter "." character on "CLT LOOP>" prompt.
//                                      after the sequence commands are complete, they are executed
//                                      immediately.
//                                      when a series of command execution is completed, it waits for
//                                      the specified number of seconds and then re-executes them.
//                                      if loop limit count is omitted, it loops until it stops.
// loopcmd [command...]                 command in loop command.
//                                      this command is used to specify successive commands to be
//                                      executed by the loop and can be used only in the command file.
//                                      this can only be specified immediately after the loop command
//                                      and the loopcmd command after the loop command.
// dbglevel [slt | err | wan | msg | dmp]
//                                      bumpup debugging level or specify level
// dchmpx <on | off>                    toggle or enable/disable chmpx debugging message
// history(his)                         display all history, you can use a command line in history by
//                                      "!<number>".
// save <file path>                     save history to file.
// load <file path>                     load and run command file.
// shell                                exit shell(same as "!" command).
// echo <string>...                     echo string
// sleep <second>                       sleep seconds
// 
// (*1) How to specify the target host
//		"hostname(IP address)":"control port"[:"cuk"[:"custom_seed"[:"control endpoints..."]]]
//
static void LineHelp(void)
{
	//------------------------------- printable -----------------------------------//
	PRN(NULL);
	PRN("Command: [command] [parameters...]");
	PRN(NULL);
	PRN("help(h)        print help");
	PRN("quit(q)/exit   quit");
	PRN("update(u)      update dynamic target chmpx nodes");
	PRN("nodes [nodyna | noupdate] [server | slave]");
	PRN("               print all/server/slave chmpx nodes. if noupdate parameter");
	PRN("               is specified, do not update before doing. if nodyna");
	PRN("               parameter is specified, only initially chmpx nodes.");
	PRN("status [self | all] [host(*1)]");
	PRN("               If the \"self\" parameter is specified, the status of the");
	PRN("               target node is returned. This is the same as the SELFSTATUS");
	PRN("               control command.");
	PRN("               If the \"all\" parameter is specified, the server node");
	PRN("               status that the target node knows is output. This is");
	PRN("               equivalent to the ALLSTATUS control command.");
	PRN("               The target node is specified as a parameter after self/all");
	PRN("               parameter. If omitted, this tool must be started with the");
	PRN("               \"-host\" option etc specified.");
	PRN("check [noupdate] [all | host(*1)]");
	PRN("               check and print all nodes status/hash/socket connection");
	PRN("               count. if host and control port is specified, check only");
	PRN("               that host and print target node status/etc which are");
	PRN("               looked by other nodes. if noupdate parameter is");
	PRN("               specified, do not update before doing.");
	PRN("statusupdate [noupdate] [all | host(*1)]");
	PRN("               push status of all/one node(s) to other nodes.");
	PRN("               if host and control port is specified, push only that");
	PRN("               host. if noupdate parameter is specified, do not update");
	PRN("               before doing.");
	PRN("servicein [noupdate] [host(*1)]");
	PRN("               service in node to RING by SERVICEIN.");
	PRN("               if tool runs with host option, target node is specified");
	PRN("               host. if noupdate parameter is specified, do not update");
	PRN("               before doing.");
	PRN("serviceout [noupdate] [host(*1)] {[target host(*1)]}");
	PRN("               service out node to RING by SERVICEOUT.");
	PRN("               if tool runs with host option, target node is specified");
	PRN("               host. if noupdate parameter is specified, do not update");
	PRN("               before doing.");
	PRN("merge [noupdate] [start | abort | complete]");
 	PRN("               control merging, start/stop/complete(finish) to RING by");
	PRN("               MERGE/ABORTMERGE/COMPMERGE.");
	PRN("suspend [noupdate]");
	PRN("               suspend auto merging to RING by SUSPENDMERGE");
	PRN("nosuspend [noupdate]");
	PRN("               not suspend auto merging to RING by NOSUSPENDMERGE");
	PRN("dump [noupdate] [host(*1)]");
	PRN("               print target node all information by DUMP.");
	PRN("               if tool runs with host option, target node is specified");
	PRN("               host. if tool runs with conf option, must specify host");
	PRN("               and control port in nodes list. if noupdate parameter");
	PRN("               is specified, do not update before doing.");
	PRN("version [nodyna | noupdate]");
	PRN("               print all chmpx nodes version. if noupdate parameter is");
	PRN("               specified, do not update before doing. if nodyna");
	PRN("               parameter is specified, only initially chmpx nodes.");
	PRN("loop [second] [loop limit count]");
	PRN("               loop command input specified with interval second");
	PRN("               enter a series of commands to be executed continuously");
	PRN("               after this command. to end the input, enter \".\" character");
	PRN("               on \"CLT LOOP>\" prompt. after the sequence commands are");
	PRN("               complete, they are executed immediately. when a series of");
	PRN("               command execution is completed, it waits for the specified");
	PRN("               number of seconds and then re-executes them. if loop limit");
	PRN("               count is omitted, it loops until it stops.");
	PRN("loopcmd [command...]");
	PRN("               command in loop command.");
	PRN("               this command is used to specify successive commands to be");
	PRN("               executed by the loop and can be used only in the command");
	PRN("               file. this can only be specified immediately after the");
	PRN("               loop command and the loopcmd command after the loop");
	PRN("               command.");
	PRN("dbglevel [slt | err | wan | msg | dmp]");
	PRN("               bumpup debugging level or specify level");
	PRN("dchmpx [on | off]");
	PRN("               toggle or enable/disable chmpx debug msg");
	PRN("history(his)");
	PRN("               display all history, you can use a command linein history");
	PRN("               by \"!<number>\".");
	PRN("save <file path>");
	PRN("               save command history to command file.");
	PRN("load <file path>");
	PRN("               load command from file and run those.");
	PRN("shell          exit shell(same as \"!\" command).");
	PRN("echo <string>  echo specified string(after echo command).");
	PRN("sleep <second> sleep specified(decimal) seconds.");
	PRN("");
	PRN("(*1) How to specify the target host");
	PRN("\"hostname(IP address)\":\"control port\"[:\"cuk\"[:\"custom_seed\"[:\"control endpoints...\"]]]");
	PRN(NULL);
}

//---------------------------------------------------------
// Utilities: Command Parser
//---------------------------------------------------------
const OPTTYPE ExecOptionTypes[] = {
	{"-help",			"-help",			0,	0},
	{"-h",				"-help",			0,	0},
	{"-conf",			"-conf",			1,	1},
	{"-json",			"-json",			1,	1},
	{"-group",			"-group",			1,	1},
	{"-host",			"-host",			1,	1},
	{"-type",			"-type",			1,	1},
	{"-hostname",		"-host",			1,	1},
	{"-port",			"-ctrlport",		1,	1},
	{"-ctlport",		"-ctrlport",		1,	1},
	{"-ctrlport",		"-ctrlport",		1,	1},
	{"-cntlport",		"-ctrlport",		1,	1},
	{"-cntrlport",		"-ctrlport",		1,	1},
	{"-cuk",			"-cuk",				1,	1},
	{"-ctlendpoints",	"-ctlendpoints",	1,	1},
	{"-ctleps",			"-ctlendpoints",	1,	1},
	{"-ceps",			"-ctlendpoints",	1,	1},
	{"-custom_seed",	"-custom_seed",		1,	1},
	{"-custom",			"-custom_seed",		1,	1},
	{"-seed",			"-custom_seed",		1,	1},
	{"-cs",				"-custom_seed",		1,	1},
	{"-server",			"-server",			0,	0},
	{"-svr",			"-server",			0,	0},
	{"-slave",			"-slave",			0,	0},
	{"-slv",			"-slave",			0,	0},
	{"-threadcount",	"-threadcnt",		1,	1},
	{"-threadcnt",		"-threadcnt",		1,	1},
	{"-thread",			"-threadcnt",		1,	1},
	{"-thcnt",			"-threadcnt",		1,	1},
	{"-tcnt",			"-threadcnt",		1,	1},
	{"-check",			"-check",			1,	1},
	{"-status",			"-status",			1,	1},
	{"-nocolor",		"-nocolor",			0,	0},
	{"-noclr",			"-nocolor",			0,	0},
	{"-lap",			"-lap",				0,	0},
	{"-d",				"-d",				1,	1},
	{"-dchmpx",			"-dchmpx",			0,	0},
	{"-g",				"-d",				1,	1},
	{"-history",		"-history",			1,	1},
	{"-his",			"-history",			1,	1},
	{"-run",			"-run",				1,	1},
	{NULL,				NULL,				0,	0}
};

const OPTTYPE LineOptionTypes[] = {
	{"help",			"help",				0,	0},
	{"h",				"help",				0,	0},
	{"quit",			"quit",				0,	0},
	{"q",				"quit",				0,	0},
	{"exit",			"quit",				0,	0},
	{"update",			"update",			0,	0},
	{"up",				"update",			0,	0},
	{"u",				"update",			0,	0},
	{"nodes",			"nodes",			0,	2},
	{"node",			"nodes",			0,	2},
	{"n",				"nodes",			0,	2},
	{"status",			"status",			0,	2},
	{"check",			"check",			0,	2},
	{"statusupdate",	"statusupdate",		0,	2},
	{"statusup",		"statusupdate",		0,	2},
	{"stsup",			"statusupdate",		0,	2},
	{"updatestatus",	"statusupdate",		0,	2},
	{"upstatus",		"statusupdate",		0,	2},
	{"upsts",			"statusupdate",		0,	2},
	{"servicein",		"servicein",		0,	2},
	{"svcin",			"servicein",		0,	2},
	{"sin",				"servicein",		0,	2},
	{"serviceout",		"serviceout",		0,	2},
	{"svcout",			"serviceout",		0,	2},
	{"sout",			"serviceout",		0,	2},
	{"merge",			"merge",			1,	2},
	{"suspend",			"suspend",			0,	1},
	{"sus",				"suspend",			0,	1},
	{"nosuspend",		"nosuspend",		0,	1},
	{"nosus",			"nosuspend",		0,	1},
	{"dump",			"dump",				0,	2},
	{"version",			"version",			0,	1},
	{"ver",				"version",			0,	1},
	{"v",				"version",			0,	1},
	{"loop",			"loop",				1,	2},
	{"loopcmd",			"loopcmd",			1,	9999},
	{"lcmd",			"loopcmd",			1,	9999},
	{"dbglevel",		"dbglevel",			0,	1},
	{"dchmpx",			"dchmpx",			0,	1},
	{"history",			"history",			0,	0},
	{"his",				"history",			0,	0},
	{"save",			"save",				1,	1},
	{"load",			"load",				1,	1},
	{"shell",			"shell",			0,	0},
	{"sh",				"shell",			0,	0},
	{"echo",			"echo",				1,	9999},
	{"sleep",			"sleep",			1,	1},
	{NULL,				NULL,				0,	0}
};

//
// Allow sub commands in Load/Loop command
//
const char* AllowedSubCommands[] = {
	"help",			"h",					// help
	"quit",			"q",		"exit",		// quit
	"update",		"up",		"u",		// update
	"nodes",		"node",		"n",		// nodes
	"status",								// status
	"check",								// check
	"statusupdate",	"statusup",	"stsup",	// statusupdate
	"servicein",	"svcin",	"sin",		// servicein
	"serviceout",	"svcout",	"sout",		// serviceout
	"merge",								// merge
	"suspend",		"sus",					// suspend
	"nosuspend",	"nosus",				// nosuspend
	"dump",									// dump
	"version",		"ver",		"v",		// version
	"dbglevel",								// dbglevel
	"dchmpx",								// dchmpx
	"history",		"his",					// history
	"save",									// save
	"echo",									// echo
	"sleep",								// sleep
	NULL
};

static bool IsAllowedSubCommand(const string& strCommand, bool is_allow_loop)
{
	string	strTmp("");
	for(string::const_iterator iter = strCommand.begin(); iter != strCommand.end(); ++iter){
		if(' ' == *iter || '\t' == *iter || '\r' == *iter || '\n' == *iter){
			break;
		}
		strTmp += *iter;
	}
	for(int cnt = 0; NULL != AllowedSubCommands[cnt]; ++cnt){
		if(0 == strcasecmp(AllowedSubCommands[cnt], strTmp.c_str())){
			return true;
		}
	}
	if(is_allow_loop && (0 == strcasecmp("loop", strTmp.c_str()) || 0 == strcasecmp("loopcmd", strTmp.c_str()))){
		return true;
	}
	return false;
}

inline void CleanOptionMap(option_t& opts)
{
	for(option_t::iterator iter = opts.begin(); iter != opts.end(); opts.erase(iter++)){
		iter->second.clear();
	}
}

static bool BaseOptionParser(strarr_t& args, CPOPTTYPE pTypes, option_t& opts)
{
	if(!pTypes){
		return false;
	}
	opts.clear();

	for(size_t Count = 0; Count < args.size(); Count++){
		if(0 < args[Count].length() && '#' == args[Count].at(0)){
			// comment line
			return false;
		}
		size_t Count2;
		for(Count2 = 0; pTypes[Count2].option; Count2++){
			if(0 == strcasecmp(args[Count].c_str(), pTypes[Count2].option)){
				if(args.size() < ((Count + 1) + pTypes[Count2].min_param_cnt)){
					ERR("Option(%s) needs %d parameter.", args[Count].c_str(), pTypes[Count2].min_param_cnt);
					return false;
				}

				size_t		Count3;
				params_t	params;
				params.clear();
				for(Count3 = 0; Count3 < static_cast<size_t>(pTypes[Count2].max_param_cnt); Count3++){
					if(args.size() <= ((Count + 1) + Count3)){
						break;
					}
					params.push_back(args[(Count + 1) + Count3].c_str());
				}
				Count += Count3;
				opts[pTypes[Count2].norm_option] = params;
				break;
			}
		}
		if(!pTypes[Count2].option){
			ERR("Unknown option(%s).", args[Count].c_str());
			return false;
		}
	}
	return true;
}

static bool ExecOptionParser(int argc, char** argv, option_t& opts, string& prgname)
{
	if(0 == argc || !argv){
		return false;
	}
	prgname = basename(argv[0]);
	// cppcheck-suppress stlIfStrFind
	if(0 == prgname.find("lt-")){
		// cut "lt-"
		prgname = prgname.substr(3);
	}

	strarr_t	args;
	for(int nCnt = 1; nCnt < argc; nCnt++){
		args.push_back(argv[nCnt]);
	}

	opts.clear();
	return BaseOptionParser(args, ExecOptionTypes, opts);
}

static bool LineOptionParser(const char* pCommand, option_t& opts)
{
	opts.clear();

	if(!pCommand){
		return false;
	}
	if(0 == strlen(pCommand)){
		return true;
	}

	strarr_t	args;
	string		strParameter;
	bool		isMakeParameter	= false;
	bool		isQuart			= false;
	for(const_pchar pPos = pCommand; '\0' != *pPos && '\n' != *pPos; ++pPos){
		if(isMakeParameter){
			// keeping parameter
			if(isQuart){
				// pattern: "...."
				if('\"' == *pPos){
					if(0 == isspace(*(pPos + sizeof(char))) && '\0' != *(pPos + sizeof(char))){
						ERR("Quart is not matching.");
						return false;
					}
					// end of quart
					isMakeParameter	= false;
					isQuart			= false;

				}else if('\\' == *pPos && '\"' == *(pPos + sizeof(char))){
					// escaped quart
					pPos++;
					strParameter += *pPos;
				}else{
					strParameter += *pPos;
				}

			}else{
				// normal pattern
				if(0 == isspace(*pPos)){
					if('\\' == *pPos){
						continue;
					}
					strParameter += *pPos;
				}else{
					isMakeParameter = false;
				}
			}
			if(!isMakeParameter){
				// end of one parameter
				if(0 < strParameter.length()){
					args.push_back(strParameter);
					strParameter.clear();
				}
			}
		}else{
			// not keeping parameter
			if(0 == isspace(*pPos)){
				strParameter.clear();
				isMakeParameter	= true;

				if('\"' == *pPos){
					isQuart		= true;
				}else{
					isQuart		= false;

					if('\\' == *pPos){
						// found escape character
						pPos++;
						if('\0' == *pPos || '\n' == *pPos){
							break;
						}
					}
					strParameter += *pPos;
				}
			}
			// skip space
		}
	}
	// last check
	if(isMakeParameter){
		if(isQuart){
			ERR("Quart is not matching.");
			return false;
		}
		if(0 < strParameter.length()){
			args.push_back(strParameter);
			strParameter.clear();
		}
	}

	if(!BaseOptionParser(args, LineOptionTypes, opts)){
		return false;
	}
	if(1 < opts.size()){
		ERR("Too many option parameter.");
		return false;
	}
	return true;
}

//---------------------------------------------------------
// Utility for Color
//---------------------------------------------------------
// Color type enum
typedef enum _esc_color_type{
	CLR_BLACK	= 0,
	CLR_RED,
	CLR_GREEN,
	CLR_YELLOW,
	CLR_BLUE,
	CLR_MAGENTA,
	CLR_CYAN,
	CLR_WHITE
}ESCCLRTYPE;

// Escapes
#define	ESC_COLOR_PART_BLACK		"0m"
#define	ESC_COLOR_PART_RED			"1m"
#define	ESC_COLOR_PART_GREEN		"2m"
#define	ESC_COLOR_PART_YELLOW		"3m"
#define	ESC_COLOR_PART_BLUE			"4m"
#define	ESC_COLOR_PART_MAGENTA		"5m"
#define	ESC_COLOR_PART_CYAN			"6m"
#define	ESC_COLOR_PART_WHITE		"7m"

#define ESC_PREFIX_ESCAPE			"\033["
#define ESC_PREFIX_STRING			ESC_PREFIX_ESCAPE "38;5;"
#define ESC_PREFIX_BACKGROUND		ESC_PREFIX_ESCAPE "48;5;"

#define ESC_RESET					ESC_PREFIX_ESCAPE "0m"										// Reset
#define ESC_BOLD					ESC_PREFIX_ESCAPE "1m"										// Bold
#define ESC_BLINK					ESC_PREFIX_ESCAPE "5m"										// Blink

#define	ESC_STR_BLACK				ESC_PREFIX_STRING ESC_COLOR_PART_BLACK						// Foreground color : black
#define	ESC_STR_RED					ESC_PREFIX_STRING ESC_COLOR_PART_RED						// Foreground color : red
#define	ESC_STR_GREEN				ESC_PREFIX_STRING ESC_COLOR_PART_GREEN						// Foreground color : green
#define	ESC_STR_YELLOW				ESC_PREFIX_STRING ESC_COLOR_PART_YELLOW						// Foreground color : yellow
#define	ESC_STR_BLUE				ESC_PREFIX_STRING ESC_COLOR_PART_BLUE						// Foreground color : blue
#define	ESC_STR_MAGENTA				ESC_PREFIX_STRING ESC_COLOR_PART_MAGENTA					// Foreground color : magenta
#define	ESC_STR_CYAN				ESC_PREFIX_STRING ESC_COLOR_PART_CYAN						// Foreground color : cyan
#define	ESC_STR_WHITE				ESC_PREFIX_STRING ESC_COLOR_PART_WHITE						// Foreground color : white

#define	ESC_BG_BLACK				ESC_STR_WHITE ESC_PREFIX_BACKGROUND ESC_COLOR_PART_BLACK	// Background color : black,	Foreground color : white
#define	ESC_BG_RED					ESC_STR_WHITE ESC_PREFIX_BACKGROUND ESC_COLOR_PART_RED		// Background color : red,		Foreground color : white
#define	ESC_BG_GREEN				ESC_STR_WHITE ESC_PREFIX_BACKGROUND ESC_COLOR_PART_GREEN	// Background color : green,	Foreground color : white
#define	ESC_BG_YELLOW				ESC_STR_WHITE ESC_PREFIX_BACKGROUND ESC_COLOR_PART_YELLOW	// Background color : yellow,	Foreground color : white
#define	ESC_BG_BLUE					ESC_STR_WHITE ESC_PREFIX_BACKGROUND ESC_COLOR_PART_BLUE		// Background color : blue,		Foreground color : white
#define	ESC_BG_MAGENTA				ESC_STR_WHITE ESC_PREFIX_BACKGROUND ESC_COLOR_PART_MAGENTA	// Background color : magenta,	Foreground color : white
#define	ESC_BG_CYAN					ESC_STR_WHITE ESC_PREFIX_BACKGROUND ESC_COLOR_PART_CYAN		// Background color : cyan,		Foreground color : white
#define	ESC_BG_WHITE				ESC_STR_BLACK ESC_PREFIX_BACKGROUND ESC_COLOR_PART_WHITE	// Background color : white,	Foreground color : black

static inline string CVT_ESC_CHAR(const char* str, ESCCLRTYPE color, bool bg = false, bool bold = false, bool blink = false)
{
	string	result("");

	if(!isColorDisplay){
		result = CHMEMPTYSTR(str) ? "" : str;
	}else{
		if(bold){
			result += ESC_BOLD;
		}
		if(blink){
			result += ESC_BLINK;
		}
		switch(color){
			case	CLR_RED:
				result += bg ? ESC_BG_RED		: ESC_STR_RED;
				break;
			case	CLR_GREEN:
				result += bg ? ESC_BG_GREEN		: ESC_STR_GREEN;
				break;
			case	CLR_YELLOW:
				result += bg ? ESC_BG_YELLOW	: ESC_STR_YELLOW;
				break;
			case	CLR_BLUE:
				result += bg ? ESC_BG_BLUE		: ESC_STR_BLUE;
				break;
			case	CLR_MAGENTA:
				result += bg ? ESC_BG_MAGENTA	: ESC_STR_MAGENTA;
				break;
			case	CLR_CYAN:
				result += bg ? ESC_BG_CYAN		: ESC_STR_CYAN;
				break;
			case	CLR_WHITE:
				result += bg ? ESC_BG_WHITE		: ESC_STR_WHITE;
				break;
			case	CLR_BLACK:
			default:
				result += bg ? ESC_BG_BLACK		: ESC_STR_BLACK;
				break;
		}
		result += CHMEMPTYSTR(str) ? "" : str;
		result += ESC_RESET;
	}
	return result;
}

static inline string CVT_ESC_BOLD_CHAR(const char* str)
{
	string	result("");

	if(!isColorDisplay){
		result += CHMEMPTYSTR(str) ? "" : str;
	}else{
		result += ESC_BOLD;
		result += CHMEMPTYSTR(str) ? "" : str;
		result += ESC_RESET;
	}
	return result;
}

static inline string CVT_ESC_STR(const string& str, ESCCLRTYPE color, bool bg = false, bool bold = false, bool blink = false)
{
	return CVT_ESC_CHAR(str.c_str(), color, bg, bold, blink);
}

static inline string CVT_ESC_BOLD_STR(const string& str)
{
	return CVT_ESC_BOLD_CHAR(str.c_str());
}

static inline string BOLD(const string& str)												{ return CVT_ESC_BOLD_STR(str);		}
static inline string BOLD(const char* str)													{ return CVT_ESC_BOLD_CHAR(str);	}

static inline string BLACK(const string& str, bool bold = false, bool blink = false)		{ return CVT_ESC_STR(str, CLR_BLACK,	false, bold, blink); }
static inline string RED(const string& str, bool bold = false, bool blink = false)			{ return CVT_ESC_STR(str, CLR_RED,		false, bold, blink); }
static inline string GREEN(const string& str, bool bold = false, bool blink = false)		{ return CVT_ESC_STR(str, CLR_GREEN,	false, bold, blink); }
static inline string YELLOW(const string& str, bool bold = false, bool blink = false)		{ return CVT_ESC_STR(str, CLR_YELLOW,	false, bold, blink); }
static inline string BLUE(const string& str, bool bold = false, bool blink = false)			{ return CVT_ESC_STR(str, CLR_BLUE,		false, bold, blink); }
static inline string MAGENTA(const string& str, bool bold = false, bool blink = false)		{ return CVT_ESC_STR(str, CLR_MAGENTA,	false, bold, blink); }
static inline string CYAN(const string& str, bool bold = false, bool blink = false)			{ return CVT_ESC_STR(str, CLR_CYAN,		false, bold, blink); }
static inline string WHITE(const string& str, bool bold = false, bool blink = false)		{ return CVT_ESC_STR(str, CLR_WHITE,	false, bold, blink); }

static inline string BG_BLACK(const string& str, bool bold = false, bool blink = false)		{ return CVT_ESC_STR(str, CLR_BLACK,	true, bold, blink); }
static inline string BG_RED(const string& str, bool bold = false, bool blink = false)		{ return CVT_ESC_STR(str, CLR_RED,		true, bold, blink); }
static inline string BG_GREEN(const string& str, bool bold = false, bool blink = false)		{ return CVT_ESC_STR(str, CLR_GREEN,	true, bold, blink); }
static inline string BG_YELLOW(const string& str, bool bold = false, bool blink = false)	{ return CVT_ESC_STR(str, CLR_YELLOW,	true, bold, blink); }
static inline string BG_BLUE(const string& str, bool bold = false, bool blink = false)		{ return CVT_ESC_STR(str, CLR_BLUE,		true, bold, blink); }
static inline string BG_MAGENTA(const string& str, bool bold = false, bool blink = false)	{ return CVT_ESC_STR(str, CLR_MAGENTA,	true, bold, blink); }
static inline string BG_CYAN(const string& str, bool bold = false, bool blink = false)		{ return CVT_ESC_STR(str, CLR_CYAN,		true, bold, blink); }
static inline string BG_WHITE(const string& str, bool bold = false, bool blink = false)		{ return CVT_ESC_STR(str, CLR_WHITE,	true, bold, blink); }

static inline string BLACK(const char* str, bool bold = false, bool blink = false)			{ return CVT_ESC_CHAR(str, CLR_BLACK,	false, bold, blink); }
static inline string RED(const char* str, bool bold = false, bool blink = false)			{ return CVT_ESC_CHAR(str, CLR_RED,		false, bold, blink); }
static inline string GREEN(const char* str, bool bold = false, bool blink = false)			{ return CVT_ESC_CHAR(str, CLR_GREEN,	false, bold, blink); }
static inline string YELLOW(const char* str, bool bold = false, bool blink = false)			{ return CVT_ESC_CHAR(str, CLR_YELLOW,	false, bold, blink); }
static inline string BLUE(const char* str, bool bold = false, bool blink = false)			{ return CVT_ESC_CHAR(str, CLR_BLUE,	false, bold, blink); }
static inline string MAGENTA(const char* str, bool bold = false, bool blink = false)		{ return CVT_ESC_CHAR(str, CLR_MAGENTA,	false, bold, blink); }
static inline string CYAN(const char* str, bool bold = false, bool blink = false)			{ return CVT_ESC_CHAR(str, CLR_CYAN,	false, bold, blink); }
static inline string WHITE(const char* str, bool bold = false, bool blink = false)			{ return CVT_ESC_CHAR(str, CLR_WHITE,	false, bold, blink); }

static inline string BG_BLACK(const char* str, bool bold = false, bool blink = false)		{ return CVT_ESC_CHAR(str, CLR_BLACK,	true, bold, blink); }
static inline string BG_RED(const char* str, bool bold = false, bool blink = false)			{ return CVT_ESC_CHAR(str, CLR_RED,		true, bold, blink); }
static inline string BG_GREEN(const char* str, bool bold = false, bool blink = false)		{ return CVT_ESC_CHAR(str, CLR_GREEN,	true, bold, blink); }
static inline string BG_YELLOW(const char* str, bool bold = false, bool blink = false)		{ return CVT_ESC_CHAR(str, CLR_YELLOW,	true, bold, blink); }
static inline string BG_BLUE(const char* str, bool bold = false, bool blink = false)		{ return CVT_ESC_CHAR(str, CLR_BLUE,	true, bold, blink); }
static inline string BG_MAGENTA(const char* str, bool bold = false, bool blink = false)		{ return CVT_ESC_CHAR(str, CLR_MAGENTA,	true, bold, blink); }
static inline string BG_CYAN(const char* str, bool bold = false, bool blink = false)		{ return CVT_ESC_CHAR(str, CLR_CYAN,	true, bold, blink); }
static inline string BG_WHITE(const char* str, bool bold = false, bool blink = false)		{ return CVT_ESC_CHAR(str, CLR_WHITE,	true, bold, blink); }

//---------------------------------------------------------
// Utility for communication
//---------------------------------------------------------
#define	RECEIVE_LENGTH					(128 * 1024)	// 128KB for one receiving data maximum from sock

//
// This function receives data as blocking.
//
static bool ReceiveControlSocket(int sock, string& strReceive)
{
	strReceive.clear();

	if(CHM_INVALID_SOCK == sock){
		ERR("Parameters are wrong.");
		return false;
	}

	char	byReceive[RECEIVE_LENGTH];
	char*	pTotalBuff	= NULL;
	size_t	totallength = RECEIVE_LENGTH * 2;
	ssize_t	pos			= 0;
	if(NULL == (pTotalBuff = reinterpret_cast<char*>(malloc(totallength)))){
		ERR("Could not allocate memory.");
		// cppcheck-suppress unmatchedSuppression
		// cppcheck-suppress unreadVariable
		CHM_CLOSESOCK(sock);
		return false;
	}

	// receive
	while(true){
		ssize_t	onerecv;

		if(-1 == (onerecv = recv(sock, byReceive, RECEIVE_LENGTH, 0))){
			if(EINTR == errno){
				MSG("Interrupted signal during receiving from sock(%d), errno=%d(EINTR).", sock, errno);

			}else if(EAGAIN == errno || EWOULDBLOCK == errno){
				MSG("There are no received data on sock(%d), so not wait. errno=%d(EAGAIN or EWOULDBLOCK).", sock, errno);
				break;

			}else if(EBADF == errno || ECONNREFUSED == errno || ENOTCONN == errno || ENOTSOCK == errno){
				WAN("There are no received data on sock(%d), errno=%d(EBADF or ECONNREFUSED or ENOTCONN or ENOTSOCK).", sock, errno);
				// cppcheck-suppress unmatchedSuppression
				// cppcheck-suppress unreadVariable
				CHM_CLOSESOCK(sock);
				CHM_Free(pTotalBuff);
				return false;

			}else{
				WAN("Failed to receive from sock(%d), errno=%d, then closing this socket.", sock, errno);
				// cppcheck-suppress unmatchedSuppression
				// cppcheck-suppress unreadVariable
				CHM_CLOSESOCK(sock);
				CHM_Free(pTotalBuff);
				return false;
			}

		}else if(0 == onerecv){
			// close sock
			//MSG("Receive 0 byte from sock(%d), it means socket is closed.", sock);
			break;

		}else{
			// read some bytes.
			memcpy(&pTotalBuff[pos], byReceive, static_cast<size_t>(onerecv));
			pos += onerecv;
			if((totallength - static_cast<size_t>(pos)) < RECEIVE_LENGTH){
				char*	pTmp;
				if(NULL == (pTmp = reinterpret_cast<char*>(realloc(pTotalBuff, totallength + RECEIVE_LENGTH)))){
					ERR("Could not allocate memory.");
					// cppcheck-suppress unmatchedSuppression
					// cppcheck-suppress unreadVariable
					CHM_CLOSESOCK(sock);
					CHM_Free(pTotalBuff);
					return false;
				}
				pTotalBuff	= pTmp;
				totallength += RECEIVE_LENGTH;
			}
		}
	}
	// close sock at first
	// cppcheck-suppress unmatchedSuppression
	// cppcheck-suppress unreadVariable
	CHM_CLOSESOCK(sock);

	if(0 < pos && '\0' != pTotalBuff[pos - 1]){
		if(totallength < static_cast<size_t>(pos + 1)){
			char*	pTmp;
			if(NULL == (pTmp = reinterpret_cast<char*>(realloc(pTotalBuff, static_cast<size_t>(pos + 1))))){
				ERR("Could not allocate memory.");
				// cppcheck-suppress unmatchedSuppression
				// cppcheck-suppress uselessAssignmentArg
				// cppcheck-suppress unreadVariable
				CHM_CLOSESOCK(sock);
				CHM_Free(pTotalBuff);
				return false;
			}
			pTotalBuff	= pTmp;
			//totallength = static_cast<size_t>(pos + 1);
		}
		pTotalBuff[pos]	= '\0';
	}
	strReceive = pTotalBuff;
	CHM_Free(pTotalBuff);

	return true;
}

//
// This function sends data as blocking. And you must block SIGPIPE.
//
static bool SendControlSocket(int sock, const char* pdata, bool& is_closed)
{
	is_closed = false;

	if(CHM_INVALID_SOCK == sock || !pdata){
		ERR("Parameters are wrong.");
		return false;
	}

	// send
	ssize_t	onesent	= 0;
	size_t	length	= strlen(pdata) + 1;
	size_t	totalsent;
	for(totalsent = 0; totalsent < length; totalsent += static_cast<size_t>(onesent)){

		if(-1 == (onesent = send(sock, &pdata[totalsent], length - totalsent, 0))){
			if(EINTR == errno){
				MSG("Interrupted signal during sending to sock(%d), errno=%d(EINTR).", sock, errno);

			}else if(EAGAIN == errno || EWOULDBLOCK == errno){
				MSG("sock(%d) does not ready for sending, errno=%d(EAGAIN or EWOULDBLOCK).", sock, errno);

			}else if(EACCES == errno || EBADF == errno || ECONNRESET == errno || ENOTCONN == errno || EDESTADDRREQ == errno || EISCONN == errno || ENOTSOCK == errno || EPIPE == errno){
				// something error to closing
				WAN("sock(%d) does not ready for sending, errno=%d(EACCES or EBADF or ECONNRESET or ENOTCONN or EDESTADDRREQ or EISCONN or ENOTSOCK or EPIPE).", sock, errno);
				is_closed = true;
				return false;

			}else{
				// failed
				WAN("Failed to send data(length:%zu), errno=%d.", length, errno);
				return false;
			}
			// continue...
			onesent = 0;
		}
	}
	return true;
}

//
// This function returns blocking socket.
//
static int OneConnectControlPort(const char* hostname, short port)
{
	const int	opt_yes			= 1;
	const int	opt_keepidle	= 60;
	const int	opt_keepinterval= 10;
	const int	opt_keepcount	= 3;

	if(CHMEMPTYSTR(hostname) || CHM_INVALID_PORT == port){
		ERR("Parameters are wrong.");
		return CHM_INVALID_SOCK;
	}

	// Get addrinfo list
	addrinfolist_t	addrinfos;
	if(!ChmNetDb::Get()->GetAddrInfoList(hostname, port, addrinfos, true)){		// if "localhost", convert fqdn.
		WAN("Failed to get addrinfo for %s:%d.", hostname, port);
		return CHM_INVALID_SOCK;
	}

	// make socket, bind, listen
	int	sockfd = CHM_INVALID_SOCK;
	for(addrinfolist_t::const_iterator iter = addrinfos.begin(); addrinfos.end() != iter && CHM_INVALID_SOCK == sockfd; ++iter){
		struct addrinfo*	paddrinfo = *iter;
		for(struct addrinfo* ptmpaddrinfo = paddrinfo; ptmpaddrinfo && CHM_INVALID_SOCK == sockfd; ptmpaddrinfo = ptmpaddrinfo->ai_next){
			if(IPPROTO_TCP != ptmpaddrinfo->ai_protocol){
				MSG("protocol in addrinfo which is made from %s:%d does not TCP, so check next addrinfo...", hostname, port);
				continue;
			}
			// socket
			if(CHM_INVALID_SOCK == (sockfd = socket(ptmpaddrinfo->ai_family, ptmpaddrinfo->ai_socktype, ptmpaddrinfo->ai_protocol))){
				WAN("Failed to make socket for %s:%d by errno=%d, but continue to make next addrinfo...", hostname, port, errno);
				continue;
			}

			// options
			setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<const void*>(&opt_yes), sizeof(int));
			setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const void*>(&opt_yes), sizeof(int));
			setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPIDLE, reinterpret_cast<const void*>(&opt_keepidle), sizeof(int));
			setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPINTVL, reinterpret_cast<const void*>(&opt_keepinterval), sizeof(int));
			setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPCNT, reinterpret_cast<const void*>(&opt_keepcount), sizeof(int));

			// connect
			if(-1 == connect(sockfd, ptmpaddrinfo->ai_addr, ptmpaddrinfo->ai_addrlen)){
				MSG("Failed to connect for %s:%d by errno=%d, but continue to make next addrinfo...", hostname, port, errno);
				// cppcheck-suppress unmatchedSuppression
				// cppcheck-suppress unreadVariable
				CHM_CLOSESOCK(sockfd);
				continue;
			}
			if(CHM_INVALID_SOCK != sockfd){
				break;
			}
		}
	}
	ChmNetDb::FreeAddrInfoList(addrinfos);

	if(CHM_INVALID_SOCK == sockfd){
		MSG("Could not make socket and connect %s:%d.", hostname, port);
	}
	return sockfd;
}

static int ConnectControlPort(const char* hostname, short port, const char* ctlendpoints)
{
	// Try from ctlendpoints first
	if(!CHMEMPTYSTR(ctlendpoints)){
		strlst_t	ctleps;
		if(str_split(ctlendpoints, ctleps, ',')){
			for(strlst_t::const_iterator iter = ctleps.begin(); ctleps.end() != iter; ++iter){
				CHMPXHP_RAWPAIR	pair;
				if(parse_hostport_pairs_from_string(iter->c_str(), &pair)){
					int sock;
					if(CHM_INVALID_SOCK != (sock = OneConnectControlPort(pair.name, pair.port))){
						return sock;
					}
				}
			}
		}
	}
	// finally, direct access
	strlst_t	hostnames;
	if(!HostnameMap::GetHostnames(hostname, hostnames) || hostnames.empty()){
		hostnames.push_back(string(hostname));
	}
	for(strlst_t::iterator iter = hostnames.begin(); iter != hostnames.end(); ++iter){
		int sock;
		if(CHM_INVALID_SOCK != (sock = OneConnectControlPort(iter->c_str(), port))){
			return sock;
		}
	}
	return CHM_INVALID_SOCK;
}

//
// This function sends and receives command to control port with blocking.
//
static bool SendCommandToControlPort(const char* hostname, short ctrlport, const char* ctlendpoints, const char* pCommand, string& strResult)
{
	strResult = "";

	if(CHMEMPTYSTR(hostname) || CHM_INVALID_PORT == ctrlport || !pCommand){
		ERR("Parameters are wrong.");
		return false;
	}
	// try to connect to control port
	int	ctlsock;
	if(CHM_INVALID_SOCK == (ctlsock = ConnectControlPort(hostname, ctrlport, ctlendpoints))){
		MSG("Could not connect to %s:%d.", hostname, ctrlport);
		return false;
	}
	MSG("Connected to %s:%d.", hostname, ctrlport);

	// send(does not lock for control socket)
	bool	is_closed = false;
	if(!SendControlSocket(ctlsock, pCommand, is_closed)){
		WAN("Could not send to %s:%d(sock:%d).", hostname, ctrlport, ctlsock);
		if(!is_closed){
			// cppcheck-suppress unmatchedSuppression
			// cppcheck-suppress unreadVariable
			CHM_CLOSESOCK(ctlsock);
		}
		return false;
	}

	// receive
	return ReceiveControlSocket(ctlsock, strResult);
}

//---------------------------------------------------------
// Utilities : Command result strings
//---------------------------------------------------------
#define	RESULT_CR						"\n"

#define	STATUS_PART_PREFIX				"["
#define	STATUS_PART_SUFFIX				"]"

#define	ALLSTATUS_KEY_CR				RESULT_CR
#define	ALLSTATUS_KEY_RINGNAME_NOSPACE	"RINGName="
#define	ALLSTATUS_KEY_NUMBER_TOP		"No."
#define	ALLSTATUS_KEY_SERVERNAME		"ServerName="
#define	ALLSTATUS_KEY_PORT				"Port="
#define	ALLSTATUS_KEY_CTLPORT			"ControlPort="
#define	ALLSTATUS_KEY_CUK				"CUK="
#define	ALLSTATUS_KEY_CUSTOM_SEED		"CustomIDSeed="
#define	ALLSTATUS_KEY_CTLEPS			"ControlEndppoints="
#define	ALLSTATUS_KEY_ISSSL				"UseSSL="
#define	ALLSTATUS_KEY_ISVERIFY			"VerifyPeer="
#define	ALLSTATUS_KEY_STATUS			"ServerStatus="
#define	ALLSTATUS_KEY_STATUS_FIRST_KEY	"("
#define	ALLSTATUS_KEY_STATUS_MID_KEY	"->"
#define	ALLSTATUS_KEY_STATUS_END_KEY	")"
#define	ALLSTATUS_KEY_LASTUPDATE		"LastUpdate="
#define	ALLSTATUS_KEY_HASH				"EnableHashValue="
#define	ALLSTATUS_KEY_PENDINGHASH		"PendingHashValue="
#define	ALLSTATUS_KEY_SSL_YES			"yes"
#define	ALLSTATUS_KEY_SSL_NO			"no"

#define	DUMP_KEY_CR						RESULT_CR
#define	DUMP_KEY_CHMPX_MAN				"chmpx_man{\n"
#define	DUMP_KEY_CHMPX_SELF				"chmpx_self="
#define	DUMP_KEY_CHMPX_SERVERS			"chmpx_servers="
#define	DUMP_KEY_CHMPX_SLAVES			"chmpx_slaves="
#define	DUMP_KEY_START					"{\n"
#define	DUMP_KEY_END					"}\n"
#define	DUMP_KEY_ARRAY_START			"["
#define	DUMP_KEY_ARRAY_END				"]={\n"
#define	DUMP_KEY_CHMPX_START			"chmpx{\n"
#define	DUMP_KEY_CHMPX_CHMPXID			"chmpxid="
#define	DUMP_KEY_CHMPX_NAME				"name="
#define	DUMP_KEY_CHMPX_CTLPORT			"ctlport="
#define	DUMP_KEY_CHMPX_CUK				"cuk="
#define	DUMP_KEY_CHMPX_CTLEPS			"ctlendpoints="
#define	DUMP_KEY_CHMPX_CUSTOMSEED		"custom_seed="
#define	DUMP_KEY_CHMPX_END				"}\n}\n"
#define	DUMP_KEY_CHMPX_HASH				"base_hash="
#define	DUMP_KEY_CHMPX_PENDING			"pending_hash="
#define	DUMP_KEY_CHMPX_SOCK				"sock="
#define	DUMP_KEY_CHMPX_LASTTIME			"last_status_time="
#define	DUMP_KEY_CHMPX_STATUS			"status="
#define	DUMP_KEY_CHMPX_PREFIXSTATUS		"->"

//---------------------------------------------------------
// Structures: create all node datas from all node's Dump result string
//---------------------------------------------------------
//
// check result flags
//
typedef	uint							CHKRESULT_TYPE;

#define	CHKRESULT_NOERR					0
#define	CHKRESULT_WARN					1
#define	CHKRESULT_ERR					3

//
// check result flags for parts
//
typedef struct _check_result_of_parts{
	CHKRESULT_TYPE	result_status;
	CHKRESULT_TYPE	result_status_ring;
	CHKRESULT_TYPE	result_status_live;
	CHKRESULT_TYPE	result_status_act;
	CHKRESULT_TYPE	result_status_opr;
	CHKRESULT_TYPE	result_status_sus;
	CHKRESULT_TYPE	result_hash;
	CHKRESULT_TYPE	result_pending;
	CHKRESULT_TYPE	result_tosockcnt;
	CHKRESULT_TYPE	result_fromsockcnt;
	CHKRESULT_TYPE	result_lastupdatetime;

	_check_result_of_parts() : result_status(CHKRESULT_NOERR), result_status_ring(CHKRESULT_NOERR), result_status_live(CHKRESULT_NOERR), result_status_act(CHKRESULT_NOERR), result_status_opr(CHKRESULT_NOERR), result_status_sus(CHKRESULT_NOERR), result_hash(CHKRESULT_NOERR), result_pending(CHKRESULT_NOERR), result_tosockcnt(CHKRESULT_NOERR), result_fromsockcnt(CHKRESULT_NOERR), result_lastupdatetime(CHKRESULT_NOERR) {}

	void clear(void)
	{
		result_status			= CHKRESULT_NOERR;
		result_status_ring		= CHKRESULT_NOERR;
		result_status_live		= CHKRESULT_NOERR;
		result_status_act		= CHKRESULT_NOERR;
		result_status_opr		= CHKRESULT_NOERR;
		result_status_sus		= CHKRESULT_NOERR;
		result_hash				= CHKRESULT_NOERR;
		result_pending			= CHKRESULT_NOERR;
		result_tosockcnt		= CHKRESULT_NOERR;
		result_fromsockcnt		= CHKRESULT_NOERR;
		result_lastupdatetime	= CHKRESULT_NOERR;
	}

	CHKRESULT_TYPE summarize(void) const
	{
		return (result_status | result_status_ring | result_status_live | result_status_act | result_status_opr | result_status_sus | result_hash | result_pending | result_tosockcnt | result_fromsockcnt | result_lastupdatetime);
	}
}CHKRESULT_PART, *PCHKRESULT_PART;

//
// one node data in dump result
//
typedef struct _node_unit_data{
	bool			is_down;
	chmpxid_t		chmpxid;
	string			hostname;
	short			ctrlport;
	string			cuk;
	string			ctlendpoints;
	string			custom_seed;
	string			hash;
	string			pendinghash;
	long			tosockcnt;						// socket count "to this node"
	long			fromsockcnt;					// socket count "from this node"
	time_t			lastupdatetime;
	string			status;
	CHKRESULT_PART	checkresult;

	_node_unit_data() : is_down(false), chmpxid(CHM_INVALID_CHMPXID), hostname(""), ctrlport(CHM_INVALID_PORT), cuk(""), ctlendpoints(""), custom_seed(""), hash(""), pendinghash(""), tosockcnt(0), fromsockcnt(0), lastupdatetime(0), status(""), checkresult() {}

	void clear(void)
	{
		is_down			= false;
		chmpxid			= CHM_INVALID_CHMPXID;
		hostname		= "";
		ctrlport		= CHM_INVALID_PORT;
		cuk				= "";
		ctlendpoints	= "";
		custom_seed		= "";
		hash			= "0xffffffffffffffff";
		pendinghash		= "0xffffffffffffffff";
		tosockcnt		= 0;
		fromsockcnt		= 0;
		lastupdatetime	= 0;
		status			= "[UNKNOWN][DOWN][n/a][Nothing][NoSuspend]";
		checkresult.clear();
	}
}NODEUNITDATA, *PNODEUNITDATA;

//
// node datas in dump result
//
typedef map<string, NODEUNITDATA>			nodesunits_t;

//
// one node's all connected nodes data in dump result
//
typedef struct _node_status_detail{
	NODEUNITDATA	self;
	nodesunits_t	servers;
	nodesunits_t	slaves;

	void clear(void)
	{
		self.clear();
		servers.clear();
		slaves.clear();
	}
}NODESTATUSDETAIL, *PNODESTATUSDETAIL;

//
// all node's all connected nodes data in dump result
//
typedef map<string, NODESTATUSDETAIL>		statusdetails_t;

//
// one node's result with compared mark
//
typedef struct _node_check_result{
	CHKRESULT_TYPE		noderesult;
	NODESTATUSDETAIL	all;

	_node_check_result() : noderesult(CHKRESULT_NOERR) {}

	void clear(void)
	{
		noderesult = CHKRESULT_NOERR;
		all.clear();
	}
}NODECHECKRESULT, *PNODECHECKRESULT;

//
// all node's result with compared mark
//
typedef map<string, NODECHECKRESULT>		nodechkresults_t;

//---------------------------------------------------------
// Utilities: DUMP Command with thread
//---------------------------------------------------------
//
// dump raw result
//
typedef struct _dump_node_result{
	chmpxid_t	chmpxid;
	string		hostname;
	short		ctrlport;
	string		cuk;
	string		ctlendpoints;
	string		custom_seed;
	bool		isError;
	string		strResult;

	_dump_node_result() : chmpxid(CHM_INVALID_CHMPXID), hostname(""), ctrlport(0), cuk(""), ctlendpoints(""), custom_seed(""), isError(false), strResult("") {}

}DUMPNODERES, *PDUMPNODERES;

typedef std::list<DUMPNODERES>	dumpnodereslist_t;
typedef std::list<PDUMPNODERES>	pdumpnodereslist_t;

//
// dump thread function parameter
//
typedef struct _thread_param{
	pdumpnodereslist_t	presults;
	pthread_t			pthreadid;			// pthread id(pthread_create)
	volatile bool*		pis_run;

	_thread_param() : pthreadid(0), pis_run(NULL) {}

}THPARAM, *PTHPARAM;

//
// Thread function for DUMP command
//
static void* SendDumpCommandThread(void* param)
{
	PTHPARAM	pThParam = reinterpret_cast<PTHPARAM>(param);
	if(!pThParam || !pThParam->pis_run){
		ERR("Parameter for thread is wrong.");
		pthread_exit(NULL);		// [NOTE] if this function is called not in thread, this result is unknown...
	}

	// wait for suspend flag off
	while(!(*(pThParam->pis_run))){
		struct timespec	sleeptime = {0L, 10 * 1000 * 1000};	// 10ms
		nanosleep(&sleeptime, NULL);
	}

	//
	// Loop
	//
	for(pdumpnodereslist_t::iterator iter = pThParam->presults.begin(); *(pThParam->pis_run) && iter != pThParam->presults.end(); ++iter){
		PDUMPNODERES	pnoderes = *iter;
		if(!SendCommandToControlPort(pnoderes->hostname.c_str(), pnoderes->ctrlport, pnoderes->ctlendpoints.c_str(), "DUMP", pnoderes->strResult)){
			MSG("Could not get DUMP data from %s:%d", pnoderes->hostname.c_str(), pnoderes->ctrlport);
			pnoderes->isError	= true;
		}else{
			pnoderes->isError	= false;
		}
	}
	if(0 != pThParam->pthreadid){
		pthread_exit(NULL);
	}
	return NULL;
}

//
// This function issues a DUMP command.
// The thread starts according to the number of threads of the global variable.
// If do not use threads, we will process them directly.
//
static bool SendDumpCommandByAutoThreads(dumpnodereslist_t& nodes)
{
	if(nodes.empty()){
		ERR("Parameter is wrong.");
		return false;
	}

	// create thread parameters
	volatile bool	is_run	= false;
	PTHPARAM		pthparam= new THPARAM[0 == nThreadCount ? 1 : nThreadCount];	// [NOTE] If no thread mode, we make one param.
	int				pos		= 0;
	for(dumpnodereslist_t::iterator iter = nodes.begin(); iter != nodes.end(); ++iter){
		pthparam[pos].presults.push_back(&(*iter));									// set node structure "pointer"
		pthparam[pos].pis_run = &is_run;											// [NOTE] This value overwrites when pos is recycled, but there is no problem.
		if(nThreadCount <= ++pos){
			pos = 0;
		}
	}

	// create thread
	for(int cnt = 0; cnt < nThreadCount && cnt < static_cast<int>(nodes.size()); ++cnt){
		// create thread
		if(0 != pthread_create(&(pthparam[cnt].pthreadid), NULL, SendDumpCommandThread, &(pthparam[cnt]))){
			ERR("Could not create thread.");
			pthparam[cnt].pthreadid	= 0;
			break;
		}
	}
	// run all thread
	is_run = true;

	if(0 == nThreadCount){
		// This case is no thread mode, then call function directly.
		// cppcheck-suppress unreadVariable
		is_run = true;
		SendDumpCommandThread(&pthparam[0]);
	}

	// wait all thread exit
	for(int cnt = 0; cnt < nThreadCount && cnt < static_cast<int>(nodes.size()); ++cnt){
		if(0 == pthparam[cnt].pthreadid){
			continue;
		}
		void*		pretval = NULL;
		int			result;
		if(0 != (result = pthread_join(pthparam[cnt].pthreadid, &pretval))){
			ERR("Failed to wait thread exit. return code(error) = %d", result);
			continue;
		}
	}
	delete []pthparam;

	return true;
}

//---------------------------------------------------------
// Utilities : Parse chmpxs from Dump result string
//---------------------------------------------------------
static string CutSpaceCharacter(const string& strBase)
{
	string	strResult;
	for(string::const_iterator iter = strBase.begin(); iter != strBase.end(); ++iter){
		if(' ' == *iter || '\t' == *iter){
			continue;
		}
		strResult += *iter;
	}
	return strResult;
}

static string CutEmptyLine(const string& strBase)
{
	string	strResult = strBase;
	if(!strResult.empty() && '\n' == strResult[strResult.length() - 1]){
		strResult.resize(strResult.length() - 1);
	}
	return strResult;
}

static string ReplaceString(const string& strBase, const string& strOrg, const string& strNew)
{
	string	strResult = strBase;
	if(!strOrg.empty()){
		string::size_type	pos = 0;
		while(string::npos != (pos = strResult.find(strOrg, pos))){
			strResult.replace(pos, strOrg.length(), strNew);
			pos += strNew.length();
		}
	}
	return strResult;
}

static string ParseChmpxListFromDumpResult(nodectrllist_t& nodes, const string& strParsed, bool is_server, bool& is_error)
{
	string				strInput = strParsed;
	string::size_type	pos;

	is_error = false;

	// must start with "{\n"
	if(string::npos == (pos = strInput.find(DUMP_KEY_START))){
		ERR("Could not found \"{\" key in DUMP result.");
		is_error = true;
		return strInput;
	}
	strInput = strInput.substr(pos + strlen(DUMP_KEY_START));

	// Loop to "}\n"
	// cppcheck-suppress stlIfStrFind
	while(0 != strInput.find(DUMP_KEY_END) && string::npos != strInput.find(DUMP_KEY_END)){
		string		strChmpxid;
		chmpxid_t	chmpxid;
		string		name;
		string		strctrlport;
		short		ctrlport;
		string		cuk;
		string		ctlendpoints;
		string		custom_seed;

		// "[XX]={\n"
		// cppcheck-suppress stlIfStrFind
		if(string::npos == strInput.find(DUMP_KEY_ARRAY_START) || 0 != strInput.find(DUMP_KEY_ARRAY_START)){
			//MSG("Could not found \"[XX]={\" key or found invalid data in DUMP result.");
			return strInput;
		}
		strInput = strInput.substr(strlen(DUMP_KEY_ARRAY_START));
		if(string::npos == (pos = strInput.find(DUMP_KEY_ARRAY_END))){
			ERR("Could not found \"[XX]={\" key in DUMP result.");
			is_error = true;
			return strInput;
		}
		strInput = strInput.substr(pos + strlen(DUMP_KEY_ARRAY_END));

		// "chmpx={\n"
		if(string::npos == (pos = strInput.find(DUMP_KEY_CHMPX_START))){
			ERR("Could not found \"chmpx={\" key in DUMP result.");
			is_error = true;
			return strInput;
		}
		strInput = strInput.substr(pos + strlen(DUMP_KEY_CHMPX_START));

		// "chmpxid="
		if(string::npos == (pos = strInput.find(DUMP_KEY_CHMPX_CHMPXID))){
			ERR("Could not found \"chmpxid=\" key in DUMP result.");
			is_error = true;
			return strInput;
		}
		strInput = strInput.substr(pos + strlen(DUMP_KEY_CHMPX_CHMPXID));

		// Get CHMPXID
		if(string::npos == (pos = strInput.find(DUMP_KEY_CR))){
			ERR("Could not found CR after \"chmpxid=\" key in DUMP result.");
			is_error = true;
			return strInput;
		}
		strChmpxid	= trim(strInput.substr(0, pos));
		strInput	= strInput.substr(pos + strlen(DUMP_KEY_CR));
		if(2 >= strChmpxid.size()){
			ERR("\"chmpxid=\" value does not start \"0x\" in DUMP result.");
			is_error = true;
			return strInput;
		}
		strChmpxid	= trim(strChmpxid.substr(2));
		chmpxid		= cvt_string_to_number(strChmpxid.c_str(), false);

		// "name="
		if(string::npos == (pos = strInput.find(DUMP_KEY_CHMPX_NAME))){
			ERR("Could not found \"name=\" key in DUMP result.");
			is_error = true;
			return strInput;
		}
		strInput = strInput.substr(pos + strlen(DUMP_KEY_CHMPX_NAME));

		// Get CHMPX NAME
		if(string::npos == (pos = strInput.find(DUMP_KEY_CR))){
			ERR("Could not found CR after \"name=\" key in DUMP result.");
			is_error = true;
			return strInput;
		}
		name		= trim(strInput.substr(0, pos));
		strInput	= strInput.substr(pos + strlen(DUMP_KEY_CR));

		// "ctlport="
		if(string::npos == (pos = strInput.find(DUMP_KEY_CHMPX_CTLPORT))){
			ERR("Could not found \"ctlport=\" key in DUMP result.");
			is_error = true;
			return strInput;
		}
		strInput = strInput.substr(pos + strlen(DUMP_KEY_CHMPX_CTLPORT));

		// Get CTLPORT
		if(string::npos == (pos = strInput.find(DUMP_KEY_CR))){
			ERR("Could not found CR after \"ctlport=\" key in DUMP result.");
			is_error = true;
			return strInput;
		}
		strctrlport	= trim(strInput.substr(0, pos));
		ctrlport	= static_cast<short>(atoi(strctrlport.c_str()));
		strInput	= strInput.substr(pos + strlen(DUMP_KEY_CR));

		// "cuk="
		if(string::npos == (pos = strInput.find(DUMP_KEY_CHMPX_CUK))){
			MSG("Could not found \"cuk=\" key in DUMP result, then skip this...");
		}else{
			strInput = strInput.substr(pos + strlen(DUMP_KEY_CHMPX_CUK));

			// Get CUK
			if(string::npos == (pos = strInput.find(DUMP_KEY_CR))){
				ERR("Could not found CR after \"cuk=\" key in DUMP result.");
				is_error = true;
				return strInput;
			}
			cuk			= trim(strInput.substr(0, pos));
			strInput	= strInput.substr(pos + strlen(DUMP_KEY_CR));
		}

		// "custom_seed="
		if(string::npos == (pos = strInput.find(DUMP_KEY_CHMPX_CUSTOMSEED))){
			MSG("Could not found \"custom_seed=\" key in DUMP result, then skip this...");
		}else{
			strInput = strInput.substr(pos + strlen(DUMP_KEY_CHMPX_CUSTOMSEED));

			// Get custom id seed
			if(string::npos == (pos = strInput.find(DUMP_KEY_CR))){
				ERR("Could not found CR after \"custom_seed=\" key in DUMP result.");
				is_error = true;
				return strInput;
			}
			custom_seed	= trim(strInput.substr(0, pos));
			strInput	= strInput.substr(pos + strlen(DUMP_KEY_CR));
		}

		// "ctlendpoints="
		if(string::npos == (pos = strInput.find(DUMP_KEY_CHMPX_CTLEPS))){
			MSG("Could not found \"ctlendpoints=\" key in DUMP result, then skip this...");
		}else{
			strInput = strInput.substr(pos + strlen(DUMP_KEY_CHMPX_CTLEPS));

			// Get control endpoints
			if(string::npos == (pos = strInput.find(DUMP_KEY_CR))){
				ERR("Could not found CR after \"ctlendpoints=\" key in DUMP result.");
				is_error = true;
				return strInput;
			}
			ctlendpoints= trim(strInput.substr(0, pos));
			strInput	= strInput.substr(pos + strlen(DUMP_KEY_CR));
		}

		// "}\n}\n"
		if(string::npos == (pos = strInput.find(DUMP_KEY_CHMPX_END))){
			ERR("Could not found \"}}\" key in DUMP result.");
			is_error = true;
			return strInput;
		}
		strInput = strInput.substr(pos + strlen(DUMP_KEY_CHMPX_END));

		// Add nodes
		if(!add_chmpx_node(nodes, chmpxid, name, ctrlport, cuk, ctlendpoints, custom_seed, is_server)){
			ERR("Failed to add node(%s: %d)", name.c_str(), ctrlport);
			is_error = true;
			return strInput;
		}
	}
	// cppcheck-suppress stlIfStrFind
	if(0 != strInput.find(DUMP_KEY_END)){
		ERR("Could not found end of chmpx \"}\" key in DUMP result.");
		is_error = true;
		return strInput;
	}
	strInput = strInput.substr(strlen(DUMP_KEY_END));

	return strInput;
}

static bool AddNodesFromDumpResult(nodectrllist_t& nodes, const string& strDump)
{
	string				strParsed;
	string::size_type	pos;
	bool				is_error = false;

	// "chmpx_man{\n"
	if(string::npos == (pos = strDump.find(DUMP_KEY_CHMPX_MAN))){
		ERR("Could not found \"chmpx_man\" key in DUMP result.");
		return false;
	}
	strParsed = strDump.substr(pos + strlen(DUMP_KEY_CHMPX_MAN));

	// Cut space characters
	strParsed = CutSpaceCharacter(strParsed);

	// "chmpx_servers=0xXXXXX\n"
	if(string::npos == (pos = strParsed.find(DUMP_KEY_CHMPX_SERVERS))){
		WAN("Could not found \"chmpx_servers=\" key in DUMP result.");
	}else{
		strParsed = strParsed.substr(pos + strlen(DUMP_KEY_CHMPX_SERVERS));
		if(string::npos == (pos = strParsed.find(DUMP_KEY_CR))){
			ERR("Could not found CR after \"chmpx_servers=\" key in DUMP result.");
			return false;
		}
		strParsed = strParsed.substr(pos + strlen(DUMP_KEY_CR));

		// Parse server chmpxs
		strParsed = ParseChmpxListFromDumpResult(nodes, strParsed, true, is_error);
		if(is_error){
			ERR("Could not parse server chmpx.");
			return false;
		}
	}

	// "chmpx_slaves=0xXXXXX\n"
	if(string::npos == (pos = strParsed.find(DUMP_KEY_CHMPX_SLAVES))){
		WAN("Could not found \"chmpx_slaves=\" key in DUMP result.");
	}else{
		strParsed = strParsed.substr(pos + strlen(DUMP_KEY_CHMPX_SLAVES));
		if(string::npos == (pos = strParsed.find(DUMP_KEY_CR))){
			ERR("Could not found CR after \"chmpx_slaves=\" key in DUMP result.");
			return false;
		}
		strParsed = strParsed.substr(pos + strlen(DUMP_KEY_CR));

		// Parse slave chmpxs
		// cppcheck-suppress unreadVariable
		strParsed = ParseChmpxListFromDumpResult(nodes, strParsed, false, is_error);
		if(is_error){
			ERR("Could not parse server chmpx.");
			return false;
		}
	}
	return true;
}

static bool CreateDynaTargetChmpx(void)
{
	// make thread parameter
	dumpnodereslist_t	nodes;
	for(nodectrllist_t::const_iterator iter = InitialAllNodes.begin(); iter != InitialAllNodes.end(); ++iter){
		DUMPNODERES	node;
		node.chmpxid		= iter->GetChmpxId();
		node.hostname		= iter->GetHostname();
		node.ctrlport		= iter->GetCtrlport();
		node.cuk			= iter->GetCuk();
		node.ctlendpoints	= iter->GetCtlendpoints();
		node.custom_seed	= iter->GetCusomSeed();
		nodes.push_back(node);
	}

	// send DUMP
	if(!SendDumpCommandByAutoThreads(nodes)){
		PRN("ERROR: Internal error occurred.");
		return false;
	}

	// parse results
	for(dumpnodereslist_t::iterator res_iter = nodes.begin(); res_iter != nodes.end(); ++res_iter){
		if(res_iter->isError){
			MSG("Could not get DUMP data from %s:%d", res_iter->hostname.c_str(), res_iter->ctrlport);
			continue;
		}
		//MSG("Receive data : \n\n%s\n", res_iter->strResult.c_str());

		if(!AddNodesFromDumpResult(TargetNodes, res_iter->strResult)){
			ERR("Parse DUMP result from %s:%d", res_iter->hostname.c_str(), res_iter->ctrlport);
			continue;
		}
		//print_chmpx_all_nodes(TargetNodes);
	}
	//if(is_print_msg){
	//	print_chmpx_all_nodes(TargetNodes);
	//}
	return true;
}

//---------------------------------------------------------
// Utilities : create all node datas from all node's Dump result string
//---------------------------------------------------------
//
// utility for key
//
// Format
// 		"hostname(IP address)":"control port":"cuk":"custom_seed":"control endpoints..."
//
static string MakeMapKeyFromAll(const string& hostname, short ctrlport, const string& cuk, const string& ctlendpoints, const string& custom_seed)
{
	string			tghost;
	short			tgport;
	string			tgcuk;
	string			tgctleps;
	string			tgcs;
	NodeCtrlInfo	node;
	if(	find_chmpx_node_by_hostname(TargetNodes, hostname, ctrlport, cuk, ctlendpoints, custom_seed, node)		||
		find_chmpx_node_by_hostname(InitialAllNodes, hostname, ctrlport, cuk, ctlendpoints, custom_seed, node)	)
	{
		tghost		= node.GetHostname();
		tgport		= node.GetCtrlport();
		tgcuk		= node.GetCuk();
		tgctleps	= node.GetCtlendpoints();
		tgcs		= node.GetCusomSeed();
	}else{
		tghost		= hostname;
		tgport		= ctrlport;
		tgcuk		= cuk;
		tgctleps	= ctlendpoints;
		tgcs		= custom_seed;
	}
	string	hostall	= tghost + string(":") + to_string(tgport) + string(":") + tgcuk + string(":") + tgcs + string(":") + tgctleps;
	return hostall;
}

//
// utility for count of sockets
//
static long GetCountFromSockString(const string& sockval)
{
	string				one;
	string::size_type	pos;
	long				count = 0L;
	for(string tmpval = trim(sockval); !tmpval.empty(); ){
		if(string::npos != (pos = tmpval.find(","))){
			one		= trim(tmpval.substr(0, pos));
			tmpval	= trim(tmpval.substr(pos + 1));
		}else{
			one		= trim(tmpval);
			tmpval.clear();
		}
		if(one != "-1"){
			++count;
		}
	}
	return count;
}

//
// parse NODEUNITDATA from dump result
//
static string ParseUnitDataFromDumpResult(NODEUNITDATA& unitdata, chmpxid_t exceptchmpxid, bool is_in_array, bool is_sock_from, bool& is_found_except, bool& is_error, const string& strDump)
{
	string				strInput = strDump;
	string				strChmpxid;
	chmpxid_t			chmpxid;
	string				name;
	string				hash;
	string				pendinghash;
	string				strctrlport;
	short				ctrlport;
	string				cuk;
	string				ctlendpoints;
	string				custom_seed;
	string				strsocks;
	long				tosockcnt;
	long				fromsockcnt;
	string				strupdatetime;
	time_t				updatetime;
	string				strstatus;
	string::size_type	pos;

	// clear
	unitdata.clear();
	is_error		= true;
	is_found_except	= false;

	// "chmpx={\n"
	if(string::npos == (pos = strInput.find(is_in_array ? DUMP_KEY_CHMPX_START : DUMP_KEY_START))){
		ERR("Could not found \"chmpx{\" or \"{\" key in DUMP result.");
		return strInput;
	}
	strInput = strInput.substr(pos + strlen(is_in_array ? DUMP_KEY_CHMPX_START : DUMP_KEY_START));

	// Get "chmpxid="
	if(string::npos == (pos = strInput.find(DUMP_KEY_CHMPX_CHMPXID))){
		ERR("Could not found \"chmpxid=\" key in DUMP result.");
		return strInput;
	}
	strInput = strInput.substr(pos + strlen(DUMP_KEY_CHMPX_CHMPXID));
	if(string::npos == (pos = strInput.find(DUMP_KEY_CR))){
		ERR("Could not found CR after \"chmpxid=\" key in DUMP result.");
		return strInput;
	}
	strChmpxid	= trim(strInput.substr(0, pos));
	strInput	= strInput.substr(pos + strlen(DUMP_KEY_CR));
	if(2 >= strChmpxid.size()){
		ERR("\"chmpxid=\" value does not start \"0x\" in DUMP result.");
		return strInput;
	}
	strChmpxid	= trim(strChmpxid.substr(2));
	chmpxid		= cvt_string_to_number(strChmpxid.c_str(), false);

	// Get "name="
	if(string::npos == (pos = strInput.find(DUMP_KEY_CHMPX_NAME))){
		ERR("Could not found \"name=\" key in DUMP result.");
		return strInput;
	}
	strInput = strInput.substr(pos + strlen(DUMP_KEY_CHMPX_NAME));
	if(string::npos == (pos = strInput.find(DUMP_KEY_CR))){
		ERR("Could not found CR after \"name=\" key in DUMP result.");
		return strInput;
	}
	name		= trim(strInput.substr(0, pos));
	strInput	= strInput.substr(pos + strlen(DUMP_KEY_CR));

	// Get "base_hash="
	if(string::npos == (pos = strInput.find(DUMP_KEY_CHMPX_HASH))){
		ERR("Could not found \"base_hash=\" key in DUMP result.");
		return strInput;
	}
	strInput = strInput.substr(pos + strlen(DUMP_KEY_CHMPX_HASH));
	if(string::npos == (pos = strInput.find(DUMP_KEY_CR))){
		ERR("Could not found CR after \"base_hash=\" key in DUMP result.");
		return strInput;
	}
	hash		= trim(strInput.substr(0, pos));
	strInput	= strInput.substr(pos + strlen(DUMP_KEY_CR));

	// Get "pending_hash="
	if(string::npos == (pos = strInput.find(DUMP_KEY_CHMPX_PENDING))){
		ERR("Could not found \"pending_hash=\" key in DUMP result.");
		return strInput;
	}
	strInput = strInput.substr(pos + strlen(DUMP_KEY_CHMPX_PENDING));
	if(string::npos == (pos = strInput.find(DUMP_KEY_CR))){
		ERR("Could not found CR after \"pending_hash=\" key in DUMP result.");
		return strInput;
	}
	pendinghash	= trim(strInput.substr(0, pos));
	strInput	= strInput.substr(pos + strlen(DUMP_KEY_CR));

	// Get "ctlport="
	if(string::npos == (pos = strInput.find(DUMP_KEY_CHMPX_CTLPORT))){
		ERR("Could not found \"ctlport=\" key in DUMP result.");
		return strInput;
	}
	strInput = strInput.substr(pos + strlen(DUMP_KEY_CHMPX_CTLPORT));
	if(string::npos == (pos = strInput.find(DUMP_KEY_CR))){
		ERR("Could not found CR after \"ctlport=\" key in DUMP result.");
		return strInput;
	}
	strctrlport	= trim(strInput.substr(0, pos));
	ctrlport	= static_cast<short>(atoi(strctrlport.c_str()));
	strInput	= strInput.substr(pos + strlen(DUMP_KEY_CR));

	// Get "cuk="
	if(string::npos == (pos = strInput.find(DUMP_KEY_CHMPX_CUK))){
		MSG("Could not found \"cuk=\" key in DUMP result, then skip this...");
	}else{
		strInput = strInput.substr(pos + strlen(DUMP_KEY_CHMPX_CUK));
		if(string::npos == (pos = strInput.find(DUMP_KEY_CR))){
			ERR("Could not found CR after \"cuk=\" key in DUMP result.");
			return strInput;
		}
		cuk			= trim(strInput.substr(0, pos));
		strInput	= strInput.substr(pos + strlen(DUMP_KEY_CR));
	}

	// Get "custom_seed="
	if(string::npos == (pos = strInput.find(DUMP_KEY_CHMPX_CUSTOMSEED))){
		MSG("Could not found \"custom_seed=\" key in DUMP result, then skip this...");
	}else{
		strInput = strInput.substr(pos + strlen(DUMP_KEY_CHMPX_CUSTOMSEED));
		if(string::npos == (pos = strInput.find(DUMP_KEY_CR))){
			ERR("Could not found CR after \"custom_seed=\" key in DUMP result.");
			is_error = true;
			return strInput;
		}
		custom_seed	= trim(strInput.substr(0, pos));
		strInput	= strInput.substr(pos + strlen(DUMP_KEY_CR));
	}

	// Get "ctlendpoints="
	if(string::npos == (pos = strInput.find(DUMP_KEY_CHMPX_CTLEPS))){
		MSG("Could not found \"ctlendpoints=\" key in DUMP result, then skip this...");
	}else{
		strInput = strInput.substr(pos + strlen(DUMP_KEY_CHMPX_CTLEPS));
		if(string::npos == (pos = strInput.find(DUMP_KEY_CR))){
			ERR("Could not found CR after \"ctlendpoints=\" key in DUMP result.");
			is_error = true;
			return strInput;
		}
		ctlendpoints= trim(strInput.substr(0, pos));
		strInput	= strInput.substr(pos + strlen(DUMP_KEY_CR));
	}

	// Get "sock="
	if(string::npos == (pos = strInput.find(DUMP_KEY_CHMPX_SOCK))){
		ERR("Could not found \"sock=\" key in DUMP result.");
		return strInput;
	}
	strInput = strInput.substr(pos + strlen(DUMP_KEY_CHMPX_SOCK));
	if(string::npos == (pos = strInput.find(DUMP_KEY_CR))){
		ERR("Could not found CR after \"sock=\" key in DUMP result.");
		return strInput;
	}
	strsocks		= trim(strInput.substr(0, pos));
	if(is_sock_from){
		tosockcnt	= 0;
		fromsockcnt	= GetCountFromSockString(strsocks);
	}else{
		tosockcnt	= GetCountFromSockString(strsocks);
		fromsockcnt	= 0;
	}
	strInput		= strInput.substr(pos + strlen(DUMP_KEY_CR));

	// Get "last_status_time="
	if(string::npos == (pos = strInput.find(DUMP_KEY_CHMPX_LASTTIME))){
		ERR("Could not found \"last_status_time=\" key in DUMP result.");
		return strInput;
	}
	strInput = strInput.substr(pos + strlen(DUMP_KEY_CHMPX_LASTTIME));
	if(string::npos == (pos = strInput.find(DUMP_KEY_CR))){
		ERR("Could not found CR after \"last_status_time=\" key in DUMP result.");
		return strInput;
	}
	strupdatetime	= trim(strInput.substr(0, pos));
	updatetime		= static_cast<time_t>(atoll(strupdatetime.c_str()));
	strInput		= strInput.substr(pos + strlen(DUMP_KEY_CR));

	// Get "status=SAFE->"
	if(string::npos == (pos = strInput.find(DUMP_KEY_CHMPX_STATUS))){
		ERR("Could not found \"status=\" key in DUMP result.");
		return strInput;
	}
	strInput = strInput.substr(pos + strlen(DUMP_KEY_CHMPX_STATUS));
	if(string::npos == (pos = strInput.find(DUMP_KEY_CHMPX_PREFIXSTATUS))){
		ERR("Could not found \"status=SAFE->\" key in DUMP result.");
		return strInput;
	}
	strInput = strInput.substr(pos + strlen(DUMP_KEY_CHMPX_PREFIXSTATUS));
	if(string::npos == (pos = strInput.find(DUMP_KEY_CR))){
		ERR("Could not found CR after \"status=SAFE->\" key in DUMP result.");
		return strInput;
	}
	strstatus		= trim(strInput.substr(0, pos));
	strInput		= strInput.substr(pos + strlen(DUMP_KEY_CR));

	// "}\n}\n" or "}\n"
	if(string::npos == (pos = strInput.find(is_in_array ? DUMP_KEY_CHMPX_END : DUMP_KEY_END))){
		ERR("Could not found \"}}\" or \"}\" key in DUMP result.");
		return strInput;
	}
	strInput = strInput.substr(pos + strlen(is_in_array ? DUMP_KEY_CHMPX_END : DUMP_KEY_END));

	//
	// succeed parsing dump result
	//
	is_error = false;

	// check same host(only server mode)
	if(CHM_INVALID_CHMPXID != exceptchmpxid && chmpxid == exceptchmpxid){
		is_found_except = true;
	}else{
		// set result
		unitdata.is_down		= false;
		unitdata.chmpxid		= chmpxid;
		unitdata.hostname		= HostnameMap::GetFirstHostname(name.c_str());
		unitdata.ctrlport		= ctrlport;
		unitdata.cuk			= cuk;
		unitdata.ctlendpoints	= ctlendpoints;
		unitdata.custom_seed	= custom_seed;
		unitdata.hash			= hash;
		unitdata.pendinghash	= pendinghash;
		unitdata.tosockcnt		= tosockcnt;
		unitdata.fromsockcnt	= fromsockcnt;
		unitdata.lastupdatetime	= updatetime;
		unitdata.status			= strstatus;
	}
	return strInput;
}

//
// parse NODEUNITDATA map from dump result
//
static string ParseUnitDatasFromDumpResult(nodesunits_t& unitdatas, chmpxid_t exceptchmpxid, bool is_sock_from, const string& strDump)
{
	string				strInput = strDump;
	string::size_type	pos;

	// must start with "{\n"
	if(string::npos == (pos = strInput.find(DUMP_KEY_START))){
		ERR("Could not found \"{\" key in DUMP result.");
		return strInput;
	}
	strInput = strInput.substr(pos + strlen(DUMP_KEY_START));

	// Loop to "}\n"
	// cppcheck-suppress stlIfStrFind
	while(0 != strInput.find(DUMP_KEY_END) && string::npos != strInput.find(DUMP_KEY_END)){
		// "[XX]={\n"
		// cppcheck-suppress stlIfStrFind
		if(string::npos == strInput.find(DUMP_KEY_ARRAY_START) || 0 != strInput.find(DUMP_KEY_ARRAY_START)){
			//MSG("Could not found \"[XX]={\" key or found invalid data in DUMP result.");
			return strInput;
		}
		strInput = strInput.substr(strlen(DUMP_KEY_ARRAY_START));
		if(string::npos == (pos = strInput.find(DUMP_KEY_ARRAY_END))){
			ERR("Could not found \"[XX]={\" key in DUMP result.");
			return strInput;
		}
		strInput = strInput.substr(pos + strlen(DUMP_KEY_ARRAY_END));

		// parse one unit data
		NODEUNITDATA	unitdata;
		bool			is_found= false;
		bool			is_error= false;
		strInput				= ParseUnitDataFromDumpResult(unitdata, exceptchmpxid, true, is_sock_from, is_found, is_error, strInput);
		if(is_error){
			continue;
		}
		if(is_found){
			continue;
		}
		// set unit data result
		string		mapkey	= MakeMapKeyFromAll(unitdata.hostname, unitdata.ctrlport, unitdata.cuk, unitdata.ctlendpoints, unitdata.custom_seed);
		unitdatas[mapkey]	= unitdata;
	}
	// cppcheck-suppress stlIfStrFind
	if(0 != strInput.find(DUMP_KEY_END)){
		ERR("Could not found end of chmpx \"}\" key in DUMP result.");
		return strInput;
	}
	strInput = strInput.substr(strlen(DUMP_KEY_END));

	return strInput;
}

//
// parse NODESTATUSDETAIL from dump result
//
static bool CreateStatusDetails(NODESTATUSDETAIL& detail, chmpxid_t chmpxid, const string& strDump)
{
	string				strParsed;
	string				strChmpxCount;
	bool				is_found_except	= false;			// not used this value
	bool				is_error		= false;
	string::size_type	pos;

	// clear
	detail.clear();

	// "chmpx_man{\n"
	if(string::npos == (pos = strDump.find(DUMP_KEY_CHMPX_MAN))){
		ERR("Could not found \"chmpx_man\" key in DUMP result.");
		return false;
	}
	strParsed = strDump.substr(pos + strlen(DUMP_KEY_CHMPX_MAN));

	// Cut space characters
	strParsed = CutSpaceCharacter(strParsed);

	// "chmpx_self=0xXXXXX\n"
	if(string::npos == (pos = strParsed.find(DUMP_KEY_CHMPX_SELF))){
		ERR("Could not found \"chmpx_self=\" key in DUMP result.");
		return false;
	}
	strParsed = strParsed.substr(pos + strlen(DUMP_KEY_CHMPX_SELF));
	if(string::npos == (pos = strParsed.find(DUMP_KEY_CR))){
		ERR("Could not found CR after \"chmpx_self=\" key in DUMP result.");
		return false;
	}
	strParsed	= strParsed.substr(pos + strlen(DUMP_KEY_CR));

	// get self chmpx information
	//
	// we call this function with no except chmpxid, then it is always not found.
	// the is_sock_from parameter is true as temporary, "fromsockcnt" should be 0.
	//
	strParsed	= ParseUnitDataFromDumpResult(detail.self, CHM_INVALID_CHMPXID, false, true, is_found_except, is_error, strParsed);
	if(is_error){
		ERR("Could not parse self chmpx by chmpxid(0x%016" PRIx64 ")", chmpxid);
		return false;
	}
	if(chmpxid != detail.self.chmpxid){
		ERR("Parsed self chmpx expected chmpxid(0x%016" PRIx64 "), but it has chmpxid(0x%016" PRIx64 ").", chmpxid, detail.self.chmpxid);
		return false;
	}
	chmpxid_t	exceptchmpxid = detail.self.chmpxid;

	// "chmpx_servers=0xXXXXX\n"
	if(string::npos == (pos = strParsed.find(DUMP_KEY_CHMPX_SERVERS))){
		WAN("Could not found \"chmpx_servers=\" key in DUMP result.");
	}else{
		strParsed = strParsed.substr(pos + strlen(DUMP_KEY_CHMPX_SERVERS));
		if(string::npos == (pos = strParsed.find(DUMP_KEY_CR))){
			ERR("Could not found CR after \"chmpx_servers=\" key in DUMP result.");
			return false;
		}
		strChmpxCount	= trim(strParsed.substr(0, pos));
		strParsed		= strParsed.substr(pos + strlen(DUMP_KEY_CR));
		if(strChmpxCount != "0"){
			// Parse server chmpxs
			strParsed = ParseUnitDatasFromDumpResult(detail.servers, exceptchmpxid, true, strParsed);
		}
	}

	// "chmpx_slaves=0xXXXXX\n"
	if(string::npos == (pos = strParsed.find(DUMP_KEY_CHMPX_SLAVES))){
		WAN("Could not found \"chmpx_slaves=\" key in DUMP result.");
	}else{
		strParsed = strParsed.substr(pos + strlen(DUMP_KEY_CHMPX_SLAVES));
		if(string::npos == (pos = strParsed.find(DUMP_KEY_CR))){
			ERR("Could not found CR after \"chmpx_slaves=\" key in DUMP result.");
			return false;
		}
		strChmpxCount	= trim(strParsed.substr(0, pos));
		strParsed		= strParsed.substr(pos + strlen(DUMP_KEY_CR));
		if(strChmpxCount != "0"){
			// Parse slave chmpxs
			// cppcheck-suppress unreadVariable
			strParsed = ParseUnitDatasFromDumpResult(detail.slaves, exceptchmpxid, false, strParsed);
		}
	}

	//
	// count sockets for self from servers/slaves structures
	//
	detail.self.tosockcnt	= 0;
	detail.self.fromsockcnt	= 0;

	nodesunits_t::const_iterator	iter;
	for(iter = detail.servers.begin(); iter != detail.servers.end(); ++iter){
		detail.self.fromsockcnt += iter->second.fromsockcnt;
	}
	for(iter = detail.slaves.begin(); iter != detail.slaves.end(); ++iter){
		detail.self.tosockcnt += iter->second.tosockcnt;
	}

	return true;
}

//
// parse statusdetails_t from all nodes dump result
//
static size_t CreateAllStatusDetails(statusdetails_t& all)
{
	all.clear();

	// make thread parameter
	dumpnodereslist_t	nodes;
	for(nodectrllist_t::const_iterator iter = TargetNodes.begin(); iter != TargetNodes.end(); ++iter){
		DUMPNODERES	node;
		node.chmpxid		= iter->GetChmpxId();
		node.hostname		= iter->GetHostname();
		node.ctrlport		= iter->GetCtrlport();
		node.cuk			= iter->GetCuk();
		node.ctlendpoints	= iter->GetCtlendpoints();
		node.custom_seed	= iter->GetCusomSeed();
		nodes.push_back(node);
	}

	// send DUMP
	if(!SendDumpCommandByAutoThreads(nodes)){
		PRN("ERROR: Internal error occurred.");
		return all.size();
	}

	// parse results
	for(dumpnodereslist_t::const_iterator res_iter = nodes.begin(); res_iter != nodes.end(); ++res_iter){
		NODESTATUSDETAIL	detail;
		if(res_iter->isError){
			MSG("Could not get DUMP data from %s:%d", res_iter->hostname.c_str(), res_iter->ctrlport);
			detail.self.is_down	= true;
		}else{
			//MSG("Receive data : \n\n%s\n", res_iter->strResult.c_str());
			if(!CreateStatusDetails(detail, res_iter->chmpxid, res_iter->strResult)){
				ERR("Parse DUMP result from %s:%d", res_iter->hostname.c_str(), res_iter->ctrlport);
				continue;
			}
		}
		// make key
		string	mapkey	= MakeMapKeyFromAll(res_iter->hostname, res_iter->ctrlport, res_iter->cuk, res_iter->ctlendpoints, res_iter->custom_seed);
		// add to all
		all[mapkey]		= detail;
	}
	return all.size();
}

//
// Utilities : Parse status string
//
static string ParseOnePartFromStatusString(string& str)
{
	string				result;
	string::size_type	pos;
	if(string::npos == (pos = str.find(STATUS_PART_PREFIX))){
		str.clear();
		return result;
	}else{
		str = str.substr(pos + strlen(STATUS_PART_PREFIX));
		if(string::npos == (pos = str.find(STATUS_PART_SUFFIX))){
			result	= str;
			str.clear();
			return result;
		}else{
			result	= trim(str.substr(0, pos));
			str		= str.substr(pos + strlen(STATUS_PART_SUFFIX));
		}
	}
	return result;
}

//
// check status and make status checking result
//
static CHKRESULT_TYPE MakeStatusCheckResult(const string& strSrc, const string& strDst, CHKRESULT_TYPE& ring, CHKRESULT_TYPE& live, CHKRESULT_TYPE& act, CHKRESULT_TYPE& opr, CHKRESULT_TYPE& sus)
{
	CHKRESULT_TYPE	total 		= CHKRESULT_NOERR;
	string			tmpSrc		= strSrc;
	string			tmpDst		= strDst;
	string			tmpSrcPart;
	string			tmpDstPart;

	// ring
	tmpSrcPart = ParseOnePartFromStatusString(tmpSrc);
	tmpDstPart = ParseOnePartFromStatusString(tmpDst);
	if(tmpSrcPart != tmpDstPart){
		total	|= CHKRESULT_ERR;
		ring	= CHKRESULT_ERR;
	}else{
		ring	= CHKRESULT_NOERR;
	}
	// live
	tmpSrcPart = ParseOnePartFromStatusString(tmpSrc);
	tmpDstPart = ParseOnePartFromStatusString(tmpDst);
	if(tmpSrcPart != tmpDstPart){
		total	|= CHKRESULT_ERR;
		live	= CHKRESULT_ERR;
	}else{
		live	= CHKRESULT_NOERR;
	}
	// action
	tmpSrcPart = ParseOnePartFromStatusString(tmpSrc);
	tmpDstPart = ParseOnePartFromStatusString(tmpDst);
	if(tmpSrcPart != tmpDstPart){
		total	|= CHKRESULT_ERR;
		act		= CHKRESULT_ERR;
	}else{
		act		= CHKRESULT_NOERR;
	}
	// opr
	tmpSrcPart = ParseOnePartFromStatusString(tmpSrc);
	tmpDstPart = ParseOnePartFromStatusString(tmpDst);
	if(tmpSrcPart != tmpDstPart){
		total	|= CHKRESULT_ERR;
		opr		= CHKRESULT_ERR;
	}else{
		opr		= CHKRESULT_NOERR;
	}
	// suspend
	tmpSrcPart = ParseOnePartFromStatusString(tmpSrc);
	tmpDstPart = ParseOnePartFromStatusString(tmpDst);
	if(tmpSrcPart != tmpDstPart){
		total	|= CHKRESULT_ERR;
		sus		= CHKRESULT_ERR;
	}else{
		sus		= CHKRESULT_NOERR;
	}

	return total;
}

//
// Utility: search and make unit for down node
//
static bool MakeNodesByHostportFromAll(const string& tghostport, const statusdetails_t& all, nodesunits_t& servers, nodesunits_t& slaves)
{
	// loop in all
	for(statusdetails_t::const_iterator iter_main = all.begin(); iter_main != all.end(); ++iter_main){
		//
		// check node(A)
		//
		NODEUNITDATA					tg_child_node;
		nodesunits_t::const_iterator	child_iter;
		bool							is_set		= false;
		bool							is_server	= false;

		// search target node in node(A)'s servers
		child_iter = iter_main->second.servers.find(tghostport);
		if(child_iter != iter_main->second.servers.end()){
			// found target node in node(A)' servers
			tg_child_node				= child_iter->second;
			tg_child_node.tosockcnt		= tg_child_node.fromsockcnt;
			tg_child_node.fromsockcnt	= 0;
			is_set						= true;
			is_server					= true;
		}
		// search target node in node(A)'s slaves
		child_iter = iter_main->second.slaves.find(tghostport);
		if(child_iter != iter_main->second.slaves.end()){
			if(!is_set){
				tg_child_node				= child_iter->second;
				tg_child_node.fromsockcnt	= tg_child_node.tosockcnt;
				tg_child_node.tosockcnt		= 0;
				is_set						= true;
			}else{
				tg_child_node.fromsockcnt	= child_iter->second.tosockcnt;
			}
		}
		if(!is_set){
			// if not found target node in node(A)'s servers/slaves.
			continue;
		}
		// set
		if(is_server){
			servers[iter_main->first]	= tg_child_node;
		}else{
			slaves[iter_main->first]	= tg_child_node;
		}
	}
	return true;
}

//
// make nodechkresults_t from statusdetails_t
//
static size_t MakeCheckNodeStatus(nodechkresults_t& results, const statusdetails_t& all, const string& hostname, short ctrlport, const string& cuk, const string& ctlendpoints, const string& custom_seed)
{
	bool	is_one_target	= !hostname.empty();
	string	mapkey	= MakeMapKeyFromAll(hostname, ctrlport, cuk, ctlendpoints, custom_seed);
	results.clear();

	//
	// At first, make basic results(without all results of checking)
	//
	// [NOTE]
	// Rebuild results mapping from all, the mapping keys are node name and values are another
	// node's viewing to key node.
	//
	for(statusdetails_t::const_iterator iter_main = all.begin(); iter_main != all.end(); ++iter_main){
		//
		// loop main node(A)
		//
		if(is_one_target && iter_main->first != mapkey){
			continue;
		}

		//
		// copy node(A) to self in results
		//
		NODECHECKRESULT	tgresult;
		tgresult.clear();
		tgresult.all.self = iter_main->second.self;

		if(iter_main->second.self.is_down){
			// node(A) is down

			//
			// search node(A) data from all node's servers/slaves
			//
			MakeNodesByHostportFromAll(iter_main->first, all, tgresult.all.servers, tgresult.all.slaves);

		}else{
			// node(A) is up
			//
			// copy node(A) data from node(B)'s view in node(A)'s servers
			//
			for(nodesunits_t::const_iterator tg_svr = iter_main->second.servers.begin(); tg_svr != iter_main->second.servers.end(); ++tg_svr){
				//
				// node(B) which is listed in nodes(A)'s servers
				//
				NODEUNITDATA					tg_child_node;
				statusdetails_t::const_iterator	child_iter	= all.find(tg_svr->first);			// search node(B) in all
				if(child_iter != all.end()){
					//
					// node(B) which is listed in all
					//
					if(child_iter->second.self.is_down){
						// node(B) is down now, then skip this.
						continue;
					}

					// search node(A) in node(B)'s servers
					bool							is_set = false;
					nodesunits_t::const_iterator	child_svr = child_iter->second.servers.find(iter_main->first);
					if(child_svr != child_iter->second.servers.end()){
						tg_child_node				= child_svr->second;
						tg_child_node.tosockcnt		= tg_child_node.fromsockcnt;
						tg_child_node.fromsockcnt	= 0;
						is_set						= true;
					}
					// search node(A) in node(B)'s slaves
					nodesunits_t::const_iterator	child_slv = child_iter->second.slaves.find(iter_main->first);
					if(child_slv != child_iter->second.slaves.end()){
						if(!is_set){
							tg_child_node				= child_slv->second;
							tg_child_node.fromsockcnt	= tg_child_node.tosockcnt;
							tg_child_node.tosockcnt		= 0;
							is_set						= true;
						}else{
							tg_child_node.fromsockcnt	= child_slv->second.tosockcnt;
						}
					}
					// if not found node(A) in node(B)'s servers/slaves.
					if(!is_set){
						tg_child_node.clear();
					}

					// switch hostname and port
					tg_child_node.chmpxid		= child_iter->second.self.chmpxid;
					tg_child_node.hostname		= HostnameMap::GetFirstHostname(child_iter->second.self.hostname.c_str());
					tg_child_node.ctrlport		= child_iter->second.self.ctrlport;
					tg_child_node.cuk			= child_iter->second.self.cuk;
					tg_child_node.ctlendpoints	= child_iter->second.self.ctlendpoints;
					tg_child_node.custom_seed	= child_iter->second.self.custom_seed;

				}else{
					// if not found node(B) in all.
					tg_child_node.clear();
					tg_child_node.chmpxid		= tg_svr->second.chmpxid;
					tg_child_node.hostname		= HostnameMap::GetFirstHostname(tg_svr->second.hostname.c_str());
					tg_child_node.ctrlport		= tg_svr->second.ctrlport;
					tg_child_node.cuk			= tg_svr->second.cuk;
					tg_child_node.ctlendpoints	= tg_svr->second.ctlendpoints;
					tg_child_node.custom_seed	= tg_svr->second.custom_seed;
				}

				// set node(B) to node(A)'s server nodes
				tgresult.all.servers[tg_svr->first] = tg_child_node;
			}

			//
			// copy node(A) data from node(C)'s view in node(A)'s slaves
			//
			for(nodesunits_t::const_iterator tg_slv = iter_main->second.slaves.begin(); tg_slv != iter_main->second.slaves.end(); ++tg_slv){
				//
				// node(C) which is listed in nodes(A)'s servers
				//
				if(tgresult.all.servers.end() != tgresult.all.servers.find(tg_slv->first)){
					// already set node(C) as node(B) in tgresult.all.servers
					continue;
				}
				NODEUNITDATA					tg_child_node;
				statusdetails_t::const_iterator	child_iter	= all.find(tg_slv->first);			// search node(C) in all
				if(child_iter != all.end()){
					//
					// node(C) which is listed in all
					//
					if(child_iter->second.self.is_down){
						// node(C) is down now, then skip this.
						continue;
					}

					// search node(A) in node(C)'s servers
					bool							is_set		= false;
					nodesunits_t::const_iterator	child_svr = child_iter->second.servers.find(iter_main->first);
					if(child_svr != child_iter->second.servers.end()){
						tg_child_node				= child_svr->second;
						tg_child_node.tosockcnt		= tg_child_node.fromsockcnt;
						tg_child_node.fromsockcnt	= 0;
						is_set						= true;
					}
					// search node(A) in node(C)'s slaves
					nodesunits_t::const_iterator	child_slv = child_iter->second.slaves.find(iter_main->first);
					if(child_slv != child_iter->second.slaves.end()){
						if(!is_set){
							tg_child_node				= child_slv->second;
							tg_child_node.fromsockcnt	= tg_child_node.tosockcnt;
							tg_child_node.tosockcnt		= 0;
							is_set						= true;
						}else{
							tg_child_node.fromsockcnt	= child_slv->second.tosockcnt;
						}
					}
					// if not found node(A) in node(C)'s servers/slaves.
					if(!is_set){
						tg_child_node.clear();
					}

					// switch hostname and port
					tg_child_node.chmpxid		= child_iter->second.self.chmpxid;
					tg_child_node.hostname		= HostnameMap::GetFirstHostname(child_iter->second.self.hostname.c_str());
					tg_child_node.ctrlport		= child_iter->second.self.ctrlport;
					tg_child_node.cuk			= child_iter->second.self.cuk;
					tg_child_node.ctlendpoints	= child_iter->second.self.ctlendpoints;
					tg_child_node.custom_seed	= child_iter->second.self.custom_seed;

				}else{
					// if not found node(C) in all.
					tg_child_node.clear();
					tg_child_node.chmpxid		= tg_slv->second.chmpxid;
					tg_child_node.hostname		= HostnameMap::GetFirstHostname(tg_slv->second.hostname.c_str());
					tg_child_node.ctrlport		= tg_slv->second.ctrlport;
					tg_child_node.cuk			= tg_slv->second.cuk;
					tg_child_node.ctlendpoints	= tg_slv->second.ctlendpoints;
					tg_child_node.custom_seed	= tg_slv->second.custom_seed;
				}

				// set node(C) to node(A)'s slave nodes
				tgresult.all.slaves[tg_slv->first] = tg_child_node;
			}
		}

		// add node(A) to results map
		results[iter_main->first] = tgresult;
	}

	//
	// Check all
	//
	for(nodechkresults_t::iterator iter = results.begin(); iter != results.end(); ++iter){
		nodesunits_t::iterator	tg_svr;
		nodesunits_t::iterator	tg_slv;
		long					tosockcnt	= 0L;
		long					fromsockcnt	= 0L;

		// count sockets
		for(tg_svr = iter->second.all.servers.begin(); tg_svr != iter->second.all.servers.end(); ++tg_svr){
			tosockcnt	+= tg_svr->second.tosockcnt;
			fromsockcnt	+= tg_svr->second.fromsockcnt;
		}
		for(tg_slv = iter->second.all.slaves.begin(); tg_slv != iter->second.all.slaves.end(); ++tg_slv){
			tosockcnt	+= tg_slv->second.tosockcnt;
			fromsockcnt	+= tg_slv->second.fromsockcnt;
		}

		// check socket count
		if(iter->second.all.self.tosockcnt != tosockcnt){
			iter->second.noderesult								= CHKRESULT_ERR;
			iter->second.all.self.checkresult.result_tosockcnt	= CHKRESULT_ERR;
		}
		if(iter->second.all.self.fromsockcnt != fromsockcnt){
			iter->second.noderesult								= CHKRESULT_ERR;
			iter->second.all.self.checkresult.result_fromsockcnt= CHKRESULT_ERR;
		}

		//
		// [NOTICE]	if target node is down, we set status and hash to self node
		//			from one of server/slave.
		//			thus the result of comparing node datas is no difference.
		//
		if(iter->second.all.self.is_down){
			//
			// target node is down, then we set status/hash to self from one of nodes
			//
			if(iter->second.all.servers.end() != (tg_svr = iter->second.all.servers.begin())){
				iter->second.all.self.hash			= tg_svr->second.hash;
				iter->second.all.self.pendinghash	= tg_svr->second.pendinghash;
				iter->second.all.self.status		= tg_svr->second.status;
			}else if(iter->second.all.slaves.end() != (tg_slv = iter->second.all.slaves.begin())){
				iter->second.all.self.hash			= tg_slv->second.hash;
				iter->second.all.self.pendinghash	= tg_slv->second.pendinghash;
				iter->second.all.self.status		= tg_slv->second.status;
			}
		}

		// check rest data
		for(tg_svr = iter->second.all.servers.begin(); tg_svr != iter->second.all.servers.end(); ++tg_svr){
			// make status result
			CHKRESULT_TYPE	total;
			CHKRESULT_TYPE	ring;
			CHKRESULT_TYPE	live;
			CHKRESULT_TYPE	act;
			CHKRESULT_TYPE	opr;
			CHKRESULT_TYPE	sus;
			total = MakeStatusCheckResult(tg_svr->second.status, iter->second.all.self.status, ring, live, act, opr, sus);

			iter->second.noderesult									|= total;
			iter->second.all.self.checkresult.result_status			|= total;
			iter->second.all.self.checkresult.result_status_ring	|= ring;
			iter->second.all.self.checkresult.result_status_live	|= live;
			iter->second.all.self.checkresult.result_status_act		|= act;
			iter->second.all.self.checkresult.result_status_opr		|= opr;
			iter->second.all.self.checkresult.result_status_sus		|= sus;
			tg_svr->second.checkresult.result_status				= total;
			tg_svr->second.checkresult.result_status_ring			= ring;
			tg_svr->second.checkresult.result_status_live			= live;
			tg_svr->second.checkresult.result_status_act			= act;
			tg_svr->second.checkresult.result_status_opr			= opr;
			tg_svr->second.checkresult.result_status_sus			= sus;

			if(tg_svr->second.hash != iter->second.all.self.hash){
				iter->second.noderesult									|= CHKRESULT_ERR;
				iter->second.all.self.checkresult.result_hash			|= CHKRESULT_ERR;
				tg_svr->second.checkresult.result_hash					= CHKRESULT_ERR;
			}
			if(tg_svr->second.pendinghash != iter->second.all.self.pendinghash){
				iter->second.noderesult									|= CHKRESULT_WARN;
				iter->second.all.self.checkresult.result_pending		|= CHKRESULT_WARN;
				tg_svr->second.checkresult.result_pending				= CHKRESULT_WARN;
			}
			//
			// [NOTE] last update time is different normally
			//
			//if(tg_svr->second.lastupdatetime != iter->second.all.self.lastupdatetime){
			//	iter->second.noderesult									|= CHKRESULT_WARN;
			//	iter->second.all.self.checkresult.result_lastupdatetime	|= CHKRESULT_WARN;
			//	tg_svr->second.checkresult.result_lastupdatetime		= CHKRESULT_WARN;
			//}
			tg_svr->second.checkresult.result_tosockcnt					= iter->second.all.self.checkresult.result_tosockcnt;
			tg_svr->second.checkresult.result_fromsockcnt				= iter->second.all.self.checkresult.result_fromsockcnt;
		}
		for(tg_slv = iter->second.all.slaves.begin(); tg_slv != iter->second.all.slaves.end(); ++tg_slv){
			// make status result
			CHKRESULT_TYPE	total;
			CHKRESULT_TYPE	ring;
			CHKRESULT_TYPE	live;
			CHKRESULT_TYPE	act;
			CHKRESULT_TYPE	opr;
			CHKRESULT_TYPE	sus;
			total = MakeStatusCheckResult(tg_slv->second.status, iter->second.all.self.status, ring, live, act, opr, sus);

			iter->second.noderesult									|= total;
			iter->second.all.self.checkresult.result_status			|= total;
			iter->second.all.self.checkresult.result_status_ring	|= ring;
			iter->second.all.self.checkresult.result_status_live	|= live;
			iter->second.all.self.checkresult.result_status_act		|= act;
			iter->second.all.self.checkresult.result_status_opr		|= opr;
			iter->second.all.self.checkresult.result_status_sus		|= sus;
			tg_slv->second.checkresult.result_status				= total;
			tg_slv->second.checkresult.result_status_ring			= ring;
			tg_slv->second.checkresult.result_status_live			= live;
			tg_slv->second.checkresult.result_status_act			= act;
			tg_slv->second.checkresult.result_status_opr			= opr;
			tg_slv->second.checkresult.result_status_sus			= sus;

			if(tg_slv->second.hash != iter->second.all.self.hash){
				iter->second.noderesult									|= CHKRESULT_ERR;
				iter->second.all.self.checkresult.result_hash			|= CHKRESULT_ERR;
				tg_slv->second.checkresult.result_hash					= CHKRESULT_ERR;
			}
			if(tg_slv->second.pendinghash != iter->second.all.self.pendinghash){
				iter->second.noderesult									|= CHKRESULT_WARN;
				iter->second.all.self.checkresult.result_pending		|= CHKRESULT_WARN;
				tg_slv->second.checkresult.result_pending				= CHKRESULT_WARN;
			}
			//
			// [NOTE] last update time is different normally
			//
			//if(tg_slv->second.lastupdatetime != iter->second.all.self.lastupdatetime){
			//	iter->second.noderesult									|= CHKRESULT_WARN;
			//	iter->second.all.self.checkresult.result_lastupdatetime	|= CHKRESULT_WARN;
			//	tg_slv->second.checkresult.result_lastupdatetime		= CHKRESULT_WARN;
			//}
			tg_slv->second.checkresult.result_tosockcnt					= iter->second.all.self.checkresult.result_tosockcnt;
			tg_slv->second.checkresult.result_fromsockcnt				= iter->second.all.self.checkresult.result_fromsockcnt;
		}
	}
	return results.size();
}

//
// dump CHKRESULT_TYPE
//
static inline string GetCheckResultString(const CHKRESULT_TYPE& type)
{
	string	strtype;
	if(CHKRESULT_NOERR  == type){
		strtype = "No error";
	}else if(CHKRESULT_WARN == type){
		strtype = "Warning";
	}else if(CHKRESULT_ERR == type){
		strtype = "Error";
	}else{
		strtype = "Unknown";
	}
	return strtype;
}

//
// dump CHKRESULT_PART
//
static void DumpCheckResultPart(const CHKRESULT_PART& chkresult, const string& prefix, const string& index)
{
	if(!is_print_dmp){
		return;
	}

	PRN("%s%s = {",							index.c_str(), prefix.empty() ? "CHKRESULT_PART" : prefix.c_str());
	PRN("%s  result_status         = %s",	index.c_str(), GetCheckResultString(chkresult.result_status).c_str());
	PRN("%s  result_status_ring    = %s",	index.c_str(), GetCheckResultString(chkresult.result_status_ring).c_str());
	PRN("%s  result_status_live    = %s",	index.c_str(), GetCheckResultString(chkresult.result_status_live).c_str());
	PRN("%s  result_status_act     = %s",	index.c_str(), GetCheckResultString(chkresult.result_status_act).c_str());
	PRN("%s  result_status_opr     = %s",	index.c_str(), GetCheckResultString(chkresult.result_status_opr).c_str());
	PRN("%s  result_status_sus     = %s",	index.c_str(), GetCheckResultString(chkresult.result_status_sus).c_str());
	PRN("%s  result_hash           = %s",	index.c_str(), GetCheckResultString(chkresult.result_hash).c_str());
	PRN("%s  result_pending        = %s",	index.c_str(), GetCheckResultString(chkresult.result_pending).c_str());
	PRN("%s  result_tosockcnt      = %s",	index.c_str(), GetCheckResultString(chkresult.result_tosockcnt).c_str());
	PRN("%s  result_fromsockcnt    = %s",	index.c_str(), GetCheckResultString(chkresult.result_fromsockcnt).c_str());
	PRN("%s  result_lastupdatetime = %s",	index.c_str(), GetCheckResultString(chkresult.result_lastupdatetime).c_str());
	PRN("%s}",								index.c_str());
}

//
// dump NODEUNITDATA
//
static void DumpNodeUnitData(const NODEUNITDATA& data, const string& prefix, const string& index, bool is_result_part)
{
	if(!is_print_dmp){
		return;
	}
	PRN("%s%s = {",								index.c_str(), prefix.empty() ? "NODEUNITDATA" : prefix.c_str());
	PRN("%s  node is        = %s",				index.c_str(), data.is_down ? "down" : "up");
	PRN("%s  chmpxid        = 0x%016" PRIx64,	index.c_str(), data.chmpxid);
	PRN("%s  hostname       = %s",				index.c_str(), data.hostname.c_str());
	PRN("%s  ctlport        = %d",				index.c_str(), data.ctrlport);
	PRN("%s  cuk            = %s",				index.c_str(), data.cuk.c_str());
	PRN("%s  ctlendpoints   = %s",				index.c_str(), data.ctlendpoints.c_str());
	PRN("%s  custom_seed    = %s",				index.c_str(), data.custom_seed.c_str());
	PRN("%s  hash           = %s",				index.c_str(), data.hash.c_str());
	PRN("%s  pendinghash    = %s",				index.c_str(), data.pendinghash.c_str());
	PRN("%s  tosockcnt      = %ld",				index.c_str(), data.tosockcnt);
	PRN("%s  fromsockcnt    = %ld",				index.c_str(), data.fromsockcnt);
	PRN("%s  lastupdatetime = %zd",				index.c_str(), data.lastupdatetime);
	PRN("%s  status         = %s",				index.c_str(), data.status.c_str());
	if(is_result_part){
		string	indexsub = index + string("  ");
		DumpCheckResultPart(data.checkresult, string("checkresult"), indexsub);
	}
	PRN("%s}",									index.c_str());
}

//
// dump nodesunits_t
//
static void DumpNodeUnits(const nodesunits_t& units, const string& prefix, const string& index, bool is_result_part)
{
	if(!is_print_dmp){
		return;
	}

	PRN("%s%s = {", index.c_str(), prefix.empty() ? "nodesunits_t" : prefix.c_str());

	string	index2 = index + string("  ");
	for(nodesunits_t::const_iterator iter = units.begin(); iter != units.end(); ++iter){
		DumpNodeUnitData(iter->second, iter->first, index2, is_result_part);
	}
	PRN("%s}", index.c_str());
}

//
// dump NODESTATUSDETAIL
//
static void DumpNodeStatusDetail(const NODESTATUSDETAIL& detail, const string& prefix, const string& index, bool is_result_part)
{
	if(!is_print_dmp){
		return;
	}

	PRN("%s%s = {", index.c_str(), prefix.empty() ? "nodesunits_t" : prefix.c_str());

	string	index2 = index + string("  ");
	DumpNodeUnitData(detail.self, string("self"), index2, is_result_part);
	DumpNodeUnits(detail.servers, string("servers"), index2, is_result_part);
	DumpNodeUnits(detail.slaves, string("slaves"), index2, is_result_part);
	PRN("%s}", index.c_str());
}

//
// dump statusdetails_t
//
static void DumpAllStatusDetails(const statusdetails_t& all)
{
	if(!is_print_dmp){
		return;
	}

	PRN("DUMP = {");
	string	index("  ");
	for(statusdetails_t::const_iterator iter = all.begin(); iter != all.end(); ++iter){
		DumpNodeStatusDetail(iter->second, iter->first, index, false);
	}
	PRN("}");
}

//
// dump NODECHECKRESULT
//
static void DumpNodeCheckResult(const NODECHECKRESULT& result, const string& prefix, const string& index, bool is_result_part)
{
	if(!is_print_dmp){
		return;
	}

	PRN("%s%s = {",				index.c_str(), prefix.empty() ? "NODECHECKRESULT" : prefix.c_str());
	PRN("%s  noderesult = %s",	index.c_str(), GetCheckResultString(result.noderesult).c_str());

	string	index2 = index + string("  ");
	DumpNodeStatusDetail(result.all, string("all"), index2, is_result_part);

	PRN("%s}", index.c_str());
}

//
// dump nodechkresults_t
//
static void DumpNodeCheckResults(const nodechkresults_t& results)
{
	if(!is_print_dmp){
		return;
	}

	PRN("DUMP = {");
	string	index("  ");
	for(nodechkresults_t::const_iterator iter = results.begin(); iter != results.end(); ++iter){
		DumpNodeCheckResult(iter->second, iter->first, index, true);
	}
	PRN("}");				// DUMP
}

//---------------------------------------------------------
// Utilities : Parse chmpxs from AllStatus result string
//---------------------------------------------------------
static string CvtStatusStrings(const string& strBase)
{
	string	result("");
	string	str = strBase;
	string	strtmp;

	// SLAVE/SERVICEOUT/SERVICEIN
	strtmp = ParseOnePartFromStatusString(str);
	if(strtmp == "SLAVE"){
		result += string("[") + GREEN("SLAVE")		+ string("]      ");
	}else if(strtmp == "SERVICEOUT"){
		result += string("[") + RED("SERVICE OUT")	+ string("]");
	}else if(strtmp == "SERVICEIN"){
		result += string("[") + GREEN("SERVICE IN")	+ string("] ");
	}else{
		result += string("[") + BG_RED("UNKNOWN")	+ string("]    ");
	}

	// UP/DOWN
	strtmp = ParseOnePartFromStatusString(str);
	if(strtmp == "UP"){
		result += string("[") + GREEN("UP")			+ string("]    ");
	}else if(strtmp == "DOWN"){
		result += string("[") + RED("DOWN")			+ string("]  ");
	}else{
		result += string("[") + BG_RED("UNKNOWN")	+ string("]");
	}

	// n/a /ADD/DELETE
	strtmp = ParseOnePartFromStatusString(str);
	if(strtmp == "n/a"){
		result += string("[") + GREEN("n/a")		+ string("]    ");
	}else if(strtmp == "ADD"){
		result += string("[") + YELLOW("ADD")		+ string("]    ");
	}else if(strtmp == "DELETE"){
		result += string("[") + YELLOW("DELETE")	+ string("] ");
	}else{
		result += string("[") + BG_RED("UNKNOWN")	+ string("]");
	}

	// Nothing/Pending/Doing/Done
	strtmp = ParseOnePartFromStatusString(str);
	if(strtmp == "Nothing"){
		result += string("[") + GREEN("Nothing")	+ string("]");
	}else if(strtmp == "Pending"){
		result += string("[") + MAGENTA("Pending")	+ string("]");
	}else if(strtmp == "Doing"){
		result += string("[") + YELLOW("Doing")		+ string("]  ");
	}else if(strtmp == "Done"){
		result += string("[") + BLUE("Done")		+ string("]   ");
	}else{
		result += string("[") + BG_RED("UNKNOWN")	+ string("]");
	}

	// Suspend/NoSuspend
	strtmp = ParseOnePartFromStatusString(str);
	if(strtmp == "Suspend"){
		result += string("[") + RED("Suspend")		+ string("]");
	}else if(strtmp == "NoSuspend"){
		result += string("[") + GREEN("NoSuspend")	+ string("]");
	}else if(strtmp == "Doing"){
	}else{
		result += string("[") + BG_RED("UNKNOWN")	+ string("]");
	}
	return result;
}

static string CvtAllStatusResult(const string& strResult, bool& is_error)
{
	string				strInput = CutSpaceCharacter(strResult);	// cut space
	string				strOutput;
	string::size_type	pos;

	// cut empty line
	is_error	= false;
	strInput	= CutEmptyLine(strInput);

	// Get Ring Name as "RINGName="
	if(string::npos == (pos = strInput.find(ALLSTATUS_KEY_RINGNAME_NOSPACE))){
		ERR("Could not found \"RING Name\" key in ALLSTATUS result.");
		is_error = true;
		return strOutput;
	}
	strInput = strInput.substr(pos + strlen(ALLSTATUS_KEY_RINGNAME_NOSPACE));
	if(string::npos == (pos = strInput.find(ALLSTATUS_KEY_CR))){
		ERR("Could not found CR after \"RING Name=\" key in ALLSTATUS result.");
		is_error = true;
		return strOutput;
	}
	string	strRingname	= trim(strInput.substr(0, pos));
	strInput			= strInput.substr(pos + strlen(ALLSTATUS_KEY_CR));
	strOutput			+= string(" ") + strRingname + string(" = {\n");

	// Loop
	while(string::npos != (pos = strInput.find(ALLSTATUS_KEY_NUMBER_TOP))){
		//
		// Parse one server parts from ALLSTATUS result
		//
		// Skip "No.XX\n"
		strInput = strInput.substr(strlen(ALLSTATUS_KEY_NUMBER_TOP));
		if(string::npos == (pos = strInput.find(ALLSTATUS_KEY_CR))){
			ERR("Could not found CR after \"No.XX\" key in ALLSTATUS result.");
			is_error = true;
			return strOutput;
		}
		strInput	= strInput.substr(pos + strlen(ALLSTATUS_KEY_CR));

		// Get Server Name as "ServerName="
		if(string::npos == (pos = strInput.find(ALLSTATUS_KEY_SERVERNAME))){
			ERR("Could not found \"Server Name\" key in ALLSTATUS result.");
			is_error = true;
			return strOutput;
		}
		strInput = strInput.substr(pos + strlen(ALLSTATUS_KEY_SERVERNAME));
		if(string::npos == (pos = strInput.find(ALLSTATUS_KEY_CR))){
			ERR("Could not found CR after \"Server Name=\" key in ALLSTATUS result.");
			is_error = true;
			return strOutput;
		}
		string	strServerName	= trim(strInput.substr(0, pos));
		strInput				= strInput.substr(pos + strlen(ALLSTATUS_KEY_CR));

		// Get Port as "Port="
		if(string::npos == (pos = strInput.find(ALLSTATUS_KEY_PORT))){
			ERR("Could not found \"Port\" key in ALLSTATUS result.");
			is_error = true;
			return strOutput;
		}
		strInput = strInput.substr(pos + strlen(ALLSTATUS_KEY_PORT));
		if(string::npos == (pos = strInput.find(ALLSTATUS_KEY_CR))){
			ERR("Could not found CR after \"Port=\" key in ALLSTATUS result.");
			is_error = true;
			return strOutput;
		}
		string	strPort			= trim(strInput.substr(0, pos));
		strInput				= strInput.substr(pos + strlen(ALLSTATUS_KEY_CR));

		// Get Control Port as "ControlPort="
		if(string::npos == (pos = strInput.find(ALLSTATUS_KEY_CTLPORT))){
			ERR("Could not found \"Control Port\" key in ALLSTATUS result.");
			is_error = true;
			return strOutput;
		}
		strInput = strInput.substr(pos + strlen(ALLSTATUS_KEY_CTLPORT));
		if(string::npos == (pos = strInput.find(ALLSTATUS_KEY_CR))){
			ERR("Could not found CR after \"Control Port=\" key in ALLSTATUS result.");
			is_error = true;
			return strOutput;
		}
		string	strCtlPort		= trim(strInput.substr(0, pos));
		strInput				= strInput.substr(pos + strlen(ALLSTATUS_KEY_CR));

		// Get CUK as "CUK="
		string	strCUK;
		if(string::npos == (pos = strInput.find(ALLSTATUS_KEY_CUK))){
			MSG("Could not found \"CUK\" key in ALLSTATUS result.");
		}else{
			strInput = strInput.substr(pos + strlen(ALLSTATUS_KEY_CUK));
			if(string::npos == (pos = strInput.find(ALLSTATUS_KEY_CR))){
				ERR("Could not found CR after \"CUK=\" key in ALLSTATUS result.");
				is_error = true;
				return strOutput;
			}
			strCUK				= trim(strInput.substr(0, pos));
			strInput			= strInput.substr(pos + strlen(ALLSTATUS_KEY_CR));
		}

		// Get Custom ID Seed as "CustomIDSeed="
		string	strCustomSeed;
		if(string::npos == (pos = strInput.find(ALLSTATUS_KEY_CUSTOM_SEED))){
			MSG("Could not found \"Custom ID Seed\" key in ALLSTATUS result.");
		}else{
			strInput = strInput.substr(pos + strlen(ALLSTATUS_KEY_CUSTOM_SEED));
			if(string::npos == (pos = strInput.find(ALLSTATUS_KEY_CR))){
				ERR("Could not found CR after \"Custom ID Seed=\" key in ALLSTATUS result.");
				is_error = true;
				return strOutput;
			}
			strCustomSeed		= trim(strInput.substr(0, pos));
			strInput			= strInput.substr(pos + strlen(ALLSTATUS_KEY_CR));
		}

		// Get Control Endppoints as "ControlEndppoints="
		string	strCtlendppoints;
		if(string::npos == (pos = strInput.find(ALLSTATUS_KEY_CTLEPS))){
			MSG("Could not found \"Control Endppoints\" key in ALLSTATUS result.");
		}else{
			strInput = strInput.substr(pos + strlen(ALLSTATUS_KEY_CTLEPS));
			if(string::npos == (pos = strInput.find(ALLSTATUS_KEY_CR))){
				ERR("Could not found CR after \"Control Endppoints=\" key in ALLSTATUS result.");
				is_error = true;
				return strOutput;
			}
			strCtlendppoints	= trim(strInput.substr(0, pos));
			strInput			= strInput.substr(pos + strlen(ALLSTATUS_KEY_CR));
		}

		// Get Use SSL as "UseSSL="
		if(string::npos == (pos = strInput.find(ALLSTATUS_KEY_ISSSL))){
			ERR("Could not found \"Use SSL\" key in ALLSTATUS result.");
			is_error = true;
			return strOutput;
		}
		strInput = strInput.substr(pos + strlen(ALLSTATUS_KEY_ISSSL));
		if(string::npos == (pos = strInput.find(ALLSTATUS_KEY_CR))){
			ERR("Could not found CR after \"Use SSL=\" key in ALLSTATUS result.");
			is_error = true;
			return strOutput;
		}
		string	strIsSSL		= trim(strInput.substr(0, pos));
		strInput				= strInput.substr(pos + strlen(ALLSTATUS_KEY_CR));
		bool	isSSL			= false;
		if(strIsSSL == ALLSTATUS_KEY_SSL_YES){
			isSSL		= true;
			strIsSSL	= string("(SSL");
		}else if(strIsSSL == ALLSTATUS_KEY_SSL_NO){
			isSSL		= false;
			strIsSSL	= string("");
		}else{
			isSSL		= false;
			strIsSSL	= string("(") + RED("unknown SSL");
		}

		// Get Verify Peer as "VerifyPeer="
		string	strIsVerify;
		// cppcheck-suppress stlIfStrFind
		if(isSSL && 0 == (pos = strInput.find(ALLSTATUS_KEY_ISVERIFY))){
			strInput = strInput.substr(pos + strlen(ALLSTATUS_KEY_ISVERIFY));
			if(string::npos == (pos = strInput.find(ALLSTATUS_KEY_CR))){
				ERR("Could not found CR after \"Verify Peer=\" key in ALLSTATUS result.");
				is_error = true;
				return strOutput;
			}
			strIsVerify			= trim(strInput.substr(0, pos));
			strInput			= strInput.substr(pos + strlen(ALLSTATUS_KEY_CR));

			if(strIsVerify == ALLSTATUS_KEY_SSL_YES){
				strIsVerify = string(":Client Verify");
			}else if(strIsVerify == ALLSTATUS_KEY_SSL_NO){
				strIsVerify = string("");
			}else{
				strIsVerify = string(":") + RED("unknown client verify");
			}
		}
		if(!strIsSSL.empty()){
			strIsSSL += strIsVerify + string(")");
		}

		// Get Server Status as "ServerStatus="
		if(string::npos == (pos = strInput.find(ALLSTATUS_KEY_STATUS))){
			ERR("Could not found \"Server Status=\" key in ALLSTATUS result.");
			is_error = true;
			return strOutput;
		}
		strInput = strInput.substr(pos + strlen(ALLSTATUS_KEY_STATUS));
		if(string::npos == (pos = strInput.find(ALLSTATUS_KEY_CR))){
			ERR("Could not found CR after \"Server Status=\" key in ALLSTATUS result.");
			is_error = true;
			return strOutput;
		}
		string	strStatusAll	= trim(strInput.substr(0, pos));
		strInput				= strInput.substr(pos + strlen(ALLSTATUS_KEY_CR));

		// SUB: status binary in server status
		if(string::npos == (pos = strStatusAll.find(ALLSTATUS_KEY_STATUS_FIRST_KEY))){
			ERR("Could not found \"0xXXXX(\" key in ALLSTATUS result.");
			is_error = true;
			return strOutput;
		}
		string	strStatusBin	= trim(strStatusAll.substr(0, pos));
		strStatusAll			= strStatusAll.substr(pos + strlen(ALLSTATUS_KEY_STATUS_FIRST_KEY));

		// SUB: string status in server status
		if(string::npos == (pos = strStatusAll.find(ALLSTATUS_KEY_STATUS_MID_KEY))){
			ERR("Could not found \"SAFE->\" key in ALLSTATUS result.");
			is_error = true;
			return strOutput;
		}
		strStatusAll			= strStatusAll.substr(pos + strlen(ALLSTATUS_KEY_STATUS_MID_KEY));
		if(string::npos == (pos = strStatusAll.find(ALLSTATUS_KEY_STATUS_END_KEY))){
			ERR("Could not found CR after \"...[STATUS...])\" key in ALLSTATUS result.");
			is_error = true;
			return strOutput;
		}
		string	strStatusString	= trim(strStatusAll.substr(0, pos));

		// Get Last Update as "LastUpdate="
		if(string::npos == (pos = strInput.find(ALLSTATUS_KEY_LASTUPDATE))){
			ERR("Could not found \"Last Update=\" key in ALLSTATUS result.");
			is_error = true;
			return strOutput;
		}
		strInput = strInput.substr(pos + strlen(ALLSTATUS_KEY_LASTUPDATE));
		if(string::npos == (pos = strInput.find(ALLSTATUS_KEY_CR))){
			ERR("Could not found CR after \"Last Update=\" key in ALLSTATUS result.");
			is_error = true;
			return strOutput;
		}
		string	strLastUpdate	= trim(strInput.substr(0, pos));
		strInput				= strInput.substr(pos + strlen(ALLSTATUS_KEY_CR));
		// SUB: Check and convert time string
		{
			// Get last update time(time_t value)
			string::size_type	pos2;
			if(string::npos != (pos2 = strLastUpdate.find("("))){
				strLastUpdate		= strLastUpdate.substr(pos2 + 1);
				if(string::npos != (pos2 = strLastUpdate.find(")"))){
					strLastUpdate.resize(pos2);
				}else{
					// why?, but continue...
				}
			}else{
				// old format, only unixtime
			}
			// dec string -> time_t -> formatted time string
			time_t	tmpUpdateTime	= static_cast<time_t>(atoll(strLastUpdate.c_str()));
			strLastUpdate			= CvtUpdateTimeToString(tmpUpdateTime) + string("(") + to_string(tmpUpdateTime) + string(")");
		}

		// Get Hash value as "EnableHashValue="
		if(string::npos == (pos = strInput.find(ALLSTATUS_KEY_HASH))){
			ERR("Could not found \"Enable Hash Value=\" key in ALLSTATUS result.");
			is_error = true;
			return strOutput;
		}
		strInput = strInput.substr(pos + strlen(ALLSTATUS_KEY_HASH));
		if(string::npos == (pos = strInput.find(ALLSTATUS_KEY_CR))){
			ERR("Could not found CR after \"Enable Hash Value=\" key in ALLSTATUS result.");
			is_error = true;
			return strOutput;
		}
		string	strHash			= trim(strInput.substr(0, pos));
		strInput				= strInput.substr(pos + strlen(ALLSTATUS_KEY_CR));

		// Get Server Name as "PendingHashValue="
		string	strPendingHash;
		if(string::npos == (pos = strInput.find(ALLSTATUS_KEY_PENDINGHASH))){
			ERR("Could not found \"Pending Hash Value=\" key in ALLSTATUS result.");
			is_error = true;
			return strOutput;
		}
		strInput = strInput.substr(pos + strlen(ALLSTATUS_KEY_PENDINGHASH));
		if(string::npos == (pos = strInput.find(ALLSTATUS_KEY_CR))){
			// probably this is last of node.
			strPendingHash		= trim(strInput);
			strInput.clear();
		}else{
			strPendingHash		= trim(strInput.substr(0, pos));
			strInput			= strInput.substr(pos + strlen(ALLSTATUS_KEY_CR));
		}

		//
		// Make one server parts
		//
		strOutput				+= string("     ") + BOLD(strServerName) + string(" = {\n");
		strOutput				+= string("         Status(Bin)   = ") + CvtStatusStrings(strStatusString) + string("(") + strStatusBin + string(")\n");
		strOutput				+= string("         Hash(Pending) = ") + strHash + string("(") + strPendingHash + string(")\n");
		strOutput				+= string("         Port/Ctlport  = ") + strPort + strIsSSL + string("/") + strCtlPort + string("\n");
		if(!strCtlendppoints.empty()){
			strOutput			+= string("         Ctl Endpoints = ") + strCtlendppoints + string("\n");
		}
		if(!strCUK.empty()){
			strOutput			+= string("         CUK           = ") + strCUK + string("\n");
		}
		if(!strCustomSeed.empty()){
			strOutput			+= string("         Custom ID Seed= ") + strCustomSeed + string("\n");
		}
		strOutput				+= string("         Last Update   = ") + strLastUpdate + string("\n");
		strOutput				+= string("     }\n");
	}
	strOutput += string(" }\n");

	return strOutput;
}

static string MakeNodeStatusResult(const string& status, const CHKRESULT_PART& checkresult)
{
	string	strOutput("");
	string	strStatusPart;
	string	strTmp			= status;

	// status: ring
	strStatusPart	= ParseOnePartFromStatusString(strTmp);
	if(strStatusPart == "SLAVE"){
		if(CHKRESULT_NOERR == checkresult.result_status_ring){
			strOutput += string("[") + GREEN("SLAVE")				+ string("]      ");
		}else{	// CHKRESULT_WARN or CHKRESULT_ERR == checkresult.result_status_ring
			strOutput += string("[") + BG_GREEN("SLAVE", true, true)+ string("]      ");
		}
	}else if(strStatusPart == "SERVICEOUT"){
		if(CHKRESULT_NOERR == checkresult.result_status_ring){
			strOutput += string("[") + RED("SERVICE OUT")				+ string("]");
		}else{	// CHKRESULT_WARN or CHKRESULT_ERR == checkresult.result_status_ring
			strOutput += string("[") + BG_RED("SERVICE OUT", true, true)+ string("]");
		}
	}else if(strStatusPart == "SERVICEIN"){
		if(CHKRESULT_NOERR == checkresult.result_status_ring){
			strOutput += string("[") + GREEN("SERVICE IN")					+ string("] ");
		}else{	// CHKRESULT_WARN or CHKRESULT_ERR == checkresult.result_status_ring
			strOutput += string("[") + BG_GREEN("SERVICE IN", true, true)	+ string("] ");
		}
	}else{
		if(CHKRESULT_NOERR == checkresult.result_status_ring){
			strOutput += string("[") + RED("UNKNOWN")				+ string("]    ");
		}else{	// CHKRESULT_WARN or CHKRESULT_ERR == checkresult.result_status_ring
			strOutput += string("[") + BG_RED("UNKNOWN", true, true)+ string("]    ");
		}
	}
	// status: live
	strStatusPart	= ParseOnePartFromStatusString(strTmp);
	if(strStatusPart == "UP"){
		if(CHKRESULT_NOERR == checkresult.result_status_live){
			strOutput += string("[") + GREEN("UP")					+ string("]    ");
		}else{	// CHKRESULT_WARN or CHKRESULT_ERR == checkresult.result_status_live
			strOutput += string("[") + BG_GREEN("UP", true, true)	+ string("]    ");
		}
	}else if(strStatusPart == "DOWN"){
		if(CHKRESULT_NOERR == checkresult.result_status_live){
			strOutput += string("[") + RED("DOWN")					+ string("]  ");
		}else{	// CHKRESULT_WARN or CHKRESULT_ERR == checkresult.result_status_live
			strOutput += string("[") + BG_RED("DOWN", true, true)	+ string("]  ");
		}
	}else{
		if(CHKRESULT_NOERR == checkresult.result_status_live){
			strOutput += string("[") + RED("UNKNOWN")				+ string("]");
		}else{	// CHKRESULT_WARN or CHKRESULT_ERR == checkresult.result_status_live
			strOutput += string("[") + BG_RED("UNKNOWN", true, true)+ string("]");
		}
	}
	// status: action
	strStatusPart	= ParseOnePartFromStatusString(strTmp);
	if(strStatusPart == "n/a"){
		if(CHKRESULT_NOERR == checkresult.result_status_act){
			strOutput += string("[") + GREEN("n/a")					+ string("]    ");
		}else{	// CHKRESULT_WARN or CHKRESULT_ERR == checkresult.result_status_act
			strOutput += string("[") + BG_GREEN("n/a", true, true)	+ string("]    ");
		}
	}else if(strStatusPart == "ADD"){
		if(CHKRESULT_NOERR == checkresult.result_status_act){
			strOutput += string("[") + YELLOW("ADD")				+ string("]    ");
		}else{	// CHKRESULT_WARN or CHKRESULT_ERR == checkresult.result_status_act
			strOutput += string("[") + BG_YELLOW("ADD", true, true)	+ string("]    ");
		}
	}else if(strStatusPart == "DELETE"){
		if(CHKRESULT_NOERR == checkresult.result_status_act){
			strOutput += string("[") + YELLOW("DELETE")					+ string("]  ");
		}else{	// CHKRESULT_WARN or CHKRESULT_ERR == checkresult.result_status_act
			strOutput += string("[") + BG_YELLOW("DELETE", true, true)	+ string("]  ");
		}
	}else{
		if(CHKRESULT_NOERR == checkresult.result_status_act){
			strOutput += string("[") + RED("UNKNOWN")				+ string("]");
		}else{	// CHKRESULT_WARN or CHKRESULT_ERR == checkresult.result_status_act
			strOutput += string("[") + BG_RED("UNKNOWN", true, true)+ string("]");
		}
	}
	// status: operation
	strStatusPart	= ParseOnePartFromStatusString(strTmp);
	if(strStatusPart == "Nothing"){
		if(CHKRESULT_NOERR == checkresult.result_status_opr){
			strOutput += string("[") + GREEN("Nothing")					+ string("]");
		}else{	// CHKRESULT_WARN or CHKRESULT_ERR == checkresult.result_status_opr
			strOutput += string("[") + BG_GREEN("Nothing", true, true)	+ string("]");
		}
	}else if(strStatusPart == "Pending"){
		if(CHKRESULT_NOERR == checkresult.result_status_opr){
			strOutput += string("[") + MAGENTA("Pending")				+ string("]");
		}else{	// CHKRESULT_WARN or CHKRESULT_ERR == checkresult.result_status_opr
			strOutput += string("[") + BG_MAGENTA("Pending", true, true)+ string("]");
		}
	}else if(strStatusPart == "Doing"){
		if(CHKRESULT_NOERR == checkresult.result_status_opr){
			strOutput += string("[") + YELLOW("Doing")					+ string("]  ");
		}else{	// CHKRESULT_WARN or CHKRESULT_ERR == checkresult.result_status_opr
			strOutput += string("[") + BG_YELLOW("Doing", true, true)	+ string("]  ");
		}
	}else if(strStatusPart == "Done"){
		if(CHKRESULT_NOERR == checkresult.result_status_opr){
			strOutput += string("[") + BLUE("Done")					+ string("]   ");
		}else{	// CHKRESULT_WARN or CHKRESULT_ERR == checkresult.result_status_opr
			strOutput += string("[") + BG_BLUE("Done", true, true)	+ string("]   ");
		}
	}else{
		if(CHKRESULT_NOERR == checkresult.result_status_opr){
			strOutput += string("[") + RED("UNKNOWN")				+ string("]");
		}else{	// CHKRESULT_WARN or CHKRESULT_ERR == checkresult.result_status_opr
			strOutput += string("[") + BG_RED("UNKNOWN", true, true)+ string("]");
		}
	}
	// status: suspend
	strStatusPart	= ParseOnePartFromStatusString(strTmp);
	if(strStatusPart == "Suspend"){
		if(CHKRESULT_NOERR == checkresult.result_status_opr){
			strOutput += string("[") + RED("Suspend")					+ string("]");
		}else{	// CHKRESULT_WARN or CHKRESULT_ERR == checkresult.result_status_opr
			strOutput += string("[") + BG_RED("Suspend", true, true)	+ string("]");
		}
	}else if(strStatusPart == "NoSuspend"){
		if(CHKRESULT_NOERR == checkresult.result_status_opr){
			strOutput += string("[") + GREEN("NoSuspend")				+ string("]");
		}else{	// CHKRESULT_WARN or CHKRESULT_ERR == checkresult.result_status_opr
			strOutput += string("[") + BG_GREEN("NoSuspend", true, true)+ string("]");
		}
	}else{
		if(CHKRESULT_NOERR == checkresult.result_status_opr){
			strOutput += string("[") + RED("UNKNOWN")					+ string("]");
		}else{	// CHKRESULT_WARN or CHKRESULT_ERR == checkresult.result_status_opr
			strOutput += string("[") + BG_RED("UNKNOWN", true, true)	+ string("]");
		}
	}
	return strOutput;
}

static string MakeNodeHashResult(const string& hash, const string& pendinghash, const CHKRESULT_PART& checkresult)
{
	string	strOutput("");

	// hash/pending hash
	if(CHKRESULT_NOERR == checkresult.result_hash){
		strOutput += hash;
	}else if(CHKRESULT_WARN == checkresult.result_hash){
		strOutput += BG_YELLOW(hash, true, true);
	}else{	// CHKRESULT_ERR == checkresult.result_hash
		strOutput += BG_RED(hash, true, true);
	}
	if(CHKRESULT_NOERR == checkresult.result_pending){
		strOutput += string("(") + pendinghash							+ string(")");
	}else if(CHKRESULT_WARN == checkresult.result_pending){
		strOutput += string("(") + BG_YELLOW(pendinghash, true, true)	+ string(")");
	}else{	// CHKRESULT_ERR == checkresult.result_pending
		strOutput += string("(") + BG_RED(pendinghash, true, true)		+ string(")");
	}
	return strOutput;
}

static string MakeNodeSockCountResult(long tosockcnt, long fromsockcnt, const CHKRESULT_PART& checkresult)
{
	string	strOutput("");

	// socket count
	if(CHKRESULT_NOERR == checkresult.result_tosockcnt){
		strOutput += to_string(tosockcnt);
	}else if(CHKRESULT_WARN == checkresult.result_tosockcnt){
		strOutput += BG_YELLOW(to_string(tosockcnt), true, true);
	}else{	// CHKRESULT_ERR == checkresult.result_tosockcnt
		strOutput += BG_RED(to_string(tosockcnt), true, true);
	}
	if(CHKRESULT_NOERR == checkresult.result_fromsockcnt){
		strOutput += string("/") + to_string(fromsockcnt);
	}else if(CHKRESULT_WARN == checkresult.result_fromsockcnt){
		strOutput += string("/") + BG_YELLOW(to_string(fromsockcnt), true, true);
	}else{	// CHKRESULT_ERR == checkresult.result_fromsockcnt
		strOutput += string("/") + BG_RED(to_string(fromsockcnt), true, true);
	}
	return strOutput;
}

static string MakeNodeUpdateTimeResult(time_t lastupdatetime, const CHKRESULT_PART& checkresult)
{
	string	strOutput("");

	// update time
	if(CHKRESULT_NOERR == checkresult.result_lastupdatetime){
		strOutput += CvtUpdateTimeToString(lastupdatetime)							+ string("(") + to_string(lastupdatetime)						+ string(")");
	}else if(CHKRESULT_WARN == checkresult.result_lastupdatetime){
		strOutput += BG_YELLOW(CvtUpdateTimeToString(lastupdatetime), true, true)	+ string("(") + BG_YELLOW(to_string(lastupdatetime), true, true)+ string(")");
	}else{	// CHKRESULT_ERR == checkresult.result_lastupdatetime
		strOutput += BG_RED(CvtUpdateTimeToString(lastupdatetime), true, true)		+ string("(") + BG_RED(to_string(lastupdatetime), true, true)	+ string(")");
	}
	return strOutput;
}

static string CvtAllNodeStatusResults(nodechkresults_t& results)
{
	string	strOutput("");

	for(nodechkresults_t::iterator iter = results.begin(); iter != results.end(); ++iter){
		// title
		string	strTmpHead;
		if(CHKRESULT_NOERR == iter->second.noderesult){
			strTmpHead = BG_GREEN("OK") + string("   ");
		}else if(CHKRESULT_WARN == iter->second.noderesult){
			strTmpHead = BG_YELLOW("WARN", true, true) + string(" ");
		}else{	// CHKRESULT_ERR == iter->second.noderesult
			strTmpHead = BG_RED("ERR", true, true) + string("  ");
		}
		strTmpHead += BOLD(iter->first) + string(" = {\n");

		// status
		string	strTmpStatus= string("    status            = ") + MakeNodeStatusResult(iter->second.all.self.status, iter->second.all.self.checkresult) + string("\n");

		// hash/pending hash
		string	strTmpHash	= string("    hash(pending)     = ") + MakeNodeHashResult(iter->second.all.self.hash, iter->second.all.self.pendinghash, iter->second.all.self.checkresult) + string("\n");

		// socket count
		string	strTmpSock	= string("    sockcount(in/out) = ") + MakeNodeSockCountResult(iter->second.all.self.tosockcnt, iter->second.all.self.fromsockcnt, iter->second.all.self.checkresult) + string("\n");

		// update time
		string	strTmpTime	= string("    lastupdatetime    = ") + MakeNodeUpdateTimeResult(iter->second.all.self.lastupdatetime, iter->second.all.self.checkresult) + string("\n");

		// add output
		strOutput += strTmpHead;
		strOutput += strTmpStatus;
		strOutput += strTmpHash;
		strOutput += strTmpSock;
		strOutput += strTmpTime;
		strOutput += string("}\n");
	}
	return strOutput;
}

static string CvtOneNodeStatusResults(nodechkresults_t& results)
{
	string						strOutput("");
	nodechkresults_t::iterator	iter = results.begin();
	if(iter == results.end()){
		return strOutput;
	}

	strOutput += BOLD(iter->first) + string(" = {\n");

	// basis(self)
	strOutput += string("  basis = {\n");
	strOutput += string("    status            = ") + MakeNodeStatusResult(iter->second.all.self.status, iter->second.all.self.checkresult) + string("\n");
	strOutput += string("    hash(pending)     = ") + MakeNodeHashResult(iter->second.all.self.hash, iter->second.all.self.pendinghash, iter->second.all.self.checkresult) + string("\n");
	strOutput += string("    sockcount(in/out) = ") + MakeNodeSockCountResult(iter->second.all.self.tosockcnt, iter->second.all.self.fromsockcnt, iter->second.all.self.checkresult) + string("\n");
	strOutput += string("    lastupdatetime    = ") + MakeNodeUpdateTimeResult(iter->second.all.self.lastupdatetime, iter->second.all.self.checkresult) + string("\n");
	strOutput += string("  }\n");

	// servers
	nodesunits_t::const_iterator	iter_unit;
	for(iter_unit = iter->second.all.servers.begin(); iter_unit != iter->second.all.servers.end(); ++iter_unit){
		// one node
		strOutput += string("  ") + BOLD(iter_unit->first) + string(" = {\n");
		strOutput += string("    status            = ") + MakeNodeStatusResult(iter_unit->second.status, iter_unit->second.checkresult) + string("\n");
		strOutput += string("    hash(pending)     = ") + MakeNodeHashResult(iter_unit->second.hash, iter_unit->second.pendinghash, iter_unit->second.checkresult) + string("\n");
		strOutput += string("    sockcount(in/out) = ") + MakeNodeSockCountResult(iter_unit->second.tosockcnt, iter_unit->second.fromsockcnt, iter_unit->second.checkresult) + string("\n");
		strOutput += string("    lastupdatetime    = ") + MakeNodeUpdateTimeResult(iter_unit->second.lastupdatetime, iter_unit->second.checkresult) + string("\n");
		strOutput += string("  }\n");
	}

	// slaves
	for(iter_unit = iter->second.all.slaves.begin(); iter_unit != iter->second.all.slaves.end(); ++iter_unit){
		// one node
		strOutput += string("  ") + BOLD(iter_unit->first) + string(" = {\n");
		strOutput += string("    status            = ") + MakeNodeStatusResult(iter_unit->second.status, iter_unit->second.checkresult) + string("\n");
		strOutput += string("    hash(pending)     = ") + MakeNodeHashResult(iter_unit->second.hash, iter_unit->second.pendinghash, iter_unit->second.checkresult) + string("\n");
		strOutput += string("    sockcount(in/out) = ") + MakeNodeSockCountResult(iter_unit->second.tosockcnt, iter_unit->second.fromsockcnt, iter_unit->second.checkresult) + string("\n");
		strOutput += string("    lastupdatetime    = ") + MakeNodeUpdateTimeResult(iter_unit->second.lastupdatetime, iter_unit->second.checkresult) + string("\n");
		strOutput += string("  }\n");
	}

	strOutput += string("}\n");

	return strOutput;
}

//---------------------------------------------------------
// Utilities : Read from file
//---------------------------------------------------------
//
// Return: if left lines, returns true.
//
static bool ReadLine(int fd, string& line)
{
	line.erase();
	while(true){
		char	szBuff;
		ssize_t	readlength;

		szBuff = '\0';
		// read one character
		if(-1 == (readlength = read(fd, &szBuff, 1))){
			line.erase();
			return false;
		}

		// check EOF
		if(0 == readlength){
			return false;
		}

		// check character
		if('\r' == szBuff || '\0' == szBuff){
			// skip words

		}else if('\n' == szBuff){
			// skip comment line & no command line
			bool	isSpace		= true;
			bool	isComment	= false;
			for(size_t cPos = 0; isSpace && cPos < line.length(); cPos++){
				if(0 == isspace(line.at(cPos))){
					isSpace = false;

					if('#' == line.at(cPos)){
						isComment = true;
					}
					break;
				}
			}
			if(!isComment && !isSpace){
				break;
			}
			// this line is comment or empty, so read next line.
			line.erase();

		}else{
			line += szBuff;
		}
	}
	return true;
}

//---------------------------------------------------------
// Command Processing
//---------------------------------------------------------
// Parameter parser
//
//	params is formatted following:
//		"hostname(IP address)":"control port"[:"cuk"[:"custom_seed"[:"control endpoints..."]]]
//
static bool parse_host_parameter(const string& params, string& host, short& ctrlport, string& cuk, string& ctlendpoints, string& custom_seed)
{
	host		= params;
	ctrlport	= CHM_INVALID_PORT;
	cuk.clear();
	ctlendpoints.clear();
	custom_seed.clear();

	// parse control port / cuk
	string::size_type	chpos;
	if(string::npos != (chpos = host.find(":"))){
		string	strport	= host.substr(chpos + 1);
		host.resize(chpos);

		if(string::npos == (chpos = strport.find(":"))){
			ctrlport	= static_cast<short>(atoi(strport.c_str()));
		}else{
			cuk			= strport.substr(chpos + 1);
			strport.resize(chpos);
			ctrlport	= static_cast<short>(atoi(strport.c_str()));

			if(string::npos != (chpos = cuk.find(":"))){
				custom_seed	= cuk.substr(chpos + 1);
				cuk.resize(chpos);

				if(string::npos != (chpos = custom_seed.find(":"))){
					ctlendpoints= custom_seed.substr(chpos + 1);
					custom_seed.resize(chpos);
				}
			}
		}
	}
	return true;
}

static bool CommandStringHandle(ConsoleInput& InputIF, const char* pCommand, bool& is_exit);

//
// Command Line: update									update dynamic target chmpx nodes
//
static bool UpdateNodesCommand(void)
{
	if(!CreateDynaTargetChmpx()){
		ERR("Failed to update dynamic chmpx node list.");
		return true;
	}
	print_chmpx_all_nodes(TargetNodes);

	return true;
}

//
// Command Line: nodes [nodyna | noupdate] [server | slave]
//
// print all/server/slave chmpx nodes.
// if noupdate parameter is specified, do not update before doing.
// if nodyna parameter is specified, only initially chmpx nodes.
//
static bool PrintNodesCommand(params_t& params)
{
	bool	is_dyna_nodes	= true;
	bool	is_update		= true;
	bool	is_server_nodes	= false;
	bool	is_slave_nodes	= false;

	for(size_t pos = 0; pos < params.size(); ++pos){
		if(0 == strcasecmp(params[pos].c_str(), "nodyna")){
			is_dyna_nodes	= false;
		}else if(0 == strcasecmp(params[pos].c_str(), "noupdate")){
			is_update		= false;
		}else if(0 == strcasecmp(params[pos].c_str(), "server")){
			is_server_nodes	= true;
		}else if(0 == strcasecmp(params[pos].c_str(), "slave")){
			is_slave_nodes	= true;
		}else{
			ERR("Unknown parameter(%s) for nodes command.", params[pos].c_str());
			return true;	// for continue.
		}
	}
	if(!is_dyna_nodes && !is_update){
		ERR("could not specify both \"nodyna\" and \"noupdate\" parameters.");
		return true;		// for continue.
	}

	// update dynamic nodes list
	if(is_dyna_nodes && is_update){
		if(!CreateDynaTargetChmpx()){
			ERR("Failed to update dynamic chmpx node list, but continue...");
		}
	}
	// print
	if((!is_server_nodes && !is_slave_nodes) || (is_server_nodes && is_slave_nodes)){
		// default both
		print_chmpx_all_nodes(is_dyna_nodes ? TargetNodes : InitialAllNodes);
	}else{
		print_chmpx_nodes_by_type((is_dyna_nodes ? TargetNodes : InitialAllNodes), is_server_nodes);
	}
	return true;	// for continue.
}

//
// Command Line: status [self | all] [host(:port)]
//
// print target node status by SELFSTATUS or ALLSTATUS.
// if tool runs with host option, target node is specified host.
// if tool runs with conf option, must specify host and control port in nodes list.
// self option means printing result of SELFSTATUS control command to node.
// all option means ALLSTATUS command.
//
static bool StatusCommand(params_t& params)
{
	bool	is_all	= false;
	bool	is_self	= false;
	bool	is_param= false;
	string	tghost	= isOneHostTarget ? strInitialHostname	: string("");
	short	tgport	= isOneHostTarget ? nInitialCtrlPort	: CHM_INVALID_PORT;
	string	tgcuk	= isOneHostTarget ? strInitialCuk		: string("");
	string	tgctleps= isOneHostTarget ? strInitialCtlEPS	: string("");
	string	tgcs	= isOneHostTarget ? strInitialCustomSeed: string("");

	for(size_t pos = 0; pos < params.size(); ++pos){
		if(0 == strcasecmp(params[pos].c_str(), "all")){
			is_all	= true;
		}else if(0 == strcasecmp(params[pos].c_str(), "self")){
			is_self	= true;
		}else{
			// host or host:port
			parse_host_parameter(params[pos], tghost, tgport, tgcuk, tgctleps, tgcs);

			// check hostname(:port) exists in initialized host list(conf)
			NodeCtrlInfo	node;
			if(!find_chmpx_node_by_hostname(InitialAllNodes, tghost, tgport, tgcuk, tgctleps, tgcs, node)){
				WAN("Not found %s in initial host list which is included from configuration file, try to check dynamic hostlist.", params[pos].c_str());
				// retry in dynamic hostlist
				if(!find_chmpx_node_by_hostname(TargetNodes, tghost, tgport, tgcuk, tgctleps, tgcs, node)){
					ERR("Not found %s in initial host list which is included from configuration file, try to check dynamic hostlist.", params[pos].c_str());
					return true;
				}
			}
			tghost		= node.GetHostname();
			tgport		= node.GetCtrlport();
			tgcuk		= node.GetCuk();
			tgctleps	= node.GetCtlendpoints();
			tgcs		= node.GetCusomSeed();
			is_param	= true;
		}
	}
	if(is_all && is_self){
		ERR("both \"all\" and \"self\" parameters are specified.");
		return true;
	}

	if(is_self){
		// Send SELFSTATUS command to control port
		if(!isOneHostTarget && !is_param){
			ERR("In order to use the \"self\" parameter, need to specify \"-host\" etc at startup, or specify host information after this parameter.");
			return true;
		}

		string	strResult;
		if(!SendCommandToControlPort(tghost.c_str(), tgport, tgctleps.c_str(), "SELFSTATUS", strResult)){
			ERR("Could not get SELFSTATUS command result from %s:%d", tghost.c_str(), tgport);
			return true;
		}

		// set indent(insert 5 space character to line head)
		strResult = CutEmptyLine(strResult);
		strResult = ReplaceString(strResult, string("\n"), string("\n     "));

		// print
		PRN(" %s:%d = {", tghost.c_str(), tgport);
		PRN("     %s", strResult.c_str());						// NOTE: first line is not inserted 5 space
		PRN(" }");

	}else{	// default all
		// Send ALLSTATUS command to control port
		if(!isOneHostTarget && !is_param){
			ERR("In order to use the \"all\" parameter(or not specified any parameter: default \"all\"), need to specify \"-host\" etc at startup, or specify host information after this parameter.");
			return true;
		}

		string	strResult;
		if(!SendCommandToControlPort(tghost.c_str(), tgport, tgctleps.c_str(), "ALLSTATUS", strResult)){
			ERR("Could not get ALLSTATUS command result from %s:%d", tghost.c_str(), tgport);
			return true;
		}

		bool	is_error= false;
		strResult		= CvtAllStatusResult(strResult, is_error);
		if(is_error){
			ERR("Something error occurred in parsing ALLSTATUS command result for %s:%d", tghost.c_str(), tgport);
			return true;
		}

		// print
		PRN("%s", strResult.c_str());
	}
	return true;
}

//
// Command Line: check [noupdate] [all | host(:port)]
//
// check and print all nodes status/hash/socket connection count.
// if host and control port is specified, check only that host
// and print target node status/etc which are looked by other nodes.
// if noupdate parameter is specified, do not update before doing.
//
static bool CheckCommand(params_t& params)
{
	bool	is_update	= true;
	bool	is_all		= true;
	string	tghost("");
	short	tgport		= CHM_INVALID_PORT;
	string	tgcuk("");
	string	tgctleps("");
	string	tgcs("");

	for(size_t pos = 0; pos < params.size(); ++pos){
		if(0 == strcasecmp(params[pos].c_str(), "noupdate")){
			is_update	= false;
		}else if(0 == strcasecmp(params[pos].c_str(), "all")){
			is_all		= true;
		}else{
			// host or host:port
			is_all		= false;
			parse_host_parameter(params[pos], tghost, tgport, tgcuk, tgctleps, tgcs);
		}
	}
	// update dynamic nodes list
	if(is_update){
		if(!CreateDynaTargetChmpx()){
			ERR("Failed to update dynamic chmpx node list, but continue...");
		}
	}
	// check host if host name is specified
	if(!is_all){
		// check hostname(:port) exists in dynamic host list(conf)
		NodeCtrlInfo	node;
		if(!find_chmpx_node_by_hostname(TargetNodes, tghost, tgport, tgcuk, tgctleps, tgcs, node)){
			PRN("Not found %s(:%d(:%s)) in dynamic host list which is included from configuration file.", tghost.c_str(), tgport, tgcuk.c_str());
			return true;
		}
		tghost		= node.GetHostname();
		tgport		= node.GetCtrlport();
		tgcuk		= node.GetCuk();
		tgctleps	= node.GetCtlendpoints();
		tgcs		= node.GetCusomSeed();
	}

	// get all status detail of nodes
	statusdetails_t		all;
	if(0 == CreateAllStatusDetails(all)){
		MSG("There is no status detail result of nodes.");
	}else{
		DumpAllStatusDetails(all);							// dump
	}

	// get target(all) node status with checking result
	nodechkresults_t	results;

	if(0 == MakeCheckNodeStatus(results, all, tghost, tgport, tgcuk, tgctleps, tgcs)){
		MSG("There is no node status result with checking status.");
	}else{
		DumpNodeCheckResults(results);						// dump
	}

	// print
	string	strOutput;
	if(is_all){
		strOutput = CvtAllNodeStatusResults(results);
	}else{
		strOutput = CvtOneNodeStatusResults(results);
	}
	PRN("%s", strOutput.c_str());

	return true;	// for continue.
}

//
// Command Line: statusupdate [noupdate] [all | host(:port)]
//
static bool StatusUpdateCommand(params_t& params)
{
	bool	is_update	= true;
	bool	is_all		= true;
	string	tghost("");
	short	tgport		= CHM_INVALID_PORT;
	string	tgcuk("");
	string	tgctleps("");
	string	tgcs("");

	for(size_t pos = 0; pos < params.size(); ++pos){
		if(0 == strcasecmp(params[pos].c_str(), "noupdate")){
			is_update	= false;
		}else if(0 == strcasecmp(params[pos].c_str(), "all")){
			is_all		= true;
		}else{
			// host or host:port
			is_all		= false;
			parse_host_parameter(params[pos], tghost, tgport, tgcuk, tgctleps, tgcs);
		}
	}
	// update dynamic nodes list
	if(is_update){
		if(!CreateDynaTargetChmpx()){
			ERR("Failed to update dynamic chmpx node list, but continue...");
		}
	}
	// check host if host name is specified
	if(!is_all){
		// check hostname(:port) exists in dynamic host list(conf)
		NodeCtrlInfo	node;
		if(!find_chmpx_node_by_hostname(TargetNodes, tghost, tgport, tgcuk, tgctleps, tgcs, node)){
			PRN("Not found %s(:%d) in dynamic host list which is included from configuration file.", tghost.c_str(), tgport);
			return true;
		}
		tghost		= node.GetHostname();
		tgport		= node.GetCtrlport();
		//tgcuk		= node.GetCuk();			// not used
		//tgctleps	= node.GetCtlendpoints();	// not used
		//tgcs		= node.GetCusomSeed();		// not used
	}

	// send "UPDATESTATUS" command to host(s)
	for(nodectrllist_t::const_iterator iter = TargetNodes.begin(); iter != TargetNodes.end(); ++iter){
		std::string	uniq_tgname = tghost;
		std::string	uniq_itname = iter->GetHostname();
		if(!is_all && (uniq_tgname != uniq_itname || tgport != iter->GetCtrlport())){
			continue;
		}

		if(!iter->IsServerNode()){
			if(!is_all){
				PRN("%s:%d host is slave node, thus could not send UPDATESTATUS.", tghost.c_str(), tgport);
				return true;
			}
			continue;
		}

		string	strResult;
		if(!SendCommandToControlPort(uniq_itname.c_str(), iter->GetCtrlport(), iter->GetCtlendpoints().c_str(), "UPDATESTATUS", strResult)){
			WAN("Failed to send UPDATESTATUS command to %s:%d, but retry to send another node.", iter->GetHostname().c_str(), iter->GetCtrlport());
		}else{
			//MSG("Receive data : \n\n%s\n", strResult.c_str());
			if(string::npos != strResult.find("SUCCEED")){
				PRN("Succeed to send UPDATESTATUS to %s:%d", iter->GetHostname().c_str(), iter->GetCtrlport());
			}else{
				PRN("Failed to send UPDATESTATUS to %s:%d by \"%s\".", iter->GetHostname().c_str(), iter->GetCtrlport(), strResult.c_str());
			}
		}
	}
	return true;
}

//
// Command Line: servicein [noupdate] [host(:port)]
//
static bool ServiceInCommand(params_t& params)
{
	bool	is_update	= true;
	string	tghost		= isOneHostTarget ? strInitialHostname	: string("");
	short	tgport		= isOneHostTarget ? nInitialCtrlPort	: CHM_INVALID_PORT;
	string	tgcuk("");
	string	tgctleps("");
	string	tgcs("");

	for(size_t pos = 0; pos < params.size(); ++pos){
		if(0 == strcasecmp(params[pos].c_str(), "noupdate")){
			is_update	= false;
		}else{
			// host or host:port
			parse_host_parameter(params[pos], tghost, tgport, tgcuk, tgctleps, tgcs);
		}
	}
	// update dynamic nodes list
	if(is_update){
		if(!CreateDynaTargetChmpx()){
			ERR("Failed to update dynamic chmpx node list, but continue...");
		}
	}
	// check hostname(:port) exists in dynamic host list(conf)
	NodeCtrlInfo	node;
	if(!find_chmpx_node_by_hostname(TargetNodes, tghost, tgport, tgcuk, tgctleps, tgcs, node)){
		PRN("Not found %s(:%d) in dynamic host list which is included from configuration file.", tghost.c_str(), tgport);
		return true;
	}
	// send "SERVICEIN" command
	string	strResult;
	if(!SendCommandToControlPort(node.GetHostname().c_str(), node.GetCtrlport(), node.GetCtlendpoints().c_str(), "SERVICEIN", strResult)){
		ERR("Failed to send SERVICEIN command to %s:%d", node.GetHostname().c_str(), node.GetCtrlport());
	}else{
		//MSG("Receive data : \n\n%s\n", strResult.c_str());
		if(string::npos != strResult.find("SUCCEED")){
			PRN("Succeed to send SERVICEIN to %s:%d", node.GetHostname().c_str(), node.GetCtrlport());
		}else{
			PRN("Failed to send SERVICEIN to %s:%d, error is %s.", node.GetHostname().c_str(), node.GetCtrlport(), strResult.c_str());
		}
	}
	return true;	// for continue.
}

//
// Command Line: serviceout [noupdate] [host(:port)]
//
static bool ServiceOutCommand(params_t& params)
{
	bool	is_update	= true;

	string	tghost("");
	short	tgport		= CHM_INVALID_PORT;
	string	tgcuk("");
	string	tgctleps("");
	string	tgcs("");

	string	sotghost("");
	short	sotgport	= CHM_INVALID_PORT;
	string	sotgcuk("");
	string	sotgctleps("");
	string	sotgcs("");

	for(size_t pos = 0; pos < params.size(); ++pos){
		if(0 == strcasecmp(params[pos].c_str(), "noupdate")){
			is_update	= false;
		}else{
			// host or host:port
			if(tghost.empty()){
				parse_host_parameter(params[pos], tghost, tgport, tgcuk, tgctleps, tgcs);
			}else if(sotghost.empty()){
				parse_host_parameter(params[pos], sotghost, sotgport, sotgcuk, sotgctleps, sotgcs);
			}
		}
	}
	if(sotghost.empty()){
		sotghost	= tghost;
		sotgport	= tgport;
		sotgcuk		= tgcuk;
		sotgctleps	= tgctleps;
		sotgcs		= tgcs;
	}

	// update dynamic nodes list
	if(is_update){
		if(!CreateDynaTargetChmpx()){
			ERR("Failed to update dynamic chmpx node list, but continue...");
		}
	}
	if(tghost.empty()){
		ERR("Target hostname is not specified, you need to specify this.");
		return true;
	}
	// check hostname(:port) exists in dynamic host list(conf)
	NodeCtrlInfo	node;
	if(!find_chmpx_node_by_hostname(TargetNodes, tghost, tgport, tgcuk, tgctleps, tgcs, node)){
		MSG("Not found %s(:%d) in dynamic host list which is included from configuration file.", tghost.c_str(), tgport);
	}else{
		tghost		= node.GetHostname();
		tgport		= node.GetCtrlport();
		tgctleps	= node.GetCtlendpoints();
		//tgcuk		= node.GetCuk();			// cppcheck reports this is unreadVariable
		//tgcs		= node.GetCusomSeed();		// cppcheck reports this is unreadVariable
	}
	if(CHM_INVALID_PORT == tgport){
		PRN("Target control port is not specified, you need to specify this.");
		return true;
	}

	// Make SERVICEOUT parameter(special expression)
	string	strCtlparam = sotghost + string(":") + to_string(sotgport);
	if(!sotgcs.empty()){
		if(!sotgcuk.empty() || !sotgctleps.empty()){
			WAN("Custom ID Seed is specified. However, CUK(%s) and Controle Endpoints(%s) are also specified at the same time. They are ignored.", sotgcuk.c_str(), sotgctleps.c_str());
		}
		strCtlparam += string(":") + sotgcs;
	}else if(!sotgcuk.empty()){
		if(!sotgctleps.empty()){
			WAN("CUK is specified. However, Controle Endpoints(%s) is also specified at the same time. It is ignored.", sotgctleps.c_str());
		}
		strCtlparam += string(":") + sotgcuk;
	}else if(!sotgctleps.empty()){
		strCtlparam += string(":") + sotgctleps;
	}
	string	strCommand	= string("SERVICEOUT ") + strCtlparam;

	// send "SERVICEOUT" command to target host
	string	strResult;
	if(!SendCommandToControlPort(tghost.c_str(), tgport, tgctleps.c_str(), strCommand.c_str(), strResult)){
		WAN("Failed to send SERVICEOUT command to %s:%d, probably the host is down. thus send another node.", tghost.c_str(), tgport);
	}else{
		//MSG("Receive data : \n\n%s\n", strResult.c_str());
		if(string::npos != strResult.find("SUCCEED")){
			PRN("Succeed to send SERVICEOUT to %s:%d", tghost.c_str(), tgport);
			return true;
		}
		WAN("Failed to send SERVICEOUT to %s:%d, error is \"%s\", but retry to send another node.", tghost.c_str(), tgport, strResult.c_str());
	}

	// send "SERVICEOUT" command to all host
	for(nodectrllist_t::const_iterator iter = TargetNodes.begin(); iter != TargetNodes.end(); ++iter){
		if(!SendCommandToControlPort(iter->GetHostname().c_str(), iter->GetCtrlport(), iter->GetCtlendpoints().c_str(), strCommand.c_str(), strResult)){
			WAN("Failed to send SERVICEOUT command to %s:%d, but retry to send another node.", iter->GetHostname().c_str(), iter->GetCtrlport());
		}else{
			//MSG("Receive data : \n\n%s\n", strResult.c_str());
			if(string::npos != strResult.find("SUCCEED")){
				PRN("Succeed to send SERVICEOUT to %s:%d", iter->GetHostname().c_str(), iter->GetCtrlport());
				return true;
			}else{
				WAN("Failed to send SERVICEOUT to %s:%d by \"%s\", but retry to send another node.", iter->GetHostname().c_str(), iter->GetCtrlport(), strResult.c_str());
			}
		}
	}
	PRN("Failed to send SERVICEOUT to all node, but no node is succeed.", strCommand.c_str());

	return true;	// for continue.
}

//
// Command Line: merge [noupdate] [start | abort | complete]
//
static bool MergeCommand(params_t& params)
{
	bool		is_update	= true;
	string		strCommand("");

	for(size_t pos = 0; pos < params.size(); ++pos){
		if(0 == strcasecmp(params[pos].c_str(), "noupdate")){
			is_update	= false;
		}else if(strCommand.empty() && 0 == strcasecmp(params[pos].c_str(), "start")){
			strCommand	= "MERGE";
		}else if(strCommand.empty() && (0 == strcasecmp(params[pos].c_str(), "abort") || 0 == strcasecmp(params[pos].c_str(), "stop"))){
			strCommand	= "ABORTMERGE";
		}else if(strCommand.empty() && (0 == strcasecmp(params[pos].c_str(), "complete") || 0 == strcasecmp(params[pos].c_str(), "comp") || 0 == strcasecmp(params[pos].c_str(), "finish") || 0 == strcasecmp(params[pos].c_str(), "fin"))){
			strCommand	= "COMPMERGE";
		}else{
			ERR("Unknown parameter(%s) for MERGE command.", params[pos].c_str());
			return true;
		}
	}
	// update dynamic nodes list
	if(is_update){
		if(!CreateDynaTargetChmpx()){
			ERR("Failed to update dynamic chmpx node list, but continue...");
		}
	}

	// send "MERGE" or "ABORTMERGE" or "COMPMERGE" command
	for(nodectrllist_t::const_iterator iter = TargetNodes.begin(); iter != TargetNodes.end(); ++iter){
		string	strResult;
		if(!SendCommandToControlPort(iter->GetHostname().c_str(), iter->GetCtrlport(), iter->GetCtlendpoints().c_str(), strCommand.c_str(), strResult)){
			ERR("Failed to send %s command to %s:%d, but retry to send another node.", strCommand.c_str(), iter->GetHostname().c_str(), iter->GetCtrlport());
		}else{
			//MSG("Receive data : \n\n%s\n", strResult.c_str());

			if(string::npos != strResult.find("SUCCEED") && string::npos != strResult.find("There no server on RING")){
				// Respond "SUCCEED: There no server on RING",
				//
				// This mean that the command is sent to slave, thus retry to send another node.
				//
				MSG("Failed to send %s to %s:%d, because this node is slave. thus retry to send another node.", strCommand.c_str(), iter->GetHostname().c_str(), iter->GetCtrlport());

			}else if(string::npos != strResult.find("SUCCEED")){
				PRN("Succeed to send %s to %s:%d", strCommand.c_str(), iter->GetHostname().c_str(), iter->GetCtrlport());
				return true;

			}else{
				WAN("Failed to send %s to %s:%d by %s, but retry to send another node.", strCommand.c_str(), iter->GetHostname().c_str(), iter->GetCtrlport(), strResult.c_str());
			}
		}
	}
	PRN("Failed to send %s to all node, but no node is succeed.", strCommand.c_str());

	return true;	// for continue.
}

//
// Command Line: suspend [noupdate]
//				 nosuspend [noupdate]
//
static bool SuspendCommand(params_t& params, bool is_suspend)
{
	bool	is_update = true;

	for(size_t pos = 0; pos < params.size(); ++pos){
		if(0 == strcasecmp(params[pos].c_str(), "noupdate")){
			is_update	= false;
		}else{
			ERR("Unknown parameter(%s) for %s command.", params[pos].c_str(), is_suspend ? "suspend" : "nosuspend");
			return true;
		}
	}
	// update dynamic nodes list
	if(is_update){
		if(!CreateDynaTargetChmpx()){
			ERR("Failed to update dynamic chmpx node list, but continue...");
		}
	}

	// send "SUSPENDMERGE" or "NOSUSPENDMERGE" command
	string	strCommand = is_suspend ? "SUSPENDMERGE" : "NOSUSPENDMERGE";
	for(nodectrllist_t::const_iterator iter = TargetNodes.begin(); iter != TargetNodes.end(); ++iter){
		string	strResult;
		if(!SendCommandToControlPort(iter->GetHostname().c_str(), iter->GetCtrlport(), iter->GetCtlendpoints().c_str(), strCommand.c_str(), strResult)){
			ERR("Failed to send %s command to %s:%d, but retry to send another node.", strCommand.c_str(), iter->GetHostname().c_str(), iter->GetCtrlport());
		}else{
			//MSG("Receive data : \n\n%s\n", strResult.c_str());

			if(string::npos != strResult.find("ERROR") && string::npos != strResult.find("executed on the slave node")){
				// Respond "ERROR: The command can not be executed on the slave node",
				//
				// This mean that the command is sent to slave, thus retry to send another node.
				//
				MSG("Failed to send %s to %s:%d, because this node is slave. thus retry to send another node.", strCommand.c_str(), iter->GetHostname().c_str(), iter->GetCtrlport());

			}else if(string::npos != strResult.find("SUCCEED")){
				PRN("Succeed to send %s to %s:%d", strCommand.c_str(), iter->GetHostname().c_str(), iter->GetCtrlport());
				return true;

			}else{
				WAN("Failed to send %s to %s:%d by %s, but retry to send another node.", strCommand.c_str(), iter->GetHostname().c_str(), iter->GetCtrlport(), strResult.c_str());
			}
		}
	}
	PRN("Failed to send %s to all node, but no node is succeed.", strCommand.c_str());

	return true;	// for continue.
}

//
// Command Line: dump [noupdate] [host(:port)]
//
static bool DumpCommand(params_t& params)
{
	bool	is_update	= true;
	string	tghost		= isOneHostTarget ? strInitialHostname	: string("");
	short	tgport		= isOneHostTarget ? nInitialCtrlPort	: CHM_INVALID_PORT;
	string	tgcuk("");
	string	tgctleps("");
	string	tgcs("");

	for(size_t pos = 0; pos < params.size(); ++pos){
		if(0 == strcasecmp(params[pos].c_str(), "noupdate")){
			is_update	= false;
		}else{
			// host or host:port
			parse_host_parameter(params[pos], tghost, tgport, tgcuk, tgctleps, tgcs);
		}
	}
	// update dynamic nodes list
	if(is_update){
		if(!CreateDynaTargetChmpx()){
			ERR("Failed to update dynamic chmpx node list, but continue...");
		}
	}
	// check hostname(:port) exists in dynamic host list(conf)
	NodeCtrlInfo	node;
	if(!find_chmpx_node_by_hostname(TargetNodes, tghost, tgport, tgcuk, tgctleps, tgcs, node)){
		PRN("Not found %s(:%d) in dynamic host list which is included from configuration file.", tghost.c_str(), tgport);
		return true;
	}

	// send "DUMP" command
	string	strResult;
	if(!SendCommandToControlPort(node.GetHostname().c_str(), node.GetCtrlport(), node.GetCtlendpoints().c_str(), "DUMP", strResult)){
		ERR("Failed to send DUMP command to %s:%d", node.GetHostname().c_str(), node.GetCtrlport());
	}else{
		//MSG("Receive data : \n\n%s\n", strResult.c_str());
		PRN("%s", strResult.c_str());
	}
	return true;	// for continue.
}

//
// Command Line: version [nodyna | noupdate]
//
// print all chmpx nodes version.
// if noupdate parameter is specified, do not update before doing.
// if nodyna parameter is specified, only initially chmpx nodes.
//
static string VersionCommandSub(const char* hostname, short ctrlport, const char* ctlendpoints)
{
	string	strResult;
	if(!SendCommandToControlPort(hostname, ctrlport, ctlendpoints, "VERSION", strResult)){
		//ERR("Could not get VERSION command result from %s:%d", hostname, ctrlport);
		strResult = "Could not get VERSION";
	}else{
		strResult = CutEmptyLine(strResult);
		if(strResult == "Unknown command(VERSION)"){
			strResult = "This chmpx version is old version, VERSION command supports after 1.0.57";
		}
	}
	return strResult;
}

static bool VersionCommand(params_t& params)
{
	bool	is_dyna_nodes	= true;
	bool	is_update		= true;
	for(size_t pos = 0; pos < params.size(); ++pos){
		if(0 == strcasecmp(params[pos].c_str(), "nodyna")){
			is_dyna_nodes	= false;
		}else if(0 == strcasecmp(params[pos].c_str(), "noupdate")){
			is_update		= false;
		}else{
			ERR("Unknown parameter(%s) for version command.", params[pos].c_str());
			return true;	// for continue.
		}
	}
	if(!is_dyna_nodes && !is_update){
		ERR("could not specify both \"nodyna\" and \"noupdate\" parameters.");
		return true;		// for continue.
	}

	// update dynamic nodes list
	if(is_dyna_nodes && is_update){
		if(!CreateDynaTargetChmpx()){
			ERR("Failed to update dynamic chmpx node list, but continue...");
		}
	}

	// print
	nodectrllist_t	svrnodes;
	nodectrllist_t	slvnodes;
	get_chmpx_nodes(is_dyna_nodes ? TargetNodes : InitialAllNodes, svrnodes, true);
	get_chmpx_nodes(is_dyna_nodes ? TargetNodes : InitialAllNodes, slvnodes, false);

	PRN(" Chmpx server nodes             : %zu", svrnodes.size());
	if(!svrnodes.empty()){
		PRN(" {");
		for(nodectrllist_t::const_iterator iter = svrnodes.begin(); iter != svrnodes.end(); ++iter){
			string	strVersion = VersionCommandSub(iter->GetHostname().c_str(), iter->GetCtrlport(), iter->GetCtlendpoints().c_str());
			PRN("    %s:%d	= %s", iter->GetHostname().c_str(), iter->GetCtrlport(), strVersion.c_str());
		}
		PRN(" }");
	}
	PRN(" Chmpx slave nodes              : %zu", slvnodes.size());
	if(!slvnodes.empty()){
		PRN(" {");
		for(nodectrllist_t::const_iterator iter = slvnodes.begin(); iter != slvnodes.end(); ++iter){
			string	strVersion = VersionCommandSub(iter->GetHostname().c_str(), iter->GetCtrlport(), iter->GetCtlendpoints().c_str());
			PRN("    %s:%d	= %s", iter->GetHostname().c_str(), iter->GetCtrlport(), strVersion.c_str());
		}
		PRN(" }");
	}
	return true;	// for continue.
}

//
// Command Line: dbglevel [slt | err | wan | msg | dmp]       bumpup debugging level or specify level
//
static bool DbglevelCommand(params_t& params)
{
	if(params.empty()){					// bumpup level
		if(is_print_dmp){				// to silent
			if(is_chmpx_dbg){
				SetChmDbgMode(CHMDBG_SILENT);
			}
			is_print_dmp = false;
			is_print_msg = false;
			is_print_wan = false;
			is_print_err = false;
		}else if(is_print_msg){			// to dump
			if(is_chmpx_dbg){
				SetChmDbgMode(CHMDBG_MSG);
			}
			is_print_dmp = true;
			is_print_msg = false;
			is_print_wan = false;
			is_print_err = false;
		}else if(is_print_wan){			// to message
			if(is_chmpx_dbg){
				SetChmDbgMode(CHMDBG_MSG);
			}
			is_print_dmp = false;
			is_print_msg = true;
			is_print_wan = true;
			is_print_err = true;
		}else if(is_print_err){			// to warning
			if(is_chmpx_dbg){
				SetChmDbgMode(CHMDBG_WARN);
			}
			is_print_dmp = false;
			is_print_msg = false;
			is_print_wan = true;
			is_print_err = true;
		}else{							// to error
			if(is_chmpx_dbg){
				SetChmDbgMode(CHMDBG_ERR);
			}
			is_print_dmp = false;
			is_print_msg = false;
			is_print_wan = false;
			is_print_err = true;
		}
	}else if(1 < params.size()){
		ERR("Unknown parameter(%s) for dbglevel command.", params[1].c_str());
		return true;

	}else{
		if(0 == strcasecmp(params[0].c_str(), "silent") || 0 == strcasecmp(params[0].c_str(), "slt")){
			if(is_chmpx_dbg){
				SetChmDbgMode(CHMDBG_SILENT);
			}
			is_print_dmp = false;
			is_print_msg = false;
			is_print_wan = false;
			is_print_err = false;
		}else if(0 == strcasecmp(params[0].c_str(), "error") || 0 == strcasecmp(params[0].c_str(), "err")){
			if(is_chmpx_dbg){
				SetChmDbgMode(CHMDBG_ERR);
			}
			is_print_dmp = false;
			is_print_msg = false;
			is_print_wan = false;
			is_print_err = true;
		}else if(0 == strcasecmp(params[0].c_str(), "warning") || 0 == strcasecmp(params[0].c_str(), "wan")){
			if(is_chmpx_dbg){
				SetChmDbgMode(CHMDBG_WARN);
			}
			is_print_dmp = false;
			is_print_msg = false;
			is_print_wan = true;
			is_print_err = true;
		}else if(0 == strcasecmp(params[0].c_str(), "info") || 0 == strcasecmp(params[0].c_str(), "msg")){
			if(is_chmpx_dbg){
				SetChmDbgMode(CHMDBG_MSG);
			}
			is_print_dmp = false;
			is_print_msg = true;
			is_print_wan = true;
			is_print_err = true;
		}else if(0 == strcasecmp(params[0].c_str(), "dump") || 0 == strcasecmp(params[0].c_str(), "dmp")){
			if(is_chmpx_dbg){
				SetChmDbgMode(CHMDBG_MSG);
			}
			is_print_dmp = true;
			is_print_msg = true;
			is_print_wan = true;
			is_print_err = true;
		}else{
			ERR("Unknown parameter(%s) for dbglevel command.", params[0].c_str());
			return true;
		}
	}
	return true;	// for continue.
}

//
// Command Line: dchmpx <on | off>
//
static bool DChmpxCommand(params_t& params)
{
	if(params.empty()){
		// toggle mode
		is_chmpx_dbg = !is_chmpx_dbg;

	}else if(1 < params.size()){
		ERR("Unknown parameter(%s) for dchmpx command.", params[1].c_str());
		return true;

	}else{
		if(0 == strcasecmp(params[0].c_str(), "on") || 0 == strcasecmp(params[0].c_str(), "yes") || 0 == strcasecmp(params[0].c_str(), "y")){
			// enable
			if(is_chmpx_dbg){
				ERR("Already dchmpx is enabled.");
				return true;
			}
			is_chmpx_dbg = true;

		}else if(0 == strcasecmp(params[0].c_str(), "off") || 0 == strcasecmp(params[0].c_str(), "no") || 0 == strcasecmp(params[0].c_str(), "n")){
			// disable
			if(!is_chmpx_dbg){
				ERR("Already comlog is disabled.");
				return true;
			}
			is_chmpx_dbg = false;

		}else{
			ERR("Unknown parameter(%s) for comlog command.", params[0].c_str());
			return true;
		}
	}

	if(!is_chmpx_dbg){
		SetChmDbgMode(CHMDBG_SILENT);
		PRN("Disable chmpx debugging message.");
	}else{
		if(is_print_msg){
			SetChmDbgMode(CHMDBG_MSG);
		}else if(is_print_wan){
			SetChmDbgMode(CHMDBG_WARN);
		}else if(is_print_err){
			SetChmDbgMode(CHMDBG_ERR);
		}else{
			SetChmDbgMode(CHMDBG_SILENT);
		}
		PRN("Enable chmpx debugging message.");
	}
	return true;	// for continue.
}

//
// Command Line: history(his)
//
static bool HistoryCommand(const ConsoleInput& InputIF)
{
	const strarr_t&	history = InputIF.GetAllHistory();

	int	nCnt = 1;
	// cppcheck-suppress postfixOperator
	for(strarr_t::const_iterator iter = history.begin(); iter != history.end(); iter++, nCnt++){
		PRN(" %d  %s", nCnt, iter->c_str());
	}
	return true;
}

//
// Utility : Check Loop command in command file
//
static bool IsLoopCommand(const string& strCommand, int& second, int& limit)
{
	string	strTmp("");
	for(string::const_iterator iter = strCommand.begin(); iter != strCommand.end(); ++iter){
		if(' ' == *iter || '\t' == *iter || '\r' == *iter || '\n' == *iter){
			break;
		}
		strTmp += *iter;
	}
	if(0 != strcasecmp("loop", strTmp.c_str())){
		return false;
	}

	// found loop command
	option_t	opts;
	if(!LineOptionParser(strCommand.c_str(), opts) || opts.end() == opts.find("loop")){
		// something wrong parameters
		return false;
	}
	params_t&	params	= opts["loop"];
	second				= atoi(params[0].c_str());
	limit				= 0;
	if(1 < params.size()){
		limit			= atoi(params[1].c_str());
	}
	return true;
}

static bool IsCommandInLoop(const string& strCommand, string& strSubCommand)
{
	string	strTmp("");
	for(string::const_iterator iter = strCommand.begin(); iter != strCommand.end(); ++iter){
		if(' ' == *iter || '\t' == *iter || '\r' == *iter || '\n' == *iter){
			for(; iter != strCommand.end(); ++iter){
				strSubCommand += *iter;
			}
			break;
		}
		strTmp += *iter;
	}
	if(0 != strcasecmp("loopcmd", strTmp.c_str())){
		return false;
	}
	// found "loopcmd" command
	strSubCommand = trim(strSubCommand);

	return true;
}

static bool GetLoopCommand(const strlst_t& CommandList, strlst_t::const_iterator& iter, int& second, int& limit, strlst_t& LoopCommandList)
{
	if(iter == CommandList.end()){
		return false;
	}
	if(!IsLoopCommand(*iter, second, limit)){
		return false;
	}

	LoopCommandList.clear();
	string	strCommand;
	for(++iter; iter != CommandList.end(); ++iter){
		strCommand.clear();

		if(!IsCommandInLoop(*iter, strCommand)){
			break;
		}
		LoopCommandList.push_back(strCommand);
	}
	return true;
}

//
// Utility : Consecutive Commands
//
// This function is reentrant for only command file.
//
static bool ConsecutiveCommands(ConsoleInput& InputIF, strlst_t& CommandList, int second, int limit, bool allow_nest, bool& is_exit)
{
	is_exit = false;

	// loop
	for(strlst_t::const_iterator iter = CommandList.begin(); !IsBreakLoop && iter != CommandList.end(); ){
		if(0 < iter->length()){
			PRN("%s%s", InputIF.GetPrompt(), iter->c_str());

			int			subsecond	= 0;
			int			sublimit	= 0;
			strlst_t	SubLoopCommandList;
			if(allow_nest && GetLoopCommand(CommandList, iter, subsecond, sublimit, SubLoopCommandList)){
				// found "loop" command in command list, thus run loop.
				if(!ConsecutiveCommands(InputIF, SubLoopCommandList, subsecond, sublimit, false, is_exit)){		// already set signal
					return false;
				}
				// [NOTE]
				// ConsecutiveCommands function is skip iterator to next.
				//
			}else{
				// run normal command
				if(!CommandStringHandle(InputIF, iter->c_str(), is_exit)){
					ERR("Something error occurred at command - \"%s\", so stop running.", iter->c_str());
					break;
				}else if(is_exit){
					break;
				}
				++iter;
			}
		}else{
			++iter;
		}
		if(iter == CommandList.end()){
			if(0 < limit){
				--limit;
				if(0 >= limit){
					break;
				}
			}
			sleep(second);
			iter = CommandList.begin();
		}
	}
	return true;
}

//
// Command Line: loop [second]
//
static bool LoopCommand(ConsoleInput& InputIF, params_t& params, bool& is_exit)
{
	is_exit			= false;
	int		second	= atoi(params[0].c_str());
	int		limit	= 0;
	if(1 < params.size()){
		limit		= atoi(params[1].c_str());
	}

	// Local prompt
	ConsoleInput	LoopInputIF;
	string			prompt = string("CLT LOOP(") + to_string(second) + string("s," + (0 >= limit ? string("nolimit") : (to_string(limit) + string("count"))) + ") COMMAND INPUT> ");
	LoopInputIF.SetPrompt(prompt.c_str());		// Prompt(ChmpxCtrl="CLT LOOP(xxs,YYcount) COMMAND INPUT> ")
	LoopInputIF.SetMax(0);						// command history 0

	// message
	PRN("Please specify the command to loop and enter \".\" to finish command input.");
	PRN("When commands input is finished, start to run commands.");

	// get input commands
	strlst_t	CommandList;
	while(true){
		// input command
		if(!LoopInputIF.GetCommand()){
			ERR("Something error occurred while reading stdin: err(%d).", LoopInputIF.LastErrno());
			continue;
		}
		// check command
		string	strLine = LoopInputIF.c_str();
		strLine			= trim(strLine);
		if(strLine == "."){
			break;
		}
		// check command
		if(!IsAllowedSubCommand(strLine, false)){
			PRN("ERROR: %s command is not allowed in LOOP command.", strLine.c_str());
			continue;
		}
		// set history
		string	strHis	= string("loopcmd ") + strLine;
		InputIF.PutHistory(strHis.c_str());

		CommandList.push_back(strLine);
	}
	if(CommandList.empty()){
		return true;
	}

	// loop
	LoopInputIF.SetPrompt("CLT LOOP> ");			// Prompt(ChmpxCtrl=CLT LOOP)
	bool	result	= ConsecutiveCommands(LoopInputIF, CommandList, second, limit, false, is_exit);		// second / limit loop
	if(IsBreakLoop){
		PRN("");									// Put CR
		IsBreakLoop	= false;
	}
	return result;
}

//
// Command Line: loopcmd [command...]
//
// This command could not be specified in command line, this must be in script.
//
static bool LoopCmdCommand(params_t& params)
{
	PRN("ERROR: \"loopcmd\" must be specified in command file. This command could not be specified in command line.");
	return true;
}

//
// Command Line: save <file path>
//
static bool SaveCommand(const ConsoleInput& InputIF, params_t& params)
{
	int	fd;
	if(-1 == (fd = open(params[0].c_str(), O_CREAT | O_RDWR | O_TRUNC, 0644))){
		ERR("Could not open file(%s) for writing history. errno(%d)", params[0].c_str(), errno);
		return true;	// for continue
	}

	const strarr_t&	history = InputIF.GetAllHistory();
	// cppcheck-suppress postfixOperator
	for(strarr_t::const_iterator iter = history.begin(); iter != history.end(); iter++){
		// check except command for writing file
		if(	0 == strncasecmp(iter->c_str(), "his",		strlen("his"))		||
			0 == strncasecmp(iter->c_str(), "history",	strlen("history"))	||
			0 == strncasecmp(iter->c_str(), "shell",	strlen("shell"))	||
			0 == strncasecmp(iter->c_str(), "sh",		strlen("sh"))		||
			0 == strncasecmp(iter->c_str(), "!!",		strlen("!!"))		||
			0 == strncasecmp(iter->c_str(), "save",		strlen("save"))		||
			0 == strncasecmp(iter->c_str(), "load",		strlen("load"))		||
			iter->at(0) == '!' )
		{
			continue;
		}
		const char*	pHistory;
		size_t		wrote_byte;
		ssize_t		one_wrote_byte;
		for(pHistory = iter->c_str(), wrote_byte = 0, one_wrote_byte = 0L; wrote_byte < iter->length(); wrote_byte += one_wrote_byte){
			if(-1 == (one_wrote_byte = write(fd, &pHistory[wrote_byte], (iter->length() - wrote_byte)))){
				ERR("Failed writing history to file(%s). errno(%d)", params[0].c_str(), errno);
				// cppcheck-suppress unmatchedSuppression
				// cppcheck-suppress unreadVariable
				CHM_CLOSE(fd);
				return true;	// for continue
			}
		}
		if(-1 == write(fd, "\n", 1)){
			ERR("Failed writing history to file(%s). errno(%d)", params[0].c_str(), errno);
			// cppcheck-suppress unmatchedSuppression
			// cppcheck-suppress unreadVariable
			CHM_CLOSE(fd);
			return true;	// for continue
		}
	}
	// cppcheck-suppress unmatchedSuppression
	// cppcheck-suppress unreadVariable
	CHM_CLOSE(fd);
	return true;
}

//
// Command Line: load <file path>
//
static bool LoadCommand(ConsoleInput& InputIF, params_t& params, bool& is_exit)
{
	int	fd;
	if(-1 == (fd = open(params[0].c_str(), O_RDONLY))){
		ERR("Could not open file(%s) for reading commands. errno(%d)", params[0].c_str(), errno);
		return true;	// for continue
	}

	// load commands
	string		CommandLine;
	strlst_t	CommandList;
	for(bool ReadResult = true; ReadResult; ){
		ReadResult = ReadLine(fd, CommandLine);
		if(0 == CommandLine.length()){
			continue;
		}
		// check command
		if(!IsAllowedSubCommand(CommandLine, true)){
			PRN("ERROR: %s command is not allowed in command file, then skip this command.", CommandLine.c_str());
			continue;
		}
		CommandList.push_back(CommandLine);
	}
	// cppcheck-suppress unmatchedSuppression
	// cppcheck-suppress unreadVariable
	CHM_CLOSE(fd);

	// run
	bool	result	= ConsecutiveCommands(InputIF, CommandList, 0, 1, true, is_exit);	// 0 sec / 1 loop
	if(IsBreakLoop){
		PRN("");									// Put CR
		IsBreakLoop	= false;
	}
	return result;
}

//
// Command Line: shell      exit shell(same as "!" command).
//
static bool ShellCommand(void)
{
	static const char*	pDefaultShell = "/bin/sh";

	if(0 == system(NULL)){
		ERR("Could not execute shell.");
		return true;	// for continue
	}

	const char*	pEnvShell = getenv("SHELL");
	if(!pEnvShell){
		pEnvShell = pDefaultShell;
	}
	if(-1 == system(pEnvShell)){
		ERR("Something error occurred by executing shell(%s).", pEnvShell);
		return true;	// for continue
	}
	return true;
}

//
// Command Line: echo <string>...                             echo string
//
static bool EchoCommand(params_t& params)
{
	string	strDisp("");
	for(size_t cnt = 0; cnt < params.size(); ++cnt){
		if(0 < cnt){
			strDisp += ' ';
		}
		strDisp += params[cnt];
	}
	if(!strDisp.empty()){
		PRN("%s", strDisp.c_str());
	}
	return true;
}

//
// Command Line: sleep <second>
//
static bool SleepCommand(params_t& params)
{
	if(1 != params.size()){
		if(1 < params.size()){
			ERR("unknown parameter %s.", params[1].c_str());
		}else{
			ERR("sleep command needs parameter.");
		}
		return true;		// for continue.
	}
	unsigned int	sec = static_cast<unsigned int>(atoi(params[0].c_str()));
	sleep(sec);
	return true;
}

static bool ExecHistoryCommand(ConsoleInput& InputIF, ssize_t history_pos, bool& is_exit)
{
	const strarr_t&	history = InputIF.GetAllHistory();

	if(-1L == history_pos && 0 < history.size()){
		history_pos = static_cast<ssize_t>(history.size() - 1UL);

	}else if(0 < history_pos && static_cast<size_t>(history_pos) < history.size()){		// last history is "!..."
		history_pos--;

	}else{
		ERR("No history number(%zd) is existed.", history_pos);
		return true;																	// for continue.
	}
	InputIF.RemoveLastHistory();														// remove last(this) command from history
	InputIF.PutHistory(history[history_pos].c_str());									// and push this command(replace history)

	// execute
	PRN(" %s", history[history_pos].c_str());
	bool	result	= CommandStringHandle(InputIF, history[history_pos].c_str(), is_exit);

	return result;
}

static bool CommandHandle(ConsoleInput& InputIF)
{
	if(!InputIF.GetCommand()){
		ERR("Something error occurred while reading stdin: err(%d).", InputIF.LastErrno());
		return false;
	}
	const string	strLine = InputIF.c_str();
	bool			is_exit = false;
	if(0 < strLine.length() && '!' == strLine[0]){
		// special character("!") command
		const char*	pSpecialCommand = strLine.c_str();
		pSpecialCommand++;

		if('\0' == *pSpecialCommand){
			// exit shell
			InputIF.RemoveLastHistory();	// remove last(this) command from history
			InputIF.PutHistory("shell");	// and push "shell" command(replace history)
			if(!ShellCommand()){
				return false;
			}
		}else{
			// execute history
			ssize_t	history_pos;
			if(1 == strlen(pSpecialCommand) && '!' == *pSpecialCommand){
				// "!!"
				history_pos = -1L;
			}else{
				history_pos = static_cast<ssize_t>(atoi(pSpecialCommand));
			}
			if(!ExecHistoryCommand(InputIF, history_pos, is_exit) || is_exit){
				return false;
			}
		}
	}else{
		if(!CommandStringHandle(InputIF, strLine.c_str(), is_exit) || is_exit){
			return false;
		}
	}
	return true;
}

//---------------------------------------------------------
// Command Handling
//---------------------------------------------------------
static bool CommandStringHandle(ConsoleInput& InputIF, const char* pCommand, bool& is_exit)
{
	is_exit = false;

	if(CHMEMPTYSTR(pCommand)){
		return true;
	}

	option_t	opts;
	if(!LineOptionParser(pCommand, opts)){
		return true;	// for continue.
	}
	if(opts.empty()){
		return true;
	}

	// Command switch
	if(opts.end() != opts.find("help")){
		LineHelp();

	}else if(opts.end() != opts.find("quit")){
		PRN("Quit.");
		is_exit = true;

	}else if(opts.end() != opts.find("update")){
		if(!UpdateNodesCommand()){
			CleanOptionMap(opts);
			return false;
		}

	}else if(opts.end() != opts.find("nodes")){
		if(!PrintNodesCommand(opts["nodes"])){
			CleanOptionMap(opts);
			return false;
		}

	}else if(opts.end() != opts.find("status")){
		if(!StatusCommand(opts["status"])){
			CleanOptionMap(opts);
			return false;
		}

	}else if(opts.end() != opts.find("check")){
		if(!CheckCommand(opts["check"])){
			CleanOptionMap(opts);
			return false;
		}

	}else if(opts.end() != opts.find("statusupdate")){
		if(!StatusUpdateCommand(opts["statusupdate"])){
			CleanOptionMap(opts);
			return false;
		}

	}else if(opts.end() != opts.find("servicein")){
		if(!ServiceInCommand(opts["servicein"])){
			CleanOptionMap(opts);
			return false;
		}

	}else if(opts.end() != opts.find("serviceout")){
		if(!ServiceOutCommand(opts["serviceout"])){
			CleanOptionMap(opts);
			return false;
		}

	}else if(opts.end() != opts.find("merge")){
		if(!MergeCommand(opts["merge"])){
			CleanOptionMap(opts);
			return false;
		}

	}else if(opts.end() != opts.find("suspend")){
		if(!SuspendCommand(opts["suspend"], true)){
			CleanOptionMap(opts);
			return false;
		}

	}else if(opts.end() != opts.find("nosuspend")){
		if(!SuspendCommand(opts["nosuspend"], false)){
			CleanOptionMap(opts);
			return false;
		}

	}else if(opts.end() != opts.find("dump")){
		if(!DumpCommand(opts["dump"])){
			CleanOptionMap(opts);
			return false;
		}

	}else if(opts.end() != opts.find("version")){
		if(!VersionCommand(opts["version"])){
			CleanOptionMap(opts);
			return false;
		}

	}else if(opts.end() != opts.find("dbglevel")){
		if(!DbglevelCommand(opts["dbglevel"])){
			CleanOptionMap(opts);
			return false;
		}

	}else if(opts.end() != opts.find("dchmpx")){
		if(!DChmpxCommand(opts["dchmpx"])){
			CleanOptionMap(opts);
			return false;
		}

	}else if(opts.end() != opts.find("history")){
		if(!HistoryCommand(InputIF)){
			CleanOptionMap(opts);
			return false;
		}

	}else if(opts.end() != opts.find("loop")){
		if(!LoopCommand(InputIF, opts["loop"], is_exit)){
			CleanOptionMap(opts);
			return false;
		}

	}else if(opts.end() != opts.find("loopcmd")){
		if(!LoopCmdCommand(opts["loopcmd"])){
			CleanOptionMap(opts);
			return false;
		}

	}else if(opts.end() != opts.find("save")){
		if(!SaveCommand(InputIF, opts["save"])){
			CleanOptionMap(opts);
			return false;
		}

	}else if(opts.end() != opts.find("load")){
		if(!LoadCommand(InputIF, opts["load"], is_exit)){
			CleanOptionMap(opts);
			return false;
		}

	}else if(opts.end() != opts.find("shell")){
		if(!ShellCommand()){
			CleanOptionMap(opts);
			return false;
		}

	}else if(opts.end() != opts.find("echo")){
		if(!EchoCommand(opts["echo"])){
			CleanOptionMap(opts);
			return false;
		}

	}else if(opts.end() != opts.find("sleep")){
		if(!SleepCommand(opts["sleep"])){
			CleanOptionMap(opts);
			return false;
		}

	}else{
		ERR("Unknown command. see \"help\".");
	}
	CleanOptionMap(opts);

	return true;
}

//---------------------------------------------------------
// Main
//---------------------------------------------------------
int main(int argc, char** argv)
{
	option_t	opts;
	string		prgname;
	bool		is_load_from_env = false;

	//----------------------
	// Console: default
	//----------------------
	ConsoleInput	InputIF;
	InputIF.SetMax(1000);						// command history 1000
	InputIF.SetPrompt("CLT> ");					// Prompt(ChmpxCtrl=CLT)

	if(!ExecOptionParser(argc, argv, opts, prgname)){
		Help(prgname.c_str());
		exit(EXIT_FAILURE);
	}

	//----------------------
	// Check and Set Options
	//----------------------
	// -dchmpx
	if(opts.end() != opts.find("-dchmpx")){
		is_chmpx_dbg = true;
	}else{
		is_chmpx_dbg = false;
		SetChmDbgMode(CHMDBG_SILENT);
	}
	// -d(-g)
	if(opts.end() != opts.find("-d")){
		if(0 == strcasecmp(opts["-d"][0].c_str(), "silent") || 0 == strcasecmp(opts["-d"][0].c_str(), "slt")){
			if(is_chmpx_dbg){
				SetChmDbgMode(CHMDBG_SILENT);
			}
			is_print_dmp = false;
			is_print_msg = false;
			is_print_wan = false;
			is_print_err = false;
		}else if(0 == strcasecmp(opts["-d"][0].c_str(), "error") || 0 == strcasecmp(opts["-d"][0].c_str(), "err")){
			if(is_chmpx_dbg){
				SetChmDbgMode(CHMDBG_ERR);
			}
			is_print_dmp = false;
			is_print_msg = false;
			is_print_wan = false;
			is_print_err = true;
		}else if(0 == strcasecmp(opts["-d"][0].c_str(), "warning") || 0 == strcasecmp(opts["-d"][0].c_str(), "wan")){
			if(is_chmpx_dbg){
				SetChmDbgMode(CHMDBG_WARN);
			}
			is_print_dmp = false;
			is_print_msg = false;
			is_print_wan = true;
			is_print_err = true;
		}else if(0 == strcasecmp(opts["-d"][0].c_str(), "info") || 0 == strcasecmp(opts["-d"][0].c_str(), "msg")){
			if(is_chmpx_dbg){
				SetChmDbgMode(CHMDBG_MSG);
			}
			is_print_dmp = false;
			is_print_msg = true;
			is_print_wan = true;
			is_print_err = true;
		}else if(0 == strcasecmp(opts["-d"][0].c_str(), "dump") || 0 == strcasecmp(opts["-d"][0].c_str(), "dmp")){
			if(is_chmpx_dbg){
				SetChmDbgMode(CHMDBG_MSG);
			}
			is_print_dmp = true;
			is_print_msg = true;
			is_print_wan = true;
			is_print_err = true;
		}else{
			ERR("Unknown parameter(%s) value for \"-d\" option.", opts["-d"][0].c_str());
			exit(EXIT_FAILURE);
		}
	}
	// -help
	if(opts.end() != opts.find("-help")){
		Help(prgname.c_str());
		exit(EXIT_SUCCESS);
	}
	// -ctrlport
	if(opts.end() != opts.find("-ctrlport")){
		string	strtmp	= opts["-ctrlport"][0];
		nInitialCtrlPort		= static_cast<short>(atoi(strtmp.c_str()));
	}
	// -cuk
	if(opts.end() != opts.find("-cuk")){
		strInitialCuk	= opts["-cuk"][0];
	}
	// -ctlendpoints
	if(opts.end() != opts.find("-ctlendpoints")){
		strInitialCtlEPS= opts["-ctlendpoints"][0];
	}
	// -custom_seed
	if(opts.end() != opts.find("-custom_seed")){
		strInitialCustomSeed= opts["-custom_seed"][0];
	}
	// -group
	if(opts.end() != opts.find("-group")){
		NodeCtrlInfo::SetGroup(opts["-group"][0].c_str());
	}
	// -host
	if(opts.end() != opts.find("-host")){
		strInitialHostname	= opts["-host"][0];
	}
	// -type
	if(opts.end() != opts.find("-type")){
		if(!NodeCtrlInfo::SetType(opts["-type"][0].c_str())){
			ERR("Unknown parameter(%s) value for \"-type\" option.", opts["-type"][0].c_str());
			exit(EXIT_FAILURE);
		}
	}

	// Confirm required options
	if(!strInitialHostname.empty() && (opts.end() != opts.find("-conf") || opts.end() != opts.find("-json"))){
		ERR("\"-host\" and \"-conf\"(or \"-json\") option cannot be specified at the same time.");
		exit(EXIT_FAILURE);

	}else if(!strInitialHostname.empty()){
		// check group name
		if(NodeCtrlInfo::GetGroup().empty()){
			ERR("If you specify the \"-host\" option, also specify the \"-group\" option.");
			exit(EXIT_FAILURE);
		}
		// check control port
		if(CHM_INVALID_PORT == nInitialCtrlPort){
			ERR("No control port(-ctrlport option) is specified.");
			exit(EXIT_FAILURE);
		}

	}else if(opts.end() != opts.find("-conf") || opts.end() != opts.find("-json")){
		if(!NodeCtrlInfo::GetGroup().empty()){
			WAN("The \"-group\" option was specified, but it is ignored.");
		}
		// -conf or -json
		if(opts.end() != opts.find("-conf")){
			strInitialConfig	= opts["-conf"][0];
		}
		if(opts.end() != opts.find("-json")){
			if(!strInitialConfig.empty()){
				ERR("both option \"-conf\" and \"-json\" could not be specified.");
				exit(EXIT_FAILURE);
			}
			strInitialConfig	= opts["-json"][0];
		}

	}else{
		// any option is not specified, try to load environment
		CHMCONFTYPE	conftype = check_chmconf_type_ex(NULL, CHM_CONFFILE_ENV_NAME, CHM_JSONCONF_ENV_NAME, &strInitialConfig);
		if(CHMCONF_TYPE_UNKNOWN == conftype || CHMCONF_TYPE_NULL == conftype){
			ERR("unknown configuration type loaded from environment.");
			exit(EXIT_FAILURE);
		}
		if(strInitialConfig.empty()){
			ERR("configuration file or json is not specified.");
			exit(EXIT_FAILURE);
		}
		if(!NodeCtrlInfo::GetGroup().empty()){
			WAN("The \"-group\" option was specified, but it is ignored.");
		}
		MSG("option \"-host\" and \"-conf\" and \"-json\" are not specified, then using environments(%s or %s).", CHM_CONFFILE_ENV_NAME, CHM_JSONCONF_ENV_NAME);
		is_load_from_env = true;
	}

	// -server / -slave
	if(opts.end() != opts.find("-server") && opts.end() != opts.find("-slave")){
		ERR("both option \"-server\" and \"-slave\" could not be specified.");
		exit(EXIT_FAILURE);
	}else if(opts.end() != opts.find("-server")){
		if(strInitialHostname.empty()){
			ERR("option \"-server\" must be specified with \"-host\" option.");
			exit(EXIT_FAILURE);
		}
		isInitialServerMode	= true;
	}else if(opts.end() != opts.find("-slave")){
		if(strInitialHostname.empty()){
			ERR("option \"-slave\" must be specified with \"-host\" option.");
			exit(EXIT_FAILURE);
		}
		isInitialServerMode	= false;
	}
	// -threadcnt
	if(opts.end() != opts.find("-threadcnt")){
		string	strtmp	= opts["-threadcnt"][0];
		nThreadCount	= atoi(strtmp.c_str());
	}
	// -check
	int	nTmpIntervalSec	= 0;
	strlst_t	StartupLoopCommand;
	if(opts.end() != opts.find("-check")){
		string	strtmp	= opts["-check"][0];
		nTmpIntervalSec	= static_cast<short>(atoi(strtmp.c_str()));
		StartupLoopCommand.push_back(string("check"));
	}
	// -status
	if(opts.end() != opts.find("-status")){
		if(!StartupLoopCommand.empty()){
			ERR("Option \"-run\" and \"-check\" and \"-status\" are exclusive specifications.");
			exit(EXIT_FAILURE);
		}
		string	strtmp	= opts["-status"][0];
		nTmpIntervalSec	= static_cast<short>(atoi(strtmp.c_str()));
		StartupLoopCommand.push_back(string("status"));
	}
	// -nocolor
	if(opts.end() != opts.find("-nocolor")){
		isColorDisplay		= false;
	}
	// -lap
	if(opts.end() != opts.find("-lap")){
		LapTime::Enable();
	}
	// -history
	if(opts.end() != opts.find("-history")){
		int	hiscount = atoi(opts["-history"][0].c_str());
		InputIF.SetMax(static_cast<size_t>(hiscount));
	}
	// -run option
	string	CommandFile("");
	if(opts.end() != opts.find("-run")){
		if(0 == opts["-run"].size()){
			ERR("Option \"-run\" needs parameter as command file path.");
			exit(EXIT_FAILURE);
		}else if(1 < opts["-run"].size()){
			ERR("Unknown parameter(%s) value for \"-run\" option.", opts["-run"][1].c_str());
			exit(EXIT_FAILURE);
		}
		// check file
		struct stat	st;
		if(0 != stat(opts["-run"][0].c_str(), &st)){
			ERR("Parameter command file path(%s) for option \"-run\" does not exist(errno=%d).", opts["-run"][0].c_str(), errno);
			exit(EXIT_FAILURE);
		}
		CommandFile = opts["-run"][0];
	}
	//
	// check conflict option
	//
	if(!StartupLoopCommand.empty() && !CommandFile.empty()){
		ERR("Option \"-run\" and \"-check\" and \"-status\" are exclusive specifications.");
		exit(EXIT_FAILURE);
	}
	// cleanup for valgrind
	CleanOptionMap(opts);

	// Initialize map
	HostnameMap::Initialize();

	//----------------------
	// initialize nodes information
	//----------------------
	if(!strInitialConfig.empty()){
		if(!load_initial_chmpx_nodes(InitialAllNodes, strInitialConfig, nInitialCtrlPort, (strInitialCuk.empty() ? NULL : strInitialCuk.c_str()))){
			ERR("Could not load nodes information by configuration/local SHM.");
			HostnameMap::Clear();
			exit(EXIT_FAILURE);
		}
		isOneHostTarget = false;
	}else if(!strInitialHostname.empty()){
		if(!add_chmpx_node(InitialAllNodes, CHM_INVALID_CHMPXID, strInitialHostname, nInitialCtrlPort, strInitialCuk, strInitialCtlEPS, strInitialCustomSeed, isInitialServerMode, true)){
			ERR("Could not load nodes information by configuration.");
			HostnameMap::Clear();
			exit(EXIT_FAILURE);
		}
		isOneHostTarget = true;
	}else{
		ERR("both \"-host\" and \"-conf(or -json)\" option(environment) are not specified.");
		HostnameMap::Clear();
		exit(EXIT_FAILURE);
	}

	//----------------------
	// Other initialize
	//----------------------
	if(!BlockSignal(SIGPIPE)){
		ERR("Could not block SIGPIPE.");
		HostnameMap::Clear();
		exit(EXIT_FAILURE);
	}
	if(!SetSigIntHandler()){
		ERR("Could not set SIGINT handler.");
		HostnameMap::Clear();
		exit(EXIT_FAILURE);
	}

	//----------------------
	// Main Loop
	//----------------------
	bool	IsWelcomMsg = true;
	IsBreakLoop			= false;
	do{
		if(!CommandFile.empty()){
			// command file at starting
			string	LoadCommandLine("load ");
			LoadCommandLine += CommandFile;

			bool	is_exit = false;
			if(!CommandStringHandle(InputIF, LoadCommandLine.c_str(), is_exit) || is_exit){
				break;
			}
			CommandFile.clear();

		}else if(!StartupLoopCommand.empty()){
			// command check or status at starting
			bool	is_exit = false;
			if(!ConsecutiveCommands(InputIF, StartupLoopCommand, nTmpIntervalSec, 0, false, is_exit) || is_exit){
				break;
			}
			StartupLoopCommand.clear();

		}else if(IsWelcomMsg){
			// print message
			PRN("-------------------------------------------------------");
			PRN("CHMPX CONTROL TOOL");
			PRN("-------------------------------------------------------");
			PRN(" CHMPX library version          : %s",	VERSION);
			PRN(" Debug level                    : %s",	(is_print_msg ? "Message(Information)" : is_print_wan ? "Warning" : is_print_err ? "Error" : "Silent"));
			PRN(" Chmpx library debug level      : %s",	(is_chmpx_dbg ? (is_print_msg ? "Message(Information)" : is_print_wan ? "Warning" : is_print_err ? "Error" : "Silent") : "Silent"));
			PRN(" Print command lap time         : %s",	LapTime::IsEnable() ? "yes" : "no");
			PRN(" Command line history count     : %zu",InputIF.GetMax());
			PRN(" Chmpx nodes specified type     : %s", (!strInitialConfig.empty() ? (is_load_from_env ? "configuration file/json from environment" : "configuration file/json") : !strInitialHostname.empty() ? "hostname" : "no"));
			if(!strInitialConfig.empty()){
				PRN("    Load Configuration          : %s",	strInitialConfig.c_str());
				if(CHM_INVALID_PORT != nInitialCtrlPort){
					PRN("    Specified Control port      : %d",	nInitialCtrlPort);
				}
				if(!strInitialCuk.empty()){
					PRN("    Specified CUK               : %s",	strInitialCuk.c_str());
				}
				if(!strInitialCtlEPS.empty()){
					PRN("    Specified Control Endpoints : %s",	strInitialCtlEPS.c_str());
				}
				if(!strInitialCustomSeed.empty()){
					PRN("    Specified Custom ID Seed    : %s",	strInitialCustomSeed.c_str());
				}
			}else if(!strInitialHostname.empty()){
				PRN("    Hostname is not specified   : (%s)",	strInitialHostname.c_str());
				PRN("    Specified Control port      : %d",	(CHM_INVALID_PORT == nInitialCtrlPort ? 0 : nInitialCtrlPort));
				PRN("    Specified CUK               : %s",	strInitialCuk.empty() ? "" : strInitialCuk.c_str());
				PRN("    Specified Control Endpoints : %s",	strInitialCtlEPS.empty() ? "" : strInitialCtlEPS.c_str());
				PRN("    Specified Custom ID Seed    : %s",	strInitialCustomSeed.empty() ? "" : strInitialCtlEPS.c_str());
				PRN("    Specified Chmpx mode        : %s",	isInitialServerMode ? "Server node" : "Slave node");
			}
			PRN("-------------------------------------------------------");
			PRN(" Chmpx nodes information at start");
			PRN("-------------------------------------------------------");
			print_chmpx_all_nodes(InitialAllNodes);
			PRN("-------------------------------------------------------");
		}
		IsWelcomMsg = false;
		IsBreakLoop = false;

		// start interactive until error occurred.
	}while(CommandHandle(InputIF) && !IsBreakLoop);

	if(IsBreakLoop){
		PRN("");				// Put CR
	}
	InputIF.Clean();

	HostnameMap::Clear();
	exit(EXIT_SUCCESS);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noexpandtab sw=4 ts=4 fdm=marker
 * vim<600: noexpandtab sw=4 ts=4
 */
