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
#ifndef	CHMSTRUCTURE_TCC
#define	CHMSTRUCTURE_TCC

#include <endian.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <unistd.h>
#include <sstream>
#include <list>

#include "chmcommon.h"
#include "chmutil.h"
#include "chmdbg.h"
#include "chmstructure.h"
#include "chmconf.h"
#include "chmhash.h"
#include "chmnetdb.h"
#include "chmregex.h"

//---------------------------------------------------------
// Typedefs
//---------------------------------------------------------
typedef std::list<chmpxid_t>				chmpxidlist_t;
typedef std::map<chmpxid_t, bool>			chmpxidmap_t;
typedef std::list<chmhash_t>				chmhashlist_t;
typedef std::map<chmhash_t, bool>			chmhashmap_t;
typedef std::list<pid_t>					pidlist_t;
typedef std::map<pid_t, bool>				pidbmap_t;
typedef std::vector<msgid_t>				msgidlist_t;
typedef std::list<int>						socklist_t;

//---------------------------------------------------------
// Utility
//---------------------------------------------------------
template<typename T>
std::string STR_CHMPXSTS_FULL(T status)
{
	std::string	str_status;
	str_status  = STR_CHMPXSTS_ISSAFE(status);
	str_status += " -> ";
	str_status += STR_CHMPXSTS_RING(status);
	str_status += STR_CHMPXSTS_LIVE(status);
	str_status += STR_CHMPXSTS_ACTION(status);
	str_status += STR_CHMPXSTS_OPERATE(status);
	str_status += STR_CHMPXSTS_SUSPEND(status);

	return str_status;
}

template<typename T>
std::string STR_MQFLAG_FULL(T flag)
{
	std::string	str_flag;
	str_flag  = STR_MQFLAG_KIND(flag);
	str_flag += STR_MQFLAG_ASSIGNED(flag);
	str_flag += STR_MQFLAG_ACTIVATED(flag);

	return str_flag;
}

template<typename T1, typename T2>
void CVT_SSL_STRUCTURE(T1& chmpx_ssl, const T2& chmnode_cfginfo)
{
	chmpx_ssl.is_ssl				= chmnode_cfginfo.is_ssl;
	chmpx_ssl.verify_peer			= chmnode_cfginfo.verify_peer;
	chmpx_ssl.is_ca_file			= chmnode_cfginfo.is_ca_file;

	strcpy(chmpx_ssl.capath,		chmnode_cfginfo.capath.c_str());
	strcpy(chmpx_ssl.server_cert,	chmnode_cfginfo.server_cert.c_str());
	strcpy(chmpx_ssl.server_prikey,	chmnode_cfginfo.server_prikey.c_str());
	strcpy(chmpx_ssl.slave_cert,	chmnode_cfginfo.slave_cert.c_str());
	strcpy(chmpx_ssl.slave_prikey,	chmnode_cfginfo.slave_prikey.c_str());
}

template<typename T>
void INIT_CHMPXSSL(const T& ssl)
{
	ssl.is_ssl		= false;
	ssl.verify_peer	= false;
	ssl.is_ca_file	= false;

	memset(ssl.capath,			0, CHM_MAX_PATH_LEN);
	memset(ssl.server_cert,		0, CHM_MAX_PATH_LEN);
	memset(ssl.server_prikey,	0, CHM_MAX_PATH_LEN);
	memset(ssl.slave_cert,		0, CHM_MAX_PATH_LEN);
	memset(ssl.slave_prikey,	0, CHM_MAX_PATH_LEN);
}

template<typename T>
void COPY_CHMPXSSL(const T& dst, const T& src)
{
	dst.is_ssl		= src.is_ssl;
	dst.verify_peer	= src.verify_peer;
	dst.is_ca_file	= src.is_ca_file;

	memcpy(dst.capath,			src.capath,			CHM_MAX_PATH_LEN);
	memcpy(dst.server_cert,		src.server_cert,	CHM_MAX_PATH_LEN);
	memcpy(dst.server_prikey,	src.server_prikey,	CHM_MAX_PATH_LEN);
	memcpy(dst.slave_cert,		src.slave_cert,		CHM_MAX_PATH_LEN);
	memcpy(dst.slave_prikey,	src.slave_prikey,	CHM_MAX_PATH_LEN);
}

// [NOTE]
// Comparison of SSL/TLS structure only compares SSL/TLS enable flag and
// Verify Peer flag.
// After CHMPX supports multiple SSL/TLS libraries, the SSL/TLS structure
// comparison result will be an error because the description of CA/Host
// certifications will be different for each library. Then we check only
// is_ssl and verify_peer member.
//
template<typename T>
bool CMP_CHMPXSSL(const T& src1, const T& src2)
{
	if(src1.is_ssl != src2.is_ssl){
		return false;
	}
	if(!src1.is_ssl){
		return true;
	}
	// [NOTE]
	// Old version compared is_ca_file, capath, server_cert, server_prikey, slave_cert, slave_prikey members with verify_peer.
	// But it does not have mean now.
	//
	if(src1.verify_peer == src2.verify_peer){
		return true;
	}
	return false;
}

template<typename T>
void FREE_HOSTSSLMAP(T& info)
{
	for(typename T::iterator iter = info.begin(); iter != info.end(); info.erase(iter++)){
		if(iter->second){
			delete iter->second;
		}
	}
}

//---------------------------------------------------------
// Utility
//---------------------------------------------------------
inline const char* get_chmpxid_seed_type_string(CHMPXID_SEED_TYPE type)
{
	if(CHMPXID_SEED_NAME == type){
		return "name";
	}else if(CHMPXID_SEED_CUK == type){
		return "cuk";
	}else if(CHMPXID_SEED_CTLENDPOINTS == type){
		return "control endpoints";
	}else if(CHMPXID_SEED_CUSTOM == type){
		return "custom";
	}
	WAN_CHMPRN("CHMPXID Seed type is unknown.");
	return "unknown";
}

inline std::string get_hostport_pairs_string(const PCHMPXHP_RAWPAIR pair, size_t length)
{
	std::string	result;

	if(!pair || 0 == length){
		return result;
	}
	for(size_t cnt = 0; cnt < length; ++cnt){
		if(CHMEMPTYSTR(pair[cnt].name) && CHM_INVALID_PORT == pair[cnt].port){
			continue;
		}
		if(!result.empty()){
			result += ",";
		}
		result += std::string(pair[cnt].name);
		if(CHM_INVALID_PORT != pair[cnt].port){
			result += ":";
			result += to_string(pair[cnt].port);
		}
	}
	return result;
}

inline bool parse_hostport_pairs_from_string(const char* hostport_pair, PCHMPXHP_RAWPAIR pair)
{
	if(CHMEMPTYSTR(hostport_pair) || !pair){
		return false;
	}
	memset(pair->name, 0, NI_MAXHOST);

	std::string				base_pair(hostport_pair);
	std::string::size_type	pos = base_pair.find(':');
	if(std::string::npos == pos){
		strncpy(pair->name, base_pair.c_str(), NI_MAXHOST - 1);
		pair->port = CHM_INVALID_PORT;
	}else{
		strncpy(pair->name, base_pair.substr(0, pos).c_str(), NI_MAXHOST - 1);
		pair->port = static_cast<short>(cvt_string_to_number(base_pair.substr(pos + 1).c_str(), true));
	}
	return true;
}

inline bool copy_hostport_pairs(PCHMPXHP_RAWPAIR pdst, const PCHMPXHP_RAWPAIR psrc, size_t length)
{
	if(!pdst || !psrc || 0 == length){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}
	for(size_t cnt = 0; cnt < length; ++cnt){
		strcpy(pdst[cnt].name, psrc[cnt].name);
		pdst[cnt].port = psrc[cnt].port;
	}
	return true;
}

inline bool compare_hostport_pairs(PCHMPXHP_RAWPAIR phpp1, const PCHMPXHP_RAWPAIR phpp2, size_t length)
{
	if(!phpp2 || !phpp1 || 0 == length){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}
	for(size_t cnt1 = 0; cnt1 < length; ++cnt1){
		bool	found;
		found = false;
		for(size_t cnt2 = 0; cnt2 < length; ++cnt2){
			if(0 == strcmp(phpp1[cnt1].name, phpp2[cnt2].name) && phpp1[cnt1].port == phpp2[cnt2].port){
				found = true;
				break;
			}
		}
		if(!found){
			return false;
		}
	}
	return true;
}

inline bool cvt_hostport_pairs(PCHMPXHP_RAWPAIR pdst, const hostport_list_t& src, size_t max)
{
	if(!pdst || 0 == max){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}
	if(max < src.size()){
		WAN_CHMPRN("Source host:port pair list(%zu) is over maximum destination array size(%zu), then a complete copy is not possible.", src.size(), max);
	}

	// copy
	hostport_list_t::const_iterator	iter;
	size_t							cnt;
	for(iter = src.begin(), cnt = 0; src.end() != iter && cnt < max; ++iter){
		if(iter->host.empty() && CHM_INVALID_PORT == iter->port){
			continue;
		}
		if(NI_MAXHOST < (iter->host.length() + 1)){
			WAN_CHMPRN("Source host(%s) is over maximum length(%d), then a complete copy is not possible.", iter->host.c_str(), NI_MAXHOST);
			memset(pdst[cnt].name, 0, NI_MAXHOST);
			strncpy(pdst[cnt].name, iter->host.c_str(), NI_MAXHOST - 1);
		}else{
			strcpy(pdst[cnt].name, iter->host.c_str());
		}
		pdst[cnt].port = iter->port;
		++cnt;
	}
	// clear rest structures
	for(; cnt < max; ++cnt){
		pdst[cnt].name[0]	= '\0';
		pdst[cnt].port		= CHM_INVALID_PORT;
	}
	return true;
}

inline bool rev_hostport_pairs(hostport_list_t& dst, PCHMPXHP_RAWPAIR psrc, size_t srccnt)
{
	if(!psrc || 0 == srccnt){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}
	dst.clear();

	// copy
	for(size_t cnt = 0; cnt < srccnt; ++cnt){
		if(!CHMEMPTYSTR(psrc[cnt].name)){
			HOSTPORTPAIR	tmp;
			tmp.host = psrc[cnt].name;
			tmp.port = psrc[cnt].port;
			dst.push_back(tmp);
		}
	}
	return true;
}

inline bool cvt_parameters_by_chmpxid_type(const CHMNODE_CFGINFO* pchmpxnode, CHMPXID_SEED_TYPE type, std::string& cuk, std::string& custom_seed, PCHMPXHP_RAWPAIR pendpoints, PCHMPXHP_RAWPAIR pctlendpoints, PCHMPXHP_RAWPAIR pforward_peers, PCHMPXHP_RAWPAIR preverse_peers)
{
	if(!pchmpxnode || !pendpoints || !pctlendpoints || !pforward_peers || !preverse_peers){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}

	// check chmpxid seed type
	if(CHMPXID_SEED_CUK == type){
		if(pchmpxnode->cuk.empty()){
			ERR_CHMPRN("CHMPXID Seed type is CUK, but CUK is empty.");
			return false;
		}
	}else if(CHMPXID_SEED_CTLENDPOINTS == type){
		if(pchmpxnode->ctlendpoints.empty()){
			ERR_CHMPRN("CHMPXID Seed type is CONTROL ENDPOINTS, but ctlendpoints is empty.");
			return false;
		}
	}

	if(CHMPXID_SEED_CUSTOM == type){
		if(pchmpxnode->custom_seed.empty()){
			ERR_CHMPRN("CHMPXID Seed type is CUSTOM ID SEED, but it is empty.");
			return false;
		}
		custom_seed = pchmpxnode->custom_seed;
	}else{
		if(!pchmpxnode->custom_seed.empty()){
			WAN_CHMPRN("CHMPXID Seed type is not CUSTOM ID SEED, but CUSTOM ID SEED is not empty, then not copy it.");
		}
		custom_seed.clear();
	}

	// set values
	cuk = pchmpxnode->cuk;
	cvt_hostport_pairs(pendpoints,		pchmpxnode->endpoints,		EXTERNAL_EP_MAX);
	cvt_hostport_pairs(pctlendpoints,	pchmpxnode->ctlendpoints,	EXTERNAL_EP_MAX);
	cvt_hostport_pairs(pforward_peers,	pchmpxnode->forward_peers,	FORWARD_PEER_MAX);
	cvt_hostport_pairs(preverse_peers,	pchmpxnode->reverse_peers,	REVERSE_PEER_MAX);

	return true;
}

inline std::string get_hostsslmap_key(const char* hostname, short ctlport, const char* cuk)
{
	std::string	key;
	if(CHMEMPTYSTR(hostname)){
		ERR_CHMPRN("Parameter is wrong.");
		return key;
	}
	key	= hostname;
	key	+= ":";
	key	+= (CHM_INVALID_PORT == ctlport) ? "" : to_string(ctlport);
	key	+= ":";
	key	+= CHMEMPTYSTR(cuk) ? "" : cuk;

	return key;
}

//---------------------------------------------------------
// Status last update time
//---------------------------------------------------------
// [NOTICE]
// The old status last update time was initially a function time(NULL)
// result value. This is now expressed as time(NULL) + us(microsecond).
// For the value, the time_t contains the time(NULL) value in the upper
// 40 bits and the us(microsecond) value in the lower 24 bits.
// There is a possibility of connection of new and old chmpx processes,
// and timespec structure can not be used as it is.
//
// We plan to change it to timespec structure in the future.
// Following utility functions are converter for new format.
//
#define	STATUS_UPDATE_TIME_HI_MASK		static_cast<time_t>(0xffffffffff000000)
#define	STATUS_UPDATE_TIME_LOW_MASK		static_cast<time_t>(0x0000000000ffffff)

static time_t get_basis_update_time(void)
{
	// [NOTE]
	// To avoid static object initialization order problem(SIOF)
	//
	static time_t	basis_update_time = (((time(NULL) / (365 * 24 * 60 * 60)) * (365 * 24 * 60 * 60)) << 24) & STATUS_UPDATE_TIME_HI_MASK;
	return basis_update_time;
}

inline time_t GetStatusLastUpdateTime(void)
{
	struct timespec	ts = {0, 0};
	if(!RT_TIMESPEC(&ts)){
		ts.tv_sec	= time(NULL);
		ts.tv_nsec	= 0;
	}
	return (((ts.tv_sec << 24) & STATUS_UPDATE_TIME_HI_MASK) | (static_cast<time_t>(ts.tv_nsec / 1000) & STATUS_UPDATE_TIME_LOW_MASK));
}

inline time_t NormalizeStatusLastUpdateTime(time_t base)
{
	if(base >= get_basis_update_time()){
		return base;
	}
	return ((base << 24) & STATUS_UPDATE_TIME_HI_MASK);
}

//
// return	-1	: src1 < src2
//			0	: src1 == src2
//			1	: src1 > src2
//
inline int CompareStatusUpdateTime(time_t src1, time_t src2)
{
	// [NOTE]
	// The comparison is made up to seconds in consideration of
	// the time difference between servers.
	//
	time_t	result = (NormalizeStatusLastUpdateTime(src1) & STATUS_UPDATE_TIME_HI_MASK) - (NormalizeStatusLastUpdateTime(src2) & STATUS_UPDATE_TIME_HI_MASK);
	return ((result < 0) ? -1 : (0 < result) ? 1 : 0);
}

inline void ParseUpdateTime(time_t updatetime, time_t& sec, long& nsec)
{
	updatetime	= NormalizeStatusLastUpdateTime(updatetime);
	sec			= (updatetime & STATUS_UPDATE_TIME_HI_MASK) >> 24;
	nsec		= static_cast<long>((updatetime & STATUS_UPDATE_TIME_LOW_MASK) * 1000);
}

inline std::string CvtUpdateTimeToString(time_t updatetime)
{
	time_t		sec	= 0;
	long		nsec= 0;
	ParseUpdateTime(updatetime, sec, nsec);

	char				buf[128];
	const struct tm*	tm = localtime(&sec);
	strftime(buf, sizeof(buf), "%Y-%m-%d %Hh %Mm %Ss ", tm);

	std::string	result = std::string(buf) + to_string(nsec / (1000 * 1000)) + std::string("ms ") + to_string((nsec / 1000) % 1000)  + std::string("us");
	return result;
}

//---------------------------------------------------------
// Basic template
//---------------------------------------------------------
template<typename T>
class structure_lap
{
	public:
		typedef T		st_type;
		typedef T*		st_ptr_type;

	protected:
		st_ptr_type		pAbsPtr;
		const void*		pShmBase;

	public:
		explicit structure_lap(st_ptr_type ptr = NULL, const void* shmbase = NULL, bool is_abs = true);
		virtual ~structure_lap();

		st_ptr_type GetAbsPtr(void) const { return pAbsPtr; }
		st_ptr_type GetRelPtr(void) const { return CHM_REL(pShmBase, pAbsPtr, st_ptr_type); }

		virtual void Reset(st_ptr_type ptr, const void* shmbase, bool is_abs = true);
		virtual bool Initialize(void);
		virtual bool Dump(std::stringstream& sstream, const char* spacer) const;
		virtual bool Clear(void);
};

template<typename T>
structure_lap<T>::structure_lap(st_ptr_type ptr, const void* shmbase, bool is_abs)
{
	Reset(ptr, shmbase, is_abs);
}

template<typename T>
structure_lap<T>::~structure_lap()
{
}

template<typename T>
void structure_lap<T>::Reset(st_ptr_type ptr, const void* shmbase, bool is_abs)
{
	if(is_abs){
		pAbsPtr	= ptr;
		pShmBase= shmbase;
	}else{
		pAbsPtr	= CHM_ABS(shmbase, ptr, st_ptr_type);
		pShmBase= shmbase;
	}
}

template<typename T>
bool structure_lap<T>::Initialize(void)
{
	return true;
}

template<typename T>
bool structure_lap<T>::Dump(std::stringstream& sstream, const char* spacer) const
{
	return true;
}

template<typename T>
bool structure_lap<T>::Clear(void)
{
	if(!pAbsPtr || !pShmBase){
		ERR_CHMPRN("Object does not set.");
		return false;
	}
	return true;
}

//---------------------------------------------------------
// For CHMSOCKLIST
//---------------------------------------------------------
template<typename T>
class chmsocklist_lap : public structure_lap<T>
{
	public:
		typedef T					st_type;
		typedef T*					st_ptr_type;
		typedef structure_lap<T>	basic_type;

	public:
		explicit chmsocklist_lap(st_ptr_type ptr = NULL, const void* shmbase = NULL, bool is_abs = true);

		virtual bool Initialize(void);
		bool Initialize(st_ptr_type prev, st_ptr_type next, bool is_abs = true);
		bool Initialize(int sock);
		bool Clear(void);
		virtual bool Dump(std::stringstream& sstream, const char* spacer) const;
		st_ptr_type Dup(void);

		st_ptr_type GetFirstPtr(bool is_abs = true);
		bool ToFirst(void);
		bool ToLast(void);
		bool ToNext(void);
		long Count(void);
		bool Insert(st_ptr_type ptr, bool is_abs);
		st_ptr_type Retrieve(int sock);
		st_ptr_type Retrieve(void);
		st_ptr_type Find(int sock, bool is_abs);

		bool GetAllSocks(socklist_t& list);
};

template<typename T>
chmsocklist_lap<T>::chmsocklist_lap(st_ptr_type ptr, const void* shmbase, bool is_abs)
{
	structure_lap<T>::Reset(ptr, shmbase, is_abs);
}

template<typename T>
bool chmsocklist_lap<T>::Initialize(void)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMSOCKLIST does not set.");
		return false;
	}
	basic_type::pAbsPtr->prev	= NULL;
	basic_type::pAbsPtr->next	= NULL;
	basic_type::pAbsPtr->sock	= CHM_INVALID_SOCK;
	return true;
}

template<typename T>
bool chmsocklist_lap<T>::Initialize(st_ptr_type prev, st_ptr_type next, bool is_abs)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMSOCKLIST does not set.");
		return false;
	}
	basic_type::pAbsPtr->prev	= is_abs ? CHM_REL(basic_type::pShmBase, prev, st_ptr_type) : prev;
	basic_type::pAbsPtr->next	= is_abs ? CHM_REL(basic_type::pShmBase, next, st_ptr_type) : next;
	basic_type::pAbsPtr->sock	= CHM_INVALID_SOCK;
	return true;
}

template<typename T>
bool chmsocklist_lap<T>::Initialize(int sock)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMSOCKLIST does not set.");
		return false;
	}
	basic_type::pAbsPtr->sock = sock;
	return true;
}

template<typename T>
bool chmsocklist_lap<T>::Clear(void)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMSOCKLIST does not set.");
		return false;
	}
	basic_type::pAbsPtr->prev	= NULL;
	basic_type::pAbsPtr->next	= NULL;
	basic_type::pAbsPtr->sock	= CHM_INVALID_SOCK;
	return true;
}

template<typename T>
bool chmsocklist_lap<T>::Dump(std::stringstream& sstream, const char* spacer) const
{
	sstream << (spacer ? spacer : "") << "sock             = ";

	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		//ERR_CHMPRN("PCHMSOCKLIST does not set.");
		sstream << static_cast<int>(CHM_INVALID_SOCK) << std::endl;
		return true;
	}

	// To first
	st_ptr_type	cur;
	for(cur = const_cast<st_ptr_type>(basic_type::pAbsPtr); cur->prev; cur = CHM_ABS(basic_type::pShmBase, cur->prev, st_ptr_type));

	long	count;
	for(count = 0L; cur; cur = CHM_ABS(basic_type::pShmBase, cur->next, st_ptr_type), count++){
		sstream << (0L < count ? ", " : "") << cur->sock;
	}
	if(0L == count){
		sstream << static_cast<int>(CHM_INVALID_SOCK);
	}
	sstream << std::endl;

	return true;
}

template<typename T>
typename chmsocklist_lap<T>::st_ptr_type chmsocklist_lap<T>::Dup(void)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		//ERR_CHMPRN("PCHMSOCKLIST does not set.");
		return NULL;
	}

	long	count = Count();
	if(0 == count){
		return NULL;
	}

	// To first
	st_ptr_type	cur;
	for(cur = const_cast<st_ptr_type>(basic_type::pAbsPtr); cur->prev; cur = CHM_ABS(basic_type::pShmBase, cur->prev, st_ptr_type));

	// duplicate
	st_ptr_type	pdst;
	if(NULL == (pdst = reinterpret_cast<st_ptr_type>(calloc(count, sizeof(st_type))))){
		ERR_CHMPRN("Could not allocation memory.");
		return NULL;
	}
	for(st_ptr_type pdstcur = pdst, pdstprev = NULL; cur; cur = CHM_ABS(basic_type::pShmBase, cur->next, st_ptr_type), pdstprev = pdstcur++){
		if(pdstprev){
			pdstprev->next = pdstcur;
		}
		pdstcur->prev = pdstprev;
		pdstcur->sock = cur->sock;
	}

	return pdst;
}

template<typename T>
typename chmsocklist_lap<T>::st_ptr_type chmsocklist_lap<T>::GetFirstPtr(bool is_abs)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		//MSG_CHMPRN("PCHMSOCKLIST does not set.");
		return NULL;
	}
	if(basic_type::pAbsPtr->prev){
		ToFirst();
	}
	return (is_abs ? chmsocklist_lap<T>::GetAbsPtr() : chmsocklist_lap<T>::GetRelPtr());

}

template<typename T>
bool chmsocklist_lap<T>::ToFirst(void)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMSOCKLIST does not set.");
		return false;
	}
	for(; basic_type::pAbsPtr->prev; basic_type::pAbsPtr = CHM_ABS(basic_type::pShmBase, basic_type::pAbsPtr->prev, st_ptr_type));
	return true;
}

template<typename T>
bool chmsocklist_lap<T>::ToLast(void)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMSOCKLIST does not set.");
		return false;
	}
	for(; basic_type::pAbsPtr->next; basic_type::pAbsPtr = CHM_ABS(basic_type::pShmBase, basic_type::pAbsPtr->next, st_ptr_type));
	return true;
}

template<typename T>
bool chmsocklist_lap<T>::ToNext(void)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMSOCKLIST does not set.");
		return false;
	}
	if(!basic_type::pAbsPtr->next){
		MSG_CHMPRN("Reached end of client process id list.");
		return false;
	}
	basic_type::pAbsPtr = CHM_ABS(basic_type::pShmBase, basic_type::pAbsPtr->next, st_ptr_type);
	return true;
}

template<typename T>
long chmsocklist_lap<T>::Count(void)
{
	if(!basic_type::pShmBase){
		ERR_CHMPRN("PCHMSOCKLIST does not set.");
		return 0L;
	}
	if(!basic_type::pAbsPtr){
		//MSG_CHMPRN("PCHMSOCKLIST does not set.");
		return 0L;
	}
	if(basic_type::pAbsPtr->prev){
		ToFirst();
	}
	long		count;
	st_ptr_type pos;
	for(count = 1L, pos = basic_type::pAbsPtr; pos->next; pos = CHM_ABS(basic_type::pShmBase, pos->next, st_ptr_type), count++);

	return count;
}

//
// This method uses liner searching, so not good performance.
//
template<typename T>
bool chmsocklist_lap<T>::Insert(st_ptr_type ptr, bool is_abs)
{
	if(!ptr){
		ERR_CHMPRN("ptr is null.");
		return false;
	}
	if(!basic_type::pShmBase){					// allow basic_type::pAbsPtr is NULL
		ERR_CHMPRN("PCHMSOCKLIST does not set.");
		return false;
	}
	st_ptr_type	relptr	= is_abs ? CHM_REL(basic_type::pShmBase, ptr, st_ptr_type) : ptr;
	st_ptr_type	absptr	= is_abs ? ptr : CHM_ABS(basic_type::pShmBase, ptr, st_ptr_type);

	if(absptr == basic_type::pAbsPtr){
		ERR_CHMPRN("Already set socklist, something wrong but continue...");
		return true;
	}
	absptr->prev = NULL;
	absptr->next = NULL;

	if(!basic_type::pAbsPtr){
		// If list is empty, add ptr to first position.
		basic_type::pAbsPtr = absptr;
		return true;
	}
	if(basic_type::pAbsPtr->prev){
		ToFirst();
	}

	basic_type::pAbsPtr->prev	= relptr;
	absptr->next				= CHM_REL(basic_type::pShmBase, basic_type::pAbsPtr, st_ptr_type);
	basic_type::pAbsPtr			= absptr;

	return true;
}

//
// [CAREFUL]
// After calling this method, be careful for pAbsPtr is NULL.
// And this method is very slow, should use Retrieve(void)
//
template<typename T>
typename chmsocklist_lap<T>::st_ptr_type chmsocklist_lap<T>::Retrieve(int sock)
{
	if(CHM_INVALID_SOCK == sock){
		ERR_CHMPRN("Parameter is wrong.");
		return NULL;
	}
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMSOCKLIST does not set.");
		return NULL;
	}
	if(basic_type::pAbsPtr->prev){
		ToFirst();
	}

	for(st_ptr_type cur = basic_type::pAbsPtr; cur; cur = CHM_ABS(basic_type::pShmBase, cur->next, st_ptr_type)){
		if(cur->sock == sock){
			// found
			if(cur == basic_type::pAbsPtr){
				// basic_type::pAbsPtr is first object in list.
				if(basic_type::pAbsPtr->next){
					basic_type::pAbsPtr			= CHM_ABS(basic_type::pShmBase, basic_type::pAbsPtr->next, st_ptr_type);
					basic_type::pAbsPtr->prev	= NULL;		// for safety
				}else{
					basic_type::pAbsPtr			= NULL;
				}
			}
			st_ptr_type	prevlist = CHM_ABS(basic_type::pShmBase, cur->prev, st_ptr_type);
			st_ptr_type	nextlist = CHM_ABS(basic_type::pShmBase, cur->next, st_ptr_type);
			if(prevlist){
				prevlist->next = cur->next;
			}
			if(nextlist){
				nextlist->prev = cur->prev;
			}
			cur->prev = NULL;
			cur->next = NULL;
			return cur;
		}
	}

	MSG_CHMPRN("Could not find sock(%d).", sock);
	return NULL;
}

//
// [CAREFUL]
// After calling this method, be careful for pAbsPtr is NULL.
//
template<typename T>
typename chmsocklist_lap<T>::st_ptr_type chmsocklist_lap<T>::Retrieve(void)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMSOCKLIST does not set.");
		return NULL;
	}

	if(basic_type::pAbsPtr->prev){
		ToFirst();
	}
	st_ptr_type	abs_cur  = basic_type::pAbsPtr;
	st_ptr_type	abs_next = CHM_ABS(basic_type::pShmBase, basic_type::pAbsPtr->next, st_ptr_type);

	if(abs_next){
		abs_next->prev	= NULL;
	}
	basic_type::pAbsPtr	= abs_next;
	abs_cur->prev 		= NULL;
	abs_cur->next 		= NULL;

	return abs_cur;
}

//
// This method is not good performance.
//
template<typename T>
typename chmsocklist_lap<T>::st_ptr_type chmsocklist_lap<T>::Find(int sock, bool is_abs)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMSOCKLIST does not set.");
		return NULL;
	}
	if(basic_type::pAbsPtr->prev){
		ToFirst();
	}

	for(st_ptr_type cur = basic_type::pAbsPtr; cur; cur = CHM_ABS(basic_type::pShmBase, cur->next, st_ptr_type)){
		if(cur->sock == sock){
			return (is_abs ? cur : CHM_REL(basic_type::pShmBase, cur, st_ptr_type));
		}
	}
	return NULL;
}

template<typename T>
bool chmsocklist_lap<T>::GetAllSocks(socklist_t& list)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMSOCKLIST does not set.");
		return false;
	}
	if(basic_type::pAbsPtr->prev){
		ToFirst();
	}
	list.clear();

	for(st_ptr_type cur = basic_type::pAbsPtr; cur; cur = CHM_ABS(basic_type::pShmBase, cur->next, st_ptr_type)){
		list.push_back(cur->sock);
	}
	return true;
}

typedef	chmsocklist_lap<CHMSOCKLIST>	chmsocklistlap;

//---------------------------------------------------------
// For CHMPX
//---------------------------------------------------------
template<typename T>
class chmpx_lap : public structure_lap<T>
{
	public:
		typedef T					st_type;
		typedef T*					st_ptr_type;
		typedef structure_lap<T>	basic_type;

	protected:
		st_ptr_type*				abs_base_arr;
		st_ptr_type*				abs_pend_arr;
		long*						abs_sock_free_cnt;
		PCHMSOCKLIST*				abs_sock_frees;

	protected:
		bool Initialize(CHMPXID_SEED_TYPE type, const char* hostname, const char* group, CHMPXMODE mode, short port, short ctlport, const char* cuk, const char* custom_seed, const PCHMPXHP_RAWPAIR pendpoints, const PCHMPXHP_RAWPAIR pctlendpoints, const PCHMPXHP_RAWPAIR pforward_peers, const PCHMPXHP_RAWPAIR preverse_peers, const CHMPXSSL& ssl);

	public:
		explicit chmpx_lap(st_ptr_type ptr = NULL, st_ptr_type* pchmpxarrbase = NULL, st_ptr_type* pchmpxarrpend = NULL, long* psockfreecnt = NULL, PCHMSOCKLIST* pabssockfrees = NULL, const void* shmbase = NULL, bool is_abs = true);

		void Reset(st_ptr_type ptr, st_ptr_type* pchmpxarrbase, st_ptr_type* pchmpxarrpend, long* psockfreecnt, PCHMSOCKLIST* pabssockfrees, const void* shmbase, bool is_abs = true);
		virtual bool Initialize(void);
		virtual bool Dump(std::stringstream& sstream, const char* spacer) const;
		virtual bool Clear(void);
		bool Copy(st_ptr_type ptr);
		void Free(st_ptr_type ptr) const;

		bool Close(int eqfd = CHM_INVALID_HANDLE);
		bool InitializeServer(CHMPXID_SEED_TYPE type, const char* hostname, const char* group, short port, short ctlport, const char* cuk, const char* custom_seed, const PCHMPXHP_RAWPAIR pendpoints, const PCHMPXHP_RAWPAIR pctlendpoints, const PCHMPXHP_RAWPAIR pforward_peers, const PCHMPXHP_RAWPAIR preverse_peers, const CHMPXSSL& ssl);
		bool InitializeServer(PCHMPXSVR chmpxsvr, const char* group, CHMPXID_SEED_TYPE type);
		bool InitializeSlave(CHMPXID_SEED_TYPE type, const char* hostname, const char* group, short ctlport, const char* cuk, const char* custom_seed, const PCHMPXHP_RAWPAIR pendpoints, const PCHMPXHP_RAWPAIR pctlendpoints, const PCHMPXHP_RAWPAIR pforward_peers, const PCHMPXHP_RAWPAIR preverse_peers, const CHMPXSSL& ssl);

		bool Set(int sock, int ctlsock, int selfsock, int selfctlsock, int type);
		bool Set(chmhash_t base, chmhash_t pending, int type);
		bool SetStatus(chmpxsts_t status);
		bool UpdateLastStatusTime(void);
		bool Remove(int sock);

		bool Get(std::string& name, chmpxid_t& chmpxid, short& port, short& ctlport, std::string* pcuk = NULL, std::string* pcustom_seed = NULL, PCHMPXHP_RAWPAIR pendpoints = NULL, PCHMPXHP_RAWPAIR pctlendpoints = NULL, PCHMPXHP_RAWPAIR pforward_peers = NULL, PCHMPXHP_RAWPAIR preverse_peers = NULL) const;
		bool Get(socklist_t& socklist, int& ctlsock, int& selfsock, int& selfctlsock) const;
		int GetSock(int type);
		bool FindSock(int sock);
		bool Get(chmhash_t& base, chmhash_t& pending) const;
		bool GetChmpxSvr(PCHMPXSVR chmpxsvr) const;
		bool MergeChmpxSvr(PCHMPXSVR chmpxsvr, CHMPXID_SEED_TYPE type, bool is_force = false, int eqfd = CHM_INVALID_HANDLE);

		bool IsServerMode(void) const { return (basic_type::pAbsPtr && CHMPX_SERVER == basic_type::pAbsPtr->mode); }
		bool IsSlaveMode(void) const { return (basic_type::pAbsPtr && CHMPX_SLAVE == basic_type::pAbsPtr->mode); }
		chmpxid_t GetChmpxId(void) const { return (basic_type::pAbsPtr ? basic_type::pAbsPtr->chmpxid : CHM_INVALID_CHMPXID); }
		chmpxsts_t GetStatus(void) const { return (basic_type::pAbsPtr ? basic_type::pAbsPtr->status : CHMPXSTS_SLAVE_DOWN_NORMAL); }

		bool IsSsl(void) const { return (basic_type::pAbsPtr ? basic_type::pAbsPtr->is_ssl : false); }
		bool IsVerifyPeer(void) const { return (basic_type::pAbsPtr ? basic_type::pAbsPtr->verify_peer : false); }
		const char* GetCApath(void) const { return ((basic_type::pAbsPtr && !basic_type::pAbsPtr->is_ca_file && !CHMEMPTYSTR(basic_type::pAbsPtr->capath)) ? basic_type::pAbsPtr->capath : NULL); }
		const char* GetCAfile(void) const { return ((basic_type::pAbsPtr && basic_type::pAbsPtr->is_ca_file && !CHMEMPTYSTR(basic_type::pAbsPtr->cafile)) ? basic_type::pAbsPtr->capath : NULL); }
		const char* GetSvrCert(void) const { return ((basic_type::pAbsPtr && !CHMEMPTYSTR(basic_type::pAbsPtr->server_cert)) ? basic_type::pAbsPtr->server_cert : NULL); }
		const char* GetSvrPriKey(void) const { return ((basic_type::pAbsPtr && !CHMEMPTYSTR(basic_type::pAbsPtr->server_prikey)) ? basic_type::pAbsPtr->server_prikey : NULL); }
		const char* GetSlvCert(void) const { return ((basic_type::pAbsPtr && !CHMEMPTYSTR(basic_type::pAbsPtr->slave_cert)) ? basic_type::pAbsPtr->slave_cert : NULL); }
		const char* GetSlvPriKey(void) const { return ((basic_type::pAbsPtr && !CHMEMPTYSTR(basic_type::pAbsPtr->slave_prikey)) ? basic_type::pAbsPtr->slave_prikey : NULL); }
		bool GetSslStructure(CHMPXSSL& ssl) const;

		int compare_order(const chmpx_lap<T>& other, CHMPXID_SEED_TYPE type) const;
};

template<typename T>
chmpx_lap<T>::chmpx_lap(st_ptr_type ptr, st_ptr_type* pchmpxarrbase, st_ptr_type* pchmpxarrpend, long* psockfreecnt, PCHMSOCKLIST* pabssockfrees, const void* shmbase, bool is_abs)
{
	Reset(ptr, pchmpxarrbase, pchmpxarrpend, psockfreecnt, pabssockfrees, shmbase, is_abs);
}

template<typename T>
void chmpx_lap<T>::Reset(st_ptr_type ptr, st_ptr_type* pchmpxarrbase, st_ptr_type* pchmpxarrpend, long* psockfreecnt, PCHMSOCKLIST* psockfrees, const void* shmbase, bool is_abs)
{
	structure_lap<T>::Reset(ptr, shmbase, is_abs);
	abs_base_arr		= pchmpxarrbase;			// abs
	abs_pend_arr		= pchmpxarrpend;			// abs
	abs_sock_free_cnt	= psockfreecnt;				// abs
	abs_sock_frees		= psockfrees;				// abs
}

template<typename T>
bool chmpx_lap<T>::Initialize(void)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPX does not set.");
		return false;
	}
	basic_type::pAbsPtr->chmpxid			= CHM_INVALID_CHMPXID;
	basic_type::pAbsPtr->name[0]			= '\0';
	basic_type::pAbsPtr->mode				= CHMPX_UNKNOWN;
	basic_type::pAbsPtr->base_hash			= CHM_INVALID_HASHVAL;
	basic_type::pAbsPtr->pending_hash		= CHM_INVALID_HASHVAL;
	basic_type::pAbsPtr->port				= CHM_INVALID_PORT;
	basic_type::pAbsPtr->ctlport			= CHM_INVALID_PORT;
	basic_type::pAbsPtr->is_ssl				= false;
	basic_type::pAbsPtr->verify_peer		= false;
	basic_type::pAbsPtr->is_ca_file			= false;
	basic_type::pAbsPtr->capath[0]			= '\0';
	basic_type::pAbsPtr->server_cert[0]		= '\0';
	basic_type::pAbsPtr->server_prikey[0]	= '\0';
	basic_type::pAbsPtr->slave_cert[0]		= '\0';
	basic_type::pAbsPtr->slave_prikey[0]	= '\0';
	basic_type::pAbsPtr->socklist			= NULL;
	basic_type::pAbsPtr->ctlsock			= CHM_INVALID_SOCK;
	basic_type::pAbsPtr->selfsock			= CHM_INVALID_SOCK;
	basic_type::pAbsPtr->selfctlsock		= CHM_INVALID_SOCK;
	basic_type::pAbsPtr->last_status_time	= 0;
	basic_type::pAbsPtr->status				= CHMPXSTS_SLAVE_DOWN_NORMAL;		// temporary
	basic_type::pAbsPtr->cuk[0]				= '\0';
	basic_type::pAbsPtr->custom_seed[0]		= '\0';

	int	cnt;
	for(cnt = 0; cnt < EXTERNAL_EP_MAX; ++cnt){
		basic_type::pAbsPtr->endpoints[cnt].name[0]		= '\0';
		basic_type::pAbsPtr->endpoints[cnt].port		= CHM_INVALID_PORT;
	}
	for(cnt = 0; cnt < EXTERNAL_EP_MAX; ++cnt){
		basic_type::pAbsPtr->ctlendpoints[cnt].name[0]	= '\0';
		basic_type::pAbsPtr->ctlendpoints[cnt].port		= CHM_INVALID_PORT;
	}
	for(cnt = 0; cnt < FORWARD_PEER_MAX; ++cnt){
		basic_type::pAbsPtr->forward_peers[cnt].name[0]	= '\0';
		basic_type::pAbsPtr->forward_peers[cnt].port	= CHM_INVALID_PORT;
	}
	for(cnt = 0; cnt < REVERSE_PEER_MAX; ++cnt){
		basic_type::pAbsPtr->reverse_peers[cnt].name[0]	= '\0';
		basic_type::pAbsPtr->reverse_peers[cnt].port	= CHM_INVALID_PORT;
	}

	return true;
}

template<typename T>
bool chmpx_lap<T>::Dump(std::stringstream& sstream, const char* spacer) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPX does not set.");
		return false;
	}
	sstream << (spacer ? spacer : "") << "chmpxid          = "	<< reinterpret_cast<void*>(basic_type::pAbsPtr->chmpxid)		<< std::endl;
	sstream << (spacer ? spacer : "") << "name             = "	<< basic_type::pAbsPtr->name									<< std::endl;
	sstream << (spacer ? spacer : "") << "mode             = "	<< STRCHMPXMODE(basic_type::pAbsPtr->mode)						<< std::endl;
	sstream << (spacer ? spacer : "") << "base_hash        = "	<< reinterpret_cast<void*>(basic_type::pAbsPtr->base_hash)		<< std::endl;
	sstream << (spacer ? spacer : "") << "pending_hash     = "	<< reinterpret_cast<void*>(basic_type::pAbsPtr->pending_hash)	<< std::endl;
	sstream << (spacer ? spacer : "") << "port             = "	<< basic_type::pAbsPtr->port									<< std::endl;
	sstream << (spacer ? spacer : "") << "ctlport          = "	<< basic_type::pAbsPtr->ctlport									<< std::endl;
	sstream << (spacer ? spacer : "") << "cuk              = "	<< basic_type::pAbsPtr->cuk										<< std::endl;
	sstream << (spacer ? spacer : "") << "custom_seed      = "	<< basic_type::pAbsPtr->custom_seed								<< std::endl;
	sstream << (spacer ? spacer : "") << "endpoints        = "	<< get_hostport_pairs_string(basic_type::pAbsPtr->endpoints,	EXTERNAL_EP_MAX)	<< std::endl;
	sstream << (spacer ? spacer : "") << "ctlendpoints     = "	<< get_hostport_pairs_string(basic_type::pAbsPtr->ctlendpoints,	EXTERNAL_EP_MAX)	<< std::endl;
	sstream << (spacer ? spacer : "") << "forward_peers    = "	<< get_hostport_pairs_string(basic_type::pAbsPtr->forward_peers,FORWARD_PEER_MAX)	<< std::endl;
	sstream << (spacer ? spacer : "") << "reverse_peers    = "	<< get_hostport_pairs_string(basic_type::pAbsPtr->reverse_peers,REVERSE_PEER_MAX)	<< std::endl;
	sstream << (spacer ? spacer : "") << "is_ssl           = "	<< (basic_type::pAbsPtr->is_ssl ? "true" : "false")				<< std::endl;
	sstream << (spacer ? spacer : "") << "verify_peer      = "	<< (basic_type::pAbsPtr->verify_peer ? "true" : "false")		<< std::endl;
	sstream << (spacer ? spacer : "") << "is_ca_file       = "	<< (basic_type::pAbsPtr->is_ca_file ? "true" : "false")			<< std::endl;
	sstream << (spacer ? spacer : "") << "capath           = "	<< basic_type::pAbsPtr->capath									<< std::endl;
	sstream << (spacer ? spacer : "") << "server_cert      = "	<< basic_type::pAbsPtr->server_cert								<< std::endl;
	sstream << (spacer ? spacer : "") << "server_prikey    = "	<< basic_type::pAbsPtr->server_prikey							<< std::endl;
	sstream << (spacer ? spacer : "") << "slave_cert       = "	<< basic_type::pAbsPtr->slave_cert								<< std::endl;
	sstream << (spacer ? spacer : "") << "slave_prikey     = "	<< basic_type::pAbsPtr->slave_prikey							<< std::endl;

	if(basic_type::pAbsPtr->socklist){
		chmsocklistlap	socklist(basic_type::pAbsPtr->socklist, basic_type::pShmBase, false);	// From Relative
		socklist.Dump(sstream, spacer);
	}else{
		sstream << (spacer ? spacer : "") << "sock             = "	<< static_cast<int>(CHM_INVALID_SOCK)						<< std::endl;
	}

	sstream << (spacer ? spacer : "") << "ctlsock          = "	<< basic_type::pAbsPtr->ctlsock									<< std::endl;
	sstream << (spacer ? spacer : "") << "selfsock         = "	<< basic_type::pAbsPtr->selfsock								<< std::endl;
	sstream << (spacer ? spacer : "") << "selfctlsock      = "	<< basic_type::pAbsPtr->selfctlsock								<< std::endl;
	sstream << (spacer ? spacer : "") << "last_status_time = "	<< basic_type::pAbsPtr->last_status_time						<< std::endl;
	sstream << (spacer ? spacer : "") << "status           = "	<< STR_CHMPXSTS_FULL(basic_type::pAbsPtr->status)				<< std::endl;

	return true;
}

template<typename T>
bool chmpx_lap<T>::Copy(st_ptr_type ptr)
{
	if(!ptr){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPX does not set.");
		return false;
	}
	ptr->chmpxid				= basic_type::pAbsPtr->chmpxid;
	ptr->mode					= basic_type::pAbsPtr->mode;
	ptr->base_hash				= basic_type::pAbsPtr->base_hash;
	ptr->pending_hash			= basic_type::pAbsPtr->pending_hash;
	ptr->port					= basic_type::pAbsPtr->port;
	ptr->ctlport				= basic_type::pAbsPtr->ctlport;
	ptr->is_ssl					= basic_type::pAbsPtr->is_ssl;
	ptr->verify_peer			= basic_type::pAbsPtr->verify_peer;
	ptr->is_ca_file				= basic_type::pAbsPtr->is_ca_file;
	ptr->ctlsock				= basic_type::pAbsPtr->ctlsock;
	ptr->selfsock				= basic_type::pAbsPtr->selfsock;
	ptr->selfctlsock			= basic_type::pAbsPtr->selfctlsock;
	ptr->last_status_time		= basic_type::pAbsPtr->last_status_time;
	ptr->status					= basic_type::pAbsPtr->status;

	strcpy(ptr->name,			basic_type::pAbsPtr->name);
	strcpy(ptr->cuk,			basic_type::pAbsPtr->cuk);
	strcpy(ptr->custom_seed,	basic_type::pAbsPtr->custom_seed);
	strcpy(ptr->capath,			basic_type::pAbsPtr->capath);
	strcpy(ptr->server_cert,	basic_type::pAbsPtr->server_cert);
	strcpy(ptr->server_prikey,	basic_type::pAbsPtr->server_prikey);
	strcpy(ptr->slave_cert,		basic_type::pAbsPtr->slave_cert);
	strcpy(ptr->slave_prikey,	basic_type::pAbsPtr->slave_prikey);

	copy_hostport_pairs(ptr->endpoints,		basic_type::pAbsPtr->endpoints,		EXTERNAL_EP_MAX);
	copy_hostport_pairs(ptr->ctlendpoints,	basic_type::pAbsPtr->ctlendpoints,	EXTERNAL_EP_MAX);
	copy_hostport_pairs(ptr->forward_peers,	basic_type::pAbsPtr->forward_peers,	FORWARD_PEER_MAX);
	copy_hostport_pairs(ptr->reverse_peers,	basic_type::pAbsPtr->reverse_peers,	REVERSE_PEER_MAX);

	if(basic_type::pAbsPtr->socklist){
		chmsocklistlap			socklist(basic_type::pAbsPtr->socklist, basic_type::pShmBase, false);	// From Relative
		ptr->socklist			= socklist.Dup();
	}else{
		ptr->socklist			= NULL;
	}
	return true;
}

template<typename T>
void chmpx_lap<T>::Free(st_ptr_type ptr) const
{
	if(ptr){
		K2H_Free(ptr->socklist);
	}
}

template<typename T>
bool chmpx_lap<T>::Initialize(CHMPXID_SEED_TYPE type, const char* hostname, const char* group, CHMPXMODE mode, short port, short ctlport, const char* cuk, const char* custom_seed, const PCHMPXHP_RAWPAIR pendpoints, const PCHMPXHP_RAWPAIR pctlendpoints, const PCHMPXHP_RAWPAIR pforward_peers, const PCHMPXHP_RAWPAIR preverse_peers, const CHMPXSSL& ssl)
{
	if(CHMEMPTYSTR(hostname) || NI_MAXHOST <= strlen(hostname) || CHMEMPTYSTR(group)){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPX does not set.");
		return false;
	}

	// [NOTE]
	// Hostname may be set to something other than an IP address string or FQDN.
	// Therefore, we need to convert it to FQDN as much as possible here.
	//
	strlst_t	expandlist;
	if(!ChmNetDb::Get()->GetHostnameList(hostname, expandlist, true) || expandlist.empty()){
		MSG_CHMPRN("Not found any hostname for %s, then try to get a normalized IP addresses.", hostname);

		if(!ChmNetDb::Get()->GetIpAddressStringList(hostname, expandlist, true) || expandlist.empty()){
			MSG_CHMPRN("Not found any hostname for %s, then try to get another hostanme allowed localhost.", hostname);

			if(!ChmNetDb::Get()->GetAllHostList(hostname, expandlist, false) || expandlist.empty()){	// last, allow localhost
				MSG_CHMPRN("Not found any IP address for %s, then set this value as is.", hostname);
			}else{
				MSG_CHMPRN("convert host(%s) to host(%s) and register as Slave Node.", hostname, expandlist.front().c_str());
				hostname = expandlist.front().c_str();
			}
		}else{
			MSG_CHMPRN("convert host(%s) to IP address(%s) and register as Slave Node.", hostname, expandlist.front().c_str());
			hostname = expandlist.front().c_str();
		}
	}else{
		MSG_CHMPRN("convert host(%s) to hostname(%s) and register as Slave Node.", hostname, expandlist.front().c_str());
		hostname = expandlist.front().c_str();
	}
	if(CHMEMPTYSTR(hostname)){
		ERR_CHMPRN("hostname after expanding is empty.");
		return false;
	}

	strcpy(basic_type::pAbsPtr->name,			hostname);
	strcpy(basic_type::pAbsPtr->cuk,			CHMEMPTYSTR(cuk) ? "" : cuk);
	strcpy(basic_type::pAbsPtr->custom_seed,	CHMEMPTYSTR(custom_seed) ? "" : custom_seed);
	strcpy(basic_type::pAbsPtr->capath,			ssl.capath);
	strcpy(basic_type::pAbsPtr->server_cert,	ssl.server_cert);
	strcpy(basic_type::pAbsPtr->server_prikey,	ssl.server_prikey);
	strcpy(basic_type::pAbsPtr->slave_cert,		ssl.slave_cert);
	strcpy(basic_type::pAbsPtr->slave_prikey,	ssl.slave_prikey);

	basic_type::pAbsPtr->chmpxid				= MakeChmpxId(group, type, hostname, ctlport, cuk, get_hostport_pairs_string(pctlendpoints, EXTERNAL_EP_MAX).c_str(), custom_seed);
	basic_type::pAbsPtr->base_hash				= CHM_INVALID_HASHVAL;
	basic_type::pAbsPtr->pending_hash			= CHM_INVALID_HASHVAL;
	basic_type::pAbsPtr->mode					= mode;
	basic_type::pAbsPtr->port					= port;
	basic_type::pAbsPtr->ctlport				= ctlport;
	basic_type::pAbsPtr->is_ssl					= ssl.is_ssl;
	basic_type::pAbsPtr->verify_peer			= ssl.verify_peer;
	basic_type::pAbsPtr->is_ca_file				= ssl.is_ca_file;
	basic_type::pAbsPtr->socklist				= NULL;
	basic_type::pAbsPtr->ctlsock				= CHM_INVALID_SOCK;
	basic_type::pAbsPtr->selfsock				= CHM_INVALID_SOCK;
	basic_type::pAbsPtr->selfctlsock			= CHM_INVALID_SOCK;
	basic_type::pAbsPtr->last_status_time		= 0;
	basic_type::pAbsPtr->status					= CHMPX_SERVER == mode ? CHMPXSTS_SRVOUT_DOWN_NORMAL : CHMPXSTS_SLAVE_DOWN_NORMAL;

	if(pendpoints){
		copy_hostport_pairs(basic_type::pAbsPtr->endpoints,		pendpoints,		EXTERNAL_EP_MAX);
	}
	if(pctlendpoints){
		copy_hostport_pairs(basic_type::pAbsPtr->ctlendpoints,	pctlendpoints,	EXTERNAL_EP_MAX);
	}
	if(pforward_peers){
		copy_hostport_pairs(basic_type::pAbsPtr->forward_peers,	pforward_peers,	FORWARD_PEER_MAX);
	}
	if(preverse_peers){
		copy_hostport_pairs(basic_type::pAbsPtr->reverse_peers,	preverse_peers,	REVERSE_PEER_MAX);
	}

	return true;
}

template<typename T>
bool chmpx_lap<T>::Clear(void)
{
	return Initialize();
}

template<typename T>
bool chmpx_lap<T>::Close(int eqfd)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPX does not set.");
		return false;
	}

	// sock is list, so loop
	if(basic_type::pAbsPtr->socklist){
		chmsocklistlap	socklist(basic_type::pAbsPtr->socklist, basic_type::pShmBase, false);	// From Relative
		chmsocklistlap	freesocklist(*abs_sock_frees, basic_type::pShmBase, false);				// From Relative

		for(PCHMSOCKLIST psocklist = socklist.Retrieve(); psocklist; psocklist = (socklist.GetFirstPtr(false) ? socklist.Retrieve() : NULL)){
			if(CHM_INVALID_SOCK != psocklist->sock){
				if(CHM_INVALID_HANDLE != eqfd){
					epoll_ctl(eqfd, EPOLL_CTL_DEL, psocklist->sock, NULL);
				}
				CHM_CLOSESOCK(psocklist->sock);
			}
			if(freesocklist.Insert(psocklist, true)){											// Set abs
				++(*abs_sock_free_cnt);
			}
		}
		// Set free list pointer
		*abs_sock_frees = freesocklist.GetFirstPtr(false);
		// clear
		basic_type::pAbsPtr->socklist = NULL;
	}

	if(CHM_INVALID_SOCK != basic_type::pAbsPtr->ctlport && CHM_INVALID_HANDLE != eqfd){
		epoll_ctl(eqfd, EPOLL_CTL_DEL, basic_type::pAbsPtr->ctlport, NULL);
	}
	if(CHM_INVALID_SOCK != basic_type::pAbsPtr->selfsock && CHM_INVALID_HANDLE != eqfd){
		epoll_ctl(eqfd, EPOLL_CTL_DEL, basic_type::pAbsPtr->selfsock, NULL);
	}
	if(CHM_INVALID_SOCK != basic_type::pAbsPtr->selfctlsock && CHM_INVALID_HANDLE != eqfd){
		epoll_ctl(eqfd, EPOLL_CTL_DEL, basic_type::pAbsPtr->selfctlsock, NULL);
	}
	CHM_CLOSESOCK(basic_type::pAbsPtr->ctlsock);
	CHM_CLOSESOCK(basic_type::pAbsPtr->selfsock);
	CHM_CLOSESOCK(basic_type::pAbsPtr->selfctlsock);

	return true;
}

template<typename T>
bool chmpx_lap<T>::InitializeServer(CHMPXID_SEED_TYPE type, const char* hostname, const char* group, short port, short ctlport, const char* cuk, const char* custom_seed, const PCHMPXHP_RAWPAIR pendpoints, const PCHMPXHP_RAWPAIR pctlendpoints, const PCHMPXHP_RAWPAIR pforward_peers, const PCHMPXHP_RAWPAIR preverse_peers, const CHMPXSSL& ssl)
{
	return chmpx_lap<T>::Initialize(type, hostname, group, CHMPX_SERVER, port, ctlport, cuk, custom_seed, pendpoints, pctlendpoints, pforward_peers, preverse_peers, ssl);
}

template<typename T>
bool chmpx_lap<T>::InitializeServer(PCHMPXSVR chmpxsvr, const char* group, CHMPXID_SEED_TYPE type)
{
	if(!chmpxsvr || CHMEMPTYSTR(group)){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}

	if(!chmpx_lap<T>::InitializeServer(type, chmpxsvr->name, group, chmpxsvr->port, chmpxsvr->ctlport, chmpxsvr->cuk, chmpxsvr->custom_seed, chmpxsvr->endpoints, chmpxsvr->ctlendpoints, chmpxsvr->forward_peers, chmpxsvr->reverse_peers, chmpxsvr->ssl)){
		return false;
	}
	if(chmpxsvr->chmpxid != basic_type::pAbsPtr->chmpxid){
		ERR_CHMPRN("Why does not same chmpxid(0x%016" PRIx64 " - 0x%016" PRIx64 ").", chmpxsvr->chmpxid, basic_type::pAbsPtr->chmpxid);
		return false;
	}
	MSG_CHMPRN("chmpxid(0x%016" PRIx64 ") status is initialized (0x%016" PRIx64 ":%s) with last update time(%zu)", basic_type::pAbsPtr->chmpxid, chmpxsvr->status, STR_CHMPXSTS_FULL(chmpxsvr->status).c_str(), chmpxsvr->last_status_time);

	basic_type::pAbsPtr->base_hash			= chmpxsvr->base_hash;
	basic_type::pAbsPtr->pending_hash		= chmpxsvr->pending_hash;
	basic_type::pAbsPtr->last_status_time	= chmpxsvr->last_status_time;
	basic_type::pAbsPtr->status				= chmpxsvr->status;

	// [NOTICE]
	// If base(pending) hash value is valid, set always pointer to chmpxlist array.
	// But not clear old pointer when old hash values is valid.
	// So it means always over writing array but not clear old.(take care for read
	// array value.)
	//
	if(CHM_INVALID_HASHVAL != basic_type::pAbsPtr->base_hash){
		if(!abs_base_arr){
			ERR_CHMPRN("abs_base_arr is NULL, could not update base hash array.");
		}else{
			abs_base_arr[basic_type::pAbsPtr->base_hash] = chmpx_lap<T>::GetRelPtr();
		}
	}
	if(CHM_INVALID_HASHVAL != basic_type::pAbsPtr->pending_hash){
		if(!abs_pend_arr){
			ERR_CHMPRN("abs_pend_arr is NULL, could not update pending hash array.");
		}else{
			abs_pend_arr[basic_type::pAbsPtr->pending_hash] = chmpx_lap<T>::GetRelPtr();
		}
	}
	return true;
}

template<typename T>
bool chmpx_lap<T>::InitializeSlave(CHMPXID_SEED_TYPE type, const char* hostname, const char* group, short ctlport, const char* cuk, const char* custom_seed, const PCHMPXHP_RAWPAIR pendpoints, const PCHMPXHP_RAWPAIR pctlendpoints, const PCHMPXHP_RAWPAIR pforward_peers, const PCHMPXHP_RAWPAIR preverse_peers, const CHMPXSSL& ssl)
{
	return chmpx_lap<T>::Initialize(type, hostname, group, CHMPX_SLAVE, CHM_INVALID_PORT, ctlport, cuk, custom_seed, pendpoints, pctlendpoints, pforward_peers, preverse_peers, ssl);
}

template<typename T>
bool chmpx_lap<T>::Set(int sock, int ctlsock, int selfsock, int selfctlsock, int type)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPX does not set.");
		return false;
	}
	if('\0' == basic_type::pAbsPtr->name[0]){
		ERR_CHMPRN("PCHMPX does not initialized.");
		return false;
	}

	if(IS_SOCKTG_SOCK(type)){
		if(CHM_INVALID_SOCK == sock){
			// socket is invalid, so do nothing
			MSG_CHMPRN("sock is %d, thus do not set(should call remove sock).", CHM_INVALID_SOCK);
		}else{
			// add socket to socklist
			if(0L >= (*abs_sock_free_cnt)){
				ERR_CHMPRN("Could not get free sock list.");
				return false;
			}
			// get one sock list
			chmsocklistlap	freesocklist(*abs_sock_frees, basic_type::pShmBase, false);				// From Relative
			PCHMSOCKLIST	pnewsocklist;
			if(NULL == (pnewsocklist = freesocklist.Retrieve())){
				ERR_CHMPRN("Could not get free sock list.");
				return false;
			}
			--(*abs_sock_free_cnt);
			*abs_sock_frees = freesocklist.GetFirstPtr(false);

			// set(next/prev members are set null in Retrieve method)
			pnewsocklist->sock = sock;

			// set socklist
			chmsocklistlap	socklist(basic_type::pAbsPtr->socklist, basic_type::pShmBase, false);	// From Relative
			if(!socklist.Insert(pnewsocklist, true)){												// Set abs
				ERR_CHMPRN("Could not set sock to sock list.");
				// recovering
				pnewsocklist->sock = CHM_INVALID_SOCK;
				if(freesocklist.Insert(pnewsocklist, true)){
					++(*abs_sock_free_cnt);
					*abs_sock_frees = freesocklist.GetFirstPtr(false);
				}
				return false;
			}
			basic_type::pAbsPtr->socklist = socklist.GetFirstPtr(false);
		}
	}
	if(IS_SOCKTG_CTLSOCK(type)){
		basic_type::pAbsPtr->ctlsock = ctlsock;
	}
	if(IS_SOCKTG_SELFSOCK(type)){
		basic_type::pAbsPtr->selfsock = selfsock;
	}
	if(IS_SOCKTG_SELFCTLSOCK(type)){
		basic_type::pAbsPtr->selfctlsock = selfctlsock;
	}
	return true;
}

template<typename T>
bool chmpx_lap<T>::Set(chmhash_t base, chmhash_t pending, int type)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPX does not set.");
		return false;
	}
	if('\0' == basic_type::pAbsPtr->name[0]){
		ERR_CHMPRN("PCHMPX does not initialized.");
		return false;
	}

	// [NOTICE]
	// If base(pending) hash value is valid, set always pointer to chmpxlist array.
	// But not clear old pointer when old hash values is valid.
	// So it means always over writing array but not clear old.(take care for read
	// array value.)
	//
	bool	is_set = false;
	if(IS_HASHTG_BASE(type)){
		is_set							= true;
		basic_type::pAbsPtr->base_hash	= base;
		if(CHM_INVALID_HASHVAL != basic_type::pAbsPtr->base_hash){
			if(!abs_base_arr){
				ERR_CHMPRN("abs_base_arr is NULL, could not update base hash array.");
			}else{
				abs_base_arr[basic_type::pAbsPtr->base_hash] = chmpx_lap<T>::GetRelPtr();
			}
		}
	}
	if(IS_HASHTG_PENDING(type)){
		is_set								= true;
		basic_type::pAbsPtr->pending_hash	= pending;
		if(CHM_INVALID_HASHVAL != basic_type::pAbsPtr->pending_hash){
			if(!abs_pend_arr){
				ERR_CHMPRN("abs_pend_arr is NULL, could not update pending hash array.");
			}else{
				abs_pend_arr[basic_type::pAbsPtr->pending_hash] = chmpx_lap<T>::GetRelPtr();
			}
		}
	}
	if(is_set){
		basic_type::pAbsPtr->last_status_time = GetStatusLastUpdateTime();	// Only update status time
	}
	return is_set;
}

template<typename T>
bool chmpx_lap<T>::Remove(int sock)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPX does not set.");
		return false;
	}
	if('\0' == basic_type::pAbsPtr->name[0]){
		ERR_CHMPRN("PCHMPX does not initialized.");
		return false;
	}
	if(CHM_INVALID_SOCK == sock){
		ERR_CHMPRN("sock is invalid.");
		return false;
	}

	if(!basic_type::pAbsPtr->socklist){
		//MSG_CHMPRN("socklist is NULL.");
		return true;
	}

	// retrieve
	chmsocklistlap	socklist(basic_type::pAbsPtr->socklist, basic_type::pShmBase, false);	// From Relative
	PCHMSOCKLIST	prmsocklist;
	if(NULL == (prmsocklist = socklist.Retrieve(sock))){
		WAN_CHMPRN("Could not find sock(%d) in socklist, already remove it.", sock);
		return true;
	}
	basic_type::pAbsPtr->socklist = socklist.GetFirstPtr(false);

	// set(next/prev members are set null in Retrieve method)
	prmsocklist->sock = CHM_INVALID_SOCK;

	// add free
	chmsocklistlap	freesocklist(*abs_sock_frees, basic_type::pShmBase, false);				// From Relative
	if(!freesocklist.Insert(prmsocklist, true)){
		WAN_CHMPRN("Could not insert free socklist, but continue...");
	}else{
		++(*abs_sock_free_cnt);
		*abs_sock_frees = freesocklist.GetFirstPtr(false);
	}
	return true;
}

template<typename T>
bool chmpx_lap<T>::SetStatus(chmpxsts_t status)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPX does not set.");
		return false;
	}
	if('\0' == basic_type::pAbsPtr->name[0]){
		ERR_CHMPRN("PCHMPX does not initialized.");
		return false;
	}
	if(!IS_SAFE_CHMPXSTS(status)){
		ERR_CHMPRN("Parameter status(0x%016" PRIx64 ":%s) iw not safe value.", status, STR_CHMPXSTS_FULL(status).c_str());
		return false;
	}

	basic_type::pAbsPtr->status				= status;
	basic_type::pAbsPtr->last_status_time	= GetStatusLastUpdateTime();	// Only update status time
	return true;
}

template<typename T>
bool chmpx_lap<T>::UpdateLastStatusTime(void)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPX does not set.");
		return false;
	}
	if('\0' == basic_type::pAbsPtr->name[0]){
		ERR_CHMPRN("PCHMPX does not initialized.");
		return false;
	}
	basic_type::pAbsPtr->last_status_time	= GetStatusLastUpdateTime();	// Only update status time
	return true;
}

template<typename T>
bool chmpx_lap<T>::Get(std::string& name, chmpxid_t& chmpxid, short& port, short& ctlport, std::string* pcuk, std::string* pcustom_seed, PCHMPXHP_RAWPAIR pendpoints, PCHMPXHP_RAWPAIR pctlendpoints, PCHMPXHP_RAWPAIR pforward_peers, PCHMPXHP_RAWPAIR preverse_peers) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPX does not set.");
		return false;
	}
	if('\0' == basic_type::pAbsPtr->name[0]){
		ERR_CHMPRN("PCHMPX does not initialized.");
		return false;
	}
	name	= basic_type::pAbsPtr->name;
	chmpxid	= basic_type::pAbsPtr->chmpxid;
	port	= basic_type::pAbsPtr->port;
	ctlport	= basic_type::pAbsPtr->ctlport;

	// following are optional
	if(pcuk){
		(*pcuk)			= basic_type::pAbsPtr->cuk;
	}
	if(pcustom_seed){
		(*pcustom_seed)	= basic_type::pAbsPtr->custom_seed;
	}
	if(pendpoints){
		copy_hostport_pairs(pendpoints,		basic_type::pAbsPtr->endpoints,		EXTERNAL_EP_MAX);
	}
	if(pctlendpoints){
		copy_hostport_pairs(pctlendpoints,	basic_type::pAbsPtr->ctlendpoints,	EXTERNAL_EP_MAX);
	}
	if(pforward_peers){
		copy_hostport_pairs(pforward_peers,	basic_type::pAbsPtr->forward_peers,	FORWARD_PEER_MAX);
	}
	if(preverse_peers){
		copy_hostport_pairs(preverse_peers,	basic_type::pAbsPtr->reverse_peers,	REVERSE_PEER_MAX);
	}
	return true;
}

template<typename T>
bool chmpx_lap<T>::Get(socklist_t& socklist, int& ctlsock, int& selfsock, int& selfctlsock) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPX does not set.");
		return false;
	}
	if('\0' == basic_type::pAbsPtr->name[0]){
		ERR_CHMPRN("PCHMPX does not initialized.");
		return false;
	}

	socklist.clear();
	if(basic_type::pAbsPtr->socklist){
		chmsocklistlap	tmpsocklist(basic_type::pAbsPtr->socklist, basic_type::pShmBase, false);	// From Relative
		if(!tmpsocklist.GetAllSocks(socklist)){
			ERR_CHMPRN("Failed to list sockets from socklist.");
			return false;
		}
	}
	ctlsock		= basic_type::pAbsPtr->ctlsock;
	selfsock	= basic_type::pAbsPtr->selfsock;
	selfctlsock	= basic_type::pAbsPtr->selfctlsock;

	return true;
}

// [NOTE]
// If type is SOCKTG_SOCK, return a sock which is listed at first position in socklist.
//
template<typename T>
int chmpx_lap<T>::GetSock(int type)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPX does not set.");
		return CHM_INVALID_SOCK;
	}
	if('\0' == basic_type::pAbsPtr->name[0]){
		ERR_CHMPRN("PCHMPX does not initialized.");
		return CHM_INVALID_SOCK;
	}
	int	rtsock = CHM_INVALID_SOCK;
	if(SOCKTG_SOCK == type){
		if(basic_type::pAbsPtr->socklist){
			chmsocklistlap	tmpsocklist(basic_type::pAbsPtr->socklist, basic_type::pShmBase, false);	// From Relative
			socklist_t		sockarr;
			if(tmpsocklist.GetAllSocks(sockarr) && !sockarr.empty()){
				// return first sock.
				rtsock = sockarr.front();
			}
		}
	}
	if(SOCKTG_CTLSOCK == type){
		rtsock = basic_type::pAbsPtr->ctlsock;
	}
	if(SOCKTG_SELFSOCK == type){
		rtsock = basic_type::pAbsPtr->selfsock;
	}
	if(SOCKTG_SELFCTLSOCK == type){
		rtsock = basic_type::pAbsPtr->selfctlsock;
	}
	return rtsock;
}

template<typename T>
bool chmpx_lap<T>::FindSock(int sock)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPX does not set.");
		return false;
	}
	if('\0' == basic_type::pAbsPtr->name[0]){
		ERR_CHMPRN("PCHMPX does not initialized.");
		return false;
	}
	if(basic_type::pAbsPtr->socklist){
		chmsocklistlap	tmpsocklist(basic_type::pAbsPtr->socklist, basic_type::pShmBase, false);	// From Relative
		if(NULL != tmpsocklist.Find(sock, true)){
			return true;
		}
	}
	return false;
}

template<typename T>
bool chmpx_lap<T>::Get(chmhash_t& base, chmhash_t& pending) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPX does not set.");
		return false;
	}
	if('\0' == basic_type::pAbsPtr->name[0]){
		ERR_CHMPRN("PCHMPX does not initialized.");
		return false;
	}
	base	= basic_type::pAbsPtr->base_hash;
	pending	= basic_type::pAbsPtr->pending_hash;
	return true;
}

template<typename T>
bool chmpx_lap<T>::GetChmpxSvr(PCHMPXSVR chmpxsvr) const
{
	if(!chmpxsvr){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPX does not set.");
		return false;
	}

	strcpy(chmpxsvr->name,				basic_type::pAbsPtr->name);
	strcpy(chmpxsvr->cuk,				basic_type::pAbsPtr->cuk);
	strcpy(chmpxsvr->custom_seed,		basic_type::pAbsPtr->custom_seed);
	strcpy(chmpxsvr->ssl.capath,		basic_type::pAbsPtr->capath);
	strcpy(chmpxsvr->ssl.server_cert,	basic_type::pAbsPtr->server_cert);
	strcpy(chmpxsvr->ssl.server_prikey,	basic_type::pAbsPtr->server_prikey);
	strcpy(chmpxsvr->ssl.slave_cert,	basic_type::pAbsPtr->slave_cert);
	strcpy(chmpxsvr->ssl.slave_prikey,	basic_type::pAbsPtr->slave_prikey);

	chmpxsvr->chmpxid					= basic_type::pAbsPtr->chmpxid;
	chmpxsvr->base_hash					= basic_type::pAbsPtr->base_hash;
	chmpxsvr->pending_hash				= basic_type::pAbsPtr->pending_hash;
	chmpxsvr->port						= basic_type::pAbsPtr->port;
	chmpxsvr->ctlport					= basic_type::pAbsPtr->ctlport;
	chmpxsvr->ssl.is_ssl				= basic_type::pAbsPtr->is_ssl;
	chmpxsvr->ssl.verify_peer			= basic_type::pAbsPtr->verify_peer;
	chmpxsvr->ssl.is_ca_file			= basic_type::pAbsPtr->is_ca_file;
	chmpxsvr->last_status_time			= basic_type::pAbsPtr->last_status_time;
	chmpxsvr->status					= basic_type::pAbsPtr->status;

	copy_hostport_pairs(chmpxsvr->endpoints,		basic_type::pAbsPtr->endpoints,		EXTERNAL_EP_MAX);
	copy_hostport_pairs(chmpxsvr->ctlendpoints,		basic_type::pAbsPtr->ctlendpoints,	EXTERNAL_EP_MAX);
	copy_hostport_pairs(chmpxsvr->forward_peers,	basic_type::pAbsPtr->forward_peers,	FORWARD_PEER_MAX);
	copy_hostport_pairs(chmpxsvr->reverse_peers,	basic_type::pAbsPtr->reverse_peers,	REVERSE_PEER_MAX);

	return true;
}

// [NOTICE]
// About merge CHMPX from CHMPXSVR
// If member value is not same, each member is merged by following rule.
// 
// name				If this value is not same, it means another chmpx data.
//					So this function is failed.
// chmpxid			There is no reason about these values are different.
//					So this function is failed.
// base_hash		Over write CHMPXSVR's value.
// pending_hash		Over write CHMPXSVR's value.
// port				If sock member is an effective value, this value is not
//					over wrote. The other case, the value is over wrote.(*1)
// ctlport			If ctlsock member is an effective value, this value is not
//					over wrote. The other case, the value is over wrote.(*1)
// cuk				always overwrite. if chmpxid_type is CHMPXID_SEED_CUK,
//					chmpxid will change.(*2)
// custom_seed		always overwrite. if chmpxid_type is CHMPXID_SEED_CUSTOM,
//					chmpxid will change.(*2)
// endpoints		like port.
// ctlendpoints		like ctlport. if chmpxid_type is CHMPXID_SEED_CTLENDPOINTS,
//					chmpxid will change.(*2)
// forward_peers	always overwrite
// reverse_peers	always overwrite
// about ssl 		If sock and ctlsock members are an effective value, these
//					value are not over wrote. The other case, the values are
//					over wrote.(*1)
// status			If CHMPXSVR last_status_time member is latest, this value
//					is over wrote.
// last_status_time	If CHMPXSVR last_status_time member is latest, this value
//					is over wrote.
// (*1)
// Should not be this case by upper layer, the upper layer should close sockets
// before calling this function.
// (*2)
// In these case, the caller should make adjustments beforehand.
//
template<typename T>
bool chmpx_lap<T>::MergeChmpxSvr(PCHMPXSVR chmpxsvr, CHMPXID_SEED_TYPE type, bool is_force, int eqfd)
{
	if(!chmpxsvr){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPX does not set.");
		return false;
	}

	// for name checking
	std::string	foundname;
	strlst_t	node_list;
	node_list.push_back(chmpxsvr->name);

	if(is_force){
		if(basic_type::pAbsPtr->chmpxid != chmpxsvr->chmpxid || !IsInHostnameList(basic_type::pAbsPtr->name, node_list, foundname, true)){
			strcpy(basic_type::pAbsPtr->name, chmpxsvr->name);
			basic_type::pAbsPtr->chmpxid = chmpxsvr->chmpxid;
		}
		if(basic_type::pAbsPtr->port != chmpxsvr->port || !compare_hostport_pairs(basic_type::pAbsPtr->endpoints, chmpxsvr->endpoints, EXTERNAL_EP_MAX)){
			if(basic_type::pAbsPtr->socklist){
				chmsocklistlap	socklist(basic_type::pAbsPtr->socklist, basic_type::pShmBase, false);	// From Relative
				chmsocklistlap	freesocklist(*abs_sock_frees, basic_type::pShmBase, false);				// From Relative

				for(PCHMSOCKLIST psocklist = socklist.Retrieve(); psocklist; psocklist = (socklist.GetFirstPtr(false) ? socklist.Retrieve() : NULL)){
					if(CHM_INVALID_SOCK != psocklist->sock){
						WAN_CHMPRN("port(%d) is opened(sock:%d), so it is closed.", basic_type::pAbsPtr->port, psocklist->sock);
						if(CHM_INVALID_HANDLE != eqfd){
							epoll_ctl(eqfd, EPOLL_CTL_DEL, psocklist->sock, NULL);
						}
						CHM_CLOSESOCK(psocklist->sock);
					}
					if(freesocklist.Insert(psocklist, true)){											// Set abs
						++(*abs_sock_free_cnt);
					}
				}
				// Set free list pointer
				*abs_sock_frees = freesocklist.GetFirstPtr(false);
				// clear
				basic_type::pAbsPtr->socklist = NULL;
			}

			if(CHM_INVALID_SOCK != basic_type::pAbsPtr->selfsock){
				WAN_CHMPRN("port(%d) is opened(selfsock:%d), so it is closed.", basic_type::pAbsPtr->port, basic_type::pAbsPtr->selfsock);
				if(CHM_INVALID_HANDLE != eqfd){
					epoll_ctl(eqfd, EPOLL_CTL_DEL, basic_type::pAbsPtr->selfsock, NULL);
				}
				CHM_CLOSESOCK(basic_type::pAbsPtr->selfsock);
			}

			basic_type::pAbsPtr->port = chmpxsvr->port;
			copy_hostport_pairs(basic_type::pAbsPtr->endpoints, chmpxsvr->endpoints, EXTERNAL_EP_MAX);
		}

		if(basic_type::pAbsPtr->ctlport != chmpxsvr->ctlport || !compare_hostport_pairs(basic_type::pAbsPtr->ctlendpoints, chmpxsvr->ctlendpoints, EXTERNAL_EP_MAX)){
			if(CHM_INVALID_SOCK != basic_type::pAbsPtr->ctlsock){
				WAN_CHMPRN("ctlport(%d) is opened(ctlsock:%d), it is closed.", basic_type::pAbsPtr->ctlport, basic_type::pAbsPtr->ctlsock);
				if(CHM_INVALID_HANDLE != eqfd){
					epoll_ctl(eqfd, EPOLL_CTL_DEL, basic_type::pAbsPtr->ctlsock, NULL);
				}
				CHM_CLOSESOCK(basic_type::pAbsPtr->ctlsock);
			}
			if(CHM_INVALID_SOCK != basic_type::pAbsPtr->selfctlsock){
				WAN_CHMPRN("ctlport(%d) is opened(selfctlsock:%d), it is closed.", basic_type::pAbsPtr->ctlport, basic_type::pAbsPtr->selfctlsock);
				if(CHM_INVALID_HANDLE != eqfd){
					epoll_ctl(eqfd, EPOLL_CTL_DEL, basic_type::pAbsPtr->selfctlsock, NULL);
				}
				CHM_CLOSESOCK(basic_type::pAbsPtr->selfctlsock);
			}

			basic_type::pAbsPtr->ctlport = chmpxsvr->ctlport;
			copy_hostport_pairs(basic_type::pAbsPtr->ctlendpoints, chmpxsvr->ctlendpoints, EXTERNAL_EP_MAX);
		}

	}else{
		// [NOTE]
		// Be care for in consideration of special situations such as when the hostname and FQDN are different,
		// or when the hostname is not registered in DNS.
		// When using CUSTOM SEED, there is a case that the hostname is set to name. Thus it may not be able
		// to resolve the name, so resolve this.
		// However, the chmpxid is conditional on the same.
		//
		if(basic_type::pAbsPtr->chmpxid != chmpxsvr->chmpxid || !IsInHostnameList(basic_type::pAbsPtr->name, node_list, foundname, true)){
			if(CHMPXID_SEED_CUSTOM != type || 0 != strncmp(basic_type::pAbsPtr->custom_seed, chmpxsvr->name, CUSTOM_ID_SEED_MAX)){
				if(CHMPXID_SEED_CUSTOM != type){
					ERR_CHMPRN("name(%s - %s) or chmpxid(0x%016" PRIx64 " - 0x%016" PRIx64 ") is different.", basic_type::pAbsPtr->name, chmpxsvr->name, basic_type::pAbsPtr->chmpxid, chmpxsvr->chmpxid);
				}else{
					ERR_CHMPRN("name(%s(%s) - %s) or chmpxid(0x%016" PRIx64 " - 0x%016" PRIx64 ") is different.", basic_type::pAbsPtr->name, basic_type::pAbsPtr->custom_seed, chmpxsvr->name, basic_type::pAbsPtr->chmpxid, chmpxsvr->chmpxid);
				}
				return false;
			}
		}
		bool	is_overwrite_port	= false;
		bool	is_overwrite_ctlport= false;
		if(basic_type::pAbsPtr->port != chmpxsvr->port || !compare_hostport_pairs(basic_type::pAbsPtr->endpoints, chmpxsvr->endpoints, EXTERNAL_EP_MAX)){
			if(basic_type::pAbsPtr->socklist){
				ERR_CHMPRN("port(%d) is opened yet.", basic_type::pAbsPtr->port);
				return false;
			}
			if(CHM_INVALID_SOCK != basic_type::pAbsPtr->selfsock){
				ERR_CHMPRN("port(%d) is opened(selfsock:%d) yet.", basic_type::pAbsPtr->port, basic_type::pAbsPtr->selfsock);
				return false;
			}
			is_overwrite_port = true;
		}
		if(basic_type::pAbsPtr->ctlport != chmpxsvr->ctlport || !compare_hostport_pairs(basic_type::pAbsPtr->ctlendpoints, chmpxsvr->ctlendpoints, EXTERNAL_EP_MAX)){
			if(CHM_INVALID_SOCK != basic_type::pAbsPtr->ctlsock){
				ERR_CHMPRN("ctlport(%d) is opened(ctlsock:%d) yet.", basic_type::pAbsPtr->ctlport, basic_type::pAbsPtr->ctlsock);
				return false;
			}
			if(CHM_INVALID_SOCK != basic_type::pAbsPtr->selfctlsock){
				ERR_CHMPRN("ctlport(%d) is opened(selfctlsock:%d) yet.", basic_type::pAbsPtr->ctlport, basic_type::pAbsPtr->selfctlsock);
				return false;
			}
			is_overwrite_ctlport = true;
		}
		if(is_overwrite_port){
			basic_type::pAbsPtr->port = chmpxsvr->port;
			copy_hostport_pairs(basic_type::pAbsPtr->endpoints, chmpxsvr->endpoints, EXTERNAL_EP_MAX);
		}
		if(is_overwrite_ctlport){
			basic_type::pAbsPtr->ctlport = chmpxsvr->ctlport;
			copy_hostport_pairs(basic_type::pAbsPtr->ctlendpoints, chmpxsvr->ctlendpoints, EXTERNAL_EP_MAX);
		}
	}

	// others(always overwrite)
	strncpy(basic_type::pAbsPtr->cuk,			chmpxsvr->cuk,			CUK_MAX);
	strncpy(basic_type::pAbsPtr->custom_seed,	chmpxsvr->custom_seed,	CUSTOM_ID_SEED_MAX);
	copy_hostport_pairs(basic_type::pAbsPtr->forward_peers, chmpxsvr->forward_peers, FORWARD_PEER_MAX);
	copy_hostport_pairs(basic_type::pAbsPtr->reverse_peers, chmpxsvr->reverse_peers, REVERSE_PEER_MAX);

	// [NOTICE]
	// If base(pending) hash value is valid, set always pointer to chmpxlist array.
	// But not clear old pointer when old hash values is valid.
	// So it means always over writing array but not clear old.(take care for read
	// array value.)
	//
	if(basic_type::pAbsPtr->base_hash != chmpxsvr->base_hash){
		basic_type::pAbsPtr->base_hash = chmpxsvr->base_hash;
		if(CHM_INVALID_HASHVAL != basic_type::pAbsPtr->base_hash){
			if(!abs_base_arr){
				ERR_CHMPRN("abs_base_arr is NULL, could not update base hash array.");
			}else{
				abs_base_arr[basic_type::pAbsPtr->base_hash] = chmpx_lap<T>::GetRelPtr();
			}
		}
	}
	if(basic_type::pAbsPtr->pending_hash != chmpxsvr->pending_hash){
		basic_type::pAbsPtr->pending_hash = chmpxsvr->pending_hash;
		if(CHM_INVALID_HASHVAL != basic_type::pAbsPtr->pending_hash){
			if(!abs_pend_arr){
				ERR_CHMPRN("abs_pend_arr is NULL, could not update pending hash array.");
			}else{
				abs_pend_arr[basic_type::pAbsPtr->pending_hash] = chmpx_lap<T>::GetRelPtr();
			}
		}
	}

	if(0 >= CompareStatusUpdateTime(basic_type::pAbsPtr->last_status_time, chmpxsvr->last_status_time)){
		MSG_CHMPRN("chmpxid(0x%016" PRIx64 ") status is changed from (0x%016" PRIx64 ":%s) to (0x%016" PRIx64 ":%s) with last update time(%zu)", basic_type::pAbsPtr->chmpxid, basic_type::pAbsPtr->status, STR_CHMPXSTS_FULL(basic_type::pAbsPtr->status).c_str(), chmpxsvr->status, STR_CHMPXSTS_FULL(chmpxsvr->status).c_str(), chmpxsvr->last_status_time);
		basic_type::pAbsPtr->last_status_time	= chmpxsvr->last_status_time;
		basic_type::pAbsPtr->status				= chmpxsvr->status;
	}else{
		MSG_CHMPRN("chmpxid(0x%016" PRIx64 ") status(update time : %zu) is did not change from (0x%016" PRIx64 ":%s) to (0x%016" PRIx64 ":%s) with last update time(%zu)", basic_type::pAbsPtr->chmpxid, basic_type::pAbsPtr->last_status_time, basic_type::pAbsPtr->status, STR_CHMPXSTS_FULL(basic_type::pAbsPtr->status).c_str(), chmpxsvr->status, STR_CHMPXSTS_FULL(chmpxsvr->status).c_str(), chmpxsvr->last_status_time);
	}
	return true;
}

template<typename T>
bool chmpx_lap<T>::GetSslStructure(CHMPXSSL& ssl) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPX does not set.");
		return false;
	}
	strcpy(ssl.capath,			basic_type::pAbsPtr->capath);
	strcpy(ssl.server_cert,		basic_type::pAbsPtr->server_cert);
	strcpy(ssl.server_prikey,	basic_type::pAbsPtr->server_prikey);
	strcpy(ssl.slave_cert,		basic_type::pAbsPtr->slave_cert);
	strcpy(ssl.slave_prikey,	basic_type::pAbsPtr->slave_prikey);

	ssl.is_ssl					= basic_type::pAbsPtr->is_ssl;
	ssl.verify_peer				= basic_type::pAbsPtr->verify_peer;
	ssl.is_ca_file				= basic_type::pAbsPtr->is_ca_file;

	return true;
}

//
// return < 0, means other is small
// return > 0, means other is big
//
template<typename T>
int chmpx_lap<T>::compare_order(const chmpx_lap<T>& other, CHMPXID_SEED_TYPE type) const
{
	int	result;

	// 1) At case of CHMPXID_SEED_CUSTOM, first checking seed.
	if(CHMPXID_SEED_CUSTOM == type){
		if(0 != (result = strcmp(other.pAbsPtr->custom_seed, basic_type::pAbsPtr->custom_seed))){
			return result;
		}
	}
	// 2) name
	if(0 != (result = strcmp(other.pAbsPtr->name, basic_type::pAbsPtr->name))){
		return result;
	}
	// 3) port
	if(other.pAbsPtr->ctlport != basic_type::pAbsPtr->ctlport){
		return ((other.pAbsPtr->ctlport < basic_type::pAbsPtr->ctlport) ? -1 : 1);
	}
	// 4) cuk
	if(0 != (result = strcmp(other.pAbsPtr->cuk, basic_type::pAbsPtr->cuk))){
		return result;
	}
	// 5) Not case of CHMPXID_SEED_CUSTOM, last checking seed.
	if(CHMPXID_SEED_CUSTOM != type){
		if(0 != (result = strcmp(other.pAbsPtr->custom_seed, basic_type::pAbsPtr->custom_seed))){
			return result;
		}
	}
	return result;
}

typedef	chmpx_lap<CHMPX>	chmpxlap;

//---------------------------------------------------------
// For CHMPXLIST
//---------------------------------------------------------
template<typename T>
class chmpxlist_lap : public structure_lap<T>
{
	public:
		typedef T					st_type;
		typedef T*					st_ptr_type;
		typedef structure_lap<T>	basic_type;

	protected:
		st_ptr_type*				pchmpxid_map;
		PCHMPX*						abs_base_arr;
		PCHMPX*						abs_pend_arr;
		long*						abs_sock_free_cnt;
		PCHMSOCKLIST*				abs_sock_frees;

	protected:
		bool SaveChmpxIdMap(st_ptr_type ptr, bool is_abs);
		bool RetrieveChmpxIdMap(st_ptr_type ptr, bool is_abs);
		st_ptr_type SearchChmpxid(chmpxid_t chmpxid);

	public:
		explicit chmpxlist_lap(st_ptr_type ptr = NULL, st_ptr_type* absmapptr = NULL, PCHMPX* pchmpxarrbase = NULL, PCHMPX* pchmpxarrpend = NULL, long* psockfreecnt = NULL, PCHMSOCKLIST* psockfrees = NULL, const void* shmbase = NULL, bool is_abs = true);

		PCHMPX GetAbsChmpxPtr(void) const { return (basic_type::pAbsPtr ? &basic_type::pAbsPtr->chmpx : NULL); }
		// cppcheck-suppress clarifyCondition
		PCHMPX GetRelChmpxPtr(void) const { return (basic_type::pAbsPtr ? CHM_REL(basic_type::pShmBase, &basic_type::pAbsPtr->chmpx, PCHMPX) : NULL); }

		void Reset(st_ptr_type ptr, st_ptr_type* absmapptr, PCHMPX* pchmpxarrbase, PCHMPX* pchmpxarrpend, long* psockfreecnt, PCHMSOCKLIST* psockfrees, const void* shmbase, bool is_abs = true);
		virtual bool Initialize(void);
		virtual bool Dump(std::stringstream& sstream, const char* spacer) const;
		virtual bool Clear(void);
		bool Clear(int eqfd);
		st_ptr_type Dup(bool only_list_top = false);
		void Free(st_ptr_type ptr) const;

		bool Initialize(st_ptr_type prev, st_ptr_type next, bool is_abs = true);
		bool SaveChmpxIdMap(void) { return SaveChmpxIdMap(basic_type::pAbsPtr, true); }
		bool RetrieveChmpxIdMap(void) { return RetrieveChmpxIdMap(basic_type::pAbsPtr, true); }

		st_ptr_type GetFirstPtr(bool is_abs = true);
		bool ToFirst(void);
		bool ToLast(void);
		bool ToNext(bool is_cycle = true, bool is_base_hash = false, bool is_up_servers = false, bool is_allow_same_server = true, bool without_suspend = false);
		bool ToNextUpServicing(bool is_cycle = true, bool is_allow_same_server = true, bool without_suspend = false);
		long Count(void);
		long Count(bool toward_normal);
		long GetChmpxIds(chmpxidlist_t& list, chmpxsts_t status_mask = CHMPXSTS_MASK_ALL, bool part_match = true, bool is_to_first = false);
		long GetChmpxIds(chmpxidlist_t& list, bool with_pending, bool without_down = true, bool without_suspend = true, bool is_to_first = false);

		long BaseHashCount(bool is_to_first = false);
		long PendingHashCount(bool is_to_first = false);
		bool UpdateHash(int type, bool is_to_first = false);
		bool IsOperating(bool is_to_first = false);
		bool CheckAllServiceOutStatus(void);

		st_ptr_type PopFront(void);
		st_ptr_type PopAny(void);
		bool Push(st_ptr_type ptr, bool is_abs, bool is_to_first = false);
		bool PushBack(st_ptr_type ptr, bool is_abs);
		bool Insert(st_ptr_type ptr, bool is_abs, CHMPXID_SEED_TYPE type);
		bool Find(chmpxid_t chmpxid);
		bool Find(int sock, bool is_to_first = false);
		bool FindByHash(chmhash_t hash, bool is_base_hash = true, bool is_to_first = false);
		chmpxid_t FindByStatus(chmpxsts_t status, bool part_match = false, bool is_to_first = false);
		chmpxid_t GetRandomChmpxId(bool is_up_servers = false);
		chmpxid_t GetChmpxIdByHash(chmhash_t hash);
		st_ptr_type Retrieve(PCHMPX ptr, bool is_abs);
		st_ptr_type Retrieve(void);
};

template<typename T>
chmpxlist_lap<T>::chmpxlist_lap(st_ptr_type ptr, st_ptr_type* absmapptr, PCHMPX* pchmpxarrbase, PCHMPX* pchmpxarrpend, long* psockfreecnt, PCHMSOCKLIST* psockfrees, const void* shmbase, bool is_abs) : pchmpxid_map(absmapptr)
{
	Reset(ptr, absmapptr, pchmpxarrbase, pchmpxarrpend, psockfreecnt, psockfrees, shmbase, is_abs);
}

template<typename T>
void chmpxlist_lap<T>::Reset(st_ptr_type ptr, st_ptr_type* absmapptr, PCHMPX* pchmpxarrbase, PCHMPX* pchmpxarrpend, long* psockfreecnt, PCHMSOCKLIST* psockfrees, const void* shmbase, bool is_abs)
{
	structure_lap<T>::Reset(ptr, shmbase, is_abs);
	pchmpxid_map		= absmapptr;
	abs_base_arr		= pchmpxarrbase;
	abs_pend_arr		= pchmpxarrpend;
	abs_sock_free_cnt	= psockfreecnt;
	abs_sock_frees		= psockfrees;
}

template<typename T>
bool chmpxlist_lap<T>::Initialize(void)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXLIST does not set.");
		return false;
	}
	basic_type::pAbsPtr->prev	= NULL;
	basic_type::pAbsPtr->next	= NULL;
	basic_type::pAbsPtr->same	= NULL;

	chmpxlap	tmpchmpx(&basic_type::pAbsPtr->chmpx, abs_base_arr, abs_pend_arr, abs_sock_free_cnt, abs_sock_frees, basic_type::pShmBase);
	if(!tmpchmpx.Initialize()){
		ERR_CHMPRN("Failed to initialize CHMPX.");
		return false;
	}
	return true;
}

template<typename T>
bool chmpxlist_lap<T>::Initialize(st_ptr_type prev, st_ptr_type next, bool is_abs)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXLIST does not set.");
		return false;
	}
	basic_type::pAbsPtr->prev	= is_abs ? CHM_REL(basic_type::pShmBase, prev, st_ptr_type) : prev;
	basic_type::pAbsPtr->next	= is_abs ? CHM_REL(basic_type::pShmBase, next, st_ptr_type) : next;
	basic_type::pAbsPtr->same	= NULL;

	chmpxlap	tmpchmpx(&basic_type::pAbsPtr->chmpx, abs_base_arr, abs_pend_arr, abs_sock_free_cnt, abs_sock_frees, basic_type::pShmBase);
	if(!tmpchmpx.Initialize()){
		ERR_CHMPRN("Failed to initialize CHMPX.");
		return false;
	}
	return true;
}

template<typename T>
bool chmpxlist_lap<T>::Dump(std::stringstream& sstream, const char* spacer) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXLIST does not set.");
		return false;
	}
	std::string	tmpspacer1 = spacer ? spacer : "";
	std::string	tmpspacer2;
	tmpspacer1 += "  ";
	tmpspacer2 += tmpspacer1 + "  ";

	// To first
	st_ptr_type	cur;
	for(cur = const_cast<st_ptr_type>(basic_type::pAbsPtr); cur->prev; cur = CHM_ABS(basic_type::pShmBase, cur->prev, st_ptr_type));

	for(long count = 0L; cur; cur = CHM_ABS(basic_type::pShmBase, cur->next, st_ptr_type), count++){
		sstream << (spacer ? spacer : "") << "[" << count << "] = {"		<< std::endl;

		sstream << tmpspacer1 << "prev = "	<< cur->prev	<< std::endl;
		sstream << tmpspacer1 << "next = "	<< cur->next	<< std::endl;
		sstream << tmpspacer1 << "same = "	<< cur->same	<< std::endl;

		sstream << tmpspacer1 << "chmpx{"	<< std::endl;

		chmpxlap	tmpchmpx(&cur->chmpx, abs_base_arr, abs_pend_arr, abs_sock_free_cnt, abs_sock_frees, basic_type::pShmBase);
		tmpchmpx.Dump(sstream, tmpspacer2.c_str());
		sstream << tmpspacer1 << "}"		<< std::endl;

		sstream << (spacer ? spacer : "") << "}"		<< std::endl;
	}
	return true;
}

// [NOTE]
// only_list_top is true, duplicate only one list(always it is self)
//
template<typename T>
typename chmpxlist_lap<T>::st_ptr_type chmpxlist_lap<T>::Dup(bool only_list_top)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXLIST does not set.");
		return NULL;
	}

	long	count = (only_list_top ? 1 : Count());
	if(0 == count){
		return NULL;
	}

	// To first
	st_ptr_type	cur;
	if(only_list_top){
		cur = const_cast<st_ptr_type>(basic_type::pAbsPtr);
	}else{
		for(cur = const_cast<st_ptr_type>(basic_type::pAbsPtr); cur->prev; cur = CHM_ABS(basic_type::pShmBase, cur->prev, st_ptr_type));
	}

	// duplicate
	st_ptr_type	pdst;
	if(NULL == (pdst = reinterpret_cast<st_ptr_type>(calloc(count, sizeof(st_type))))){
		ERR_CHMPRN("Could not allocation memory.");
		return NULL;
	}
	if(only_list_top){
		pdst->next = NULL;
		pdst->prev = NULL;
		chmpxlap	tmpchmpx(&cur->chmpx, abs_base_arr, abs_pend_arr, abs_sock_free_cnt, abs_sock_frees, basic_type::pShmBase);
		if(!tmpchmpx.Copy(&(pdst->chmpx))){
			ERR_CHMPRN("Failed to copy top of chmpx structure list.");
			CHM_Free(pdst);
			return NULL;
		}
	}else{
		for(st_ptr_type pdstcur = pdst, pdstprev = NULL; cur; cur = CHM_ABS(basic_type::pShmBase, cur->next, st_ptr_type), pdstprev = pdstcur++){
			if(pdstprev){
				pdstprev->next = pdstcur;
			}
			pdstcur->prev = pdstprev;

			chmpxlap	tmpchmpx(&cur->chmpx, abs_base_arr, abs_pend_arr, abs_sock_free_cnt, abs_sock_frees, basic_type::pShmBase);
			if(!tmpchmpx.Copy(&(pdstcur->chmpx))){
				WAN_CHMPRN("Failed to copy chmpx structure, but continue...");
			}
		}
	}
	return pdst;
}

template<typename T>
void chmpxlist_lap<T>::Free(st_ptr_type ptr) const
{
	if(ptr){
		for(st_ptr_type pcur = ptr; pcur; pcur = pcur->next){
			chmpxlap	tmpchmpx;				// [NOTE] pointer is not on Shm, but we use chmpxlap object. thus using only Free method.
			tmpchmpx.Free(&pcur->chmpx);
		}
		K2H_Free(ptr);
	}
}

template<typename T>
bool chmpxlist_lap<T>::Clear(void)
{
	return Clear(CHM_INVALID_HANDLE);
}

template<typename T>
bool chmpxlist_lap<T>::Clear(int eqfd)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPCLIST does not set.");
		return false;
	}
	RetrieveChmpxIdMap();				// basic_type::pAbsPtr->same
	basic_type::pAbsPtr->prev = NULL;
	basic_type::pAbsPtr->next = NULL;

	chmpxlap	tmpchmpx(&basic_type::pAbsPtr->chmpx, abs_base_arr, abs_pend_arr, abs_sock_free_cnt, abs_sock_frees, basic_type::pShmBase);
	if(CHM_INVALID_HANDLE != eqfd){
		tmpchmpx.Close(eqfd);
	}
	if(!tmpchmpx.Clear()){
		ERR_CHMPRN("Failed to clear CHMPX.");
		return false;
	}
	return true;
}

template<typename T>
bool chmpxlist_lap<T>::SaveChmpxIdMap(st_ptr_type ptr, bool is_abs)
{
	if(!ptr){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPCLIST does not set.");
		return false;
	}
	st_ptr_type	absptr	= is_abs ? ptr : CHM_ABS(basic_type::pShmBase, ptr, st_ptr_type);
	chmpxlap	tmpchmpx(&absptr->chmpx, abs_base_arr, abs_pend_arr, abs_sock_free_cnt, abs_sock_frees, basic_type::pShmBase);
	chmpxid_t	chmpxid = tmpchmpx.GetChmpxId();

	if(CHM_INVALID_CHMPXID == chmpxid){
		return true;		// Not need to save
	}
	if(!pchmpxid_map){
		ERR_CHMPRN("pchmpxid_map is NULL.");
		return false;
	}

	// set to last pos(check same pointer exists)
	st_ptr_type*	ppabsmap;
	st_ptr_type		pabsmap;
	for(ppabsmap = &pchmpxid_map[chmpxid & CHMPXID_MAP_MASK], pabsmap = CHM_ABS(basic_type::pShmBase, pchmpxid_map[chmpxid & CHMPXID_MAP_MASK], st_ptr_type);
		pabsmap;
		ppabsmap = &pabsmap->same, pabsmap = CHM_ABS(basic_type::pShmBase, pabsmap->same, st_ptr_type))
	{
		if(pabsmap == absptr){
			// already set.(why?)
			break;
		}
	}
	if(!pabsmap){
		*ppabsmap = CHM_REL(basic_type::pShmBase, absptr, st_ptr_type);
	}
	if(absptr->same){
		WAN_CHMPRN("ptr->same is not NULL, so force to set NULL.");
		absptr->same = NULL;
	}

	return true;
}

template<typename T>
bool chmpxlist_lap<T>::RetrieveChmpxIdMap(st_ptr_type ptr, bool is_abs)
{
	if(!ptr){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPCLIST does not set.");
		return false;
	}
	st_ptr_type	absptr	= is_abs ? ptr : CHM_ABS(basic_type::pShmBase, ptr, st_ptr_type);
	chmpxlap	tmpchmpx(&absptr->chmpx, abs_base_arr, abs_pend_arr, abs_sock_free_cnt, abs_sock_frees, basic_type::pShmBase);
	chmpxid_t	chmpxid = tmpchmpx.GetChmpxId();

	if(!pchmpxid_map){
		WAN_CHMPRN("pchmpxid_map is NULL.");
	}
	if(CHM_INVALID_CHMPXID != chmpxid && pchmpxid_map){
		st_ptr_type*	ppabsmap;
		st_ptr_type		pabsmap;
		for(ppabsmap = &pchmpxid_map[chmpxid & CHMPXID_MAP_MASK], pabsmap = CHM_ABS(basic_type::pShmBase, pchmpxid_map[chmpxid & CHMPXID_MAP_MASK], st_ptr_type);
			pabsmap;
			ppabsmap = &pabsmap->same, pabsmap = CHM_ABS(basic_type::pShmBase, pabsmap->same, st_ptr_type))
		{
			if(pabsmap == absptr){
				*ppabsmap = absptr->same;
				break;
			}
		}
	}
	absptr->same = NULL;

	return true;
}

template<typename T>
typename chmpxlist_lap<T>::st_ptr_type chmpxlist_lap<T>::SearchChmpxid(chmpxid_t chmpxid)
{
	if(CHM_INVALID_CHMPXID == chmpxid){
		ERR_CHMPRN("Parameter is wrong.");
		return NULL;
	}
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPCLIST does not set.");
		return NULL;
	}
	if(!pchmpxid_map){
		ERR_CHMPRN("pchmpxid_map is NULL.");
		return NULL;
	}

	chmpxlap	tmpchmpx(&basic_type::pAbsPtr->chmpx, abs_base_arr, abs_pend_arr, abs_sock_free_cnt, abs_sock_frees, basic_type::pShmBase);
	bool		is_server = tmpchmpx.IsServerMode();

	for(st_ptr_type pabsmap = CHM_ABS(basic_type::pShmBase, pchmpxid_map[chmpxid & CHMPXID_MAP_MASK], st_ptr_type); pabsmap; pabsmap = CHM_ABS(basic_type::pShmBase, pabsmap->same, st_ptr_type)){
		tmpchmpx.Reset(&pabsmap->chmpx, abs_base_arr, abs_pend_arr, abs_sock_free_cnt, abs_sock_frees, basic_type::pShmBase);
		if(is_server == tmpchmpx.IsServerMode() && chmpxid == tmpchmpx.GetChmpxId()){
			// found same mode & chmpxid
			return pabsmap;
		}
	}
	// Not found
	return NULL;
}

template<typename T>
typename chmpxlist_lap<T>::st_ptr_type chmpxlist_lap<T>::GetFirstPtr(bool is_abs)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		//MSG_CHMPRN("PCHMPXLIST does not set.");
		return NULL;
	}
	if(basic_type::pAbsPtr->prev){
		ToFirst();
	}
	return (is_abs ? chmpxlist_lap<T>::GetAbsPtr() : chmpxlist_lap<T>::GetRelPtr());

}

template<typename T>
bool chmpxlist_lap<T>::ToFirst(void)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXLIST does not set.");
		return false;
	}
	for(; basic_type::pAbsPtr->prev; basic_type::pAbsPtr = CHM_ABS(basic_type::pShmBase, basic_type::pAbsPtr->prev, st_ptr_type));
	return true;
}

template<typename T>
bool chmpxlist_lap<T>::ToLast(void)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXLIST does not set.");
		return false;
	}
	for(; basic_type::pAbsPtr->next; basic_type::pAbsPtr = CHM_ABS(basic_type::pShmBase, basic_type::pAbsPtr->next, st_ptr_type));
	return true;
}

//
// If is_allow_same_server is false when is_cycle is true, this method returns false when the next server
// is same as start server.
//
template<typename T>
bool chmpxlist_lap<T>::ToNext(bool is_cycle, bool is_base_hash, bool is_up_servers, bool is_allow_same_server, bool without_suspend)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXLIST does not set.");
		return false;
	}
	st_ptr_type	startpos = basic_type::pAbsPtr;

	if(!basic_type::pAbsPtr->next){
		if(!is_cycle){
			return is_allow_same_server;
		}
		ToFirst();
	}else{
		basic_type::pAbsPtr = CHM_ABS(basic_type::pShmBase, basic_type::pAbsPtr->next, st_ptr_type);
	}

	chmpxlap	curchmpx;
	while(startpos != basic_type::pAbsPtr){
		if(!is_base_hash && !is_up_servers){
			return true;
		}
		curchmpx.Reset(&basic_type::pAbsPtr->chmpx, abs_base_arr, abs_pend_arr, abs_sock_free_cnt, abs_sock_frees, basic_type::pShmBase);

		chmpxsts_t	status = curchmpx.GetStatus();
		if((!is_base_hash || IS_CHMPXSTS_BASEHASH(status)) && (!is_up_servers || IS_CHMPXSTS_UP(status)) && (!without_suspend || IS_CHMPXSTS_NOSUP(status))){
			return true;
		}

		if(!basic_type::pAbsPtr->next){
			if(!is_cycle){
				basic_type::pAbsPtr = startpos;		// restore
				return is_allow_same_server;
			}
			ToFirst();
		}else{
			basic_type::pAbsPtr = CHM_ABS(basic_type::pShmBase, basic_type::pAbsPtr->next, st_ptr_type);
		}
	}
	return is_allow_same_server;
}

//
// If is_allow_same_server is false when is_cycle is true, this method returns false when the next server
// is same as start server.
//
template<typename T>
bool chmpxlist_lap<T>::ToNextUpServicing(bool is_cycle, bool is_allow_same_server, bool without_suspend)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXLIST does not set.");
		return false;
	}
	chmpxlap	curchmpx;
	bool		is_cycled= false;
	st_ptr_type	startpos = basic_type::pAbsPtr;

	if(!basic_type::pAbsPtr->next){
		if(!is_cycle){
			if(is_allow_same_server){
				curchmpx.Reset(&basic_type::pAbsPtr->chmpx, abs_base_arr, abs_pend_arr, abs_sock_free_cnt, abs_sock_frees, basic_type::pShmBase);
				chmpxsts_t	status = curchmpx.GetStatus();
				if(IS_CHMPXSTS_SERVICING(status) && (!without_suspend || IS_CHMPXSTS_NOSUP(status))){
					return true;
				}
				// current(same) server is not servicing
			}
			return false;
		}
		is_cycled = true;
		ToFirst();
	}else{
		basic_type::pAbsPtr = CHM_ABS(basic_type::pShmBase, basic_type::pAbsPtr->next, st_ptr_type);
	}

	while(startpos != basic_type::pAbsPtr){
		curchmpx.Reset(&basic_type::pAbsPtr->chmpx, abs_base_arr, abs_pend_arr, abs_sock_free_cnt, abs_sock_frees, basic_type::pShmBase);

		chmpxsts_t	status = curchmpx.GetStatus();
		if(IS_CHMPXSTS_SERVICING(status) && (!without_suspend || IS_CHMPXSTS_NOSUP(status))){
			return true;
		}
		if(!basic_type::pAbsPtr->next){
			if(!is_cycle || is_cycled){
				basic_type::pAbsPtr = startpos;		// restore
				if(is_allow_same_server){
					curchmpx.Reset(&basic_type::pAbsPtr->chmpx, abs_base_arr, abs_pend_arr, abs_sock_free_cnt, abs_sock_frees, basic_type::pShmBase);
					status = curchmpx.GetStatus();
					if(IS_CHMPXSTS_SERVICING(status) && (!without_suspend || IS_CHMPXSTS_NOSUP(status))){
						return true;
					}
					// current(same) server is not servicing
				}
				return false;
			}
			is_cycled = true;
			ToFirst();
		}else{
			basic_type::pAbsPtr = CHM_ABS(basic_type::pShmBase, basic_type::pAbsPtr->next, st_ptr_type);
		}
	}

	if(is_allow_same_server){
		curchmpx.Reset(&basic_type::pAbsPtr->chmpx, abs_base_arr, abs_pend_arr, abs_sock_free_cnt, abs_sock_frees, basic_type::pShmBase);
		chmpxsts_t	status = curchmpx.GetStatus();
		if(IS_CHMPXSTS_SERVICING(status) && (!without_suspend || IS_CHMPXSTS_NOSUP(status))){
			return true;
		}
		// current(same) server is not servicing
	}
	return false;
}

template<typename T>
long chmpxlist_lap<T>::Count(void)
{
	if(!basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXLIST does not set.");
		return 0L;
	}
	if(!basic_type::pAbsPtr){
		//MSG_CHMPRN("PCHMPXLIST does not set.");
		return 0L;
	}
	ToFirst();
	long		count;
	st_ptr_type pos;
	for(count = 1L, pos = basic_type::pAbsPtr; pos->next; pos = CHM_ABS(basic_type::pShmBase, pos->next, st_ptr_type), count++);

	return count;
}

template<typename T>
long chmpxlist_lap<T>::Count(bool toward_normal)
{
	if(!basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXLIST does not set.");
		return 0L;
	}
	if(!basic_type::pAbsPtr){
		//MSG_CHMPRN("PCHMPXLIST does not set.");
		return 0L;
	}
	long		count;
	st_ptr_type pos;
	for(count = 1L, pos = basic_type::pAbsPtr; NULL != (toward_normal ? pos->next : pos->prev); pos = CHM_ABS(basic_type::pShmBase, (toward_normal ? pos->next : pos->prev), st_ptr_type), count++);

	return count;
}

template<typename T>
long chmpxlist_lap<T>::GetChmpxIds(chmpxidlist_t& list, chmpxsts_t status_mask, bool part_match, bool is_to_first)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		//MSG_CHMPRN("PCHMPXLIST does not set.");
		return 0L;
	}
	if(is_to_first){
		ToFirst();
	}
	list.clear();

	chmpxlap	curchmpx;
	for(st_ptr_type cur = basic_type::pAbsPtr; cur; cur = CHM_ABS(basic_type::pShmBase, cur->next, st_ptr_type)){
		curchmpx.Reset(&cur->chmpx, abs_base_arr, abs_pend_arr, abs_sock_free_cnt, abs_sock_frees, basic_type::pShmBase);

		chmpxsts_t	status = curchmpx.GetStatus();
		if(part_match){
			if(0 == (status & status_mask)){
				continue;
			}
		}else{
			if(status_mask != (status & status_mask)){
				continue;
			}
		}
		list.push_back(curchmpx.GetChmpxId());
	}
	return static_cast<long>(list.size());
}

template<typename T>
long chmpxlist_lap<T>::GetChmpxIds(chmpxidlist_t& list, bool with_pending, bool without_down, bool without_suspend, bool is_to_first)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXLIST does not set.");
		return 0L;
	}
	if(is_to_first){
		ToFirst();
	}
	list.clear();

	chmpxlap	curchmpx;
	for(st_ptr_type cur = basic_type::pAbsPtr; cur; cur = CHM_ABS(basic_type::pShmBase, cur->next, st_ptr_type)){
		curchmpx.Reset(&cur->chmpx, abs_base_arr, abs_pend_arr, abs_sock_free_cnt, abs_sock_frees, basic_type::pShmBase);

		chmpxsts_t	status = curchmpx.GetStatus();
		if(without_down && IS_CHMPXSTS_DOWN(status)){
			continue;
		}
		if(without_suspend && IS_CHMPXSTS_SUSPEND(status)){
			continue;
		}
		if(IS_CHMPXSTS_BASEHASH(status) || (with_pending && IS_CHMPXSTS_PENDINGHASH(status))){
			list.push_back(curchmpx.GetChmpxId());
		}
	}
	return static_cast<long>(list.size());
}

//
// This method is linear search in list, then it is not good performance.
//
template<typename T>
long chmpxlist_lap<T>::BaseHashCount(bool is_to_first)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXLIST does not set.");
		return 0L;
	}
	if(is_to_first){
		ToFirst();
	}

	long		count;
	chmpxlap	curchmpx;
	st_ptr_type cur;
	for(count = 0L, cur = basic_type::pAbsPtr; cur; cur = CHM_ABS(basic_type::pShmBase, cur->next, st_ptr_type)){
		curchmpx.Reset(&cur->chmpx, abs_base_arr, abs_pend_arr, abs_sock_free_cnt, abs_sock_frees, basic_type::pShmBase);

		chmpxsts_t	status = curchmpx.GetStatus();
		if(IS_CHMPXSTS_BASEHASH(status)){
			count++;
		}
	}
	return count;
}

//
// This method is linear search in list, then it is not good performance.
//
template<typename T>
long chmpxlist_lap<T>::PendingHashCount(bool is_to_first)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXLIST does not set.");
		return 0L;
	}
	if(is_to_first){
		ToFirst();
	}
	long		count;
	chmpxlap	curchmpx;
	st_ptr_type cur;
	for(count = 0L, cur = basic_type::pAbsPtr; cur; cur = CHM_ABS(basic_type::pShmBase, cur->next, st_ptr_type)){
		curchmpx.Reset(&cur->chmpx, abs_base_arr, abs_pend_arr, abs_sock_free_cnt, abs_sock_frees, basic_type::pShmBase);

		chmpxsts_t	status = curchmpx.GetStatus();
		if(IS_CHMPXSTS_PENDINGHASH(status)){
			count++;
		}
	}
	return count;
}

//
// This method is linear search in list, then it is not good performance.
//
template<typename T>
bool chmpxlist_lap<T>::UpdateHash(int type, bool is_to_first)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXLIST does not set.");
		return false;
	}
	if(is_to_first){
		ToFirst();
	}

	st_ptr_type cur;
	chmhash_t	newbase;
	chmhash_t	newpending;

	ToFirst();
	for(newbase = 0L, newpending = 0L, cur = basic_type::pAbsPtr; cur; cur = CHM_ABS(basic_type::pShmBase, cur->next, st_ptr_type)){
		chmpxlap	curchmpx;
		chmpxsts_t	status;
		chmhash_t	base;
		chmhash_t	pending;

		curchmpx.Reset(&cur->chmpx, abs_base_arr, abs_pend_arr, abs_sock_free_cnt, abs_sock_frees, basic_type::pShmBase);

		status	= curchmpx.GetStatus();
		base	= CHM_INVALID_HASHVAL;
		pending	= CHM_INVALID_HASHVAL;

		if(!curchmpx.Get(base, pending)){
			ERR_CHMPRN("Failed to get hash values.");
			return false;
		}

		if(IS_HASHTG_BASE(type)){
			// Over write new base hash value
			if(IS_CHMPXSTS_BASEHASH(status)){
				curchmpx.Set(newbase, 0L, HASHTG_BASE);
				++newbase;
			}else{
				curchmpx.Set(CHM_INVALID_HASHVAL, 0L, HASHTG_BASE);			// set CHM_INVALID_HASHVAL
			}
		}
		if(IS_HASHTG_PENDING(type)){
			// Over write new pending hash value
			if(IS_CHMPXSTS_PENDINGHASH(status)){
				curchmpx.Set(0L, newpending, HASHTG_PENDING);
				++newpending;
			}else{
				curchmpx.Set(0L, CHM_INVALID_HASHVAL, HASHTG_PENDING);		// set CHM_INVALID_HASHVAL
			}
		}
	}
	return true;
}

//
// This method is linear search in list, then it is not good performance.
//
template<typename T>
bool chmpxlist_lap<T>::IsOperating(bool is_to_first)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXLIST does not set.");
		return false;
	}
	if(is_to_first){
		ToFirst();
	}

	st_ptr_type cur;
	chmpxlap	curchmpx;
	for(cur = basic_type::pAbsPtr; cur; cur = CHM_ABS(basic_type::pShmBase, cur->next, st_ptr_type)){
		curchmpx.Reset(&cur->chmpx, abs_base_arr, abs_pend_arr, abs_sock_free_cnt, abs_sock_frees, basic_type::pShmBase);

		chmpxsts_t	status = curchmpx.GetStatus();
		if(IS_CHMPXSTS_OPERATING(status)){
			return true;
		}
	}
	return false;
}

//
// This method checks all the server chmpx's status required when all the chmpx servers
// on RING go to the DOWN status.
// In this case, it is possible that there is a server with SERVICE IN status.
// However, at this time all the server chmpx on the RING should be SERVICE OUT status
// for the slave chmpx.
// Therefore, this function checks all the server chmpx's status and sets all server
// chmpx to the appropriate status(SERVICE OUT and DOWN).
//
template<typename T>
bool chmpxlist_lap<T>::CheckAllServiceOutStatus(void)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		//MSG_CHMPRN("PCHMPXLIST does not set.");
		return false;
	}

	// search UP chmpx server
	ToFirst();
	bool		is_found_svr = false;
	st_ptr_type	cur;
	chmpxlap	curchmpx;
	for(cur = basic_type::pAbsPtr; cur; cur = CHM_ABS(basic_type::pShmBase, cur->next, st_ptr_type)){
		curchmpx.Reset(&cur->chmpx, abs_base_arr, abs_pend_arr, abs_sock_free_cnt, abs_sock_frees, basic_type::pShmBase);

		chmpxsts_t	status = curchmpx.GetStatus();
		if(IS_CHMPXSTS_SLAVE(status)){
			// this is slave chmpx
			continue;
		}
		is_found_svr = true;

		if(IS_CHMPXSTS_UP(status)){
			// find UP chmpx server, then nothing to do no more.
			return true;
		}
	}
	if(!is_found_svr){
		// there is no server chmpx.
		return true;
	}

	// Do change status to SERVICE OUT & DOWN to all server chmpx
	ToFirst();
	bool	result	= true;
	for(cur = basic_type::pAbsPtr; cur; cur = CHM_ABS(basic_type::pShmBase, cur->next, st_ptr_type)){
		curchmpx.Reset(&cur->chmpx, abs_base_arr, abs_pend_arr, abs_sock_free_cnt, abs_sock_frees, basic_type::pShmBase);

		// set SERVICE OUT & DOWN status, and invalid base/pending hash value
		if(!curchmpx.SetStatus(CHMPXSTS_SRVOUT_DOWN_NORMAL) || !curchmpx.Set(CHM_INVALID_HASHVAL, CHM_INVALID_HASHVAL, HASHTG_BOTH)){
			WAN_CHMPRN("Could not set status(0x%016" PRIx64 ":%s) to chmpxid(0x%016" PRIx64 ") when no server chmpx up.", static_cast<chmpxsts_t>(CHMPXSTS_SRVOUT_DOWN_NORMAL), STR_CHMPXSTS_FULL(static_cast<chmpxsts_t>(CHMPXSTS_SRVOUT_DOWN_NORMAL)).c_str(), curchmpx.GetChmpxId());
			result = false;
		}
	}
	return result;
}

template<typename T>
typename chmpxlist_lap<T>::st_ptr_type chmpxlist_lap<T>::PopFront(void)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXLIST does not set.");
		return NULL;
	}
	// find list first
	st_ptr_type	first;
	for(first = basic_type::pAbsPtr; first->prev; first = CHM_ABS(basic_type::pShmBase, first->prev, st_ptr_type));

	if(NULL != (basic_type::pAbsPtr = CHM_ABS(basic_type::pShmBase, first->next, st_ptr_type))){
		basic_type::pAbsPtr->prev = NULL;
	}
	first->next = NULL;

	// Do not change "same" member at here.
	// If first has valid chmpxid, it maps in pchmpxid_map.

	return first;		// Absolute
}

//
// [NOTICE]
// This method retrieves current chmpxlist object if it is not first of list object.
//
template<typename T>
typename chmpxlist_lap<T>::st_ptr_type chmpxlist_lap<T>::PopAny(void)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXLIST does not set.");
		return NULL;
	}

	st_ptr_type	resultptr	= NULL;
	st_ptr_type	tmpptr		= NULL;
	if(basic_type::pAbsPtr->next){
		resultptr	= CHM_ABS(basic_type::pShmBase, basic_type::pAbsPtr->next, st_ptr_type);
		tmpptr		= CHM_ABS(basic_type::pShmBase, resultptr->next, st_ptr_type);

		basic_type::pAbsPtr->next = resultptr->next;
		if(tmpptr){
			tmpptr->prev = CHM_REL(basic_type::pShmBase, basic_type::pAbsPtr, st_ptr_type);
		}
	}else{
		resultptr = basic_type::pAbsPtr;
		if(basic_type::pAbsPtr->prev){
			basic_type::pAbsPtr			= CHM_ABS(basic_type::pShmBase, basic_type::pAbsPtr->prev, st_ptr_type);
			basic_type::pAbsPtr->next	= NULL;
		}else{
			basic_type::pAbsPtr	= NULL;
		}
	}
	resultptr->prev = NULL;
	resultptr->next = NULL;

	// Do not change "same" member at here.
	// If first has valid chmpxid, it maps in pchmpxid_map.

	return resultptr;
}

template<typename T>
bool chmpxlist_lap<T>::Push(st_ptr_type ptr, bool is_abs, bool is_to_first)
{
	if(!ptr){
		ERR_CHMPRN("ptr is null.");
		return false;
	}
	if(!basic_type::pShmBase){					// allow basic_type::pAbsPtr is NULL
		ERR_CHMPRN("PCHMPXLIST does not set.");
		return false;
	}

	st_ptr_type	current = basic_type::pAbsPtr;
	if(is_to_first){
		for(; current && current->prev; current = CHM_ABS(basic_type::pShmBase, current->prev, st_ptr_type));
	}

	st_ptr_type	relptr	= is_abs ? CHM_REL(basic_type::pShmBase, ptr, st_ptr_type) : ptr;
	st_ptr_type	absptr	= is_abs ? ptr : CHM_ABS(basic_type::pShmBase, ptr, st_ptr_type);
	st_ptr_type	prevptr	= current ? CHM_REL(basic_type::pShmBase, current->prev, st_ptr_type) : NULL;

	if(prevptr){
		prevptr->next	= relptr;
	}
	absptr->prev		= current ? current->prev : NULL;
	absptr->next		= current ? CHM_REL(basic_type::pShmBase, current, st_ptr_type) : NULL;
	if(current){
		current->prev	= relptr;
	}
	basic_type::pAbsPtr = absptr;

	if(!SaveChmpxIdMap(absptr, true)){
		ERR_CHMPRN("Failed to set into chmpxid map area.");
		return false;
	}
	return true;
}

template<typename T>
bool chmpxlist_lap<T>::PushBack(st_ptr_type ptr, bool is_abs)
{
	if(!ptr){
		ERR_CHMPRN("ptr is null.");
		return false;
	}
	if(!basic_type::pShmBase){					// allow basic_type::pAbsPtr is NULL
		ERR_CHMPRN("PCHMPXLIST does not set.");
		return false;
	}
	// find list last
	st_ptr_type	last;
	for(last = basic_type::pAbsPtr; last && last->next; last = CHM_ABS(basic_type::pShmBase, last->next, st_ptr_type));

	st_ptr_type	relptr	= is_abs ? CHM_REL(basic_type::pShmBase, ptr, st_ptr_type) : ptr;
	st_ptr_type	absptr	= is_abs ? ptr : CHM_ABS(basic_type::pShmBase, ptr, st_ptr_type);

	if(last){
		last->next		= relptr;
	}
	absptr->prev		= last ? CHM_REL(basic_type::pShmBase, last, st_ptr_type) : NULL;
	absptr->next		= NULL;
	basic_type::pAbsPtr	= absptr;

	if(!SaveChmpxIdMap(absptr, true)){
		ERR_CHMPRN("Failed to set into chmpxid map area.");
		return false;
	}
	return true;
}

template<typename T>
bool chmpxlist_lap<T>::Insert(st_ptr_type ptr, bool is_abs, CHMPXID_SEED_TYPE type)
{
	if(!ptr){
		ERR_CHMPRN("ptr is null.");
		return false;
	}
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXLIST does not set.");
		return false;
	}
	// To top
	ToFirst();

	// insert
	st_ptr_type	newptr = is_abs ? ptr : CHM_ABS(basic_type::pShmBase, ptr, st_ptr_type);
	chmpxlap	target(&newptr->chmpx, abs_base_arr, abs_pend_arr, abs_sock_free_cnt, abs_sock_frees, basic_type::pShmBase);
	chmpxlap	curchmpx;
	st_ptr_type	cur;
	st_ptr_type	last;
	for(cur = basic_type::pAbsPtr, last = NULL; cur; cur = CHM_ABS(basic_type::pShmBase, cur->next, st_ptr_type), last = cur){
		curchmpx.Reset(&cur->chmpx, abs_base_arr, abs_pend_arr, abs_sock_free_cnt, abs_sock_frees, basic_type::pShmBase);
		if(0 > curchmpx.compare_order(target, type)){
			st_ptr_type	prev= CHM_ABS(basic_type::pShmBase, cur->prev, st_ptr_type);
			newptr->prev	= CHM_REL(basic_type::pShmBase, prev, st_ptr_type);
			newptr->next	= CHM_REL(basic_type::pShmBase, cur, st_ptr_type);
			cur->prev		= CHM_REL(basic_type::pShmBase, newptr, st_ptr_type);
			if(prev){
				prev->next	= CHM_REL(basic_type::pShmBase, newptr, st_ptr_type);
			}
			break;
		}
	}
	if(!cur){
		if(last){
			last->next		= CHM_REL(basic_type::pShmBase, newptr, st_ptr_type);
			newptr->prev	= CHM_REL(basic_type::pShmBase, last, st_ptr_type);
			newptr->next	= NULL;
		}else{
			// come here, on slave case when no slave.
			return PushBack(ptr, is_abs);
		}
	}
	if(!SaveChmpxIdMap(newptr, true)){
		ERR_CHMPRN("Failed to set into chmpxid map area.");
		return false;
	}
	return true;
}

template<typename T>
bool chmpxlist_lap<T>::Find(chmpxid_t chmpxid)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		//MSG_CHMPRN("PCHMPXLIST does not set.");
		return false;
	}

	st_ptr_type	found = SearchChmpxid(chmpxid);
	if(!found){
		return false;
	}

	basic_type::pAbsPtr = found;
	return true;
}

template<typename T>
bool chmpxlist_lap<T>::Find(int sock, bool is_to_first)
{
	if(CHM_INVALID_SOCK == sock){
		return false;
	}
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		//MSG_CHMPRN("PCHMPXLIST does not set.");
		return false;
	}
	if(is_to_first){
		ToFirst();
	}

	// find
	chmpxlap	curchmpx;
	for(st_ptr_type	cur = basic_type::pAbsPtr; cur; cur = CHM_ABS(basic_type::pShmBase, cur->next, st_ptr_type)){
		curchmpx.Reset(&cur->chmpx, abs_base_arr, abs_pend_arr, abs_sock_free_cnt, abs_sock_frees, basic_type::pShmBase);
		if(curchmpx.FindSock(sock)){
			// set current
			basic_type::pAbsPtr = cur;
			return true;
		}
	}
	return false;
}

//
// This method searches liner in list, so do not use this as possible as you can.
//
template<typename T>
bool chmpxlist_lap<T>::FindByHash(chmhash_t hash, bool is_base_hash, bool is_to_first)
{
	if(hash == CHM_INVALID_HASHVAL){
		return false;
	}
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXLIST does not set.");
		return false;
	}
	if(is_to_first){
		ToFirst();
	}

	// find
	chmpxlap	curchmpx;
	chmhash_t	basehash;
	chmhash_t	pendinghash;
	for(st_ptr_type	cur = basic_type::pAbsPtr; cur; cur = CHM_ABS(basic_type::pShmBase, cur->next, st_ptr_type)){
		curchmpx.Reset(&cur->chmpx, abs_base_arr, abs_pend_arr, abs_sock_free_cnt, abs_sock_frees, basic_type::pShmBase);
		basehash	= CHM_INVALID_HASHVAL;
		pendinghash	= CHM_INVALID_HASHVAL;
		if(curchmpx.Get(basehash, pendinghash)){
			if((is_base_hash && basehash == hash) || (!is_base_hash && pendinghash == hash)){
				// set current
				basic_type::pAbsPtr = cur;
				return true;
			}
		}
	}
	return false;
}

//
// Returns found first same status chmpxid in list.
//
// This method is linear search in list, then it is not good performance.
//
template<typename T>
chmpxid_t chmpxlist_lap<T>::FindByStatus(chmpxsts_t status, bool part_match, bool is_to_first)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXLIST does not set.");
		return CHM_INVALID_CHMPXID;
	}
	if(is_to_first){
		ToFirst();
	}

	chmpxlap	curchmpx;
	for(st_ptr_type	cur = basic_type::pAbsPtr; cur; cur = CHM_ABS(basic_type::pShmBase, cur->next, st_ptr_type)){
		curchmpx.Reset(&cur->chmpx, abs_base_arr, abs_pend_arr, abs_sock_free_cnt, abs_sock_frees, basic_type::pShmBase);
		if(part_match){
			if(status == (curchmpx.GetStatus() & status)){
				return curchmpx.GetChmpxId();
			}
		}else{
			if(status == curchmpx.GetStatus()){
				return curchmpx.GetChmpxId();
			}
		}
	}
	return CHM_INVALID_CHMPXID;
}

//
// This method is not used now.(don't use this because not good performance)
//
// This method calls BaseHashCount(), then it is not good performance.
// And rand() function is not good.
// Should use chmpxman_lap<T>::GetRandomServerChmpxId() instead of this.
//
// [NOTE]
// This method returns UP server node, it means UP with SUSPEND/PENDING/DOING/DONE
// status.
//
template<typename T>
chmpxid_t chmpxlist_lap<T>::GetRandomChmpxId(bool is_up_servers)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXLIST does not set.");
		return CHM_INVALID_CHMPXID;
	}
	ToFirst();

	// for random position.
	// Using rand(), and BaseHashCount() is all enable server count.
	//
	long	basecnt = BaseHashCount(false);
	if(0L >= basecnt){
		return CHM_INVALID_CHMPXID;
	}
	long		pos = static_cast<long>(rand()) % basecnt;		// random position.

	chmpxlap	curchmpx;
	for(; 0 < pos && basic_type::pAbsPtr->next; basic_type::pAbsPtr = CHM_ABS(basic_type::pShmBase, basic_type::pAbsPtr->next, st_ptr_type)){
		chmpxsts_t	status;

		curchmpx.Reset(&basic_type::pAbsPtr->chmpx, abs_base_arr, abs_pend_arr, abs_sock_free_cnt, abs_sock_frees, basic_type::pShmBase);
		status	= curchmpx.GetStatus();
		if(IS_CHMPXSTS_BASEHASH(status) && (!is_up_servers || IS_CHMPXSTS_UP(status))){
			--pos;
		}
	}
	curchmpx.Reset(&basic_type::pAbsPtr->chmpx, abs_base_arr, abs_pend_arr, abs_sock_free_cnt, abs_sock_frees, basic_type::pShmBase);
	return curchmpx.GetChmpxId();
}

//
// This method is not used now.(don't use this because not good performance)
//
// This method calls BaseHashCount(), then it is not good performance.
// Should use chmpxman_lap<T>::GetServerChmpxIdByHash() instead of this.
//
template<typename T>
chmpxid_t chmpxlist_lap<T>::GetChmpxIdByHash(chmhash_t hash)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXLIST does not set.");
		return CHM_INVALID_CHMPXID;
	}
	ToFirst();

	long	basecnt = BaseHashCount(false);
	if(0L >= basecnt){
		return CHM_INVALID_CHMPXID;
	}
	hash = hash % static_cast<chmhash_t>(basecnt);

	ToFirst();
	chmpxlap	curchmpx;
	chmhash_t	base;
	chmhash_t	pending;
	for(st_ptr_type	cur = basic_type::pAbsPtr; cur; cur = CHM_ABS(basic_type::pShmBase, cur->next, st_ptr_type)){
		curchmpx.Reset(&cur->chmpx, abs_base_arr, abs_pend_arr, abs_sock_free_cnt, abs_sock_frees, basic_type::pShmBase);
		if(curchmpx.Get(base, pending) && base == hash){
			return curchmpx.GetChmpxId();
		}
	}
	return CHM_INVALID_CHMPXID;
}

//
// This method is not used now.(don't use this because not good performance)
//
// [CAREFUL]
// After calling this method, be careful for pAbsPtr is NULL.
//
template<typename T>
typename chmpxlist_lap<T>::st_ptr_type chmpxlist_lap<T>::Retrieve(PCHMPX ptr, bool is_abs)
{
	if(!ptr){
		ERR_CHMPRN("ptr is null.");
		return NULL;
	}
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXLIST does not set.");
		return NULL;
	}
	ToFirst();

	if(!is_abs){
		ptr = CHM_ABS(basic_type::pShmBase, ptr, PCHMPX);
	}
	for(st_ptr_type cur = basic_type::pAbsPtr; cur; cur = CHM_ABS(basic_type::pShmBase, cur->next, st_ptr_type)){
		if(&(cur->chmpx) == ptr){
			// found
			if(cur == basic_type::pAbsPtr){
				// basic_type::pAbsPtr is first object in list.
				if(basic_type::pAbsPtr->next){
					basic_type::pAbsPtr = CHM_ABS(basic_type::pShmBase, basic_type::pAbsPtr->next, st_ptr_type);
				}else{
					basic_type::pAbsPtr = NULL;
				}
			}
			st_ptr_type	prevlist = CHM_ABS(basic_type::pShmBase, cur->prev, st_ptr_type);
			st_ptr_type	nextlist = CHM_ABS(basic_type::pShmBase, cur->next, st_ptr_type);
			if(prevlist){
				prevlist->next = cur->next;
			}
			if(nextlist){
				nextlist->prev = cur->prev;
			}
			cur->next = NULL;
			cur->prev = NULL;

			RetrieveChmpxIdMap(cur, true);		// basic_type::pAbsPtr->same
			return cur;
		}
	}
	WAN_CHMPRN("Could not find %p PCHMPX.", ptr);
	return NULL;
}

template<typename T>
typename chmpxlist_lap<T>::st_ptr_type chmpxlist_lap<T>::Retrieve(void)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXLIST does not set.");
		return NULL;
	}
	st_ptr_type	current	= basic_type::pAbsPtr;

	if(basic_type::pAbsPtr->next){
		basic_type::pAbsPtr = CHM_ABS(basic_type::pShmBase, basic_type::pAbsPtr->next, st_ptr_type);
	}else if(basic_type::pAbsPtr->prev){
		basic_type::pAbsPtr = CHM_ABS(basic_type::pShmBase, basic_type::pAbsPtr->prev, st_ptr_type);
	}else{
		basic_type::pAbsPtr = NULL;
	}

	st_ptr_type	prevlist= CHM_ABS(basic_type::pShmBase, current->prev, st_ptr_type);
	st_ptr_type	nextlist= CHM_ABS(basic_type::pShmBase, current->next, st_ptr_type);
	if(prevlist){
		prevlist->next	= current->next;
	}
	if(nextlist){
		nextlist->prev	= current->prev;
	}
	current->next = NULL;
	current->prev = NULL;

	// Following lines are as same as RetrieveChmpxIdMap method
	chmpxlap	tmpchmpx(&current->chmpx, abs_base_arr, abs_pend_arr, abs_sock_free_cnt, abs_sock_frees, basic_type::pShmBase);
	chmpxid_t	curchmpxid = tmpchmpx.GetChmpxId();
	if(CHM_INVALID_CHMPXID != curchmpxid && pchmpxid_map){
		st_ptr_type*	ppabsmap;
		st_ptr_type		pabsmap;
		for(ppabsmap = &pchmpxid_map[curchmpxid & CHMPXID_MAP_MASK], pabsmap = CHM_ABS(basic_type::pShmBase, pchmpxid_map[curchmpxid & CHMPXID_MAP_MASK], st_ptr_type);
			pabsmap;
			ppabsmap = &pabsmap->same, pabsmap = CHM_ABS(basic_type::pShmBase, pabsmap->same, st_ptr_type))
		{
			if(pabsmap == current){
				*ppabsmap = current->same;
				break;
			}
		}
	}else{
		WAN_CHMPRN("pchmpxid_map is NULL, or current chmpxid is invalid.");
	}
	current->same = NULL;

	return current;
}

typedef	chmpxlist_lap<CHMPXLIST>	chmpxlistlap;

//---------------------------------------------------------
// For CHMSTAT
//---------------------------------------------------------
template<typename T>
class chmstat_lap : public structure_lap<T>
{
	public:
		typedef T					st_type;
		typedef T*					st_ptr_type;
		typedef structure_lap<T>	basic_type;

	public:
		explicit chmstat_lap(st_ptr_type ptr = NULL, const void* shmbase = NULL, bool is_abs = true);

		virtual bool Initialize(void);
		virtual bool Dump(std::stringstream& sstream, const char* spacer) const;
		virtual bool Clear(void);
		bool Copy(st_ptr_type ptr) const;

		bool Add(bool is_sent, size_t length, const struct timespec& elapsed_time);
		bool Get(st_ptr_type pstat) const;
};

template<typename T>
chmstat_lap<T>::chmstat_lap(st_ptr_type ptr, const void* shmbase, bool is_abs)
{
	structure_lap<T>::Reset(ptr, shmbase, is_abs);
}

template<typename T>
bool chmstat_lap<T>::Initialize(void)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMSTAT does not set.");
		return false;
	}
	basic_type::pAbsPtr->total_sent_count		= 0L;
	basic_type::pAbsPtr->total_received_count	= 0L;
	basic_type::pAbsPtr->total_body_bytes		= 0L;
	basic_type::pAbsPtr->min_body_bytes			= 0L;
	basic_type::pAbsPtr->max_body_bytes			= 0L;
	INIT_TIMESPEC(&basic_type::pAbsPtr->total_elapsed_time);
	INIT_TIMESPEC(&basic_type::pAbsPtr->min_elapsed_time);
	INIT_TIMESPEC(&basic_type::pAbsPtr->max_elapsed_time);

	return true;
}

template<typename T>
bool chmstat_lap<T>::Dump(std::stringstream& sstream, const char* spacer) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMSTAT does not set.");
		return false;
	}
	sstream << (spacer ? spacer : "") << "total_sent_count     = "	<< 	basic_type::pAbsPtr->total_sent_count			<< std::endl;
	sstream << (spacer ? spacer : "") << "total_received_count = "	<< 	basic_type::pAbsPtr->total_received_count		<< std::endl;
	sstream << (spacer ? spacer : "") << "total_body_bytes     = "	<< 	basic_type::pAbsPtr->total_body_bytes			<< std::endl;
	sstream << (spacer ? spacer : "") << "min_body_bytes       = "	<< 	basic_type::pAbsPtr->min_body_bytes				<< std::endl;
	sstream << (spacer ? spacer : "") << "max_body_bytes       = "	<< 	basic_type::pAbsPtr->max_body_bytes				<< std::endl;
	sstream << (spacer ? spacer : "") << "total_elapsed_time   = "	<< 	basic_type::pAbsPtr->total_elapsed_time.tv_sec	<< "s " << basic_type::pAbsPtr->total_elapsed_time.tv_nsec	<< "ns" << std::endl;
	sstream << (spacer ? spacer : "") << "min_elapsed_time     = "	<< 	basic_type::pAbsPtr->min_elapsed_time.tv_sec	<< "s " << basic_type::pAbsPtr->min_elapsed_time.tv_nsec	<< "ns" << std::endl;
	sstream << (spacer ? spacer : "") << "max_elapsed_time     = "	<< 	basic_type::pAbsPtr->max_elapsed_time.tv_sec	<< "s " << basic_type::pAbsPtr->max_elapsed_time.tv_nsec	<< "ns" << std::endl;

	return true;
}

template<typename T>
bool chmstat_lap<T>::Copy(st_ptr_type ptr) const
{
	if(!ptr){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMSTAT does not set.");
		return false;
	}
	ptr->total_sent_count			= basic_type::pAbsPtr->total_sent_count;
	ptr->total_received_count		= basic_type::pAbsPtr->total_received_count;
	ptr->total_body_bytes			= basic_type::pAbsPtr->total_body_bytes;
	ptr->min_body_bytes				= basic_type::pAbsPtr->min_body_bytes;
	ptr->max_body_bytes				= basic_type::pAbsPtr->max_body_bytes;
	ptr->total_elapsed_time.tv_sec	= basic_type::pAbsPtr->total_elapsed_time.tv_sec;
	ptr->total_elapsed_time.tv_nsec	= basic_type::pAbsPtr->total_elapsed_time.tv_nsec;
	ptr->min_elapsed_time.tv_sec	= basic_type::pAbsPtr->min_elapsed_time.tv_sec;
	ptr->min_elapsed_time.tv_nsec	= basic_type::pAbsPtr->min_elapsed_time.tv_nsec;
	ptr->max_elapsed_time.tv_sec	= basic_type::pAbsPtr->max_elapsed_time.tv_sec;
	ptr->max_elapsed_time.tv_nsec	= basic_type::pAbsPtr->max_elapsed_time.tv_nsec;

	return true;
}

template<typename T>
bool chmstat_lap<T>::Clear(void)
{
	return Initialize();
}

template<typename T>
bool chmstat_lap<T>::Add(bool is_sent, size_t length, const struct timespec& elapsed_time)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMSTAT does not set.");
		return false;
	}

	if(is_sent){
		basic_type::pAbsPtr->total_sent_count++;
	}else{
		basic_type::pAbsPtr->total_received_count++;
	}

	if(0 == basic_type::pAbsPtr->min_body_bytes || length < basic_type::pAbsPtr->min_body_bytes){
		basic_type::pAbsPtr->min_body_bytes = length;
	}
	if(basic_type::pAbsPtr->max_body_bytes < length){
		basic_type::pAbsPtr->max_body_bytes = length;
	}
	basic_type::pAbsPtr->total_body_bytes += length;

	if((0 == basic_type::pAbsPtr->min_elapsed_time.tv_sec && 0 == basic_type::pAbsPtr->min_elapsed_time.tv_nsec) || 0 > COMPARE_TIMESPEC(&(basic_type::pAbsPtr->min_elapsed_time), &elapsed_time)){
		COPY_TIMESPEC(&(basic_type::pAbsPtr->min_elapsed_time), &elapsed_time);
	}
	if(0 < COMPARE_TIMESPEC(&(basic_type::pAbsPtr->max_elapsed_time), &elapsed_time)){
		COPY_TIMESPEC(&(basic_type::pAbsPtr->max_elapsed_time), &elapsed_time);
	}
	ADD_TIMESPEC(&(basic_type::pAbsPtr->total_elapsed_time), &elapsed_time);

	return true;
}

template<typename T>
bool chmstat_lap<T>::Get(st_ptr_type pstat) const
{
	if(!pstat){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMSTAT does not set.");
		return false;
	}

	pstat->total_sent_count		= basic_type::pAbsPtr->total_sent_count;
	pstat->total_received_count	= basic_type::pAbsPtr->total_received_count;
	pstat->total_body_bytes		= basic_type::pAbsPtr->total_body_bytes;
	pstat->min_body_bytes		= basic_type::pAbsPtr->min_body_bytes;
	pstat->max_body_bytes		= basic_type::pAbsPtr->max_body_bytes;
	COPY_TIMESPEC(&(pstat->total_elapsed_time),	&(basic_type::pAbsPtr->total_elapsed_time));
	COPY_TIMESPEC(&(pstat->min_elapsed_time),	&(basic_type::pAbsPtr->min_elapsed_time));
	COPY_TIMESPEC(&(pstat->max_elapsed_time),	&(basic_type::pAbsPtr->max_elapsed_time));

	return true;
}

typedef	chmstat_lap<CHMSTAT>	chmstatlap;

//---------------------------------------------------------
// For MQMSGHEAD
//---------------------------------------------------------
template<typename T>
class mqmsghead_lap : public structure_lap<T>
{
	public:
		typedef T					st_type;
		typedef T*					st_ptr_type;
		typedef structure_lap<T>	basic_type;

	public:
		explicit mqmsghead_lap(st_ptr_type ptr = NULL, const void* shmbase = NULL, bool is_abs = true);

		virtual bool Initialize(void);
		virtual bool Dump(std::stringstream& sstream, const char* spacer) const;
		virtual bool Clear(void);
		bool Copy(st_ptr_type ptr) const;

		msgid_t GetMsgId(void) const { return (basic_type::pAbsPtr ? basic_type::pAbsPtr->msgid : CHM_INVALID_MSGID); }
		pid_t GetPid(void) const { return (basic_type::pAbsPtr ? basic_type::pAbsPtr->pid : CHM_INVALID_PID); }

		bool IsChmpxProc(void) const { return (basic_type::pAbsPtr ? IS_MQFLAG_CHMPXPROC(basic_type::pAbsPtr->flag) : false); }
		bool IsClientProc(void) const { return (basic_type::pAbsPtr ? IS_MQFLAG_CLIENTPROC(basic_type::pAbsPtr->flag) : false); }
		bool IsAssigned(void) const { return (basic_type::pAbsPtr ? IS_MQFLAG_ASSIGNED(basic_type::pAbsPtr->flag) : false); }
		bool IsNotAssigned(void) const { return (basic_type::pAbsPtr ? IS_MQFLAG_NOTASSIGNED(basic_type::pAbsPtr->flag) : true); }
		bool IsActivated(void) const { return (basic_type::pAbsPtr ? IS_MQFLAG_ACTIVATED(basic_type::pAbsPtr->flag) : false); }
		bool IsDisactivated(void) const { return (basic_type::pAbsPtr ? IS_MQFLAG_DISACTIVATED(basic_type::pAbsPtr->flag) : true); }

		bool NotAccountMqFlag(void);
		bool InitializeMqFlag(bool is_chmpxproc, bool is_activated);
		bool SetMqFlagStatus(bool is_assigned, bool is_activated);
};

template<typename T>
mqmsghead_lap<T>::mqmsghead_lap(st_ptr_type ptr, const void* shmbase, bool is_abs)
{
	structure_lap<T>::Reset(ptr, shmbase, is_abs);
}

template<typename T>
bool mqmsghead_lap<T>::Initialize(void)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PMQMSGHEAD does not set.");
		return false;
	}
	basic_type::pAbsPtr->msgid	= CHM_INVALID_MSGID;
	basic_type::pAbsPtr->pid	= CHM_INVALID_PID;
	NotAccountMqFlag();

	return true;
}

template<typename T>
bool mqmsghead_lap<T>::Clear(void)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PMQMSGHEAD does not set.");
		return false;
	}
	NotAccountMqFlag();

	return true;
}

template<typename T>
bool mqmsghead_lap<T>::Dump(std::stringstream& sstream, const char* spacer) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PMQMSGHEAD does not set.");
		return false;
	}
	sstream << (spacer ? spacer : "") << "msgid  = " << reinterpret_cast<void*>(basic_type::pAbsPtr->msgid)	<< std::endl;
	sstream << (spacer ? spacer : "") << "pid    = " << basic_type::pAbsPtr->pid							<< std::endl;
	sstream << (spacer ? spacer : "") << "flag   = " << reinterpret_cast<void*>(basic_type::pAbsPtr->flag)	<< " - " << STR_MQFLAG_FULL(basic_type::pAbsPtr->flag) << std::endl;

	return true;
}

template<typename T>
bool mqmsghead_lap<T>::Copy(st_ptr_type ptr) const
{
	if(!ptr){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PMQMSGHEAD does not set.");
		return false;
	}
	ptr->msgid	= basic_type::pAbsPtr->msgid;
	ptr->flag	= basic_type::pAbsPtr->flag;
	ptr->pid	= basic_type::pAbsPtr->pid;

	return true;
}

template<typename T>
bool mqmsghead_lap<T>::NotAccountMqFlag(void)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PMQMSGHEAD does not set.");
		return false;
	}
	SET_MQFLAG_INITIALIZED(basic_type::pAbsPtr->flag);
	basic_type::pAbsPtr->pid = CHM_INVALID_PID;
	return true;
}

template<typename T>
bool mqmsghead_lap<T>::InitializeMqFlag(bool is_chmpxproc, bool is_activated)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PMQMSGHEAD does not set.");
		return false;
	}

	if(is_chmpxproc){
		if(IS_MQFLAG_CLIENTPROC(basic_type::pAbsPtr->flag)){
			WAN_CHMPRN("msgid(0x%016" PRIx64 ") is client process used, but force to set.", basic_type::pAbsPtr->msgid);
		}
		SET_MQFLAG_CHMPXPROC(basic_type::pAbsPtr->flag);
		is_activated = true;		// force
	}else{
		if(IS_MQFLAG_CHMPXPROC(basic_type::pAbsPtr->flag)){
			WAN_CHMPRN("msgid(0x%016" PRIx64 ") is chmpx process used, but force to set", basic_type::pAbsPtr->msgid);
		}
		SET_MQFLAG_CLIENTPROC(basic_type::pAbsPtr->flag);
	}

	if(is_activated){
		SET_MQFLAG_ACTIVATED(basic_type::pAbsPtr->flag);
	}else{
		SET_MQFLAG_DISACTIVATED(basic_type::pAbsPtr->flag);
	}

	basic_type::pAbsPtr->pid = getpid();

	return true;
}

template<typename T>
bool mqmsghead_lap<T>::SetMqFlagStatus(bool is_assigned, bool is_activated)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PMQMSGHEAD does not set.");
		return false;
	}

	if(IS_MQFLAG_CHMPXPROC(basic_type::pAbsPtr->flag)){
		if(is_assigned && !is_activated){
			WAN_CHMPRN("msgid(0x%016" PRIx64 ") is used by chmpx process, but specified assigned and disactivated, so force activated.", basic_type::pAbsPtr->msgid);
			is_activated = true;
		}
	}else if(!IS_MQFLAG_CLIENTPROC(basic_type::pAbsPtr->flag)){
		ERR_CHMPRN("msgid(0x%016" PRIx64 ") kind is not initialized.", basic_type::pAbsPtr->msgid);
		return false;
	}

	if(!is_assigned){
		if(is_activated){
			WAN_CHMPRN("Specified activated & not assigned, so parameter is wrong, force set not assigned(disactivated).");
		}

		if(IS_MQFLAG_NOTASSIGNED(basic_type::pAbsPtr->flag) && IS_MQFLAG_DISACTIVATED(basic_type::pAbsPtr->flag)){
			MSG_CHMPRN("MQ flag=%s is already not assigned.", STR_MQFLAG_FULL(basic_type::pAbsPtr->flag).c_str());
		}else{
			SET_MQFLAG_NOTASSIGNED(basic_type::pAbsPtr->flag);
		}
		basic_type::pAbsPtr->pid = CHM_INVALID_PID;

	}else if(!is_activated){	// is_assigned
		if(IS_MQFLAG_ASSIGNED(basic_type::pAbsPtr->flag) && IS_MQFLAG_DISACTIVATED(basic_type::pAbsPtr->flag)){
			MSG_CHMPRN("MQ flag=%s is already assigned & disactivated.", STR_MQFLAG_FULL(basic_type::pAbsPtr->flag).c_str());
		}else{
			SET_MQFLAG_DISACTIVATED(basic_type::pAbsPtr->flag);
		}
		basic_type::pAbsPtr->pid = getpid();

	}else{						// is_assigned && is_activated
		if(IS_MQFLAG_ASSIGNED(basic_type::pAbsPtr->flag) && IS_MQFLAG_ACTIVATED(basic_type::pAbsPtr->flag)){
			MSG_CHMPRN("MQ flag=%s is already assigned & activated.", STR_MQFLAG_FULL(basic_type::pAbsPtr->flag).c_str());
		}else{
			SET_MQFLAG_ACTIVATED(basic_type::pAbsPtr->flag);
		}
		basic_type::pAbsPtr->pid = getpid();
	}
	return true;
}

typedef	mqmsghead_lap<MQMSGHEAD>	mqmsgheadlap;

//---------------------------------------------------------
// For MQMSGHEADLIST(Array)
//---------------------------------------------------------
template<typename T>
class mqmsgheadarr_lap : public structure_lap<T>
{
	public:
		typedef T					st_type;
		typedef T*					st_ptr_type;
		typedef structure_lap<T>	basic_type;

	private:
		long						chmpxmsg_count;

	public:
		explicit mqmsgheadarr_lap(st_ptr_type ptr = NULL, const void* shmbase = NULL, long max_msg_count = 0L, bool is_abs = true);

		bool FillMsgId(msgid_t base_msgid);
		long Count(void) const { return chmpxmsg_count; }
		st_ptr_type Find(msgid_t msgid, bool is_abs = true);
};

template<typename T>
mqmsgheadarr_lap<T>::mqmsgheadarr_lap(st_ptr_type ptr, const void* shmbase, long max_msg_count, bool is_abs) : chmpxmsg_count(max_msg_count)
{
	structure_lap<T>::Reset(ptr, shmbase, is_abs);
}

template<typename T>
bool mqmsgheadarr_lap<T>::FillMsgId(msgid_t base_msgid)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PMQMSGHEADLIST does not set.");
		return false;
	}
	for(long cnt = 0L; cnt < chmpxmsg_count; cnt++){
		(basic_type::pAbsPtr)[cnt].msghead.msgid = base_msgid++;
	}
	return true;
}

template<typename T>
typename mqmsgheadarr_lap<T>::st_ptr_type mqmsgheadarr_lap<T>::Find(msgid_t msgid, bool is_abs)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PMQMSGHEADLIST does not set.");
		return NULL;
	}
	if(msgid < basic_type::pAbsPtr->msghead.msgid || (basic_type::pAbsPtr->msghead.msgid + chmpxmsg_count) <= msgid){
		ERR_CHMPRN("msgid(0x%016" PRIx64 ") is over range(0x%016" PRIx64 " - 0x%016" PRIx64 ").", msgid, basic_type::pAbsPtr->msghead.msgid, (basic_type::pAbsPtr->msghead.msgid + chmpxmsg_count));
		return NULL;
	}
	st_ptr_type	resultptr = &(basic_type::pAbsPtr[msgid - basic_type::pAbsPtr->msghead.msgid]);

	if(resultptr->msghead.msgid != msgid){
		ERR_CHMPRN("%p is found in msghead area for msgid(0x%016" PRIx64 "), but msgid(0x%016" PRIx64 ") is not same msgid.", resultptr, msgid, resultptr->msghead.msgid);
		return NULL;
	}
	if(!is_abs){
		resultptr = CHM_REL(basic_type::pShmBase, resultptr, st_ptr_type);
	}
	return resultptr;
}

typedef	mqmsgheadarr_lap<MQMSGHEADLIST>	mqmsgheadarrlap;

//---------------------------------------------------------
// For MQMSGHEADLIST
//---------------------------------------------------------
template<typename T>
class mqmsgheadlist_lap : public structure_lap<T>
{
	public:
		typedef T					st_type;
		typedef T*					st_ptr_type;
		typedef structure_lap<T>	basic_type;

	public:
		explicit mqmsgheadlist_lap(st_ptr_type ptr = NULL, const void* shmbase = NULL, bool is_abs = true);

		PMQMSGHEAD GetAbsMqMsgHeadPtr(void) const { return (basic_type::pAbsPtr ? &basic_type::pAbsPtr->msghead : NULL); }
		// cppcheck-suppress clarifyCondition
		PMQMSGHEAD GetRelMqMsgHeadPtr(void) const { return (basic_type::pAbsPtr ? CHM_REL(basic_type::pShmBase, &basic_type::pAbsPtr->msghead, PMQMSGHEAD) : NULL); }

		virtual bool Initialize(void);
		bool Initialize(st_ptr_type prev, st_ptr_type next, bool is_abs = true);
		bool Clear(void);
		virtual bool Dump(std::stringstream& sstream, const char* spacer) const;
		st_ptr_type Dup(void);
		void Free(st_ptr_type ptr) const;

		st_ptr_type GetFirstPtr(bool is_abs = true);
		bool ToFirst(void);
		bool ToLast(void);
		bool ToNext(bool is_cycle = false);
		long Count(void);
		long Count(bool toward_normal);
		bool Push(st_ptr_type ptr, bool is_abs, bool is_to_first = false);
		bool PushBack(st_ptr_type ptr, bool is_abs);
		st_ptr_type PopFront(void);
		st_ptr_type Retrieve(msgid_t msgid);
		st_ptr_type Retrieve(void);

		PMQMSGHEAD Find(msgid_t msgid, bool is_abs = true, bool is_to_first = false);
		msgid_t GetRandomMsgId(void);
		bool GetMsgidListByPid(pid_t pid, msgidlist_t& list, bool is_clear_list = false);
		bool GetMsgidListByUniqPid(msgidlist_t& list, bool is_clear_list = false);
};

template<typename T>
mqmsgheadlist_lap<T>::mqmsgheadlist_lap(st_ptr_type ptr, const void* shmbase, bool is_abs)
{
	structure_lap<T>::Reset(ptr, shmbase, is_abs);
}

template<typename T>
bool mqmsgheadlist_lap<T>::Initialize(void)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PMQMSGHEADLIST does not set.");
		return false;
	}
	basic_type::pAbsPtr->prev	= NULL;
	basic_type::pAbsPtr->next	= NULL;

	mqmsgheadlap	tmpmqmsghead(&basic_type::pAbsPtr->msghead, basic_type::pShmBase);
	if(!tmpmqmsghead.Initialize()){
		ERR_CHMPRN("Failed to initialize CHMMQMSGHEAD.");
		return false;
	}
	return true;
}

template<typename T>
bool mqmsgheadlist_lap<T>::Initialize(st_ptr_type prev, st_ptr_type next, bool is_abs)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PMQMSGHEADLIST does not set.");
		return false;
	}
	basic_type::pAbsPtr->prev	= is_abs ? CHM_REL(basic_type::pShmBase, prev, st_ptr_type) : prev;
	basic_type::pAbsPtr->next	= is_abs ? CHM_REL(basic_type::pShmBase, next, st_ptr_type) : next;

	mqmsgheadlap	tmpmqmsghead(&basic_type::pAbsPtr->msghead, basic_type::pShmBase);
	if(!tmpmqmsghead.Initialize()){
		ERR_CHMPRN("Failed to initialize CHMMQMSGHEAD.");
		return false;
	}
	return true;
}

template<typename T>
bool mqmsgheadlist_lap<T>::Clear(void)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PMQMSGHEADLIST does not set.");
		return false;
	}
	basic_type::pAbsPtr->prev = NULL;
	basic_type::pAbsPtr->next = NULL;

	mqmsgheadlap	tmpmqmsghead(&basic_type::pAbsPtr->msghead, basic_type::pShmBase);
	if(!tmpmqmsghead.Clear()){
		ERR_CHMPRN("Failed to clear CHMMQMSGHEAD.");
		return false;
	}
	return true;
}

template<typename T>
bool mqmsgheadlist_lap<T>::Dump(std::stringstream& sstream, const char* spacer) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PMQMSGHEADLIST does not set.");
		return false;
	}
	std::string	tmpspacer1 = spacer ? spacer : "";
	std::string	tmpspacer2;
	tmpspacer1 += "  ";
	tmpspacer2 += tmpspacer1 + "  ";

	// To first
	st_ptr_type	cur;
	for(cur = const_cast<st_ptr_type>(basic_type::pAbsPtr); cur->prev; cur = CHM_ABS(basic_type::pShmBase, cur->prev, st_ptr_type));

	for(long count = 0L; cur; cur = CHM_ABS(basic_type::pShmBase, cur->next, st_ptr_type), count++){
		sstream << (spacer ? spacer : "") << "[" << count << "] = {"		<< std::endl;

		sstream << tmpspacer1 << "prev = "	<< cur->prev	<< std::endl;
		sstream << tmpspacer1 << "next = "	<< cur->next	<< std::endl;

		sstream << tmpspacer1 << "msghead{"	<< std::endl;
		mqmsgheadlap	tmpmqmsghead(&cur->msghead, basic_type::pShmBase);
		tmpmqmsghead.Dump(sstream, tmpspacer2.c_str());
		sstream << tmpspacer1 << "}"		<< std::endl;

		sstream << (spacer ? spacer : "") << "}"		<< std::endl;
	}
	return true;
}

template<typename T>
typename mqmsgheadlist_lap<T>::st_ptr_type mqmsgheadlist_lap<T>::Dup(void)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PMQMSGHEADLIST does not set.");
		return NULL;
	}

	long	count = Count();
	if(0 == count){
		return NULL;
	}

	// To first
	st_ptr_type	cur;
	for(cur = const_cast<st_ptr_type>(basic_type::pAbsPtr); cur->prev; cur = CHM_ABS(basic_type::pShmBase, cur->prev, st_ptr_type));

	// duplicate
	st_ptr_type	pdst;
	if(NULL == (pdst = reinterpret_cast<st_ptr_type>(calloc(count, sizeof(st_type))))){
		ERR_CHMPRN("Could not allocation memory.");
		return NULL;
	}
	for(st_ptr_type pdstcur = pdst, pdstprev = NULL; cur; cur = CHM_ABS(basic_type::pShmBase, cur->next, st_ptr_type), pdstprev = pdstcur++){
		if(pdstprev){
			pdstprev->next = pdstcur;
		}
		pdstcur->prev = pdstprev;

		mqmsgheadlap	tmpmqmsghead(&cur->msghead, basic_type::pShmBase);
		if(!tmpmqmsghead.Copy(&(pdstcur->msghead))){
			WAN_CHMPRN("Failed to copy msghead structure, but continue...");
		}
	}
	return pdst;
}

template<typename T>
void mqmsgheadlist_lap<T>::Free(st_ptr_type ptr) const
{
	if(ptr){
		K2H_Free(ptr);
	}
}

template<typename T>
typename mqmsgheadlist_lap<T>::st_ptr_type mqmsgheadlist_lap<T>::GetFirstPtr(bool is_abs)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		//MSG_CHMPRN("PMQMSGHEADLIST does not set.");
		return NULL;
	}
	if(basic_type::pAbsPtr->prev){
		ToFirst();
	}
	return (is_abs ? mqmsgheadlist_lap<T>::GetAbsPtr() : mqmsgheadlist_lap<T>::GetRelPtr());

}

template<typename T>
bool mqmsgheadlist_lap<T>::ToFirst(void)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PMQMSGHEADLIST does not set.");
		return false;
	}
	for(; basic_type::pAbsPtr->prev; basic_type::pAbsPtr = CHM_ABS(basic_type::pShmBase, basic_type::pAbsPtr->prev, st_ptr_type));
	return true;
}

template<typename T>
bool mqmsgheadlist_lap<T>::ToLast(void)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PMQMSGHEADLIST does not set.");
		return false;
	}
	for(; basic_type::pAbsPtr->next; basic_type::pAbsPtr = CHM_ABS(basic_type::pShmBase, basic_type::pAbsPtr->next, st_ptr_type));
	return true;
}

template<typename T>
bool mqmsgheadlist_lap<T>::ToNext(bool is_cycle)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PMQMSGHEADLIST does not set.");
		return false;
	}
	if(!basic_type::pAbsPtr->next){
		if(!is_cycle){
			MSG_CHMPRN("Reached end of msg list.");
			return false;
		}
		ToFirst();
	}else{
		basic_type::pAbsPtr = CHM_ABS(basic_type::pShmBase, basic_type::pAbsPtr->next, st_ptr_type);
	}
	return true;
}

template<typename T>
long mqmsgheadlist_lap<T>::Count(void)
{
	if(!basic_type::pShmBase){
		ERR_CHMPRN("PMQMSGHEADLIST does not set.");
		return 0L;
	}
	if(!basic_type::pAbsPtr){
		//MSG_CHMPRN("PMQMSGHEADLIST does not set.");
		return 0L;
	}
	ToFirst();
	long		count;
	st_ptr_type pos;
	for(count = 1L, pos = basic_type::pAbsPtr; pos->next; pos = CHM_ABS(basic_type::pShmBase, pos->next, st_ptr_type), count++);

	return count;
}

template<typename T>
long mqmsgheadlist_lap<T>::Count(bool toward_normal)
{
	if(!basic_type::pShmBase){
		ERR_CHMPRN("PMQMSGHEADLIST does not set.");
		return 0L;
	}
	if(!basic_type::pAbsPtr){
		//MSG_CHMPRN("PMQMSGHEADLIST does not set.");
		return 0L;
	}
	long		count;
	st_ptr_type pos;
	for(count = 1L, pos = basic_type::pAbsPtr; NULL != (toward_normal ? pos->next : pos->prev); pos = CHM_ABS(basic_type::pShmBase, (toward_normal ? pos->next : pos->prev), st_ptr_type), count++);

	return count;
}

template<typename T>
bool mqmsgheadlist_lap<T>::Push(st_ptr_type ptr, bool is_abs, bool is_to_first)
{
	if(!ptr){
		ERR_CHMPRN("ptr is null.");
		return false;
	}
	if(!basic_type::pShmBase){					// allow basic_type::pAbsPtr is NULL
		ERR_CHMPRN("PMQMSGHEADLIST does not set.");
		return false;
	}

	st_ptr_type	current = basic_type::pAbsPtr;
	if(is_to_first){
		for(; current && current->prev; current = CHM_ABS(basic_type::pShmBase, current->prev, st_ptr_type));
	}

	st_ptr_type	relptr	= is_abs ? CHM_REL(basic_type::pShmBase, ptr, st_ptr_type) : ptr;
	st_ptr_type	absptr	= is_abs ? ptr : CHM_ABS(basic_type::pShmBase, ptr, st_ptr_type);
	st_ptr_type	prevptr	= current ? CHM_ABS(basic_type::pShmBase, current->prev, st_ptr_type) : NULL;

	if(prevptr){
		prevptr->next	= relptr;
	}
	absptr->prev		= current ? current->prev : NULL;
	absptr->next		= current ? CHM_REL(basic_type::pShmBase, current, st_ptr_type) : NULL;
	if(current){
		current->prev	= relptr;
	}
	basic_type::pAbsPtr = absptr;

	return true;
}

template<typename T>
bool mqmsgheadlist_lap<T>::PushBack(st_ptr_type ptr, bool is_abs)
{
	if(!ptr){
		ERR_CHMPRN("ptr is null.");
		return false;
	}
	if(!basic_type::pShmBase){					// allow basic_type::pAbsPtr is NULL
		ERR_CHMPRN("PMQMSGHEADLIST does not set.");
		return false;
	}
	// find list last
	st_ptr_type	last;
	for(last = basic_type::pAbsPtr; last && last->next; last = CHM_ABS(basic_type::pShmBase, last->next, st_ptr_type));

	st_ptr_type	relptr	= is_abs ? CHM_REL(basic_type::pShmBase, ptr, st_ptr_type) : ptr;
	st_ptr_type	absptr	= is_abs ? ptr : CHM_ABS(basic_type::pShmBase, ptr, st_ptr_type);

	if(last){
		last->next		= relptr;
	}
	absptr->prev		= last ? CHM_REL(basic_type::pShmBase, last, st_ptr_type) : NULL;
	absptr->next		= NULL;
	basic_type::pAbsPtr	= absptr;

	return true;
}

template<typename T>
typename mqmsgheadlist_lap<T>::st_ptr_type mqmsgheadlist_lap<T>::PopFront(void)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PMQMSGHEADLIST does not set.");
		return NULL;
	}
	// find list first
	st_ptr_type	first;
	for(first = basic_type::pAbsPtr; first->prev; first = CHM_ABS(basic_type::pShmBase, first->prev, st_ptr_type));

	if(NULL != (basic_type::pAbsPtr = CHM_ABS(basic_type::pShmBase, first->next, st_ptr_type))){
		basic_type::pAbsPtr->prev = NULL;
	}
	first->next = NULL;

	return first;		// Absolute
}

//
// [CAREFUL]
// After calling this method, be careful for pAbsPtr is NULL.
// And this method is very slow, should use Retrieve(void)
//
template<typename T>
typename mqmsgheadlist_lap<T>::st_ptr_type mqmsgheadlist_lap<T>::Retrieve(msgid_t msgid)
{
	if(CHM_INVALID_MSGID == msgid){
		ERR_CHMPRN("Parameter is wrong.");
		return NULL;
	}
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PMQMSGHEADLIST does not set.");
		return NULL;
	}
	ToFirst();

	mqmsgheadlap	mqmsghead;
	for(st_ptr_type cur = basic_type::pAbsPtr; cur; cur = CHM_ABS(basic_type::pShmBase, cur->next, st_ptr_type)){
		mqmsghead.Reset(&cur->msghead, basic_type::pShmBase);
		if(msgid == mqmsghead.GetMsgId()){
			// found
			if(cur == basic_type::pAbsPtr){
				// basic_type::pAbsPtr is first object in list.
				if(basic_type::pAbsPtr->next){
					basic_type::pAbsPtr = CHM_ABS(basic_type::pShmBase, basic_type::pAbsPtr->next, st_ptr_type);
				}else{
					basic_type::pAbsPtr = NULL;
				}
			}
			st_ptr_type	prevlist = CHM_ABS(basic_type::pShmBase, cur->prev, st_ptr_type);
			st_ptr_type	nextlist = CHM_ABS(basic_type::pShmBase, cur->next, st_ptr_type);
			if(prevlist){
				prevlist->next = cur->next;
			}
			if(nextlist){
				nextlist->prev = cur->prev;
			}
			cur->prev = NULL;
			cur->next = NULL;
			return cur;
		}
	}
	WAN_CHMPRN("Could not find msgid(x%016" PRIx64 ").", msgid);
	return NULL;
}

//
// [CAREFUL]
// After calling this method, be careful for pAbsPtr is NULL.
//
template<typename T>
typename mqmsgheadlist_lap<T>::st_ptr_type mqmsgheadlist_lap<T>::Retrieve(void)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PMQMSGHEADLIST does not set.");
		return NULL;
	}

	st_ptr_type	abs_cur  = basic_type::pAbsPtr;
	st_ptr_type	abs_prev = CHM_ABS(basic_type::pShmBase, basic_type::pAbsPtr->prev, st_ptr_type);
	st_ptr_type	abs_next = CHM_ABS(basic_type::pShmBase, basic_type::pAbsPtr->next, st_ptr_type);

	if(abs_prev){
		abs_prev->next			= basic_type::pAbsPtr->next;
		if(abs_next){
			abs_next->prev		= basic_type::pAbsPtr->prev;
			basic_type::pAbsPtr	= abs_next;
		}else{
			basic_type::pAbsPtr	= abs_prev;
		}
	}else{
		if(abs_next){
			abs_next->prev		= basic_type::pAbsPtr->prev;
			basic_type::pAbsPtr	= abs_next;
		}else{
			basic_type::pAbsPtr	= NULL;
		}
	}
	abs_cur->prev = NULL;
	abs_cur->next = NULL;
	return abs_cur;
}

//
// This method is not good performance.
// Should use mqmsgheadarr_lap<T>::Find() instead of this.
//
template<typename T>
PMQMSGHEAD mqmsgheadlist_lap<T>::Find(msgid_t msgid, bool is_abs, bool is_to_first)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PMQMSGHEADLIST does not set.");
		return NULL;
	}
	if(is_to_first){
		ToFirst();
	}

	for(st_ptr_type cur = basic_type::pAbsPtr; cur; cur = CHM_ABS(basic_type::pShmBase, cur->next, st_ptr_type)){
		mqmsgheadlap	tmpmqmsghead(&cur->msghead, basic_type::pShmBase);
		if(msgid == tmpmqmsghead.GetMsgId()){
			// cppcheck-suppress clarifyCondition
			return (is_abs ? &cur->msghead : CHM_REL(basic_type::pShmBase, &cur->msghead, PMQMSGHEAD));
		}
	}
	return NULL;
}

//
// This method calls Count() and rand(), then it is not good performance.
// Should use chminfo_lap<T>::GetRandomMsgId() instead of this.
//
template<typename T>
msgid_t mqmsgheadlist_lap<T>::GetRandomMsgId(void)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PMQMSGHEADLIST does not set.");
		return CHM_INVALID_MSGID;
	}

	// for random position.
	// using rand() which calls random() for linux, so thread-safe.(maybe)
	// If thread unsafe, but we don't care for it.:-p
	//
	long	msgidcnt = Count();
	if(0L >= msgidcnt){
		return CHM_INVALID_MSGID;
	}
	long	randcnt = static_cast<long>(rand()) % msgidcnt;		// random position.

	ToFirst();

	mqmsgheadlap	mqmsghead;
	st_ptr_type 	pos;
	for(pos = basic_type::pAbsPtr; pos; pos = CHM_ABS(basic_type::pShmBase, pos->next, st_ptr_type)){
		mqmsghead.Reset(&pos->msghead, basic_type::pShmBase);
		if(randcnt <= 0L){
			return mqmsghead.GetMsgId();
		}
		--randcnt;
	}
	return CHM_INVALID_MSGID;
}

//
// This method is not good performance.
//
template<typename T>
bool mqmsgheadlist_lap<T>::GetMsgidListByPid(pid_t pid, msgidlist_t& list, bool is_clear_list)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PMQMSGHEADLIST does not set.");
		return false;
	}
	if(basic_type::pAbsPtr->prev){
		ToFirst();
	}
	if(is_clear_list){
		list.clear();
	}

	for(st_ptr_type cur = basic_type::pAbsPtr; cur; cur = CHM_ABS(basic_type::pShmBase, cur->next, st_ptr_type)){
		mqmsgheadlap	tmpmqmsghead(&cur->msghead, basic_type::pShmBase);
		if(pid == tmpmqmsghead.GetPid()){
			list.push_back(tmpmqmsghead.GetMsgId());
		}
	}
	return true;
}

//
// This method is not good performance.
//
template<typename T>
bool mqmsgheadlist_lap<T>::GetMsgidListByUniqPid(msgidlist_t& list, bool is_clear_list)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PMQMSGHEADLIST does not set.");
		return false;
	}
	if(basic_type::pAbsPtr->prev){
		ToFirst();
	}
	if(is_clear_list){
		list.clear();
	}

	pidbmap_t	tmpmap;		// pid map for temporary
	for(st_ptr_type cur = basic_type::pAbsPtr; cur; cur = CHM_ABS(basic_type::pShmBase, cur->next, st_ptr_type)){
		mqmsgheadlap	tmpmqmsghead(&cur->msghead, basic_type::pShmBase);
		pid_t			pid = tmpmqmsghead.GetPid();
		if(CHM_INVALID_PID == pid){
			continue;
		}
		if(tmpmap.end() != tmpmap.find(pid)){
			// already set
			continue;
		}
		list.push_back(tmpmqmsghead.GetMsgId());
		tmpmap[pid] = true;			// do not care for value.
	}
	return true;
}

typedef	mqmsgheadlist_lap<MQMSGHEADLIST>	mqmsgheadlistlap;

//---------------------------------------------------------
// For CHMLOGRAW
//---------------------------------------------------------
template<typename T>
class chmlograw_lap : public structure_lap<T>
{
	public:
		typedef T					st_type;
		typedef T*					st_ptr_type;
		typedef structure_lap<T>	basic_type;

	public:
		explicit chmlograw_lap(st_ptr_type ptr = NULL, const void* shmbase = NULL, bool is_abs = true);

		virtual bool Initialize(void);
		virtual bool Dump(std::stringstream& sstream, const char* spacer) const;
		virtual bool Clear(void);
		bool Copy(st_ptr_type ptr) const;

		bool Set(logtype_t logtype, size_t length, const struct timespec& start, const struct timespec& fin);
		bool Get(st_ptr_type plograw) const;
};

template<typename T>
chmlograw_lap<T>::chmlograw_lap(st_ptr_type ptr, const void* shmbase, bool is_abs)
{
	structure_lap<T>::Reset(ptr, shmbase, is_abs);
}

template<typename T>
bool chmlograw_lap<T>::Initialize(void)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMLOGRAW does not set.");
		return false;
	}
	basic_type::pAbsPtr->log_type			= CHMLOG_TYPE_NOTASSIGNED;
	basic_type::pAbsPtr->length				= 0L;
	INIT_TIMESPEC(&(basic_type::pAbsPtr->start_time));
	INIT_TIMESPEC(&(basic_type::pAbsPtr->fin_time));

	return true;
}

template<typename T>
bool chmlograw_lap<T>::Dump(std::stringstream& sstream, const char* spacer) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMLOGRAW does not set.");
		return false;
	}
	sstream << (spacer ? spacer : "") << "log_type      = "	<< STR_CHMLOG_TYPE(basic_type::pAbsPtr->log_type)		<< std::endl;
	sstream << (spacer ? spacer : "") << "length        = "	<< basic_type::pAbsPtr->length							<< std::endl;
	sstream << (spacer ? spacer : "") << "start_time    = "	<< basic_type::pAbsPtr->start_time.tv_sec				<< "s " << basic_type::pAbsPtr->start_time.tv_nsec	<< "ns" << std::endl;
	sstream << (spacer ? spacer : "") << "fin_time      = "	<< basic_type::pAbsPtr->fin_time.tv_sec					<< "s " << basic_type::pAbsPtr->fin_time.tv_nsec	<< "ns" << std::endl;

	return true;
}

template<typename T>
bool chmlograw_lap<T>::Copy(st_ptr_type ptr) const
{
	if(!ptr){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMLOGRAW does not set.");
		return false;
	}
	ptr->log_type			= basic_type::pAbsPtr->log_type;
	ptr->length				= basic_type::pAbsPtr->length;
	ptr->start_time.tv_sec	= basic_type::pAbsPtr->start_time.tv_sec;
	ptr->start_time.tv_nsec	= basic_type::pAbsPtr->start_time.tv_nsec;
	ptr->fin_time.tv_sec	= basic_type::pAbsPtr->fin_time.tv_sec;
	ptr->fin_time.tv_nsec	= basic_type::pAbsPtr->fin_time.tv_nsec;

	return true;
}

template<typename T>
bool chmlograw_lap<T>::Clear(void)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMLOGRAW does not set.");
		return false;
	}
	basic_type::pAbsPtr->length	= 0L;
	INIT_TIMESPEC(&(basic_type::pAbsPtr->start_time));
	INIT_TIMESPEC(&(basic_type::pAbsPtr->fin_time));

	return true;
}

template<typename T>
bool chmlograw_lap<T>::Set(logtype_t logtype, size_t length, const struct timespec& start, const struct timespec& fin)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMLOGRAW does not set.");
		return false;
	}
	if(!IS_SAFE_CHMLOG_TYPE(logtype)){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	basic_type::pAbsPtr->log_type	= logtype;
	basic_type::pAbsPtr->length		= length;
	COPY_TIMESPEC(&(basic_type::pAbsPtr->start_time),	&start);
	COPY_TIMESPEC(&(basic_type::pAbsPtr->fin_time),		&fin);

	return true;
}

template<typename T>
bool chmlograw_lap<T>::Get(st_ptr_type plograw) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMLOGRAW does not set.");
		return false;
	}
	if(!plograw){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	plograw->log_type	= basic_type::pAbsPtr->log_type;
	plograw->length		= basic_type::pAbsPtr->length;
	COPY_TIMESPEC(&(plograw->start_time),	&(basic_type::pAbsPtr->start_time));
	COPY_TIMESPEC(&(plograw->fin_time),		&(basic_type::pAbsPtr->fin_time));

	return true;
}

typedef	chmlograw_lap<CHMLOGRAW>	chmlograwlap;

//---------------------------------------------------------
// For CHMLOG
//---------------------------------------------------------
template<typename T>
class chmlog_lap : public structure_lap<T>
{
	public:
		typedef T					st_type;
		typedef T*					st_ptr_type;
		typedef structure_lap<T>	basic_type;

	public:
		explicit chmlog_lap(st_ptr_type ptr = NULL, const void* shmbase = NULL, bool is_abs = true);

		virtual bool Initialize(void);
		virtual bool Dump(std::stringstream& sstream, const char* spacer) const;
		virtual bool Clear(void);
		bool Copy(st_ptr_type ptr) const;

		bool Initialize(PCHMLOGRAW rel_lograwarea, long max_log_count);

		long GetHistoryCount(void) const { return (basic_type::pAbsPtr ? basic_type::pAbsPtr->max_log_count : 0L); }
		bool IsEnable(void) const { return (basic_type::pAbsPtr ? basic_type::pAbsPtr->enable : false); }
		bool Enable(void);
		bool Disable(void);
		bool Add(logtype_t logtype, size_t length, const struct timespec& start, const struct timespec& fin);
		bool Get(PCHMLOGRAW plograwarr, long& arrsize, logtype_t dirmask, logtype_t devmask) const;
};

template<typename T>
chmlog_lap<T>::chmlog_lap(st_ptr_type ptr, const void* shmbase, bool is_abs)
{
	structure_lap<T>::Reset(ptr, shmbase, is_abs);
}

template<typename T>
bool chmlog_lap<T>::Initialize(void)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMLOG does not set.");
		return false;
	}
	basic_type::pAbsPtr->enable				= false;
	basic_type::pAbsPtr->start_time			= 0L;
	basic_type::pAbsPtr->stop_time			= 0L;
	basic_type::pAbsPtr->max_log_count		= 0L;
	basic_type::pAbsPtr->next_pos			= 0L;
	basic_type::pAbsPtr->start_log_rel_area	= NULL;

	return true;
}

template<typename T>
bool chmlog_lap<T>::Dump(std::stringstream& sstream, const char* spacer) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMLOG does not set.");
		return false;
	}
	sstream << (spacer ? spacer : "") << "enable             = "	<< (basic_type::pAbsPtr->enable ? "true" : "false")	<< std::endl;
	sstream << (spacer ? spacer : "") << "start_time         = "	<< basic_type::pAbsPtr->start_time					<< std::endl;
	sstream << (spacer ? spacer : "") << "stop_time          = "	<< basic_type::pAbsPtr->stop_time					<< std::endl;
	sstream << (spacer ? spacer : "") << "max_log_count      = "	<< basic_type::pAbsPtr->max_log_count				<< std::endl;
	sstream << (spacer ? spacer : "") << "next_pos           = "	<< basic_type::pAbsPtr->next_pos					<< std::endl;
	sstream << (spacer ? spacer : "") << "start_log_rel_area = "	<< basic_type::pAbsPtr->start_log_rel_area			<< std::endl;

	return true;
}

// [NOTICE]
// This method does not copy(duplicate) start_log_rel_area member.
// Thus this method name is Copy instead of Dup.
//
template<typename T>
bool chmlog_lap<T>::Copy(st_ptr_type ptr) const
{
	if(!ptr){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMLOG does not set.");
		return false;
	}
	ptr->enable				= basic_type::pAbsPtr->enable;
	ptr->start_time			= basic_type::pAbsPtr->start_time;
	ptr->stop_time			= basic_type::pAbsPtr->stop_time;
	ptr->max_log_count		= basic_type::pAbsPtr->max_log_count;
	ptr->next_pos			= basic_type::pAbsPtr->next_pos;
	ptr->start_log_rel_area	= NULL;									// Always NULL

	return true;
}

template<typename T>
bool chmlog_lap<T>::Initialize(PCHMLOGRAW rel_lograwarea, long max_log_count)
{
	if(!rel_lograwarea){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMLOG does not set.");
		return false;
	}
	basic_type::pAbsPtr->enable				= false;
	basic_type::pAbsPtr->start_time			= time(NULL);
	basic_type::pAbsPtr->stop_time			= time(NULL);
	basic_type::pAbsPtr->max_log_count		= max_log_count;
	basic_type::pAbsPtr->next_pos			= 0L;
	basic_type::pAbsPtr->start_log_rel_area	= rel_lograwarea;

	return true;
}

template<typename T>
bool chmlog_lap<T>::Clear(void)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMLOG does not set.");
		return false;
	}
	basic_type::pAbsPtr->start_time			= 0L;
	basic_type::pAbsPtr->stop_time			= 0L;
	basic_type::pAbsPtr->max_log_count		= 0L;
	basic_type::pAbsPtr->next_pos			= 0L;
	basic_type::pAbsPtr->start_log_rel_area	= NULL;

	return true;
}

template<typename T>
bool chmlog_lap<T>::Enable(void)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMLOG does not set.");
		return false;
	}
	if(IsEnable()){
		MSG_CHMPRN("History logging is already enabled.");
		return true;
	}
	if(!basic_type::pAbsPtr->start_log_rel_area || 0L == basic_type::pAbsPtr->max_log_count){
		ERR_CHMPRN("History logging size is invalid.");
		return false;
	}

	// set terminal data
	PCHMLOGRAW		abs_lograw = CHM_ABS(basic_type::pShmBase, basic_type::pAbsPtr->start_log_rel_area, PCHMLOGRAW);
	chmlograwlap	lograw(&(abs_lograw[basic_type::pAbsPtr->max_log_count - 1L]), basic_type::pShmBase, true);			// From abs
	lograw.Initialize();

	// set enable flag
	basic_type::pAbsPtr->start_time	= time(NULL);
	basic_type::pAbsPtr->stop_time	= time(NULL);
	basic_type::pAbsPtr->next_pos	= 0L;
	basic_type::pAbsPtr->enable		= true;

	return true;
}

template<typename T>
bool chmlog_lap<T>::Disable(void)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMLOG does not set.");
		return false;
	}
	if(!IsEnable()){
		MSG_CHMPRN("History logging is already disabled.");
		return true;
	}

	// set disable flag
	basic_type::pAbsPtr->enable		= false;
	basic_type::pAbsPtr->stop_time	= time(NULL);

	return true;
}

template<typename T>
bool chmlog_lap<T>::Add(logtype_t logtype, size_t length, const struct timespec& start, const struct timespec& fin)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMLOG does not set.");
		return false;
	}
	if(!IsEnable()){
		//MSG_CHMPRN("Now history logging is disabled.");
		return true;
	}
	if(!basic_type::pAbsPtr->start_log_rel_area || 0L == basic_type::pAbsPtr->max_log_count){
		WAN_CHMPRN("History logging size is invalid.");
		return false;
	}

	long	selfpos = basic_type::pAbsPtr->next_pos;
	if(basic_type::pAbsPtr->max_log_count <= (basic_type::pAbsPtr->next_pos + 1)){
		// set next pos
		basic_type::pAbsPtr->next_pos = 0L;
	}else{
		++(basic_type::pAbsPtr->next_pos);
	}

	PCHMLOGRAW		abs_lograw = CHM_ABS(basic_type::pShmBase, basic_type::pAbsPtr->start_log_rel_area, PCHMLOGRAW);
	chmlograwlap	lograw(&(abs_lograw[selfpos]), basic_type::pShmBase, true);					// From abs
	return lograw.Set(logtype, length, start, fin);
}

template<typename T>
bool chmlog_lap<T>::Get(PCHMLOGRAW plograwarr, long& arrsize, logtype_t dirmask, logtype_t devmask) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMLOG does not set.");
		return false;
	}
	if(!plograwarr || !IS_SAFE_CHMLOG_MASK(dirmask) || !IS_SAFE_CHMLOG_MASK(devmask)){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}

	// set arrsize is maximum history count if it too big.
	if(basic_type::pAbsPtr->max_log_count < arrsize){
		arrsize = basic_type::pAbsPtr->max_log_count;
	}
	PCHMLOGRAW	abs_lograw = CHM_ABS(basic_type::pShmBase, basic_type::pAbsPtr->start_log_rel_area, PCHMLOGRAW);

	// set start pos
	bool	is_circled;
	long	cur_pos;
	// cppcheck-suppress nullPointer
	if(CHMLOG_TYPE_NOTASSIGNED == abs_lograw[basic_type::pAbsPtr->max_log_count - 1L].log_type){
		is_circled	= true;
		cur_pos		= 0L;
	}else{
		is_circled	= false;
		cur_pos		= basic_type::pAbsPtr->next_pos;
	}

	// Loop
	long	set_pos;
	for(set_pos = 0L; set_pos < arrsize; ++cur_pos){
		if(is_circled && basic_type::pAbsPtr->next_pos <= cur_pos){
			break;
		}

		if(basic_type::pAbsPtr->max_log_count <= cur_pos){
			if(!is_circled){
				is_circled = true;
				cur_pos = 0L;
			}else{
				break;	// for safe
			}
		}
		if(!IS_CHMLOG_ASSIGNED(abs_lograw[cur_pos].log_type)){
			continue;
		}
		if(0 == (abs_lograw[cur_pos].log_type & dirmask) || 0 == (abs_lograw[cur_pos].log_type & devmask)){
			continue;
		}
		plograwarr[set_pos].log_type	= abs_lograw[cur_pos].log_type;
		plograwarr[set_pos].length		= abs_lograw[cur_pos].length;
		COPY_TIMESPEC(&(plograwarr[set_pos].start_time),	&(abs_lograw[cur_pos].start_time));
		COPY_TIMESPEC(&(plograwarr[set_pos].fin_time),		&(abs_lograw[cur_pos].fin_time));
		++set_pos;
	}
	arrsize = set_pos;

	return true;
}

typedef	chmlog_lap<CHMLOG>	chmloglap;

//---------------------------------------------------------
// For CLTPROCLIST
//---------------------------------------------------------
template<typename T>
class cltproclist_lap : public structure_lap<T>
{
	public:
		typedef T					st_type;
		typedef T*					st_ptr_type;
		typedef structure_lap<T>	basic_type;

	public:
		explicit cltproclist_lap(st_ptr_type ptr = NULL, const void* shmbase = NULL, bool is_abs = true);

		virtual bool Initialize(void);
		bool Initialize(st_ptr_type prev, st_ptr_type next, bool is_abs = true);
		bool Initialize(pid_t pid);
		bool Clear(void);
		virtual bool Dump(std::stringstream& sstream, const char* spacer) const;
		st_ptr_type Dup(void);
		void Free(st_ptr_type ptr) const;

		st_ptr_type GetFirstPtr(bool is_abs = true);
		bool ToFirst(void);
		bool ToLast(void);
		bool ToNext(void);
		long Count(void);
		bool Insert(st_ptr_type ptr, bool is_abs);
		st_ptr_type Retrieve(pid_t pid);
		st_ptr_type Retrieve(void);
		st_ptr_type Find(pid_t pid, bool is_abs);

		bool GetAllPids(pidlist_t& list);
};

template<typename T>
cltproclist_lap<T>::cltproclist_lap(st_ptr_type ptr, const void* shmbase, bool is_abs)
{
	structure_lap<T>::Reset(ptr, shmbase, is_abs);
}

template<typename T>
bool cltproclist_lap<T>::Initialize(void)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCLTPROCLIST does not set.");
		return false;
	}
	basic_type::pAbsPtr->prev	= NULL;
	basic_type::pAbsPtr->next	= NULL;
	basic_type::pAbsPtr->pid	= CHM_INVALID_PID;
	return true;
}

template<typename T>
bool cltproclist_lap<T>::Initialize(st_ptr_type prev, st_ptr_type next, bool is_abs)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCLTPROCLIST does not set.");
		return false;
	}
	basic_type::pAbsPtr->prev	= is_abs ? CHM_REL(basic_type::pShmBase, prev, st_ptr_type) : prev;
	basic_type::pAbsPtr->next	= is_abs ? CHM_REL(basic_type::pShmBase, next, st_ptr_type) : next;
	basic_type::pAbsPtr->pid	= CHM_INVALID_PID;
	return true;
}

template<typename T>
bool cltproclist_lap<T>::Initialize(pid_t pid)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCLTPROCLIST does not set.");
		return false;
	}
	basic_type::pAbsPtr->pid = pid;
	return true;
}

template<typename T>
bool cltproclist_lap<T>::Clear(void)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCLTPROCLIST does not set.");
		return false;
	}
	basic_type::pAbsPtr->prev	= NULL;
	basic_type::pAbsPtr->next	= NULL;
	basic_type::pAbsPtr->pid	= CHM_INVALID_PID;
	return true;
}

template<typename T>
bool cltproclist_lap<T>::Dump(std::stringstream& sstream, const char* spacer) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCLTPROCLIST does not set.");
		return false;
	}
	std::string	tmpspacer1 = spacer ? spacer : "";
	tmpspacer1 += "  ";

	// To first
	st_ptr_type	cur;
	for(cur = const_cast<st_ptr_type>(basic_type::pAbsPtr); cur->prev; cur = CHM_ABS(basic_type::pShmBase, cur->prev, st_ptr_type));

	for(long count = 0L; cur; cur = CHM_ABS(basic_type::pShmBase, cur->next, st_ptr_type), count++){
		sstream << (spacer ? spacer : "") << "[" << count << "] = {"		<< std::endl;

		sstream << tmpspacer1 << "prev = "	<< cur->prev	<< std::endl;
		sstream << tmpspacer1 << "next = "	<< cur->next	<< std::endl;
		sstream << tmpspacer1 << "pid  = "	<< cur->pid		<< std::endl;

		sstream << (spacer ? spacer : "") << "}"		<< std::endl;
	}
	return true;
}

template<typename T>
typename cltproclist_lap<T>::st_ptr_type cltproclist_lap<T>::Dup(void)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCLTPROCLIST does not set.");
		return NULL;
	}

	long	count = Count();
	if(0 == count){
		return NULL;
	}

	// To first
	st_ptr_type	cur;
	for(cur = const_cast<st_ptr_type>(basic_type::pAbsPtr); cur->prev; cur = CHM_ABS(basic_type::pShmBase, cur->prev, st_ptr_type));

	// duplicate
	st_ptr_type	pdst;
	if(NULL == (pdst = reinterpret_cast<st_ptr_type>(calloc(count, sizeof(st_type))))){
		ERR_CHMPRN("Could not allocation memory.");
		return NULL;
	}
	for(st_ptr_type pdstcur = pdst, pdstprev = NULL; cur; cur = CHM_ABS(basic_type::pShmBase, cur->next, st_ptr_type), pdstprev = pdstcur++){
		if(pdstprev){
			pdstprev->next = pdstcur;
		}
		pdstcur->prev	= pdstprev;
		pdstcur->pid	= cur->pid;
	}
	return pdst;
}

template<typename T>
void cltproclist_lap<T>::Free(st_ptr_type ptr) const
{
	K2H_Free(ptr);
}

template<typename T>
typename cltproclist_lap<T>::st_ptr_type cltproclist_lap<T>::GetFirstPtr(bool is_abs)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		//MSG_CHMPRN("PCLTPROCLIST does not set.");
		return NULL;
	}
	if(basic_type::pAbsPtr->prev){
		ToFirst();
	}
	return (is_abs ? cltproclist_lap<T>::GetAbsPtr() : cltproclist_lap<T>::GetRelPtr());

}

template<typename T>
bool cltproclist_lap<T>::ToFirst(void)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCLTPROCLIST does not set.");
		return false;
	}
	for(; basic_type::pAbsPtr->prev; basic_type::pAbsPtr = CHM_ABS(basic_type::pShmBase, basic_type::pAbsPtr->prev, st_ptr_type));
	return true;
}

template<typename T>
bool cltproclist_lap<T>::ToLast(void)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCLTPROCLIST does not set.");
		return false;
	}
	for(; basic_type::pAbsPtr->next; basic_type::pAbsPtr = CHM_ABS(basic_type::pShmBase, basic_type::pAbsPtr->next, st_ptr_type));
	return true;
}

template<typename T>
bool cltproclist_lap<T>::ToNext(void)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCLTPROCLIST does not set.");
		return false;
	}
	if(!basic_type::pAbsPtr->next){
		MSG_CHMPRN("Reached end of client process id list.");
		return false;
	}
	basic_type::pAbsPtr = CHM_ABS(basic_type::pShmBase, basic_type::pAbsPtr->next, st_ptr_type);
	return true;
}

template<typename T>
long cltproclist_lap<T>::Count(void)
{
	if(!basic_type::pShmBase){
		ERR_CHMPRN("PCLTPROCLIST does not set.");
		return 0L;
	}
	if(!basic_type::pAbsPtr){
		//MSG_CHMPRN("PCLTPROCLIST does not set.");
		return 0L;
	}
	if(basic_type::pAbsPtr->prev){
		ToFirst();
	}
	long		count;
	st_ptr_type pos;
	for(count = 1L, pos = basic_type::pAbsPtr; pos->next; pos = CHM_ABS(basic_type::pShmBase, pos->next, st_ptr_type), count++);

	return count;
}

//
// This method uses liner searching, so not good performance.
//
template<typename T>
bool cltproclist_lap<T>::Insert(st_ptr_type ptr, bool is_abs)
{
	if(!ptr){
		ERR_CHMPRN("ptr is null.");
		return false;
	}
	if(!basic_type::pShmBase){					// allow basic_type::pAbsPtr is NULL
		ERR_CHMPRN("PCLTPROCLIST does not set.");
		return false;
	}
	if(basic_type::pAbsPtr->prev){
		ToFirst();
	}

	st_ptr_type	relptr	= is_abs ? CHM_REL(basic_type::pShmBase, ptr, st_ptr_type) : ptr;
	st_ptr_type	absptr	= is_abs ? ptr : CHM_ABS(basic_type::pShmBase, ptr, st_ptr_type);

	for(st_ptr_type cur = basic_type::pAbsPtr; cur; cur = CHM_ABS(basic_type::pShmBase, cur->next, st_ptr_type)){
		if(CHM_INVALID_PID != absptr->pid && cur->pid == absptr->pid){
			ERR_CHMPRN("Found same pid in list.");
			return false;
		}else if(cur->pid >= absptr->pid){
			st_ptr_type	prevptr	= CHM_ABS(basic_type::pShmBase, cur->prev, st_ptr_type);
			if(prevptr){
				prevptr->next	= relptr;
			}
			absptr->prev		= cur->prev;
			absptr->next		= CHM_REL(basic_type::pShmBase, cur, st_ptr_type);
			cur->prev			= relptr;
			basic_type::pAbsPtr = absptr;
			break;
		}else if(!cur->next){
			// add into end of list
			cur->next			= relptr;
			absptr->prev		= CHM_REL(basic_type::pShmBase, cur, st_ptr_type);
			absptr->next		= NULL;
			basic_type::pAbsPtr = absptr;
			break;
		}
	}
	return true;
}

//
// [CAREFUL]
// After calling this method, be careful for pAbsPtr is NULL.
// And this method is very slow, should use Retrieve(void)
//
template<typename T>
typename cltproclist_lap<T>::st_ptr_type cltproclist_lap<T>::Retrieve(pid_t pid)
{
	if(CHM_INVALID_PID == pid){
		ERR_CHMPRN("Parameter is wrong.");
		return NULL;
	}
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCLTPROCLIST does not set.");
		return NULL;
	}
	if(basic_type::pAbsPtr->prev){
		ToFirst();
	}

	for(st_ptr_type cur = basic_type::pAbsPtr; cur; cur = CHM_ABS(basic_type::pShmBase, cur->next, st_ptr_type)){
		if(cur->pid == pid){
			// found
			if(cur == basic_type::pAbsPtr){
				// basic_type::pAbsPtr is first object in list.
				if(basic_type::pAbsPtr->next){
					basic_type::pAbsPtr = CHM_ABS(basic_type::pShmBase, basic_type::pAbsPtr->next, st_ptr_type);
				}else{
					basic_type::pAbsPtr = NULL;
				}
			}
			st_ptr_type	prevlist = CHM_ABS(basic_type::pShmBase, cur->prev, st_ptr_type);
			st_ptr_type	nextlist = CHM_ABS(basic_type::pShmBase, cur->next, st_ptr_type);
			if(prevlist){
				prevlist->next = cur->next;
			}
			if(nextlist){
				nextlist->prev = cur->prev;
			}
			cur->prev = NULL;
			cur->next = NULL;
			return cur;
		}
	}

	MSG_CHMPRN("Could not find pid(%d).", pid);
	return NULL;
}

//
// [CAREFUL]
// After calling this method, be careful for pAbsPtr is NULL.
//
template<typename T>
typename cltproclist_lap<T>::st_ptr_type cltproclist_lap<T>::Retrieve(void)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCLTPROCLIST does not set.");
		return NULL;
	}

	st_ptr_type	abs_cur  = basic_type::pAbsPtr;
	st_ptr_type	abs_prev = CHM_ABS(basic_type::pShmBase, basic_type::pAbsPtr->prev, st_ptr_type);
	st_ptr_type	abs_next = CHM_ABS(basic_type::pShmBase, basic_type::pAbsPtr->next, st_ptr_type);

	if(abs_prev){
		abs_prev->next			= basic_type::pAbsPtr->next;
		if(abs_next){
			abs_next->prev		= basic_type::pAbsPtr->prev;
			basic_type::pAbsPtr	= abs_next;
		}else{
			basic_type::pAbsPtr	= abs_prev;
		}
	}else{
		if(abs_next){
			abs_next->prev		= basic_type::pAbsPtr->prev;
			basic_type::pAbsPtr	= abs_next;
		}else{
			basic_type::pAbsPtr	= NULL;
		}
	}
	abs_cur->prev = NULL;
	abs_cur->next = NULL;

	return abs_cur;
}

//
// This method is not good performance.
//
template<typename T>
typename cltproclist_lap<T>::st_ptr_type cltproclist_lap<T>::Find(pid_t pid, bool is_abs)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCLTPROCLIST does not set.");
		return NULL;
	}
	if(basic_type::pAbsPtr->prev){
		ToFirst();
	}

	for(st_ptr_type cur = basic_type::pAbsPtr; cur; cur = CHM_ABS(basic_type::pShmBase, cur->next, st_ptr_type)){
		if(cur->pid == pid){
			return (is_abs ? cur : CHM_REL(basic_type::pShmBase, cur, st_ptr_type));
		}
	}
	return NULL;
}

template<typename T>
bool cltproclist_lap<T>::GetAllPids(pidlist_t& list)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCLTPROCLIST does not set.");
		return false;
	}
	if(basic_type::pAbsPtr->prev){
		ToFirst();
	}
	list.clear();

	for(st_ptr_type cur = basic_type::pAbsPtr; cur; cur = CHM_ABS(basic_type::pShmBase, cur->next, st_ptr_type)){
		list.push_back(cur->pid);
	}
	return true;
}

typedef	cltproclist_lap<CLTPROCLIST>	cltproclistlap;

//---------------------------------------------------------
// For CHMPXMAN
//---------------------------------------------------------
template<typename T>
class chmpxman_lap : public structure_lap<T>
{
	public:
		typedef T					st_type;
		typedef T*					st_ptr_type;
		typedef structure_lap<T>	basic_type;

	protected:
		long GetChmpxListCount(bool is_server) const;
		PCHMPX* AbsBaseArr(void) const { return (basic_type::pAbsPtr ? CHM_ABS(basic_type::pShmBase, basic_type::pAbsPtr->chmpx_base_srvs, PCHMPX*) : NULL); }
		PCHMPX* AbsPendArr(void) const { return (basic_type::pAbsPtr ? CHM_ABS(basic_type::pShmBase, basic_type::pAbsPtr->chmpx_pend_srvs, PCHMPX*) : NULL); }
		long* AbsSockFreeCnt(void) const { return (basic_type::pAbsPtr ? &(basic_type::pAbsPtr->sock_free_count) : NULL); }
		PCHMSOCKLIST* AbsSockFrees(void) const { return (basic_type::pAbsPtr ? &(basic_type::pAbsPtr->sock_frees) : NULL); }
		bool RawCheckContainsChmpxSvrs(const char* hostname, const short* pctlport, const char* pcuk, std::string* pnormalizedname, PCHMPXSSL pssl) const;

	public:
		explicit chmpxman_lap(st_ptr_type ptr = NULL, const void* shmbase = NULL, bool is_abs = true);

		virtual bool Initialize(void);
		virtual bool Dump(std::stringstream& sstream, const char* spacer) const;
		virtual bool Clear(void);
		bool Copy(st_ptr_type ptr);
		chmpxlap::st_ptr_type DupSelfChmpxSvr(void);
		void Free(st_ptr_type ptr) const;

		bool Close(int eqfd, int type = CLOSETG_BOTH);
		bool Initialize(const CHMCFGINFO* pchmcfg, const CHMNODE_CFGINFO* pselfnode, const char* pselfname, PCHMPXLIST relchmpxlist, PCHMSOCKLIST relchmsockarea, PCHMPX* rel_pchmpxarrbase, PCHMPX* rel_pchmpxarrpend);
		bool ReloadConfiguration(const CHMCFGINFO* pchmcfg, int eqfd);
		bool UpdateChmpxSvrs(const CHMCFGINFO* pchmcfg, int eqfd);

		bool GetSelfChmpxSvr(PCHMPXSVR chmpxsvr) const;
		bool GetChmpxSvr(chmpxid_t chmpxid, PCHMPXSVR chmpxsvr) const;
		bool GetChmpxSvrs(PCHMPXSVR* ppchmpxsvrs, long& count) const;
		bool MergeChmpxSvrs(PCHMPXSVR pchmpxsvrs, CHMPXID_SEED_TYPE type, long count, bool is_client_join, bool is_remove = true, bool is_init_process = false, int eqfd = CHM_INVALID_HANDLE);

		bool GetGroup(std::string& group) const;
		bool IsServerMode(chmpxid_t chmpxid = CHM_INVALID_CHMPXID) const;
		bool IsSlaveMode(chmpxid_t chmpxid = CHM_INVALID_CHMPXID) const;
		long GetReplicaCount(void) const { return (basic_type::pAbsPtr ? basic_type::pAbsPtr->replica_count : DEFAULT_REPLICA_COUNT); }
		chmpxid_t GetSelfChmpxId(void) const;
		chmpxid_t GetNextRingChmpxId(chmpxid_t chmpxid = CHM_INVALID_CHMPXID) const;

		long GetServerCount(void) const { return GetChmpxListCount(true); }
		long GetSlaveCount(void) const { return GetChmpxListCount(false); }
		long GetUpServerCount(void) const;
		chmpxpos_t GetSelfServerPos(void) const;
		chmpxpos_t GetNextServerPos(chmpxpos_t startpos, chmpxpos_t nowpos, bool is_skip_self, bool is_cycle) const;
		bool CheckContainsChmpxSvrs(const char* hostname) const;
		bool CheckStrictlyContainsChmpxSvrs(const char* hostname, const short* pctlport, const char* pcuk, std::string* pnormalizedname, PCHMPXSSL pssl) const;

		bool IsOperating(void);
		bool IsServerChmpxId(chmpxid_t chmpxid) const;
		chmpxid_t GetChmpxIdBySock(int sock, int type = CLOSETG_BOTH) const;
		chmpxid_t GetChmpxIdByToServerName(CHMPXID_SEED_TYPE type, const char* hostname, short ctlport, const char* cuk, const char* ctlendpoints, const char* custom_seed) const;
		chmpxid_t GetChmpxIdByStatus(chmpxsts_t status, bool part_match = false) const;
		chmpxid_t GetRandomServerChmpxId(bool without_suspend = false);
		chmpxid_t GetServerChmpxIdByHash(chmhash_t hash) const;
		bool GetServerChmpxIdAndBaseHashByHashs(chmhash_t hash, chmpxidlist_t& chmpxids, chmhashlist_t& basehashs, bool with_pending, bool without_down = true, bool without_suspend = true);
		bool GetServerChmHashsByHashs(chmhash_t hash, chmhashlist_t& basehashs, bool with_pending, bool without_down = true, bool without_suspend = true);
		bool GetServerChmpxIdByHashs(chmhash_t hash, chmpxidlist_t& chmpxids, bool with_pending, bool without_down = true, bool without_suspend = true);
		long GetServerChmpxIds(chmpxidlist_t& list, bool with_pending, bool without_down = true, bool without_suspend = true) const;

		bool GetServerBase(chmpxpos_t pos, std::string* pname = NULL, chmpxid_t* pchmpxid = NULL, short* pport = NULL, short* pctlport = NULL, hostport_list_t* pendpoints = NULL, hostport_list_t* pctlendpoints = NULL) const;
		bool GetServerBase(chmpxid_t chmpxid, std::string* pname = NULL, short* pport = NULL, short* pctlport = NULL, hostport_list_t* pendpoints = NULL, hostport_list_t* pctlendpoints = NULL) const;
		bool GetServerBase(chmpxid_t chmpxid, CHMPXSSL& ssl) const;
		bool GetServerSocks(chmpxid_t chmpxid, socklist_t& socklist, int& ctlsock) const;
		bool GetServerHash(chmpxid_t chmpxid, chmhash_t& base, chmhash_t& pending) const;
		bool GetMaxHashCount(chmhash_t& basehash, chmhash_t& pengindhash) const;
		chmpxsts_t GetServerStatus(chmpxid_t chmpxid) const;
		bool GetSelfPorts(short& port, short& ctlport) const;
		bool GetSelfSocks(int& sock, int& ctlsock) const;
		bool GetSelfHash(chmhash_t& base, chmhash_t& pending) const;
		chmpxsts_t GetSelfStatus(void) const;
		bool GetSelfSsl(CHMPXSSL& ssl) const;
		bool GetSelfBase(std::string* pname = NULL, short* pport = NULL, short* pctlport = NULL, std::string* pcuk = NULL, std::string* pcustom_seed = NULL, hostport_list_t* pendpoints = NULL, hostport_list_t* pctlendpoints = NULL, hostport_list_t* pforward_peers = NULL, hostport_list_t* preverse_peers = NULL) const;
		long GetSlaveChmpxIds(chmpxidlist_t& list) const;
		bool GetSlaveBase(chmpxid_t chmpxid, std::string* pname = NULL, short* pctlport = NULL, std::string* pcuk = NULL, std::string* pcustom_seed = NULL, hostport_list_t* pendpoints = NULL, hostport_list_t* pctlendpoints = NULL, hostport_list_t* pforward_peers = NULL, hostport_list_t* preverse_peers = NULL) const;
		bool GetSlaveSock(chmpxid_t chmpxid, socklist_t& socklist) const;
		chmpxsts_t GetSlaveStatus(chmpxid_t chmpxid) const;

		bool SetServerSocks(chmpxid_t chmpxid, int sock, int ctlsock, int type);
		bool SetServerHash(chmpxid_t chmpxid, chmhash_t base, chmhash_t pending, int type);
		bool SetServerStatus(chmpxid_t chmpxid, chmpxsts_t status, bool is_client_join);
		bool UpdateLastStatusTime(chmpxid_t chmpxid = CHM_INVALID_CHMPXID);
		bool RemoveServerSock(chmpxid_t chmpxid, int sock);
		bool UpdateHash(int type, bool is_allow_operating = true, bool is_allow_slave_mode = true);
		bool SetSelfSocks(int sock, int ctlsock);
		bool SetSelfHash(chmhash_t base, chmhash_t pending, int type);
		bool SetSelfStatus(chmpxsts_t status, bool is_client_join);

		bool SetSlaveBase(CHMPXID_SEED_TYPE type, chmpxid_t chmpxid, const char* hostname, short ctlport, const char* cuk, const char* custom_seed, const hostport_list_t& endpoints, const hostport_list_t& ctlendpoints, const hostport_list_t& forward_peers, const hostport_list_t& reverse_peers, const PCHMPXSSL pssl);
		bool SetSlaveSock(chmpxid_t chmpxid, int sock);
		bool SetSlaveStatus(chmpxid_t chmpxid, chmpxsts_t status);
		bool RemoveSlaveSock(chmpxid_t chmpxid, int sock);
		bool RemoveSlave(chmpxid_t chmpxid, int eqfd);
		bool CheckSockInAllChmpx(int sock) const;

		bool AddStat(chmpxid_t chmpxid, bool is_sent, size_t length, const struct timespec& elapsed_time);
		bool GetStat(PCHMSTAT pserver, PCHMSTAT pslave) const;
};

template<typename T>
chmpxman_lap<T>::chmpxman_lap(st_ptr_type ptr, const void* shmbase, bool is_abs)
{
	structure_lap<T>::Reset(ptr, shmbase, is_abs);
}

template<typename T>
bool chmpxman_lap<T>::Initialize(void)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return false;
	}
	basic_type::pAbsPtr->group[0]			= '\0';
	basic_type::pAbsPtr->cfg_revision		= 0;
	basic_type::pAbsPtr->cfg_date			= 0L;
	basic_type::pAbsPtr->replica_count		= DEFAULT_REPLICA_COUNT;
	basic_type::pAbsPtr->chmpx_count		= 0;
	basic_type::pAbsPtr->chmpx_base_srvs	= NULL;
	basic_type::pAbsPtr->chmpx_pend_srvs	= NULL;
	basic_type::pAbsPtr->chmpx_self			= NULL;
	basic_type::pAbsPtr->chmpx_bhash_count	= 0;
	basic_type::pAbsPtr->chmpx_server_count	= 0;
	basic_type::pAbsPtr->chmpx_servers		= NULL;
	basic_type::pAbsPtr->chmpx_slave_count	= 0;
	basic_type::pAbsPtr->chmpx_slaves		= NULL;
	basic_type::pAbsPtr->chmpx_free_count	= 0;
	basic_type::pAbsPtr->chmpx_frees		= NULL;
	basic_type::pAbsPtr->sock_free_count	= 0;
	basic_type::pAbsPtr->sock_frees			= NULL;
	basic_type::pAbsPtr->is_operating		= false;
	basic_type::pAbsPtr->last_chmpxid		= CHM_INVALID_CHMPXID;

	for(int cnt = 0; cnt < CHMPXID_MAP_SIZE; ++cnt){
		basic_type::pAbsPtr->chmpxid_map[cnt] = NULL;
	}

	chmstatlap	tmpchmstat;
	tmpchmstat.Reset(&basic_type::pAbsPtr->server_stat, basic_type::pShmBase);
	if(!tmpchmstat.Initialize()){
		ERR_CHMPRN("Failed to initialize CHMSTAT.");
		return false;
	}
	tmpchmstat.Reset(&basic_type::pAbsPtr->slave_stat, basic_type::pShmBase);
	if(!tmpchmstat.Initialize()){
		ERR_CHMPRN("Failed to initialize CHMSTAT.");
		return false;
	}

	return true;
}

template<typename T>
bool chmpxman_lap<T>::Dump(std::stringstream& sstream, const char* spacer) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return false;
	}

	chmstatlap	tmpchmstat;
	std::string	tmpspacer = spacer ? spacer : "";
	tmpspacer += "  ";

	sstream << (spacer ? spacer : "") << "group              = " << basic_type::pAbsPtr->group								<< std::endl;
	sstream << (spacer ? spacer : "") << "cfg_revision       = " << basic_type::pAbsPtr->cfg_revision						<< std::endl;
	sstream << (spacer ? spacer : "") << "cfg_date           = " << basic_type::pAbsPtr->cfg_date							<< std::endl;
	sstream << (spacer ? spacer : "") << "replica_count      = " << basic_type::pAbsPtr->replica_count						<< std::endl;
	sstream << (spacer ? spacer : "") << "chmpx_count        = " << basic_type::pAbsPtr->chmpx_count						<< std::endl;
	sstream << (spacer ? spacer : "") << "chmpxid_map["			<< to_string(CHMPXID_MAP_SIZE) << "]"						<< std::endl;

	sstream << (spacer ? spacer : "") << "chmpx_base_srvs    = " << basic_type::pAbsPtr->chmpx_base_srvs					<< std::endl;
	if(basic_type::pAbsPtr->chmpx_base_srvs){
		PCHMPX*			abs_base_arr		= AbsBaseArr();
		PCHMPX*			abs_pend_arr		= AbsPendArr();
		long*			abs_sock_free_cnt	= AbsSockFreeCnt();
		PCHMSOCKLIST*	abs_sock_frees		= AbsSockFrees();
		chmpxlap		tmpchmpx;
		sstream << (spacer ? spacer : "") << "[" << basic_type::pAbsPtr->chmpx_bhash_count << "] {"							<< std::endl;
		for(long cnt = 0; cnt < basic_type::pAbsPtr->chmpx_bhash_count; cnt++){
			tmpchmpx.Reset(abs_base_arr[cnt], abs_base_arr, abs_pend_arr, abs_sock_free_cnt, abs_sock_frees, basic_type::pShmBase, false);		// From rel
			sstream << (spacer ? spacer : "") << "  [" << cnt << "] = 0x" << to_hexstring(tmpchmpx.GetChmpxId())			<< std::endl;
		}
		sstream << (spacer ? spacer : "") << "}" << std::endl;
	}

	sstream << (spacer ? spacer : "") << "chmpx_pend_srvs    = " << basic_type::pAbsPtr->chmpx_pend_srvs					<< std::endl;
	if(basic_type::pAbsPtr->chmpx_pend_srvs){
		PCHMPX*			abs_base_arr		= AbsBaseArr();
		PCHMPX*			abs_pend_arr		= AbsPendArr();
		long*			abs_sock_free_cnt	= AbsSockFreeCnt();
		PCHMSOCKLIST*	abs_sock_frees		= AbsSockFrees();
		chmpxlap		tmpchmpx;
		sstream << (spacer ? spacer : "") << "[" << basic_type::pAbsPtr->chmpx_server_count << "] {"						<< std::endl;
		for(long cnt = 0; cnt < basic_type::pAbsPtr->chmpx_server_count; cnt++){
			tmpchmpx.Reset(abs_pend_arr[cnt], abs_base_arr, abs_pend_arr, abs_sock_free_cnt, abs_sock_frees, basic_type::pShmBase, false);		// From rel
			sstream << (spacer ? spacer : "") << "  [" << cnt << "] = 0x" << to_hexstring(tmpchmpx.GetChmpxId())			<< std::endl;
		}
		sstream << (spacer ? spacer : "") << "}" << std::endl;
	}

	sstream << (spacer ? spacer : "") << "chmpx_self         = " << basic_type::pAbsPtr->chmpx_self							<< std::endl;

	sstream << (spacer ? spacer : "") << "{" << std::endl;
	chmpxlistlap	selfchmpxlist(basic_type::pAbsPtr->chmpx_self, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);// Get CHMPXLIST from Relative
	chmpxlap		selfchmpx(selfchmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);	// Get CHMPX from Absolute

	selfchmpx.Dump(sstream, tmpspacer.c_str());
	sstream << (spacer ? spacer : "") << "}" << std::endl;

	sstream << (spacer ? spacer : "") << "chmpx_bhash_count  = " << basic_type::pAbsPtr->chmpx_bhash_count					<< std::endl;
	sstream << (spacer ? spacer : "") << "chmpx_server_count = " << basic_type::pAbsPtr->chmpx_server_count					<< std::endl;
	sstream << (spacer ? spacer : "") << "chmpx_servers      = " << basic_type::pAbsPtr->chmpx_servers						<< std::endl;

	if(0L < basic_type::pAbsPtr->chmpx_server_count){
		sstream << (spacer ? spacer : "") << "{" << std::endl;
		chmpxlistlap	svrchmpxlist(basic_type::pAbsPtr->chmpx_servers, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From rel
		svrchmpxlist.Dump(sstream, tmpspacer.c_str());
		sstream << (spacer ? spacer : "") << "}" << std::endl;
	}

	sstream << (spacer ? spacer : "") << "chmpx_slave_count  = " << basic_type::pAbsPtr->chmpx_slave_count					<< std::endl;
	sstream << (spacer ? spacer : "") << "chmpx_slaves       = " << basic_type::pAbsPtr->chmpx_slaves						<< std::endl;

	if(0L < basic_type::pAbsPtr->chmpx_slave_count){
		sstream << (spacer ? spacer : "") << "{" << std::endl;
		chmpxlistlap	svrchmpxlist(basic_type::pAbsPtr->chmpx_slaves, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From rel
		svrchmpxlist.Dump(sstream, tmpspacer.c_str());
		sstream << (spacer ? spacer : "") << "}" << std::endl;
	}

	sstream << (spacer ? spacer : "") << "chmpx_free_count   = " << basic_type::pAbsPtr->chmpx_free_count					<< std::endl;
	sstream << (spacer ? spacer : "") << "chmpx_frees        = " << basic_type::pAbsPtr->chmpx_frees						<< std::endl;

	sstream << (spacer ? spacer : "") << "sock_free_count    = " << basic_type::pAbsPtr->sock_free_count					<< std::endl;
	sstream << (spacer ? spacer : "") << "sock_frees         = " << basic_type::pAbsPtr->sock_frees							<< std::endl;

	sstream << (spacer ? spacer : "") << "is_operating       = " << (basic_type::pAbsPtr->is_operating ? "yes" : "no")		<< std::endl;
	sstream << (spacer ? spacer : "") << "last_chmpxid       = 0x" << to_hexstring(basic_type::pAbsPtr->last_chmpxid)		<< std::endl;

	sstream << (spacer ? spacer : "") << "server_stat{"	<< std::endl;
	tmpchmstat.Reset(&basic_type::pAbsPtr->server_stat, basic_type::pShmBase);
	tmpchmstat.Dump(sstream, tmpspacer.c_str());
	sstream << (spacer ? spacer : "") << "}"			<< std::endl;

	sstream << (spacer ? spacer : "") << "slave_stat{"	<< std::endl;
	tmpchmstat.Reset(&basic_type::pAbsPtr->slave_stat, basic_type::pShmBase);
	tmpchmstat.Dump(sstream, tmpspacer.c_str());
	sstream << (spacer ? spacer : "") << "}"			<< std::endl;

	return true;
}

template<typename T>
bool chmpxman_lap<T>::Copy(st_ptr_type ptr)
{
	if(!ptr){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return false;
	}

	ptr->cfg_revision		= basic_type::pAbsPtr->cfg_revision;
	ptr->cfg_date			= basic_type::pAbsPtr->cfg_date;
	ptr->replica_count		= basic_type::pAbsPtr->replica_count;
	ptr->chmpx_count		= basic_type::pAbsPtr->chmpx_count;
	ptr->chmpx_base_srvs	= NULL;												// Always NULL
	ptr->chmpx_pend_srvs	= NULL;												// Always NULL
	ptr->chmpx_bhash_count	= basic_type::pAbsPtr->chmpx_bhash_count;
	ptr->chmpx_server_count	= basic_type::pAbsPtr->chmpx_server_count;
	ptr->chmpx_slave_count	= basic_type::pAbsPtr->chmpx_slave_count;
	ptr->chmpx_free_count	= basic_type::pAbsPtr->chmpx_free_count;
	ptr->chmpx_frees		= NULL;												// Always NULL
	ptr->sock_free_count	= basic_type::pAbsPtr->sock_free_count;
	ptr->sock_frees			= NULL;												// Always NULL
	ptr->is_operating		= basic_type::pAbsPtr->is_operating;
	ptr->last_chmpxid		= basic_type::pAbsPtr->last_chmpxid;

	strcpy(ptr->group,		basic_type::pAbsPtr->group);
	memset(ptr->chmpxid_map, 0, sizeof(PCHMPXLIST) * CHMPXID_MAP_SIZE);			// [NOTE] clear all for pointer array

	chmpxlistlap			selfchmpxlist(basic_type::pAbsPtr->chmpx_self, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From rel
	ptr->chmpx_self			= selfchmpxlist.Dup(true);							// [NOTE] top of list is self chmpx

	chmpxlistlap			svrchmpxlist(basic_type::pAbsPtr->chmpx_servers, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From rel
	ptr->chmpx_servers		= svrchmpxlist.Dup();

	chmpxlistlap			slvchmpxlist(basic_type::pAbsPtr->chmpx_slaves, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From rel
	ptr->chmpx_slaves		= slvchmpxlist.Dup();

	chmstatlap				chmstat;
	chmstat.Reset(&basic_type::pAbsPtr->server_stat, basic_type::pShmBase);
	if(!chmstat.Copy(&ptr->server_stat)){
		WAN_CHMPRN("Failed to copy server_stat structure, but continue...");
	}

	chmstat.Reset(&basic_type::pAbsPtr->slave_stat, basic_type::pShmBase);
	if(!chmstat.Copy(&ptr->slave_stat)){
		WAN_CHMPRN("Failed to copy slave_stat structure, but continue...");
	}

	return true;
}

template<typename T>
chmpxlap::st_ptr_type chmpxman_lap<T>::DupSelfChmpxSvr(void)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return NULL;
	}
	chmpxlap::st_ptr_type	pselfchmpx;
	if(NULL == (pselfchmpx = reinterpret_cast<chmpxlap::st_ptr_type>(calloc(1, sizeof(chmpxlap::st_type))))){
		ERR_CHMPRN("Could not allocation memory.");
		return NULL;
	}
	chmpxlistlap	selfchmpxlist(basic_type::pAbsPtr->chmpx_self, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// Get CHMPXLIST from Relative
	chmpxlap		selfchmpx(selfchmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);	// Get CHMPX from Absolute
	if(!selfchmpx.Copy(pselfchmpx)){
		ERR_CHMPRN("Could not get self chmpx structure.");
		K2H_Free(pselfchmpx);
		return NULL;
	}
	return pselfchmpx;
}

template<typename T>
void chmpxman_lap<T>::Free(st_ptr_type ptr) const
{
	if(ptr){
		chmpxlistlap		tmpchmpxlist;		// [NOTE] pointer is not on Shm, but we use chmpxlistlap object. thus using only Free method.
		tmpchmpxlist.Free(ptr->chmpx_self);
		tmpchmpxlist.Free(ptr->chmpx_servers);
		tmpchmpxlist.Free(ptr->chmpx_slaves);
	}
}

template<typename T>
bool chmpxman_lap<T>::Initialize(const CHMCFGINFO* pchmcfg, const CHMNODE_CFGINFO* pselfnode, const char* pselfname, PCHMPXLIST relchmpxlist, PCHMSOCKLIST relchmsockarea, PCHMPX* rel_pchmpxarrbase, PCHMPX* rel_pchmpxarrpend)
{
	if(!pchmcfg || !relchmpxlist || !pselfnode || !rel_pchmpxarrbase || !rel_pchmpxarrpend){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}
	if(MAX_GROUP_LENGTH <= pchmcfg->groupname.length() || MAX_CHMPX_COUNT < pchmcfg->max_chmpx_count){
		ERR_CHMPRN("Configuration information are wrong.");
		return false;
	}
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return false;
	}

	// node name
	std::string	strselfname = CHMEMPTYSTR(pselfname) ? pselfnode->name : pselfname;

	strcpy(basic_type::pAbsPtr->group, pchmcfg->groupname.c_str());

	basic_type::pAbsPtr->cfg_revision		= pchmcfg->revision;
	basic_type::pAbsPtr->cfg_date			= pchmcfg->date;
	basic_type::pAbsPtr->replica_count		= pchmcfg->replica_count;
	basic_type::pAbsPtr->chmpx_count		= pchmcfg->max_chmpx_count;
	basic_type::pAbsPtr->chmpx_base_srvs	= rel_pchmpxarrbase;
	basic_type::pAbsPtr->chmpx_pend_srvs	= rel_pchmpxarrpend;

	// Remaining list is free sock list
	chmsocklistlap	freesocklist(relchmsockarea, basic_type::pShmBase, false);			// From Relative
	basic_type::pAbsPtr->sock_free_count	= freesocklist.Count();						// To first of list
	basic_type::pAbsPtr->sock_frees			= freesocklist.GetFirstPtr(false);			// Get Relative

	// Free PCHMPXLIST
	chmpxlistlap	freechmpxlist(relchmpxlist, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);			// From Relative

	// initialize servers/slaves list
	basic_type::pAbsPtr->chmpx_bhash_count	= 0;
	basic_type::pAbsPtr->chmpx_server_count	= 0;
	basic_type::pAbsPtr->chmpx_servers		= NULL;
	basic_type::pAbsPtr->chmpx_slave_count	= 0;
	basic_type::pAbsPtr->chmpx_slaves		= NULL;

	// self node's buffers for temporary
	std::string		self_cuk;
	std::string		self_custom_seed;
	CHMPXHP_RAWPAIR	self_endpoints[EXTERNAL_EP_MAX];
	CHMPXHP_RAWPAIR	self_ctlendpoints[EXTERNAL_EP_MAX];
	CHMPXHP_RAWPAIR	self_forward_peers[FORWARD_PEER_MAX];
	CHMPXHP_RAWPAIR	self_reverse_peers[REVERSE_PEER_MAX];

	// self
	{
		// Get one PCHMPXLIST for self from front of free list
		// Get self PCHMPX
		chmpxlistlap	selfchmpxlist(freechmpxlist.PopFront(), basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);	// Get one CHMPXLIST from Absolute
		chmpxlap		selfchmpx(selfchmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);									// Get CHMPX from Absolute
		CHMPXSSL		ssl;
		CVT_SSL_STRUCTURE(ssl, *pselfnode);

		// get(convert) parameters wich checking by chmpxid seed type
		if(!cvt_parameters_by_chmpxid_type(pselfnode, pchmcfg->chmpxid_type, self_cuk, self_custom_seed, self_endpoints, self_ctlendpoints, self_forward_peers, self_reverse_peers)){
			ERR_CHMPRN("Failed to convert some parameters for self chmpx information from configuration.");
			return false;
		}

		// Initialize self PCHMPX for server in self list
		if(pchmcfg->is_server_mode){
			selfchmpx.InitializeServer(pchmcfg->chmpxid_type, strselfname.c_str(), pchmcfg->groupname.c_str(), pselfnode->port, pselfnode->ctlport, self_cuk.c_str(), self_custom_seed.c_str(), self_endpoints, self_ctlendpoints, self_forward_peers, self_reverse_peers, ssl);

			basic_type::pAbsPtr->chmpx_servers		= selfchmpxlist.GetRelPtr();	// Set Self chmpxlist into servers
			basic_type::pAbsPtr->chmpx_server_count	= 1;

			// [NOTE]
			// chmpx_bhash_count is 0, because InitializeServer sets status DOWN.
		}else{
			selfchmpx.InitializeSlave(pchmcfg->chmpxid_type, strselfname.c_str(), pchmcfg->groupname.c_str(), pselfnode->ctlport, self_cuk.c_str(), self_custom_seed.c_str(), self_endpoints, self_ctlendpoints, self_forward_peers, self_reverse_peers, ssl);
		}
		selfchmpxlist.SaveChmpxIdMap();												// Set chmpxid map
		basic_type::pAbsPtr->chmpx_self = selfchmpxlist.GetRelPtr();				// Set Self chmpxlist
	}

	// Set servers list
	chmpxlistlap	cursvrlist;
	chmpxlap		cursvrchmpx;
	for(chmnode_cfginfos_t::const_iterator iter = pchmcfg->servers.begin(); iter != pchmcfg->servers.end(); ++iter){
		// each node's buffers for temporary
		std::string		cuk;
		std::string		custom_seed;
		CHMPXHP_RAWPAIR	endpoints[EXTERNAL_EP_MAX];
		CHMPXHP_RAWPAIR	ctlendpoints[EXTERNAL_EP_MAX];
		CHMPXHP_RAWPAIR	forward_peers[FORWARD_PEER_MAX];
		CHMPXHP_RAWPAIR	reverse_peers[REVERSE_PEER_MAX];

		// get(convert) parameters wich checking by chmpxid seed type
		if(!cvt_parameters_by_chmpxid_type(&(*iter), pchmcfg->chmpxid_type, cuk, custom_seed, endpoints, ctlendpoints, forward_peers, reverse_peers)){
			ERR_CHMPRN("Failed to convert some parameters for other server chmpx information from configuration.");
			return false;
		}

		if(pchmcfg->is_server_mode){
			// If server mode, already set self chmpx into server list.
			// So skip it.
			if(	iter->name		== pselfnode->name		&&
				iter->ctlport	== pselfnode->ctlport	&&
				cuk				== self_cuk				&&
				custom_seed		== self_custom_seed		&&
				compare_hostport_pairs(endpoints,		self_endpoints,		EXTERNAL_EP_MAX)	&&
				compare_hostport_pairs(ctlendpoints,	self_ctlendpoints,	EXTERNAL_EP_MAX)	&&
				compare_hostport_pairs(forward_peers,	self_forward_peers,	FORWARD_PEER_MAX)	&&
				compare_hostport_pairs(reverse_peers,	self_reverse_peers,	REVERSE_PEER_MAX)	)
			{
				continue;
			}
		}

		// set CHMPX
		CHMPXSSL	ssl;
		CVT_SSL_STRUCTURE(ssl, *iter);
		cursvrlist.Reset(freechmpxlist.PopFront(), basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);	// Get one CHMPXLIST from Absolute
		cursvrchmpx.Reset(cursvrlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);									// Get CHMPX from Absolute
		cursvrchmpx.InitializeServer(pchmcfg->chmpxid_type, iter->name.c_str(), pchmcfg->groupname.c_str(), iter->port, iter->ctlport, cuk.c_str(), custom_seed.c_str(), endpoints, ctlendpoints, forward_peers, reverse_peers, ssl);

		// inter CHMPX into servers list
		if(basic_type::pAbsPtr->chmpx_servers){
			chmpxlistlap	svrchmpxlist(basic_type::pAbsPtr->chmpx_servers, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From rel
			svrchmpxlist.Insert(cursvrlist.GetAbsPtr(), true, pchmcfg->chmpxid_type);	// From abs

			basic_type::pAbsPtr->chmpx_server_count++;
			basic_type::pAbsPtr->chmpx_servers		= svrchmpxlist.GetFirstPtr(false);
		}else{
			cursvrlist.SaveChmpxIdMap();												// Set chmpxid map
			basic_type::pAbsPtr->chmpx_server_count	= 1;
			basic_type::pAbsPtr->chmpx_servers		= cursvrlist.GetRelPtr();
		}
		// [NOTE]
		// chmpx_bhash_count is 0, because InitializeServer sets status DOWN.
	}

	// Remaining list is free chmpx list
	basic_type::pAbsPtr->chmpx_free_count	= freechmpxlist.Count();					// To first of list
	basic_type::pAbsPtr->chmpx_frees		= freechmpxlist.GetFirstPtr(false);			// Get Relative

	// operating flag/last chmpxid
	basic_type::pAbsPtr->is_operating		= false;
	basic_type::pAbsPtr->last_chmpxid		= CHM_INVALID_CHMPXID;

	chmstatlap	tmpchmstat;
	tmpchmstat.Reset(&basic_type::pAbsPtr->server_stat, basic_type::pShmBase);
	if(!tmpchmstat.Initialize()){
		ERR_CHMPRN("Failed to initialize CHMSTAT.");
		return false;
	}
	tmpchmstat.Reset(&basic_type::pAbsPtr->slave_stat, basic_type::pShmBase);
	if(!tmpchmstat.Initialize()){
		ERR_CHMPRN("Failed to initialize CHMSTAT.");
		return false;
	}
	return true;
}

template<typename T>
bool chmpxman_lap<T>::ReloadConfiguration(const CHMCFGINFO* pchmcfg, int eqfd)
{
	if(!pchmcfg){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return false;
	}
	// check
	if(MAX_GROUP_LENGTH <= pchmcfg->groupname.length() || MAX_CHMPX_COUNT < pchmcfg->max_chmpx_count){
		ERR_CHMPRN("Configuration information are wrong.");
		return false;
	}
	if(0 != strcmp(basic_type::pAbsPtr->group, pchmcfg->groupname.c_str())){
		ERR_CHMPRN("Could not change groupname(%s -> %s).", basic_type::pAbsPtr->group, pchmcfg->groupname.c_str());
		return false;
	}

	// reset
	basic_type::pAbsPtr->cfg_revision		= pchmcfg->revision;
	basic_type::pAbsPtr->cfg_date			= pchmcfg->date;
	basic_type::pAbsPtr->replica_count		= pchmcfg->replica_count;
	basic_type::pAbsPtr->chmpx_count		= pchmcfg->max_chmpx_count;

	// update server chmpx(if there is not it in configuration, remove it in list)
	return UpdateChmpxSvrs(pchmcfg, eqfd);
}

// [NOTE]
// Delete the server nodes that are not included in the loaded Configuration,
// just in case the Configuration is reloaded.
// Remove only server nodes that have a SERVICEOUT/DOWN/NOACT status.
//
template<typename T>
bool chmpxman_lap<T>::UpdateChmpxSvrs(const CHMCFGINFO* pchmcfg, int eqfd)
{
	if(!pchmcfg){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return false;
	}
	if(MAX_GROUP_LENGTH <= pchmcfg->groupname.length() || MAX_CHMPX_COUNT < pchmcfg->max_chmpx_count){
		ERR_CHMPRN("Configuration information are wrong.");
		return false;
	}

	// First, if there is server which is not current server list, it is added into list.
	chmpxlistlap						freechmpxlist(basic_type::pAbsPtr->chmpx_frees, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From Relative(allow NULL)
	chmpxlistlap						svrchmpxlist;
	chmnode_cfginfos_t::const_iterator	iter;
	for(iter = pchmcfg->servers.begin(); iter != pchmcfg->servers.end(); ++iter){
		// get parameters wich checking by chmpxid seed type
		std::string		cuk;
		std::string		custom_seed;
		CHMPXHP_RAWPAIR	endpoints[EXTERNAL_EP_MAX];
		CHMPXHP_RAWPAIR	ctlendpoints[EXTERNAL_EP_MAX];
		CHMPXHP_RAWPAIR	forward_peers[FORWARD_PEER_MAX];
		CHMPXHP_RAWPAIR	reverse_peers[REVERSE_PEER_MAX];
		if(!cvt_parameters_by_chmpxid_type(&(*iter), pchmcfg->chmpxid_type, cuk, custom_seed, endpoints, ctlendpoints, forward_peers, reverse_peers)){
			WAN_CHMPRN("Failed to convert some parameters for chmpx information from configuration.");
			continue;
		}

		// search target chmpx in current server list
		bool			found = false;
		svrchmpxlist.Reset(basic_type::pAbsPtr->chmpx_servers, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From rel
		for(bool result = svrchmpxlist.ToFirst(); result; result = svrchmpxlist.ToNext(false, false, false, false)){
			// get structure for target chmpx
			chmpxlap	svrchmpx(svrchmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);	// Get CHMPX from Absolute
			CHMPXSVR	rawsvrchmpx;
			if(!svrchmpx.GetChmpxSvr(&rawsvrchmpx)){
				ERR_CHMPRN("Failed to get chmpxsvr structure.");
				continue;
			}

			// at first, check name because it will be converted some hostname and ip addresses
			std::string	foundname;
			strlst_t	node_list;
			foundname.clear();
			node_list.clear();
			node_list.push_back(rawsvrchmpx.name);
			if(IsInHostnameList(iter->name.c_str(), node_list, foundname, true)){
				// the target matched by name, then do compare other parameters
				if(	iter->ctlport == rawsvrchmpx.ctlport													&&
					0 == strncmp(cuk.c_str(), rawsvrchmpx.cuk, CUK_MAX)										&&
					0 == strncmp(custom_seed.c_str(), rawsvrchmpx.custom_seed, CUSTOM_ID_SEED_MAX)			&&
					compare_hostport_pairs(endpoints,		rawsvrchmpx.endpoints,		EXTERNAL_EP_MAX)	&&
					compare_hostport_pairs(ctlendpoints,	rawsvrchmpx.ctlendpoints,	EXTERNAL_EP_MAX)	&&
					compare_hostport_pairs(forward_peers,	rawsvrchmpx.forward_peers,	FORWARD_PEER_MAX)	&&
					compare_hostport_pairs(reverse_peers,	rawsvrchmpx.reverse_peers,	REVERSE_PEER_MAX)	)
				{
					found = true;
					break;
				}
			}
		}
		if(found){
			continue;
		}

		// add CHMPX
		chmpxlistlap	cursvrlist;
		chmpxlap		cursvrchmpx;
		CHMPXSSL		ssl;
		CVT_SSL_STRUCTURE(ssl, *iter);
		cursvrlist.Reset(freechmpxlist.PopFront(), basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);	// Get one CHMPXLIST from Absolute
		cursvrchmpx.Reset(cursvrlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);									// Get CHMPX from Absolute
		cursvrchmpx.InitializeServer(pchmcfg->chmpxid_type, iter->name.c_str(), pchmcfg->groupname.c_str(), iter->port, iter->ctlport, cuk.c_str(), custom_seed.c_str(), endpoints, ctlendpoints, forward_peers, reverse_peers, ssl);

		// inter CHMPX into servers list
		if(basic_type::pAbsPtr->chmpx_servers){
			svrchmpxlist.Reset(basic_type::pAbsPtr->chmpx_servers, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From rel
			svrchmpxlist.Insert(cursvrlist.GetAbsPtr(), true, pchmcfg->chmpxid_type);	// From abs

			basic_type::pAbsPtr->chmpx_server_count++;
			basic_type::pAbsPtr->chmpx_servers		= svrchmpxlist.GetFirstPtr(false);
		}else{
			cursvrlist.SaveChmpxIdMap();												// Set chmpxid map
			basic_type::pAbsPtr->chmpx_server_count	= 1;
			basic_type::pAbsPtr->chmpx_servers		= cursvrlist.GetRelPtr();
		}
		// [NOTE]
		// chmpx_bhash_count is 0, because InitializeServer sets status DOWN.

		// Reset free area
		if(0 < basic_type::pAbsPtr->chmpx_free_count){
			basic_type::pAbsPtr->chmpx_free_count--;
		}
		basic_type::pAbsPtr->chmpx_frees		= freechmpxlist.GetFirstPtr(false);		// Get Relative
	}

	// get self chmpxid
	chmpxid_t		selfchmpxid = CHM_INVALID_CHMPXID;
	if(IsServerMode()){
		selfchmpxid = GetSelfChmpxId();
	}

	// Retrieve SERVICEOUT/DOWN server in current server list which is not existed in configuration.
	svrchmpxlist.Reset(basic_type::pAbsPtr->chmpx_servers, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From rel
	for(bool result = svrchmpxlist.ToFirst(), topoflist = true; result; ){
		// target chmpx
		chmpxlap	svrchmpx(svrchmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);	// Get CHMPX from Absolute

		// check chmpxid
		if(selfchmpxid == svrchmpx.GetChmpxId()){
			// skip self chmpx
			result    = svrchmpxlist.ToNext(false, false, false, false);	// not cycle, all server(not only base hash servers, up/down servers), (not same server)
			topoflist = false;
			continue;
		}

		// check status
		chmpxsts_t	status = svrchmpx.GetStatus();
		if(!IS_CHMPXSTS_DOWN(status) || !IS_CHMPXSTS_SRVOUT(status) || !IS_CHMPXSTS_NOACT(status)){
			// Status is not service out and down, skip this.
			result    = svrchmpxlist.ToNext(false, false, false, false);	// not cycle, all server(not only base hash servers, up/down servers), (not same server)
			topoflist = false;
			continue;
		}

		// get structure for target chmpx
		CHMPXSVR	rawsvrchmpx;
		if(!svrchmpx.GetChmpxSvr(&rawsvrchmpx)){
			ERR_CHMPRN("Failed to get chmpxsvr structure.");
			result    = svrchmpxlist.ToNext(false, false, false, false);	// not cycle, all server(not only base hash servers, up/down servers), (not same server)
			topoflist = false;
			continue;
		}

		// search rawsvrchmpx from server list in configuration
		bool	found = false;
		for(iter = pchmcfg->servers.begin(); iter != pchmcfg->servers.end(); ++iter){
			// get parameters wich checking by chmpxid seed type
			std::string		cuk;
			std::string		custom_seed;
			CHMPXHP_RAWPAIR	endpoints[EXTERNAL_EP_MAX];
			CHMPXHP_RAWPAIR	ctlendpoints[EXTERNAL_EP_MAX];
			CHMPXHP_RAWPAIR	forward_peers[FORWARD_PEER_MAX];
			CHMPXHP_RAWPAIR	reverse_peers[REVERSE_PEER_MAX];
			if(!cvt_parameters_by_chmpxid_type(&(*iter), pchmcfg->chmpxid_type, cuk, custom_seed, endpoints, ctlendpoints, forward_peers, reverse_peers)){
				WAN_CHMPRN("Failed to convert some parameters for chmpx information from configuration.");
				continue;
			}

			// at first, check name because it will be converted some hostname and ip addresses
			std::string	foundname;
			strlst_t	node_list;
			foundname.clear();
			node_list.clear();
			node_list.push_back(rawsvrchmpx.name);
			if(IsInHostnameList(iter->name.c_str(), node_list, foundname, true)){
				// the target matched by name, then do compare other parameters
				if(	iter->ctlport == rawsvrchmpx.ctlport													&&
					0 == strncmp(cuk.c_str(), rawsvrchmpx.cuk, CUK_MAX)										&&
					0 == strncmp(custom_seed.c_str(), rawsvrchmpx.custom_seed, CUSTOM_ID_SEED_MAX)			&&
					compare_hostport_pairs(endpoints,		rawsvrchmpx.endpoints,		EXTERNAL_EP_MAX)	&&
					compare_hostport_pairs(ctlendpoints,	rawsvrchmpx.ctlendpoints,	EXTERNAL_EP_MAX)	&&
					compare_hostport_pairs(forward_peers,	rawsvrchmpx.forward_peers,	FORWARD_PEER_MAX)	&&
					compare_hostport_pairs(reverse_peers,	rawsvrchmpx.reverse_peers,	REVERSE_PEER_MAX)	)
				{
					found = true;
					break;
				}
			}
		}
		if(found){
			// found target chmpx, do not remove it
			result    = svrchmpxlist.ToNext(false, false, false, false);	// not cycle, all server(not only base hash servers, up/down servers), (not same server)
			topoflist = false;
			continue;
		}

		// Not found svrchmpx -> remove it from chmpxlist
		PCHMPXLIST	retrivelist;
		if(NULL == (retrivelist = svrchmpxlist.Retrieve())){
			// Failed to remove it, continue next.
			WAN_CHMPRN("Failed to remove CHMPX(0x%016" PRIx64 ") from CHMPXSVR, but continue...", svrchmpx.GetChmpxId());
			result    = svrchmpxlist.ToNext(false, false, false, false);	// not cycle, all server(not only base hash servers, up/down servers), (not same server)
			topoflist = false;
			continue;
		}

		// [NOTICE]
		// If Retrieve() is succeed, list current in list is set next point(or end of list)
		result = true;

		// For debug message
		chmpxlistlap	retrivechmpxlist(retrivelist, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, true);	// From Abs
		{
			chmpxlap	retrivechmpx(retrivechmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);						// Get CHMPX from Absolute
			MSG_CHMPRN("chmpxid(0x%016" PRIx64 ") with status(0x%016" PRIx64 ":%s) is retrieved.", retrivechmpx.GetChmpxId(), retrivechmpx.GetStatus(), STR_CHMPXSTS_FULL(retrivechmpx.GetStatus()).c_str());
		}
		retrivechmpxlist.Clear(eqfd);

		// Free chmpx list
		freechmpxlist.Reset(basic_type::pAbsPtr->chmpx_frees, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From Relative(allow NULL)
		if(!freechmpxlist.Push(retrivelist, true)){
			WAN_CHMPRN("Failed to push back new free CHMPXLIST to freelist, but continue...");
		}else{
			basic_type::pAbsPtr->chmpx_free_count++;
			basic_type::pAbsPtr->chmpx_frees = freechmpxlist.GetFirstPtr(false);
		}

		// Check and reset top chmpxlist
		if(topoflist){
			basic_type::pAbsPtr->chmpx_servers = svrchmpxlist.GetRelPtr();
		}
		if(0L < basic_type::pAbsPtr->chmpx_server_count){
			basic_type::pAbsPtr->chmpx_server_count--;
		}
	}

	return true;
}

template<typename T>
bool chmpxman_lap<T>::GetSelfChmpxSvr(PCHMPXSVR chmpxsvr) const
{
	if(!chmpxsvr){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return false;
	}
	chmpxlistlap	selfchmpxlist(basic_type::pAbsPtr->chmpx_self, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// Get CHMPXLIST from Relative
	chmpxlap		selfchmpx(selfchmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);	// Get CHMPX from Absolute
	return selfchmpx.GetChmpxSvr(chmpxsvr);
}

template<typename T>
bool chmpxman_lap<T>::GetChmpxSvr(chmpxid_t chmpxid, PCHMPXSVR chmpxsvr) const
{
	if(CHM_INVALID_CHMPXID == chmpxid || !chmpxsvr){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return false;
	}
	chmpxlistlap	svrchmpxlist(basic_type::pAbsPtr->chmpx_servers, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);// From rel
	if(!svrchmpxlist.Find(chmpxid)){
		MSG_CHMPRN("Could not find chmpxid(0x%016" PRIx64 ") in servers list.", chmpxid);
		return false;
	}
	chmpxlap		svrchmpx(svrchmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);		// Get CHMPX from Absolute
	return svrchmpx.GetChmpxSvr(chmpxsvr);
}

template<typename T>
bool chmpxman_lap<T>::GetChmpxSvrs(PCHMPXSVR* ppchmpxsvrs, long& count) const
{
	if(!ppchmpxsvrs){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return false;
	}

	if(0 == (count = basic_type::pAbsPtr->chmpx_server_count)){
		*ppchmpxsvrs = NULL;
		return false;
	}
	if(NULL == ((*ppchmpxsvrs) = reinterpret_cast<PCHMPXSVR>(malloc(sizeof(CHMPXSVR) * count)))){
		ERR_CHMPRN("Could not allocation memory.");
		return false;
	}
	chmpxlistlap	svrchmpxlist(basic_type::pAbsPtr->chmpx_servers, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From rel
	chmpxlap		svrchmpx;
	long			setpos = 0;
	do{
		svrchmpx.Reset(svrchmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);	// Get CHMPX from Absolute
		svrchmpx.GetChmpxSvr(&((*ppchmpxsvrs)[setpos++]));
	}while(svrchmpxlist.ToNext(false, false, false, false));	// not cycle, all server(not only base hash servers, up/down servers), (not same server)

	return true;
}

template<typename T>
bool chmpxman_lap<T>::MergeChmpxSvrs(PCHMPXSVR pchmpxsvrs, CHMPXID_SEED_TYPE type, long count, bool is_client_join, bool is_remove, bool is_init_process, int eqfd)
{
	if(!pchmpxsvrs || count <= 0L){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return false;
	}
	chmpxlistlap	svrchmpxlist(basic_type::pAbsPtr->chmpx_servers, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From rel
	chmpxid_t		selfchmpxid = CHM_INVALID_CHMPXID;
	if(IsServerMode()){
		selfchmpxid = GetSelfChmpxId();
	}
	bool	is_error = false;

	//
	// First) over write/insert chmpx status data to existed imdata
	//
	for(long cnt = 0; cnt < count; cnt++){
		if(svrchmpxlist.GetAbsPtr() && svrchmpxlist.Find(pchmpxsvrs[cnt].chmpxid)){
			//
			// Found chmpxid in imdata -> need to merge
			//
			chmpxlap	svrchmpx(svrchmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);	// Get CHMPX from Absolute

			// check status for self
			if(selfchmpxid == pchmpxsvrs[cnt].chmpxid && svrchmpx.GetStatus() != pchmpxsvrs[cnt].status){
				//
				// check self status changing
				//
				if(IS_CHMPXSTS_DOWN(pchmpxsvrs[cnt].status) && (is_init_process || CHM_INVALID_SOCK != svrchmpx.GetSock(SOCKTG_SELFSOCK))){
					// This case is that the server is up after the server have been down.
					//
					// [NOTICE]
					// You should service out the down server, before you up it.
					//
					if(IS_CHMPXSTS_SRVIN(pchmpxsvrs[cnt].status)){
						if(IS_CHMPXSTS_NOACT(pchmpxsvrs[cnt].status)){
							if(!is_client_join){
								// [SERVICE IN] [DOWN] [NOACT] [NOTHING] [SUSPEND] ---> [SERVICE IN] [UP] [NOACT] [NOTHING] [SUSPEND]
								WAN_CHMPRN("Self new chmpx status(0x%016" PRIx64 ":%s), so new status is CHMPXSTS_SRVIN_UP_NORMAL(suspend).", pchmpxsvrs[cnt].status, STR_CHMPXSTS_FULL(pchmpxsvrs[cnt].status).c_str());
								pchmpxsvrs[cnt].status = CHMPXSTS_SRVIN_UP_NORMAL;
							}else{
								// [SERVICE IN] [DOWN] [NOACT] [NOTHING] [NOSUSPEND] ---> [SERVICE IN] [UP] [NOACT] [PENDING] [NOSUSPEND]
								WAN_CHMPRN("Self new chmpx status(0x%016" PRIx64 ":%s), so new status is CHMPXSTS_SRVIN_UP_PENDING(nosuspend).", pchmpxsvrs[cnt].status, STR_CHMPXSTS_FULL(pchmpxsvrs[cnt].status).c_str());
								pchmpxsvrs[cnt].status = CHMPXSTS_SRVIN_UP_PENDING;
							}
						}else if(IS_CHMPXSTS_DELETE(pchmpxsvrs[cnt].status)){
							if(IS_CHMPXSTS_PENDING(pchmpxsvrs[cnt].status)){
								// [SERVICE IN] [DOWN] [DELETE] [PENDING] [SUSPEND/NOSUSPEND] ---> [SERVICE IN] [UP] [DELETE] [PENDING] [SUSPEND/NOSUSPEND]
								WAN_CHMPRN("Self new chmpx status(0x%016" PRIx64 ":%s), so new status is CHMPXSTS_SRVIN_UP_DELPENDING.", pchmpxsvrs[cnt].status, STR_CHMPXSTS_FULL(pchmpxsvrs[cnt].status).c_str());
								pchmpxsvrs[cnt].status = CHMPXSTS_SRVIN_UP_DELPENDING;
							}else{
								// [SERVICE IN] [DOWN] [DELETE] [DONE] [SUSPEND/NOSUSPEND] ---> [SERVICE IN] [UP] [DELETE] [DONE] [SUSPEND/NOSUSPEND]
								WAN_CHMPRN("Self new chmpx status(0x%016" PRIx64 ":%s), so new status is CHMPXSTS_SRVIN_UP_DELETED.", pchmpxsvrs[cnt].status, STR_CHMPXSTS_FULL(pchmpxsvrs[cnt].status).c_str());
								pchmpxsvrs[cnt].status = CHMPXSTS_SRVIN_UP_DELETED;
							}
						}

					}else if(IS_CHMPXSTS_SRVOUT(pchmpxsvrs[cnt].status)){
						// [SERVICE OUT] [DOWN] [NOACT] [NOTHING] [SUSPEND/NOSUSPEND] ---> [SERVICE OUT] [UP] [NOACT] [NOTHING] [SUSPEND/NOSUSPEND]
						WAN_CHMPRN("Self new chmpx status(0x%016" PRIx64 ":%s), so new status is CHMPXSTS_SRVOUT_UP_NORMAL.", pchmpxsvrs[cnt].status, STR_CHMPXSTS_FULL(pchmpxsvrs[cnt].status).c_str());
						pchmpxsvrs[cnt].status = CHMPXSTS_SRVOUT_UP_NORMAL;
					}else{
						// [SLAVE] [DOWN] [NOACT] [NOTHING] [SUSPEND/NOSUSPEND] ---> [SLAVE] [UP] [NOACT] [NOTHING] [SUSPEND/NOSUSPEND]
						WAN_CHMPRN("Self new chmpx status(0x%016" PRIx64 ":%s), so new status is CHMPXSTS_SLAVE_UP_NORMAL.", pchmpxsvrs[cnt].status, STR_CHMPXSTS_FULL(pchmpxsvrs[cnt].status).c_str());
						pchmpxsvrs[cnt].status = CHMPXSTS_SLAVE_UP_NORMAL;
					}
					// SET [SUSPEND/NOSUSPEND]
					CHANGE_CHMPXSTS_SUSPEND_FLAG(pchmpxsvrs[cnt].status, !is_client_join);

				}else if(IS_CHMPXSTS_UP(pchmpxsvrs[cnt].status) && !is_init_process && CHM_INVALID_SOCK == svrchmpx.GetSock(SOCKTG_SELFSOCK)){
					//
					// This case is impossible.
					// However, since there is a possibility of occurrence in communication in PX2PX, then confirm it and change to DOWN.
					//
					if(IS_CHMPXSTS_SRVIN(pchmpxsvrs[cnt].status)){
						if(IS_CHMPXSTS_NOACT(pchmpxsvrs[cnt].status)){
							// [SERVICE IN] [UP] [NOACT] [NOTHING/PENDING/DOING/DONE] [SUSPEND/NOSUSPEND] ---> [SERVICE IN] [DOWN] [NOACT] [NOTHING] [NOSUSPEND]
							WAN_CHMPRN("Self new chmpx status(0x%016" PRIx64 ":%s) is wrong, because self server is down, so new status is CHMPXSTS_SRVIN_DOWN_NORMAL.", pchmpxsvrs[cnt].status, STR_CHMPXSTS_FULL(pchmpxsvrs[cnt].status).c_str());
							pchmpxsvrs[cnt].status = CHMPXSTS_SRVIN_DOWN_NORMAL;

						}else if(IS_CHMPXSTS_ADD(pchmpxsvrs[cnt].status)){
							// [SERVICE IN] [UP] [ADD] [NOTHING/PENDING/DOING/DONE] [SUSPEND/NOSUSPEND] ---> [SERVICE OUT] [DOWN] [NOACT] [NOTHING] [NOSUSPEND]
							WAN_CHMPRN("Self new chmpx status(0x%016" PRIx64 ":%s) is wrong, because self server is down, so new status is CHMPXSTS_SRVOUT_DOWN_NORMAL.", pchmpxsvrs[cnt].status, STR_CHMPXSTS_FULL(pchmpxsvrs[cnt].status).c_str());
							pchmpxsvrs[cnt].status = CHMPXSTS_SRVOUT_DOWN_NORMAL;

						}else if(IS_CHMPXSTS_DELETE(pchmpxsvrs[cnt].status)){
							if(IS_CHMPXSTS_PENDING(pchmpxsvrs[cnt].status)){
								// [SERVICE IN] [UP] [DELETE] [PENDING] [SUSPEND/NOSUSPEND] ---> [SERVICE IN] [DOWN] [DELETE] [PENDING] [NOSUSPEND]
								WAN_CHMPRN("Self new chmpx status(0x%016" PRIx64 ":%s) is wrong, because self server is down, so new status is CHMPXSTS_SRVIN_DOWN_DELPENDING.", pchmpxsvrs[cnt].status, STR_CHMPXSTS_FULL(pchmpxsvrs[cnt].status).c_str());
								pchmpxsvrs[cnt].status = CHMPXSTS_SRVIN_DOWN_DELPENDING;
							}else{
								// [SERVICE IN] [UP] [DELETE] [DOING/DONE] [SUSPEND/NOSUSPEND] ---> [SERVICE IN] [DOWN] [DELETE] [DONE] [NOSUSPEND]
								WAN_CHMPRN("Self new chmpx status(0x%016" PRIx64 ":%s) is wrong, because self server is down, so new status is CHMPXSTS_SRVIN_DOWN_DELETED.", pchmpxsvrs[cnt].status, STR_CHMPXSTS_FULL(pchmpxsvrs[cnt].status).c_str());
								pchmpxsvrs[cnt].status = CHMPXSTS_SRVIN_DOWN_DELETED;
							}
						}

					}else if(IS_CHMPXSTS_SRVOUT(pchmpxsvrs[cnt].status)){
						// [SERVICE OUT] [UP] [NOACT] [NOTHING] [SUSPEND/NOSUSPEND] ---> [SERVICE OUT] [DOWN] [NOACT] [NOTHING] [NOSUSPEND]
						WAN_CHMPRN("Self new chmpx status(0x%016" PRIx64 ":%s) is wrong, because self server is down, so new status is CHMPXSTS_SRVOUT_UP_NORMAL.", pchmpxsvrs[cnt].status, STR_CHMPXSTS_FULL(pchmpxsvrs[cnt].status).c_str());
						pchmpxsvrs[cnt].status = CHMPXSTS_SRVOUT_UP_NORMAL;
					}else{
						// [SLAVE] [UP] [NOACT] [NOTHING] [SUSPEND/NOSUSPEND] ---> [SLAVE] [DOWN] [NOACT] [NOTHING] [NOSUSPEND]
						WAN_CHMPRN("Self new chmpx status(0x%016" PRIx64 ":%s) is wrong, because self server is down, so new status is CHMPXSTS_SLAVE_UP_NORMAL.", pchmpxsvrs[cnt].status, STR_CHMPXSTS_FULL(pchmpxsvrs[cnt].status).c_str());
						pchmpxsvrs[cnt].status = CHMPXSTS_SLAVE_UP_NORMAL;
					}
					// SET [NOSUSPEND](DOWN status server has always NOSUSPEND status, event if there is no joined client)
					SET_CHMPXSTS_NOSUP(pchmpxsvrs[cnt].status);
				}
			}
			// merge new status
			//
			// [NOTE]
			// Here, this method merges information of chmpx, but the base/pending hash
			// value can not be merged correctly.
			// Since there are addition and deletion of chmpx in the processing after here,
			// those hash values are recalculated after all processing is completed.
			//
			if(!svrchmpx.MergeChmpxSvr(&pchmpxsvrs[cnt], type, false, eqfd)){						// not force(port/ctlport)
				WAN_CHMPRN("Failed to merge CHMPX(%s: 0x%016" PRIx64 ") from CHMPXSVR, but continue...", pchmpxsvrs[cnt].name, pchmpxsvrs[cnt].chmpxid);
			}

		}else{
			//
			// Not found chmpxid in imdata -> insert new chmpx
			//
			if(basic_type::pAbsPtr->chmpx_free_count <= 0L){
				ERR_CHMPRN("No more free CHMPXLIST, but continue to process till end of this method for hash values.");
				is_error = true;
				continue;
			}
			if(CHMPXSTS_SRVOUT_DOWN_NORMAL == pchmpxsvrs[cnt].status){
				WAN_CHMPRN("CHMPX(%s: 0x%016" PRIx64 ") from CHMPXSVR is SERVICEOUT / DOWN status, thus it is not added server list.", pchmpxsvrs[cnt].name, pchmpxsvrs[cnt].chmpxid);

			}else{
				// make new chmpx(list) from free chmpx list
				chmpxlistlap	freechmpxlist(basic_type::pAbsPtr->chmpx_frees, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From Relative
				chmpxlistlap	newchmpxlist(freechmpxlist.PopAny(), basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);	// Get one CHMPXLIST from Absolute
				chmpxlap		newchmpx(newchmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);								// Get CHMPX from Absolute

				basic_type::pAbsPtr->chmpx_free_count--;
				basic_type::pAbsPtr->chmpx_frees = freechmpxlist.GetFirstPtr(false);

				newchmpxlist.Clear();

				if(!newchmpx.InitializeServer(&pchmpxsvrs[cnt], basic_type::pAbsPtr->group, type)){
					WAN_CHMPRN("Failed to initialize CHMPX(%s: 0x%016" PRIx64 ") from CHMPXSVR, but continue...", pchmpxsvrs[cnt].name, pchmpxsvrs[cnt].chmpxid);
				}

				if(svrchmpxlist.GetAbsPtr()){
					// insert
					svrchmpxlist.Insert(newchmpxlist.GetAbsPtr(), true, type);					// From abs
					basic_type::pAbsPtr->chmpx_server_count++;
					basic_type::pAbsPtr->chmpx_servers = svrchmpxlist.GetFirstPtr(false);
				}else{
					// insert new
					newchmpxlist.SaveChmpxIdMap();												// Set chmpxid map
					basic_type::pAbsPtr->chmpx_server_count	= 1;
					basic_type::pAbsPtr->chmpx_servers		= newchmpxlist.GetRelPtr();
				}
			}
		}
	}

	//
	// Next) remove not found chmpx from imdata
	//
	if(!is_error && is_remove){
		for(bool result = svrchmpxlist.ToFirst(); result; ){
			chmpxlap	svrchmpx(svrchmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);	// Get CHMPX from Absolute
			chmpxid_t	chmpxid = svrchmpx.GetChmpxId();
			if(chmpxid == selfchmpxid){
				// self chmpxid, skip this.
				result = svrchmpxlist.ToNext(false, false, false, false);		// not cycle, all server(not only base hash servers, up/down servers), (not same server)
				continue;
			}

			// check chmpxid
			long	chkcnt;
			for(chkcnt = 0; chkcnt < count; chkcnt++){
				if(chmpxid == pchmpxsvrs[chkcnt].chmpxid){
					break;
				}
			}
			if(chkcnt < count){
				// Found -> not remove
				result = svrchmpxlist.ToNext(false, false, false, false);		// not cycle, all server(not only base hash servers, up/down servers), (not same server)
				continue;
			}

			// Not found chmpxid -> remove this chmpxlist
			PCHMPXLIST	retrivelist;
			if(NULL == (retrivelist = svrchmpxlist.Retrieve())){
				// Failed to remove it, continue next.
				WAN_CHMPRN("Failed to remove CHMPX(0x%016" PRIx64 ") from CHMPXSVR, but continue...", chmpxid);
				is_error= true;
				result	= svrchmpxlist.ToNext(false, false, false, false);		// not cycle, all server(not only base hash servers, up/down servers), (not same server)
				continue;
			}
			// [NOTICE]
			// If Retrieve() is succeed, list current in list is set next point(or end of list)
			//
			result = true;

			// Remove Success
			chmpxlistlap	retrivechmpxlist(retrivelist, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, true);	// From Abs
			{
				// For debug message
				chmpxlap	retrivechmpx(retrivechmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);						// Get CHMPX from Absolute
				MSG_CHMPRN("chmpxid(0x%016" PRIx64 ") with status(0x%016" PRIx64 ":%s) is retrieved.", retrivechmpx.GetChmpxId(), retrivechmpx.GetStatus(), STR_CHMPXSTS_FULL(retrivechmpx.GetStatus()).c_str());
			}
			retrivechmpxlist.Clear(eqfd);

			chmpxlistlap	freechmpxlist(basic_type::pAbsPtr->chmpx_frees, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From Relative(allow NULL)
			if(!freechmpxlist.Push(retrivelist, true)){
				WAN_CHMPRN("Failed to push back new free CHMPXLIST to freelist, but continue...");
			}else{
				basic_type::pAbsPtr->chmpx_free_count++;
				basic_type::pAbsPtr->chmpx_frees = freechmpxlist.GetFirstPtr(false);
			}
			// [NOTICE]
			// decrement here, but setting pointer is after this loop.
			//
			if(0L < basic_type::pAbsPtr->chmpx_server_count){
				basic_type::pAbsPtr->chmpx_server_count--;
			}
		}

		// [NOTICE]
		// Set only pointer.
		//
		if(basic_type::pAbsPtr->chmpx_server_count <= 0L){
			basic_type::pAbsPtr->chmpx_servers = NULL;
		}else{
			basic_type::pAbsPtr->chmpx_servers = svrchmpxlist.GetFirstPtr(false);
		}
	}

	// [NOTE]
	// With the above processing, addition and deletion of chmpx servers and change of those
	// status are carried out.
	// Thus we must recalculate here to keep the proper hash value.
	//
	if(!UpdateHash(HASHTG_BOTH)){
		ERR_CHMPRN("Failed recalculating base/pending hash values after merging chmpx data(above processing is %s).", is_error ? "failed" : "succeed");
		is_error = true;
	}

	// set base hash server count
	basic_type::pAbsPtr->chmpx_bhash_count	= svrchmpxlist.BaseHashCount(true);
	// operating flag
	basic_type::pAbsPtr->is_operating		= svrchmpxlist.IsOperating(true);

	return !is_error;
}

template<typename T>
bool chmpxman_lap<T>::Clear(void)
{
	return Initialize();
}

template<typename T>
bool chmpxman_lap<T>::Close(int eqfd, int type)
{
	if(IS_CLOSETG_SERVERS(type) && basic_type::pAbsPtr->chmpx_servers){
		chmpxlistlap	svrchmpxlist(basic_type::pAbsPtr->chmpx_servers, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);// From rel
		for(bool result = svrchmpxlist.ToFirst(); result; result = svrchmpxlist.ToNext(false, false, false, false)){	// not cycle, all server(not only base hash servers, up/down servers), (not same server)
			chmpxlap	svrchmpx(svrchmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);	// Get CHMPX from Absolute
			svrchmpx.Close(eqfd);
		}
	}
	if(IS_CLOSETG_SLAVES(type) && basic_type::pAbsPtr->chmpx_slaves){
		chmpxlistlap	slvchmpxlist(basic_type::pAbsPtr->chmpx_slaves, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From rel
		for(bool result = slvchmpxlist.ToFirst(); result; result = slvchmpxlist.ToNext(false, false, false, false)){	// not cycle, all server(not only base hash servers, up/down servers), (not same server)
			chmpxlap	slvchmpx(slvchmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);	// Get CHMPX from Absolute
			slvchmpx.Close(eqfd);
		}
	}
	return true;
}

template<typename T>
bool chmpxman_lap<T>::GetGroup(std::string& group) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return false;
	}
	group = basic_type::pAbsPtr->group;
	return true;
}

template<typename T>
bool chmpxman_lap<T>::IsServerMode(chmpxid_t chmpxid) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return false;
	}
	chmpxlistlap	svrchmpxlist;
	if(CHM_INVALID_CHMPXID == chmpxid){
		svrchmpxlist.Reset(basic_type::pAbsPtr->chmpx_self, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// Get CHMPXLIST from Relative
		chmpxlap	selfchmpx(svrchmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);	// Get CHMPX from Absolute
		return selfchmpx.IsServerMode();
	}else{
		svrchmpxlist.Reset(basic_type::pAbsPtr->chmpx_servers, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);// From rel
		if(svrchmpxlist.Find(chmpxid)){
			// found in server list
			return true;
		}
	}
	return false;
}

template<typename T>
bool chmpxman_lap<T>::IsSlaveMode(chmpxid_t chmpxid) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return false;
	}
	chmpxlistlap	svrchmpxlist;
	if(CHM_INVALID_CHMPXID == chmpxid){
		svrchmpxlist.Reset(basic_type::pAbsPtr->chmpx_self, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// Get CHMPXLIST from Relative
		chmpxlap	selfchmpx(svrchmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);	// Get CHMPX from Absolute
		return selfchmpx.IsSlaveMode();
	}else{
		svrchmpxlist.Reset(basic_type::pAbsPtr->chmpx_slaves, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From rel
		if(svrchmpxlist.Find(chmpxid)){
			// found in slave list
			return true;
		}
	}
	return false;
}

template<typename T>
chmpxid_t chmpxman_lap<T>::GetSelfChmpxId(void) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return CHM_INVALID_CHMPXID;
	}
	chmpxlistlap	selfchmpxlist(basic_type::pAbsPtr->chmpx_self, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);// Get CHMPXLIST from Relative
	chmpxlap		selfchmpx(selfchmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);		// Get CHMPX from Absolute
	return selfchmpx.GetChmpxId();
}

template<typename T>
chmpxid_t chmpxman_lap<T>::GetNextRingChmpxId(chmpxid_t chmpxid) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return CHM_INVALID_CHMPXID;
	}
	if(!IsServerMode()){
		ERR_CHMPRN("Self chmpx does not server mode.");
		return CHM_INVALID_CHMPXID;
	}

	// set start position.
	chmpxlistlap	svrchmpxlist;
	if(CHM_INVALID_CHMPXID == chmpxid){
		// start self
		// [NOTICE] self is in servers list.
		svrchmpxlist.Reset(basic_type::pAbsPtr->chmpx_self, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From Relative
	}else{
		svrchmpxlist.Reset(basic_type::pAbsPtr->chmpx_servers, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);// From Relative
		if(!svrchmpxlist.Find(chmpxid)){
			ERR_CHMPRN("Could not find chmpxid(0x%016" PRIx64 ").", chmpxid);
			return CHM_INVALID_CHMPXID;
		}
	}

	if(svrchmpxlist.ToNext(true, false, true, false)){		// cycle, not only base hash servers, up servers, not allow same server
		chmpxlap	svrchmpx(svrchmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);	// Get CHMPX from Absolute
		return svrchmpx.GetChmpxId();
	}

	MSG_CHMPRN("Could not get next server in server list.");
	return CHM_INVALID_CHMPXID;
}

template<typename T>
long chmpxman_lap<T>::GetChmpxListCount(bool is_server) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return 0L;
	}
	return (is_server ? basic_type::pAbsPtr->chmpx_server_count : basic_type::pAbsPtr->chmpx_slave_count);
}

template<typename T>
long chmpxman_lap<T>::GetUpServerCount(void) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return 0L;
	}
	if(basic_type::pAbsPtr->chmpx_server_count <= 0 || !basic_type::pAbsPtr->chmpx_servers){
		// there is no server list.
		return 0L;
	}
	chmpxlistlap	svrchmpxlist(basic_type::pAbsPtr->chmpx_servers, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From rel
	chmpxidlist_t	list;
	return svrchmpxlist.GetChmpxIds(list, static_cast<chmpxsts_t>(CHMPXSTS_VAL_UP), false);
}

template<typename T>
chmpxpos_t chmpxman_lap<T>::GetSelfServerPos(void) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return CHM_INVALID_CHMPXLISTPOS;
	}
	return reinterpret_cast<chmpxpos_t>(basic_type::pAbsPtr->chmpx_self);		// result is rel
}

//
// If startpos is CHM_INVALID_CHMPXLISTPOS, get first pos in list.
//
template<typename T>
chmpxpos_t chmpxman_lap<T>::GetNextServerPos(chmpxpos_t startpos, chmpxpos_t nowpos, bool is_skip_self, bool is_cycle) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return CHM_INVALID_CHMPXLISTPOS;
	}
	if(basic_type::pAbsPtr->chmpx_server_count <= 0 || !basic_type::pAbsPtr->chmpx_servers){
		ERR_CHMPRN("There is no server in list.");
		return CHM_INVALID_CHMPXLISTPOS;
	}

	if(CHM_INVALID_CHMPXLISTPOS == startpos && CHM_INVALID_CHMPXLISTPOS != nowpos){
		startpos = reinterpret_cast<chmpxpos_t>(basic_type::pAbsPtr->chmpx_servers);
	}
	PCHMPXLIST	pStartPos	= reinterpret_cast<PCHMPXLIST>(startpos);
	PCHMPXLIST	pNowPos		= reinterpret_cast<PCHMPXLIST>(nowpos);

	// normalize
	chmpxlistlap	svrchmpxlist;
	if(CHM_INVALID_CHMPXLISTPOS == nowpos){
		svrchmpxlist.Reset(basic_type::pAbsPtr->chmpx_servers, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);// From rel
	}else{
		svrchmpxlist.Reset(pNowPos, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);							// From rel
		// next
		if(!svrchmpxlist.ToNext(is_cycle, false, false, true)){		// cycle(or not cycle), all server(not only base hash servers, up/down servers), allow same server
			MSG_CHMPRN("Reached end of server chmpx list.");
			return CHM_INVALID_CHMPXLISTPOS;
		}
	}
	if(pNowPos == svrchmpxlist.GetRelPtr() || pStartPos == svrchmpxlist.GetRelPtr()){
		MSG_CHMPRN("There is no more chmpx object in list.");
		return CHM_INVALID_CHMPXLISTPOS;
	}

	if(is_skip_self){
		// check self
		if(basic_type::pAbsPtr->chmpx_self == svrchmpxlist.GetRelPtr()){
			// found, skip self
			if(!svrchmpxlist.ToNext(is_cycle, false, false, true)){	// cycle(or not cycle), all server(not only base hash servers, up/down servers), allow same server
				MSG_CHMPRN("Reached end of server chmpx list.");
				return CHM_INVALID_CHMPXLISTPOS;
			}

			if(pNowPos == svrchmpxlist.GetRelPtr() || pStartPos == svrchmpxlist.GetRelPtr() || basic_type::pAbsPtr->chmpx_self == svrchmpxlist.GetRelPtr()){
				MSG_CHMPRN("There is no more chmpx object in list.");
				return CHM_INVALID_CHMPXLISTPOS;
			}
		}
	}
	return reinterpret_cast<chmpxpos_t>(svrchmpxlist.GetRelPtr());
}

//
// Check strictly or generosity to exist in hostname(IP address) in all server nodes.
//
// [NOTE]
// If both pctlport(and pcuk) is NULL, check for generosity.
// If both pctlport(and pcuk) is not NULL, do a strict check.
// In this case, if pssl is not NULL, the detected CHMPX SSL structure is copied.
//
template<typename T>
bool chmpxman_lap<T>::RawCheckContainsChmpxSvrs(const char* hostname, const short* pctlport, const char* pcuk, std::string* pnormalizedname, PCHMPXSSL pssl) const
{
	if(CHMEMPTYSTR(hostname)){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return false;
	}

	// strict
	bool		is_strict;
	std::string	strcuk;
	if(pctlport){
		is_strict = true;
		if(!CHMEMPTYSTR(pcuk)){
			// cppcheck-suppress nullPointer
			strcuk = pcuk;
		}
	}else{
		is_strict = false;
	}

	chmpxlistlap	svrchmpxlist(basic_type::pAbsPtr->chmpx_servers, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From Relative
	bool			result;
	for(result = svrchmpxlist.ToFirst(); result; result = svrchmpxlist.ToNext(false, false, false, false)){		// not cycle, all server(not only base hash servers, up/down servers), not allow same server
		chmpxlap		svrchmpx(svrchmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);	// Get CHMPX from Absolute
		std::string		name;
		chmpxid_t		chmpxid;
		short			port;
		short			ctlport;
		std::string		cuk;
		CHMPXHP_RAWPAIR	forward_peers[EXTERNAL_EP_MAX];

		chmpxid = CHM_INVALID_CHMPXID;
		if(!svrchmpx.Get(name, chmpxid, port, ctlport, &cuk, NULL, NULL, NULL, forward_peers, NULL)){
			WAN_CHMPRN("Could not get name/chmpxid/port/ctlport/forward_peers.");
			continue;
		}

		// check name
		strlst_t	node_list;
		std::string	globalname;
		node_list.clear();
		node_list.push_back(name);
		if(IsInHostnameList(hostname, node_list, globalname, true)){
			// matched
			if(!is_strict){
				if(pnormalizedname){
					*pnormalizedname = globalname;
				}
				if(pssl){
					svrchmpx.GetSslStructure(*pssl);
				}
				MSG_CHMPRN("Hostname(%s) is matched(not strictly) name(%s):chmpxid(0x%016" PRIx64 ") to globalname(%s).", hostname, name.c_str(), chmpxid, globalname.c_str());
				return true;
			}
			if(pctlport && (CHM_INVALID_PORT == *pctlport || *pctlport == ctlport) && IsMatchCuk(strcuk, cuk)){
				// strictly matched
				if(pnormalizedname){
					*pnormalizedname = globalname;
				}
				if(pssl){
					svrchmpx.GetSslStructure(*pssl);
				}
				MSG_CHMPRN("Hostname(%s) is matched(strictly) name(%s):chmpxid(0x%016" PRIx64 ") to globalname(%s).", hostname, name.c_str(), chmpxid, globalname.c_str());
				return true;
			}
		}

		// check forward peers
		node_list.clear();
		for(int cnt = 0; cnt < EXTERNAL_EP_MAX; ++cnt){
			if(CHMEMPTYSTR(forward_peers[cnt].name)){
				continue;
			}
			node_list.push_back(std::string(forward_peers[cnt].name));
		}
		if(IsInHostnameList(hostname, node_list, globalname, true)){
			// matched
			if(!is_strict){
				if(pnormalizedname){
					*pnormalizedname = globalname;
				}
				if(pssl){
					svrchmpx.GetSslStructure(*pssl);
				}
				MSG_CHMPRN("Hostname(%s) is matched(not strictly) forward peer in name(%s):chmpxid(0x%016" PRIx64 ") to globalname(%s).", hostname, name.c_str(), chmpxid, globalname.c_str());
				return true;
			}
			if((CHM_INVALID_PORT == *pctlport || *pctlport == ctlport) && IsMatchCuk(strcuk, cuk)){
				// strictly matched
				if(pnormalizedname){
					*pnormalizedname = globalname;
				}
				if(pssl){
					svrchmpx.GetSslStructure(*pssl);
				}
				MSG_CHMPRN("Hostname(%s) is matched(strictly) forward peer in name(%s):chmpxid(0x%016" PRIx64 ") to globalname(%s).", hostname, name.c_str(), chmpxid, globalname.c_str());
				return true;
			}
		}
	}
	return false;
}

//
// Check generosity to exist in hostname(IP address) in all server nodes.
//
template<typename T>
bool chmpxman_lap<T>::CheckContainsChmpxSvrs(const char* hostname) const
{
	return RawCheckContainsChmpxSvrs(hostname, NULL, NULL, NULL, NULL);
}

//
// Check strictly to exist in hostname(IP address) in all server nodes.
//
template<typename T>
bool chmpxman_lap<T>::CheckStrictlyContainsChmpxSvrs(const char* hostname, const short* pctlport, const char* pcuk, std::string* pnormalizedname, PCHMPXSSL pssl) const
{
	if(!pctlport){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	return RawCheckContainsChmpxSvrs(hostname, pctlport, pcuk, pnormalizedname, pssl);
}

template<typename T>
bool chmpxman_lap<T>::IsServerChmpxId(chmpxid_t chmpxid) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return false;
	}
	chmpxlistlap	svrchmpxlist(basic_type::pAbsPtr->chmpx_servers, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From rel
	return svrchmpxlist.Find(chmpxid);
}

//
// This method searches liner in list, so it is not good performance.
//
template<typename T>
chmpxid_t chmpxman_lap<T>::GetChmpxIdBySock(int sock, int type) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return CHM_INVALID_CHMPXID;
	}

	PCHMPX	abschmpx = NULL;
	if(IS_CLOSETG_SERVERS(type)){
		chmpxlistlap	svrchmpxlist(basic_type::pAbsPtr->chmpx_servers, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);// From rel
		if(svrchmpxlist.Find(sock)){
			abschmpx = svrchmpxlist.GetAbsChmpxPtr();
		}
	}
	if(!abschmpx && IS_CLOSETG_SLAVES(type)){
		chmpxlistlap	slvchmpxlist(basic_type::pAbsPtr->chmpx_slaves, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From rel
		if(slvchmpxlist.Find(sock)){
			abschmpx = slvchmpxlist.GetAbsChmpxPtr();
		}
	}
	if(!abschmpx){
		return CHM_INVALID_CHMPXID;
	}
	chmpxlap	svrchmpx(abschmpx, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);	// Get CHMPX from Absolute
	return svrchmpx.GetChmpxId();
}

template<typename T>
chmpxid_t chmpxman_lap<T>::GetChmpxIdByToServerName(CHMPXID_SEED_TYPE type, const char* hostname, short ctlport, const char* cuk, const char* ctlendpoints, const char* custom_seed) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return CHM_INVALID_CHMPXID;
	}

	chmpxid_t	chmpxid;
	if(CHMEMPTYSTR(hostname) || CHM_INVALID_CHMPXID == (chmpxid = MakeChmpxId(basic_type::pAbsPtr->group, type, hostname, ctlport, cuk, ctlendpoints, custom_seed))){
		ERR_CHMPRN("Parameter are wrong.");
		return CHM_INVALID_CHMPXID;
	}
	chmpxlistlap	svrchmpxlist(basic_type::pAbsPtr->chmpx_servers, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From rel
	if(!svrchmpxlist.Find(chmpxid)){
		return CHM_INVALID_CHMPXID;
	}
	return chmpxid;
}

//
// This method is linear search in list, then it is not good performance.
//
template<typename T>
chmpxid_t chmpxman_lap<T>::GetChmpxIdByStatus(chmpxsts_t status, bool part_match) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return CHM_INVALID_CHMPXID;
	}
	chmpxlistlap	svrchmpxlist(basic_type::pAbsPtr->chmpx_servers, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From rel
	return svrchmpxlist.FindByStatus(status, part_match);
}

template<typename T>
chmpxid_t chmpxman_lap<T>::GetRandomServerChmpxId(bool without_suspend)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return CHM_INVALID_CHMPXID;
	}
	if(0 == basic_type::pAbsPtr->chmpx_bhash_count){
		ERR_CHMPRN("There is no up servers.");
		return CHM_INVALID_CHMPXID;
	}
	chmpxlistlap	svrchmpxlist(basic_type::pAbsPtr->chmpx_servers, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From rel

	if(CHM_INVALID_CHMPXID == basic_type::pAbsPtr->last_chmpxid){
		MSG_CHMPRN("last random chmpxid is not initialized by valid id, so using first chmpxid.");
	}else{
		if(!svrchmpxlist.Find(basic_type::pAbsPtr->last_chmpxid)){
			WAN_CHMPRN("last random chmpxid(0x%016" PRIx64 ") is not found, so using first chmpxid.", basic_type::pAbsPtr->last_chmpxid);
		}else{
			// Found last_chmpxid in list, then skip current
			if(!svrchmpxlist.ToNextUpServicing(true, true, without_suspend)){		// get strict servicing server
				ERR_CHMPRN("Failed to get next chmpxid by last random chmpxid(0x%016" PRIx64 "), so using first chmpxid.", basic_type::pAbsPtr->last_chmpxid);
			}
		}
	}

	// get next server from current server
	if(!svrchmpxlist.ToNextUpServicing(true, true, without_suspend)){				// get strict servicing server
		ERR_CHMPRN("Failed to get next chmpxid for random chmpxid.");
		return CHM_INVALID_CHMPXID;
	}
	// Set last chmpxid
	chmpxlap	svrchmpx(svrchmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);		// Get CHMPX from Absolute
	basic_type::pAbsPtr->last_chmpxid = svrchmpx.GetChmpxId();

	return basic_type::pAbsPtr->last_chmpxid;
}

//
// This method returns only MAIN chmpxid.
//
template<typename T>
chmpxid_t chmpxman_lap<T>::GetServerChmpxIdByHash(chmhash_t hash) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return CHM_INVALID_CHMPXID;
	}
	if(0 == basic_type::pAbsPtr->chmpx_bhash_count){
		ERR_CHMPRN("There is no up servers.");
		return CHM_INVALID_CHMPXID;
	}
	PCHMPX*	abs_base_arr	= AbsBaseArr();
	PCHMPX*	abs_pend_arr	= AbsPendArr();

	if(!abs_base_arr || !abs_pend_arr){
		ERR_CHMPRN("based(pending) hash array pointer is NULL.");
		return CHM_INVALID_CHMPXID;
	}

	// normalize for base hash
	chmpxlap	svrchmpx;
	chmhash_t	base_hash = hash % static_cast<chmhash_t>(basic_type::pAbsPtr->chmpx_bhash_count);

	if(abs_base_arr[base_hash]){
		svrchmpx.Reset(abs_base_arr[base_hash], abs_base_arr, abs_pend_arr, AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From rel
	}else{
		// Something wrong, but try to search liner.(not good performance)
		WAN_CHMPRN("There is no chmpxid in base hashed array for hash(0x%016" PRIx64 ") - base hash(0x%016" PRIx64 "), but retry to do by liner searching.", hash, base_hash);

		chmpxlistlap	svrchmpxlist(basic_type::pAbsPtr->chmpx_servers, basic_type::pAbsPtr->chmpxid_map, abs_base_arr, abs_pend_arr, AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From rel
		if(!svrchmpxlist.FindByHash(base_hash, true)){
			ERR_CHMPRN("There is no chmpxid in base hashed array and list for hash(0x%016" PRIx64 ") - base hash(0x%016" PRIx64 ").", hash, base_hash);
			return CHM_INVALID_CHMPXID;
		}
		svrchmpx.Reset(svrchmpxlist.GetAbsChmpxPtr(), abs_base_arr, abs_pend_arr, AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);	// Get CHMPX from Absolute
	}

	basic_type::pAbsPtr->last_chmpxid = svrchmpx.GetChmpxId();	// for random

	return basic_type::pAbsPtr->last_chmpxid;
}

//
// Return		true / false(no target)
// chmpxids		Set all chmpxid( for replication )
//				If operating and with_pending is true, this value includes other chmpxid by pending hash.
// basehashs	Set all chmhash( for replication by manual )
//				If operating and with_pending is true, this value includes other chmpx's chmhash.
//
template<typename T>
bool chmpxman_lap<T>::GetServerChmpxIdAndBaseHashByHashs(chmhash_t hash, chmpxidlist_t& chmpxids, chmhashlist_t& basehashs, bool with_pending, bool without_down, bool without_suspend)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return false;
	}
	if(0 == basic_type::pAbsPtr->chmpx_bhash_count){
		ERR_CHMPRN("There is no up servers.");
		return false;
	}
	PCHMPX*	abs_base_arr	= AbsBaseArr();
	PCHMPX*	abs_pend_arr	= AbsPendArr();

	if(!abs_base_arr || !abs_pend_arr){
		ERR_CHMPRN("based(pending) hash array pointer is NULL.");
		return false;
	}
	chmpxids.clear();
	basehashs.clear();

	// normalize for target hash
	chmpxid_t		last_chmpxid		= CHM_INVALID_CHMPXID;		// for set last used chmpxid
	chmhash_t		hash_maxval			= 0;						// count of chmpx has base/pending hash value
	chmpxidmap_t	chmpxidmap;										// for the prevention of duplicate registration
	chmhashmap_t	chmhashmap;										// for the prevention of duplicate registration
	chmpxlistlap	svrchmpxlist;
	chmpxlap		svrchmpx;
	chmpxid_t		tmpchmpxid			= CHM_INVALID_CHMPXID;
	chmpxsts_t		status				= 0;

	// For Base hash
	hash_maxval = static_cast<chmhash_t>(basic_type::pAbsPtr->chmpx_bhash_count);
	if(0 == hash_maxval){
		ERR_CHMPRN("There is no up servers for base hash.");
	}else{
		chmhash_t	target_start_hash	= hash % hash_maxval;		// target hash value for base hash count from hash parameter
		for(long cnt = 0; cnt < (basic_type::pAbsPtr->replica_count + 1); cnt++){
			chmhash_t	target_repl_hash;							// target hash value for pending hash count from hash parameter
			chmhash_t	chmpx_base_hash;							// chmpx base hash value
			chmhash_t	chmpx_pending_hash;							// chmpx pending hash value

			target_repl_hash = (target_start_hash + static_cast<chmhash_t>(cnt)) % hash_maxval;

			// get chmpx by base hash value
			if(abs_base_arr[target_repl_hash]){
				svrchmpx.Reset(abs_base_arr[target_repl_hash], abs_base_arr, abs_pend_arr, AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From rel
			}else{
				// Something wrong, but try to search liner.(not good performance)
				WAN_CHMPRN("There is no chmpxid in base hashed array for hash(0x%016" PRIx64 ") - base hash(0x%016" PRIx64 "), but retry to do by liner searching.", hash, target_repl_hash);

				svrchmpxlist.Reset(basic_type::pAbsPtr->chmpx_servers, basic_type::pAbsPtr->chmpxid_map, abs_base_arr, abs_pend_arr, AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From rel
				if(!svrchmpxlist.FindByHash(target_repl_hash, true)){
					ERR_CHMPRN("There is no chmpxid in base hashed array and list for hash(0x%016" PRIx64 ") - base hash(0x%016" PRIx64 ").", hash, target_repl_hash);
					continue;
				}
				svrchmpx.Reset(svrchmpxlist.GetAbsChmpxPtr(), abs_base_arr, abs_pend_arr, AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);	// Get CHMPX from Absolute
			}

			// check
			if(CHM_INVALID_CHMPXID == (tmpchmpxid = svrchmpx.GetChmpxId())){
				ERR_CHMPRN("Base hash(0x%016" PRIx64 ") object does not have chmpxid(not initialized).", target_repl_hash);
				continue;
			}
			chmpx_base_hash		= CHM_INVALID_HASHVAL;
			chmpx_pending_hash	= CHM_INVALID_HASHVAL;
			if(!svrchmpx.Get(chmpx_base_hash, chmpx_pending_hash) || CHM_INVALID_HASHVAL == chmpx_base_hash){
				//MSG_CHMPRN("Failed to get base and pending hash value for Base hash(0x%016" PRIx64 ") object.", target_repl_hash);
				continue;
			}
			status = svrchmpx.GetStatus();
			if(without_down && IS_CHMPXSTS_DOWN(status)){
				//MSG_CHMPRN("Base hash(0x%016" PRIx64 ") object is chmpxid(0x%016" PRIx64 "), but this server is DOWN.", target_repl_hash, tmpchmpxid);
				continue;
			}
			if(without_suspend && IS_CHMPXSTS_SUSPEND(status)){
				//MSG_CHMPRN("Base hash(0x%016" PRIx64 ") object is chmpxid(0x%016" PRIx64 "), but this server is SUSPEND.", target_repl_hash, tmpchmpxid);
				continue;
			}

			// register
			if(chmpxidmap.end() == chmpxidmap.find(tmpchmpxid)){				// check duplicate registration
				chmpxidmap[tmpchmpxid]	= true;
				chmpxids.push_back(tmpchmpxid);
			}else{
				//MSG_CHMPRN("chmpx(0x%016" PRIx64 ") is already registered.", tmpchmpxid);
			}
			if(chmhashmap.end() == chmhashmap.find(chmpx_base_hash)){			// check duplicate registration
				chmhashmap[chmpx_base_hash] = true;
				basehashs.push_back(chmpx_base_hash);
			}else{
				//MSG_CHMPRN("chmhash(0x%016" PRIx64 ") is already registered.", chmpx_base_hash);
			}

			// backup lastest chmpxid for random
			last_chmpxid = tmpchmpxid;
		}
	}

	// For pending hash(if operating, adding other chmpxids)
	if(with_pending && chmpxman_lap<T>::IsOperating()){
		svrchmpxlist.Reset(basic_type::pAbsPtr->chmpx_servers, basic_type::pAbsPtr->chmpxid_map, abs_base_arr, abs_pend_arr, AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From rel
		hash_maxval = static_cast<chmhash_t>(svrchmpxlist.PendingHashCount());	// svrchmpxlist is top of list

		if(0 == hash_maxval){
			ERR_CHMPRN("There is no up servers for pending hash.");
		}else{
			chmhash_t	target_start_hash	= hash % hash_maxval;		// target hash value for base hash count from hash parameter
			for(long cnt = 0; cnt < (basic_type::pAbsPtr->replica_count + 1); cnt++){
				chmhash_t	target_repl_hash;							// target hash value for pending hash count from hash parameter
				chmhash_t	chmpx_base_hash;							// chmpx base hash value
				chmhash_t	chmpx_pending_hash;							// chmpx pending hash value

				target_repl_hash = (target_start_hash + static_cast<chmhash_t>(cnt)) % hash_maxval;

				// get chmpx by pending hash value
				if(abs_pend_arr[target_repl_hash]){
					svrchmpx.Reset(abs_pend_arr[target_repl_hash], abs_base_arr, abs_pend_arr, AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From rel
				}else{
					// Something wrong, but try to search liner.(not good performance)
					WAN_CHMPRN("There is no chmpxid in pending hashed array for hash(0x%016" PRIx64 ") - pending hash(0x%016" PRIx64 "), but retry to do by liner searching.", hash, target_repl_hash);

					svrchmpxlist.Reset(basic_type::pAbsPtr->chmpx_servers, basic_type::pAbsPtr->chmpxid_map, abs_base_arr, abs_pend_arr, AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From rel
					if(!svrchmpxlist.FindByHash(target_repl_hash, false)){
						ERR_CHMPRN("There is no chmpxid in pending hashed array and list for hash(0x%016" PRIx64 ") - pending hash(0x%016" PRIx64 ").", hash, target_repl_hash);
						continue;
					}
					svrchmpx.Reset(svrchmpxlist.GetAbsChmpxPtr(), abs_base_arr, abs_pend_arr, AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);	// Get CHMPX from Absolute
				}

				// check
				if(CHM_INVALID_CHMPXID == (tmpchmpxid = svrchmpx.GetChmpxId())){
					ERR_CHMPRN("Pending hash(0x%016" PRIx64 ") object does not have chmpxid(not initialized).", target_repl_hash);
					continue;
				}
				chmpx_base_hash		= CHM_INVALID_HASHVAL;
				chmpx_pending_hash	= CHM_INVALID_HASHVAL;
				if(!svrchmpx.Get(chmpx_base_hash, chmpx_pending_hash) || CHM_INVALID_HASHVAL == chmpx_pending_hash){
					//MSG_CHMPRN("Failed to get base and pending hash value for Base hash(0x%016" PRIx64 ") object.", target_repl_hash);
					continue;
				}
				status = svrchmpx.GetStatus();
				if(without_down && IS_CHMPXSTS_DOWN(status)){
					//MSG_CHMPRN("Pending hash(0x%016" PRIx64 ") object is chmpxid(0x%016" PRIx64 "), but this server is DOWN.", target_repl_hash, tmpchmpxid);
					continue;
				}
				if(without_suspend && IS_CHMPXSTS_SUSPEND(status)){
					//MSG_CHMPRN("Pending hash(0x%016" PRIx64 ") object is chmpxid(0x%016" PRIx64 "), but this server is SUSPEND.", target_repl_hash, tmpchmpxid);
					continue;
				}

				// register
				if(chmpxidmap.end() == chmpxidmap.find(tmpchmpxid)){			// check duplicate registration
					chmpxidmap[tmpchmpxid]	= true;
					chmpxids.push_back(tmpchmpxid);
				}else{
					//MSG_CHMPRN("chmpx(0x%016" PRIx64 ") is already registered.", tmpchmpxid);
				}
				if(chmhashmap.end() == chmhashmap.find(chmpx_pending_hash)){	// check duplicate registration
					chmhashmap[chmpx_pending_hash] = true;
					basehashs.push_back(chmpx_pending_hash);
				}else{
					//MSG_CHMPRN("chmhash(0x%016" PRIx64 ") is already registered.", chmpx_base_hash);
				}
			}
		}
	}

	if(CHM_INVALID_CHMPXID != last_chmpxid){
		basic_type::pAbsPtr->last_chmpxid = last_chmpxid;						// for random
	}
	return (!chmpxids.empty());
}

//
// Return		true / false(no target)
// basehashs	Set all chmhash( for replication by manual )
//				If operating and with_pending is true, this value includes other chmpx's chmhash.
//
template<typename T>
bool chmpxman_lap<T>::GetServerChmHashsByHashs(chmhash_t hash, chmhashlist_t& basehashs, bool with_pending, bool without_down, bool without_suspend)
{
	chmpxidlist_t	chmpxids;		// not use
	return GetServerChmpxIdAndBaseHashByHashs(hash, chmpxids, basehashs, with_pending, without_down, without_suspend);
}

//
// Return		true / false(no target)
// chmpxids		Set all chmpxid( for replication )
//				If operating and with_pending is true, this value includes other chmpxid by pending hash.
//
template<typename T>
bool chmpxman_lap<T>::GetServerChmpxIdByHashs(chmhash_t hash, chmpxidlist_t& chmpxids, bool with_pending, bool without_down, bool without_suspend)
{
	chmhashlist_t	basehashs;		// not use
	return GetServerChmpxIdAndBaseHashByHashs(hash, chmpxids, basehashs, with_pending, without_down, without_suspend);
}

template<typename T>
long chmpxman_lap<T>::GetServerChmpxIds(chmpxidlist_t& list, bool with_pending, bool without_down, bool without_suspend) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return 0L;
	}
	if(!basic_type::pAbsPtr->chmpx_servers){
		return 0L;
	}
	chmpxlistlap	svrchmpxlist(basic_type::pAbsPtr->chmpx_servers, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From rel
	return svrchmpxlist.GetChmpxIds(list, with_pending, without_down, without_suspend);
}

template<typename T>
bool chmpxman_lap<T>::GetServerBase(chmpxpos_t pos, std::string* pname, chmpxid_t* pchmpxid, short* pport, short* pctlport, hostport_list_t* pendpoints, hostport_list_t* pctlendpoints) const
{
	if(CHM_INVALID_CHMPXLISTPOS == pos){
		ERR_CHMPRN("pos is invalid.");
		return false;
	}
	PCHMPXLIST		pchmpxlist = reinterpret_cast<PCHMPXLIST>(pos);
	chmpxlistlap	svrchmpxlist(pchmpxlist, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From rel
	chmpxlap		svrchmpx(svrchmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);							// Get CHMPX from Absolute

	std::string		name;
	chmpxid_t		chmpxid;
	short			port;
	short			ctlport;
	CHMPXHP_RAWPAIR	endpoints[EXTERNAL_EP_MAX];
	CHMPXHP_RAWPAIR	ctlendpoints[EXTERNAL_EP_MAX];
	if(!svrchmpx.Get(name, chmpxid, port, ctlport, NULL, NULL, endpoints, ctlendpoints, NULL, NULL)){
		return false;
	}
	if(pname){
		*pname		= name;
	}
	if(pchmpxid){
		*pchmpxid	= chmpxid;
	}
	if(pport){
		*pport		= port;
	}
	if(pctlport){
		*pctlport	= ctlport;
	}
	if(pendpoints){
		rev_hostport_pairs(*pendpoints,		endpoints,		EXTERNAL_EP_MAX);
	}
	if(pctlendpoints){
		rev_hostport_pairs(*pctlendpoints,	ctlendpoints,	EXTERNAL_EP_MAX);
	}
	return true;
}

template<typename T>
bool chmpxman_lap<T>::GetServerBase(chmpxid_t chmpxid, std::string* pname, short* pport, short* pctlport, hostport_list_t* pendpoints, hostport_list_t* pctlendpoints) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return false;
	}
	if(!basic_type::pAbsPtr->chmpx_servers){
		return false;
	}
	chmpxlistlap	svrchmpxlist(basic_type::pAbsPtr->chmpx_servers, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From rel
	if(!svrchmpxlist.Find(chmpxid)){
		ERR_CHMPRN("Could not find chmpxid(0x%016" PRIx64 ").", chmpxid);
		return false;
	}
	chmpxlap		svrchmpx(svrchmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);		// Get CHMPX from Absolute

	std::string		name;
	short			port;
	short			ctlport;
	CHMPXHP_RAWPAIR	endpoints[EXTERNAL_EP_MAX];
	CHMPXHP_RAWPAIR	ctlendpoints[EXTERNAL_EP_MAX];
	if(!svrchmpx.Get(name, chmpxid, port, ctlport, NULL, NULL, endpoints, ctlendpoints, NULL, NULL)){
		return false;
	}
	if(pname){
		*pname		= name;
	}
	if(pport){
		*pport		= port;
	}
	if(pctlport){
		*pctlport	= ctlport;
	}
	if(pendpoints){
		rev_hostport_pairs(*pendpoints,		endpoints,		EXTERNAL_EP_MAX);
	}
	if(pctlendpoints){
		rev_hostport_pairs(*pctlendpoints,	ctlendpoints,	EXTERNAL_EP_MAX);
	}
	return true;
}

template<typename T>
bool chmpxman_lap<T>::GetServerBase(chmpxid_t chmpxid, CHMPXSSL& ssl) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return false;
	}
	if(!basic_type::pAbsPtr->chmpx_servers){
		return false;
	}
	chmpxlistlap	svrchmpxlist(basic_type::pAbsPtr->chmpx_servers, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From rel
	if(!svrchmpxlist.Find(chmpxid)){
		ERR_CHMPRN("Could not find chmpxid(0x%016" PRIx64 ").", chmpxid);
		return false;
	}
	chmpxlap	svrchmpx(svrchmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);		// Get CHMPX from Absolute
	return svrchmpx.GetSslStructure(ssl);
}

template<typename T>
bool chmpxman_lap<T>::GetServerSocks(chmpxid_t chmpxid, socklist_t& socklist, int& ctlsock) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return false;
	}
	if(!basic_type::pAbsPtr->chmpx_servers){
		return false;
	}
	chmpxlistlap	svrchmpxlist(basic_type::pAbsPtr->chmpx_servers, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From rel
	if(!svrchmpxlist.Find(chmpxid)){
		//MSG_CHMPRN("Could not find chmpxid(0x%016" PRIx64 ").", chmpxid);
		return false;
	}
	chmpxlap	svrchmpx(svrchmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);	// Get CHMPX from Absolute
	int			selfsock;
	int			selfctlsock;
	return svrchmpx.Get(socklist, ctlsock, selfsock, selfctlsock);
}

template<typename T>
bool chmpxman_lap<T>::GetServerHash(chmpxid_t chmpxid, chmhash_t& base, chmhash_t& pending) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return false;
	}
	if(!basic_type::pAbsPtr->chmpx_servers){
		return false;
	}
	chmpxlistlap	svrchmpxlist(basic_type::pAbsPtr->chmpx_servers, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From rel
	if(!svrchmpxlist.Find(chmpxid)){
		ERR_CHMPRN("Could not find chmpxid(0x%016" PRIx64 ").", chmpxid);
		return false;
	}
	chmpxlap	svrchmpx(svrchmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);		// Get CHMPX from Absolute
	return svrchmpx.Get(base, pending);
}

template<typename T>
bool chmpxman_lap<T>::GetMaxHashCount(chmhash_t& basehash, chmhash_t& pengindhash) const
{
	basehash	= 0;
	pengindhash	= 0;

	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return false;
	}
	chmpxlistlap	svrchmpxlist(basic_type::pAbsPtr->chmpx_servers, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From rel
	basehash	= static_cast<chmhash_t>(basic_type::pAbsPtr->chmpx_bhash_count);
	pengindhash	= static_cast<chmhash_t>(svrchmpxlist.PendingHashCount());		// svrchmpxlist is top of list
	return true;
}

template<typename T>
chmpxsts_t chmpxman_lap<T>::GetServerStatus(chmpxid_t chmpxid) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return CHMPXSTS_SRVOUT_DOWN_NORMAL;
	}
	if(!basic_type::pAbsPtr->chmpx_servers){
		return false;
	}
	chmpxlistlap	svrchmpxlist(basic_type::pAbsPtr->chmpx_servers, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From rel
	if(!svrchmpxlist.Find(chmpxid)){
		ERR_CHMPRN("Could not find chmpxid(0x%016" PRIx64 ").", chmpxid);
		return CHMPXSTS_SRVOUT_DOWN_NORMAL;
	}
	chmpxlap	svrchmpx(svrchmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);		// Get CHMPX from Absolute
	return svrchmpx.GetStatus();
}

template<typename T>
bool chmpxman_lap<T>::GetSelfPorts(short& port, short& ctlport) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return false;
	}
	chmpxlistlap	selfchmpxlist(basic_type::pAbsPtr->chmpx_self, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// Get CHMPXLIST from Relative
	chmpxlap		selfchmpx(selfchmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);	// Get CHMPX from Absolute
	std::string		name;		// tmp
	chmpxid_t		chmpxid;	// tmp

	return selfchmpx.Get(name, chmpxid, port, ctlport);
}

template<typename T>
bool chmpxman_lap<T>::GetSelfSocks(int& sock, int& ctlsock) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return false;
	}
	chmpxlistlap	selfchmpxlist(basic_type::pAbsPtr->chmpx_self, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// Get CHMPXLIST from Relative
	chmpxlap		selfchmpx(selfchmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);	// Get CHMPX from Absolute
	socklist_t		tmpsocklist;
	int				tmpctlsock;
	return selfchmpx.Get(tmpsocklist, tmpctlsock, sock, ctlsock);
}

template<typename T>
bool chmpxman_lap<T>::GetSelfHash(chmhash_t& base, chmhash_t& pending) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return false;
	}
	chmpxlistlap	selfchmpxlist(basic_type::pAbsPtr->chmpx_self, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// Get CHMPXLIST from Relative
	chmpxlap		selfchmpx(selfchmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);	// Get CHMPX from Absolute
	return selfchmpx.Get(base, pending);
}

template<typename T>
chmpxsts_t chmpxman_lap<T>::GetSelfStatus(void) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return CHMPXSTS_SLAVE_DOWN_NORMAL;
	}
	chmpxlistlap	selfchmpxlist(basic_type::pAbsPtr->chmpx_self, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// Get CHMPXLIST from Relative
	chmpxlap		selfchmpx(selfchmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);	// Get CHMPX from Absolute
	return selfchmpx.GetStatus();
}

template<typename T>
bool chmpxman_lap<T>::GetSelfSsl(CHMPXSSL& ssl) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return false;
	}
	chmpxlistlap	selfchmpxlist(basic_type::pAbsPtr->chmpx_self, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// Get CHMPXLIST from Relative
	chmpxlap		selfchmpx(selfchmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);	// Get CHMPX from Absolute
	return selfchmpx.GetSslStructure(ssl);
}

template<typename T>
bool chmpxman_lap<T>::GetSelfBase(std::string* pname, short* pport, short* pctlport, std::string* pcuk, std::string* pcustom_seed, hostport_list_t* pendpoints, hostport_list_t* pctlendpoints, hostport_list_t* pforward_peers, hostport_list_t* preverse_peers) const
{
	if(!pname && !pport && !pctlport && !pcuk && !pcustom_seed && !pendpoints && !pctlendpoints && !pforward_peers && !preverse_peers){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return false;
	}
	chmpxlistlap	selfchmpxlist(basic_type::pAbsPtr->chmpx_self, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// Get CHMPXLIST from Relative
	chmpxlap		selfchmpx(selfchmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);	// Get CHMPX from Absolute

	std::string		name;
	chmpxid_t		chmpxid;	// tmp
	short			port;
	short			ctlport;
	std::string		cuk;
	std::string		custom_seed;
	CHMPXHP_RAWPAIR	endpoints[EXTERNAL_EP_MAX];
	CHMPXHP_RAWPAIR	ctlendpoints[EXTERNAL_EP_MAX];
	CHMPXHP_RAWPAIR	forward_peers[FORWARD_PEER_MAX];
	CHMPXHP_RAWPAIR	reverse_peers[REVERSE_PEER_MAX];
	if(!selfchmpx.Get(name, chmpxid, port, ctlport, &cuk, &custom_seed, endpoints, ctlendpoints, forward_peers, reverse_peers)){
		return false;
	}

	if(pname){
		(*pname)		= name;
	}
	if(pport){
		(*pport)		= port;
	}
	if(pctlport){
		(*pctlport)		= ctlport;
	}
	if(pcuk){
		(*pcuk)			= cuk;
	}
	if(pcustom_seed){
		(*pcustom_seed)	= custom_seed;
	}
	if(pendpoints){
		rev_hostport_pairs(*pendpoints,		endpoints,		EXTERNAL_EP_MAX);
	}
	if(pctlendpoints){
		rev_hostport_pairs(*pctlendpoints,	ctlendpoints,	EXTERNAL_EP_MAX);
	}
	if(pforward_peers){
		rev_hostport_pairs(*pforward_peers,	forward_peers,	FORWARD_PEER_MAX);
	}
	if(preverse_peers){
		rev_hostport_pairs(*preverse_peers,	reverse_peers,	REVERSE_PEER_MAX);
	}
	return true;
}

template<typename T>
long chmpxman_lap<T>::GetSlaveChmpxIds(chmpxidlist_t& list) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return 0L;
	}
	if(!basic_type::pAbsPtr->chmpx_slaves){
		return 0L;
	}
	chmpxlistlap	slvchmpxlist(basic_type::pAbsPtr->chmpx_slaves, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From rel
	return slvchmpxlist.GetChmpxIds(list);			// All
}

template<typename T>
bool chmpxman_lap<T>::GetSlaveBase(chmpxid_t chmpxid, std::string* pname, short* pctlport, std::string* pcuk, std::string* pcustom_seed, hostport_list_t* pendpoints, hostport_list_t* pctlendpoints, hostport_list_t* pforward_peers, hostport_list_t* preverse_peers) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return false;
	}
	if(!basic_type::pAbsPtr->chmpx_slaves){
		return false;
	}
	chmpxlistlap	slvchmpxlist(basic_type::pAbsPtr->chmpx_slaves, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From rel
	if(!slvchmpxlist.Find(chmpxid)){
		//MSG_CHMPRN("Could not find chmpxid(0x%016" PRIx64 ").", chmpxid);
		return false;
	}
	chmpxlap		slvchmpx(slvchmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);		// Get CHMPX from Absolute

	std::string		name;
	short			port;
	short			ctlport;
	std::string		cuk;
	std::string		custom_seed;
	CHMPXHP_RAWPAIR	endpoints[EXTERNAL_EP_MAX];
	CHMPXHP_RAWPAIR	ctlendpoints[EXTERNAL_EP_MAX];
	CHMPXHP_RAWPAIR	forward_peers[FORWARD_PEER_MAX];
	CHMPXHP_RAWPAIR	reverse_peers[REVERSE_PEER_MAX];
	if(!slvchmpx.Get(name, chmpxid, port, ctlport, &cuk, &custom_seed, endpoints, ctlendpoints, forward_peers, reverse_peers)){
		return false;
	}

	if(pname){
		(*pname)		= name;
	}
	if(pctlport){
		(*pctlport)		= ctlport;
	}
	if(pcuk){
		(*pcuk)			= cuk;
	}
	if(pcustom_seed){
		(*pcustom_seed)	= custom_seed;
	}
	if(pendpoints){
		rev_hostport_pairs(*pendpoints,		endpoints,		EXTERNAL_EP_MAX);
	}
	if(pctlendpoints){
		rev_hostport_pairs(*pctlendpoints,	ctlendpoints,	EXTERNAL_EP_MAX);
	}
	if(pforward_peers){
		rev_hostport_pairs(*pforward_peers,	forward_peers,	FORWARD_PEER_MAX);
	}
	if(preverse_peers){
		rev_hostport_pairs(*preverse_peers,	reverse_peers,	REVERSE_PEER_MAX);
	}
	return true;
}

template<typename T>
bool chmpxman_lap<T>::GetSlaveSock(chmpxid_t chmpxid, socklist_t& socklist) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return false;
	}
	if(!basic_type::pAbsPtr->chmpx_slaves){
		return false;
	}
	chmpxlistlap	slvchmpxlist(basic_type::pAbsPtr->chmpx_slaves, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From rel
	if(!slvchmpxlist.Find(chmpxid)){
		//MSG_CHMPRN("Could not find chmpxid(0x%016" PRIx64 ").", chmpxid);
		return false;
	}
	chmpxlap	slvchmpx(slvchmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);		// Get CHMPX from Absolute
	int			ctlsock;
	int			selfsock;
	int			selfctlsock;
	return slvchmpx.Get(socklist, ctlsock, selfsock, selfctlsock);
}

template<typename T>
chmpxsts_t chmpxman_lap<T>::GetSlaveStatus(chmpxid_t chmpxid) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return CHMPXSTS_SLAVE_DOWN_NORMAL;
	}
	if(!basic_type::pAbsPtr->chmpx_slaves){
		return false;
	}
	chmpxlistlap	slvchmpxlist(basic_type::pAbsPtr->chmpx_slaves, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From rel
	if(!slvchmpxlist.Find(chmpxid)){
		ERR_CHMPRN("Could not find chmpxid(0x%016" PRIx64 ").", chmpxid);
		return CHMPXSTS_SLAVE_DOWN_NORMAL;
	}
	chmpxlap	slvchmpx(slvchmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);		// Get CHMPX from Absolute
	return slvchmpx.GetStatus();
}

template<typename T>
bool chmpxman_lap<T>::SetServerSocks(chmpxid_t chmpxid, int sock, int ctlsock, int type)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return false;
	}
	if(!basic_type::pAbsPtr->chmpx_servers){
		return false;
	}
	chmpxlistlap	svrchmpxlist(basic_type::pAbsPtr->chmpx_servers, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From rel
	if(!svrchmpxlist.Find(chmpxid)){
		ERR_CHMPRN("Could not find chmpxid(0x%016" PRIx64 ").", chmpxid);
		return false;
	}
	chmpxlap	svrchmpx(svrchmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);		// Get CHMPX from Absolute
	return svrchmpx.Set(sock, ctlsock, CHM_INVALID_SOCK, CHM_INVALID_SOCK, (type & SOCKTG_BOTH));
}

template<typename T>
bool chmpxman_lap<T>::RemoveServerSock(chmpxid_t chmpxid, int sock)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return false;
	}
	if(!basic_type::pAbsPtr->chmpx_servers){
		return false;
	}
	chmpxlistlap	svrchmpxlist(basic_type::pAbsPtr->chmpx_servers, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From rel
	if(!svrchmpxlist.Find(chmpxid)){
		ERR_CHMPRN("Could not find chmpxid(0x%016" PRIx64 ").", chmpxid);
		return false;
	}
	chmpxlap	svrchmpx(svrchmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);		// Get CHMPX from Absolute
	return svrchmpx.Remove(sock);
}

template<typename T>
bool chmpxman_lap<T>::SetServerHash(chmpxid_t chmpxid, chmhash_t base, chmhash_t pending, int type)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return false;
	}

	// If server mode & self chmpxid, set value to self chmpx
	if(IsServerMode() && GetSelfChmpxId() == chmpxid){
		return SetSelfHash(base, pending, type);
	}

	// Other servers
	chmpxlistlap	svrchmpxlist(basic_type::pAbsPtr->chmpx_servers, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From rel
	if(!svrchmpxlist.Find(chmpxid)){
		ERR_CHMPRN("Could not find chmpxid(0x%016" PRIx64 ").", chmpxid);
		return false;
	}
	chmpxlap	svrchmpx(svrchmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);		// Get CHMPX from Absolute
	if(!svrchmpx.Set(base, pending, type)){
		ERR_CHMPRN("Could not set hash value to chmpxid(0x%016" PRIx64 ").", chmpxid);
		return false;
	}
	return true;
}

template<typename T>
bool chmpxman_lap<T>::SetServerStatus(chmpxid_t chmpxid, chmpxsts_t status, bool is_client_join)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return false;
	}

	chmpxlistlap	svrchmpxlist;
	chmpxlap		svrchmpx;
	bool			need_check_all_down = false;

	// check self status
	if(IsServerMode() && GetSelfChmpxId() == chmpxid){
		svrchmpxlist.Reset(basic_type::pAbsPtr->chmpx_self, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);		// Get CHMPXLIST from Relative
		svrchmpx.Reset(svrchmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);	// Get CHMPX from Absolute

		// check self status changing
		if(IS_CHMPXSTS_DOWN(status) && CHM_INVALID_SOCK != svrchmpx.GetSock(SOCKTG_SELFSOCK)){
			ERR_CHMPRN("Self new chmpx status(0x%016" PRIx64 ":%s) is wrong, because self server is up.", status, STR_CHMPXSTS_FULL(status).c_str());
			return false;
		}else if(IS_CHMPXSTS_UP(status) && CHM_INVALID_SOCK == svrchmpx.GetSock(SOCKTG_SELFSOCK)){
			WAN_CHMPRN("Self new chmpx status(0x%016" PRIx64 ":%s) is wrong, because self server is down.", status, STR_CHMPXSTS_FULL(status).c_str());
			return false;
		}
		// Check suspend flag
		CHANGE_CHMPXSTS_SUSPEND_FLAG(status, !is_client_join);

	}else{
		svrchmpxlist.Reset(basic_type::pAbsPtr->chmpx_servers, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// Get CHMPXLIST from Relative
		if(!svrchmpxlist.Find(chmpxid)){
			ERR_CHMPRN("Could not find chmpxid(0x%016" PRIx64 ").", chmpxid);
			return false;
		}
		svrchmpx.Reset(svrchmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);	// Get CHMPX from Absolute

		// need to check all server chmpx down
		need_check_all_down = true;
	}

	// set status
	if(!svrchmpx.SetStatus(status)){
		return false;
	}

	// need to check all server down
	//
	// [NOTE]
	// This case is only on slave mode chmpx when changing sever status.
	// If all server status is "DOWN", it means no server has "SERVICE IN".
	// Then we have to check and set "SERVICE OUT" for all server here.
	//
	if(need_check_all_down && IS_CHMPXSTS_DOWN(status)){
		if(!svrchmpxlist.CheckAllServiceOutStatus()){
			ERR_CHMPRN("Failed to check status(all server chmpx down), and set status(all server chmpx to service out).");
			return false;
		}
	}

	// set base hash server count
	basic_type::pAbsPtr->chmpx_bhash_count	= svrchmpxlist.BaseHashCount(true);

	// operating flag
	basic_type::pAbsPtr->is_operating		= svrchmpxlist.IsOperating(true);

	return true;
}

template<typename T>
bool chmpxman_lap<T>::UpdateLastStatusTime(chmpxid_t chmpxid)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return false;
	}

	chmpxlistlap	svrchmpxlist;
	chmpxlap		svrchmpx;
	if(CHM_INVALID_CHMPXID == chmpxid || GetSelfChmpxId() == chmpxid){
		svrchmpxlist.Reset(basic_type::pAbsPtr->chmpx_self, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);		// Get CHMPXLIST from Relative
		svrchmpx.Reset(svrchmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);	// Get CHMPX from Absolute
	}else{
		svrchmpxlist.Reset(basic_type::pAbsPtr->chmpx_servers, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From rel
		if(!svrchmpxlist.Find(chmpxid)){
			ERR_CHMPRN("Could not find chmpxid(0x%016" PRIx64 ").", chmpxid);
			return false;
		}
		svrchmpx.Reset(svrchmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);	// Get CHMPX from Absolute
	}
	return svrchmpx.UpdateLastStatusTime();
}

template<typename T>
bool chmpxman_lap<T>::IsOperating(void)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return false;
	}
	return basic_type::pAbsPtr->is_operating;
}

//
// Calculate and Update all server pending hash. Servers which are set pending hash must be
// SERVICE ON/OUT on RING. Thus the server which is DOWN and SERVICE OUT is not a target for 
// calculating hash.
//
template<typename T>
bool chmpxman_lap<T>::UpdateHash(int type, bool is_allow_operating, bool is_allow_slave_mode)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return false;
	}
	if(!is_allow_slave_mode && !IsServerMode()){
		ERR_CHMPRN("Updating hash must be on Server mode.");
		return false;
	}
	if(!is_allow_operating && basic_type::pAbsPtr->is_operating){
		ERR_CHMPRN("Failed to request updating hash, but blocks it because of operating now.");
		return false;
	}
	chmpxlistlap	svrchmpxlist(basic_type::pAbsPtr->chmpx_servers, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From rel
	if(!svrchmpxlist.UpdateHash(type, false)){
		ERR_CHMPRN("Failed to set hash value to servers.");
		return false;
	}
	return true;
}

template<typename T>
bool chmpxman_lap<T>::SetSelfSocks(int sock, int ctlsock)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return false;
	}
	chmpxlistlap	selfchmpxlist(basic_type::pAbsPtr->chmpx_self, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// Get CHMPXLIST from Relative
	chmpxlap		selfchmpx(selfchmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);		// Get CHMPX from Absolute
	return selfchmpx.Set(CHM_INVALID_SOCK, CHM_INVALID_SOCK, sock, ctlsock, SOCKTG_BOTHSELF);
}

template<typename T>
bool chmpxman_lap<T>::SetSelfHash(chmhash_t base, chmhash_t pending, int type)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return false;
	}
	chmpxlistlap	selfchmpxlist(basic_type::pAbsPtr->chmpx_self, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// Get CHMPXLIST from Relative
	chmpxlap		selfchmpx(selfchmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);		// Get CHMPX from Absolute
	if(!selfchmpx.Set(base, pending, type)){
		ERR_CHMPRN("Could not set hash value to self.");
		return false;
	}
	return true;
}

template<typename T>
bool chmpxman_lap<T>::SetSelfStatus(chmpxsts_t status, bool is_client_join)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return false;
	}
	chmpxlistlap	selfchmpxlist(basic_type::pAbsPtr->chmpx_self, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// Get CHMPXLIST from Relative
	chmpxlap		selfchmpx(selfchmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);		// Get CHMPX from Absolute

	// check status
	if(IsServerMode()){
		// check self status changing
		if(IS_CHMPXSTS_UP(status) && CHM_INVALID_SOCK == selfchmpx.GetSock(SOCKTG_SELFSOCK)){
			WAN_CHMPRN("Self new chmpx status(0x%016" PRIx64 ":%s) is wrong, because self server is down.", status, STR_CHMPXSTS_FULL(status).c_str());
			return false;
		}
		// Check suspend flag
		CHANGE_CHMPXSTS_SUSPEND_FLAG(status, !is_client_join);
	}
	if(!selfchmpx.SetStatus(status)){
		ERR_CHMPRN("Could not set status value to self.");
		return false;
	}

	if(IsServerMode()){
		// set base hash server count
		basic_type::pAbsPtr->chmpx_bhash_count = selfchmpxlist.BaseHashCount(true);
		// operating flag
		basic_type::pAbsPtr->is_operating		= selfchmpxlist.IsOperating(true);
	}
	return true;
}

template<typename T>
bool chmpxman_lap<T>::SetSlaveBase(CHMPXID_SEED_TYPE type, chmpxid_t chmpxid, const char* hostname, short ctlport, const char* cuk, const char* custom_seed, const hostport_list_t& endpoints, const hostport_list_t& ctlendpoints, const hostport_list_t& forward_peers, const hostport_list_t& reverse_peers, const PCHMPXSSL pssl)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return false;
	}

	// check
	if(CHMEMPTYSTR(hostname) || !pssl || chmpxid != MakeChmpxId(basic_type::pAbsPtr->group, type, hostname, ctlport, cuk, get_hostports_string(ctlendpoints).c_str(), custom_seed)){
		ERR_CHMPRN("Parameter are wrong.");
		return false;
	}

	// buffers for temporary
	CHMPXHP_RAWPAIR	tmp_endpoints[EXTERNAL_EP_MAX];
	CHMPXHP_RAWPAIR	tmp_ctlendpoints[EXTERNAL_EP_MAX];
	CHMPXHP_RAWPAIR	tmp_forward_peers[FORWARD_PEER_MAX];
	CHMPXHP_RAWPAIR	tmp_reverse_peers[REVERSE_PEER_MAX];

	cvt_hostport_pairs(tmp_endpoints,		endpoints,		EXTERNAL_EP_MAX);
	cvt_hostport_pairs(tmp_ctlendpoints,	ctlendpoints,	EXTERNAL_EP_MAX);
	cvt_hostport_pairs(tmp_forward_peers,	forward_peers,	FORWARD_PEER_MAX);
	cvt_hostport_pairs(tmp_reverse_peers,	reverse_peers,	REVERSE_PEER_MAX);

	bool			is_new = true;
	chmpxlistlap	slvchmpxlist;
	if(basic_type::pAbsPtr->chmpx_slaves){
		slvchmpxlist.Reset(basic_type::pAbsPtr->chmpx_slaves, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From rel
		if(slvchmpxlist.Find(chmpxid)){
			is_new = false;
		}
	}
	if(is_new){
		// add new slave chmpx
		if(basic_type::pAbsPtr->chmpx_free_count <= 0L){
			ERR_CHMPRN("No more free CHMPXLIST.");
			return false;
		}
		// Get one PCHMPXLIST for self from front of free list
		chmpxlistlap	freechmpxlist(basic_type::pAbsPtr->chmpx_frees, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From Relative
		chmpxlistlap	newchmpxlist(freechmpxlist.PopAny(), basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);						// Get one CHMPXLIST from Absolute
		chmpxlap		newchmpx(newchmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);													// Get CHMPX from Absolute

		basic_type::pAbsPtr->chmpx_free_count--;
		basic_type::pAbsPtr->chmpx_frees = freechmpxlist.GetFirstPtr(false);

		// Add
		newchmpx.InitializeSlave(type, hostname, basic_type::pAbsPtr->group, ctlport, cuk, custom_seed, tmp_endpoints, tmp_ctlendpoints, tmp_forward_peers, tmp_reverse_peers, *pssl);
		if(basic_type::pAbsPtr->chmpx_slaves){
			slvchmpxlist.Insert(newchmpxlist.GetAbsPtr(), true, type);						// From abs
			basic_type::pAbsPtr->chmpx_slave_count++;
			basic_type::pAbsPtr->chmpx_slaves = slvchmpxlist.GetFirstPtr(false);
		}else{
			newchmpxlist.SaveChmpxIdMap();													// Set chmpxid map
			basic_type::pAbsPtr->chmpx_slave_count	= 1;
			basic_type::pAbsPtr->chmpx_slaves		= newchmpxlist.GetRelPtr();
		}
	}else{
		// overwrite slave chmpx
		chmpxlap	slvchmpx(slvchmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);	// Get CHMPX from Absolute
		slvchmpx.InitializeSlave(type, hostname, basic_type::pAbsPtr->group, ctlport, cuk, custom_seed, tmp_endpoints, tmp_ctlendpoints, tmp_forward_peers, tmp_reverse_peers, *pssl);
	}
	return true;
}

template<typename T>
bool chmpxman_lap<T>::SetSlaveSock(chmpxid_t chmpxid, int sock)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return false;
	}
	if(CHM_INVALID_SOCK == sock){
		ERR_CHMPRN("sock is invalid.");
		return false;
	}
	chmpxlistlap	slvchmpxlist(basic_type::pAbsPtr->chmpx_slaves, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From rel
	if(!slvchmpxlist.Find(chmpxid)){
		ERR_CHMPRN("Could not find chmpxid(0x%016" PRIx64 ").", chmpxid);
		return false;
	}
	chmpxlap	slvchmpx(slvchmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);	// Get CHMPX from Absolute
	return slvchmpx.Set(sock, CHM_INVALID_SOCK, CHM_INVALID_SOCK, CHM_INVALID_SOCK, SOCKTG_SOCK);
}

template<typename T>
bool chmpxman_lap<T>::RemoveSlaveSock(chmpxid_t chmpxid, int sock)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return false;
	}
	chmpxlistlap	slvchmpxlist(basic_type::pAbsPtr->chmpx_slaves, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From rel
	if(!slvchmpxlist.Find(chmpxid)){
		ERR_CHMPRN("Could not find chmpxid(0x%016" PRIx64 ").", chmpxid);
		return false;
	}
	chmpxlap	slvchmpx(slvchmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);	// Get CHMPX from Absolute

	return slvchmpx.Remove(sock);
}

template<typename T>
bool chmpxman_lap<T>::SetSlaveStatus(chmpxid_t chmpxid, chmpxsts_t status)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return false;
	}
	chmpxlistlap	slvchmpxlist(basic_type::pAbsPtr->chmpx_slaves, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From rel
	if(!slvchmpxlist.Find(chmpxid)){
		ERR_CHMPRN("Could not find chmpxid(0x%016" PRIx64 ").", chmpxid);
		return false;
	}
	chmpxlap	slvchmpx(slvchmpxlist.GetAbsChmpxPtr(), AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase);		// Get CHMPX from Absolute

	// Check suspend flag
	if(IS_CHMPXSTS_SUSPEND(status)){
		WAN_CHMPRN("Slave new chmpx status(0x%016" PRIx64 ":%s) is wrong, because slave server could not have SUSPEND status. so remove SUSPEND flag.", status, STR_CHMPXSTS_FULL(status).c_str());
		SET_CHMPXSTS_NOSUP(status);
	}
	return slvchmpx.SetStatus(status);
}

template<typename T>
bool chmpxman_lap<T>::RemoveSlave(chmpxid_t chmpxid, int eqfd)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return false;
	}
	// Find
	chmpxlistlap	slvchmpxlist(basic_type::pAbsPtr->chmpx_slaves, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From rel
	if(!slvchmpxlist.Find(chmpxid)){
		WAN_CHMPRN("Could not find chmpxid(0x%016" PRIx64 ").", chmpxid);
		return true;
	}

	// Remove from slave list
	PCHMPXLIST	retrivelist;
	if(NULL == (retrivelist = slvchmpxlist.Retrieve())){
		// Failed to remove it
		ERR_CHMPRN("Failed to remove CHMPX(0x%016" PRIx64 ") from CHMPXSLV.", chmpxid);
		return false;
	}

	// Rechain slave list
	//
	// Be careful about slvchmpxlist after Retrieve().
	// If there is no object in list, this list is invalid.
	//
	if(slvchmpxlist.GetAbsPtr()){
		basic_type::pAbsPtr->chmpx_slave_count--;
		basic_type::pAbsPtr->chmpx_slaves		= slvchmpxlist.GetFirstPtr(false);
	}else{
		basic_type::pAbsPtr->chmpx_slave_count	= 0;
		basic_type::pAbsPtr->chmpx_slaves		= NULL;
	}

	// If has opened socket, close it.
	chmpxlistlap	retrivechmpxlist(retrivelist, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, true);	// From Abs
	retrivechmpxlist.Clear(eqfd);

	// Add free list
	chmpxlistlap	freechmpxlist(basic_type::pAbsPtr->chmpx_frees, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From Relative(allow NULL)
	if(!freechmpxlist.Push(retrivelist, true)){
		WAN_CHMPRN("Failed to push back new free CHMPXLIST to freelist, but continue...");
	}else{
		basic_type::pAbsPtr->chmpx_free_count++;
		basic_type::pAbsPtr->chmpx_frees = freechmpxlist.GetFirstPtr(false);
	}
	return true;
}

template<typename T>
bool chmpxman_lap<T>::CheckSockInAllChmpx(int sock) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return false;
	}
	if(basic_type::pAbsPtr->chmpx_servers){
		chmpxlistlap	svrchmpxlist(basic_type::pAbsPtr->chmpx_servers, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From rel
		if(svrchmpxlist.Find(sock, false)){
			// found
			return true;
		}
	}
	if(basic_type::pAbsPtr->chmpx_slaves){
		chmpxlistlap	slvchmpxlist(basic_type::pAbsPtr->chmpx_slaves, basic_type::pAbsPtr->chmpxid_map, AbsBaseArr(), AbsPendArr(), AbsSockFreeCnt(), AbsSockFrees(), basic_type::pShmBase, false);	// From rel
		if(slvchmpxlist.Find(sock, false)){
			// found
			return true;
		}
	}
	return false;
}

template<typename T>
bool chmpxman_lap<T>::AddStat(chmpxid_t chmpxid, bool is_sent, size_t length, const struct timespec& elapsed_time)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return false;
	}
	if(CHM_INVALID_CHMPXID == chmpxid){
		ERR_CHMPRN("Parameter are wrong.");
		return false;
	}

	chmstatlap	statlap;
	if(IsServerMode(chmpxid)){
		statlap.Reset(&(basic_type::pAbsPtr->server_stat), basic_type::pShmBase, true);		// From abs
	}else{
		statlap.Reset(&(basic_type::pAbsPtr->slave_stat), basic_type::pShmBase, true);		// From abs
	}
	return statlap.Add(is_sent, length, elapsed_time);
}

template<typename T>
bool chmpxman_lap<T>::GetStat(PCHMSTAT pserver, PCHMSTAT pslave) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMPXMAN does not set.");
		return false;
	}
	if(!pserver && !pslave){
		ERR_CHMPRN("Parameter are wrong.");
		return false;
	}

	chmstatlap	statlap;
	if(pserver){
		statlap.Reset(&(basic_type::pAbsPtr->server_stat), basic_type::pShmBase, true);		// From abs
		if(!statlap.Get(pserver)){
			ERR_CHMPRN("Could not get server stat.");
			return false;
		}
	}
	if(pslave){
		statlap.Reset(&(basic_type::pAbsPtr->slave_stat), basic_type::pShmBase, true);		// From abs
		if(!statlap.Get(pslave)){
			ERR_CHMPRN("Could not get slave stat.");
			return false;
		}
	}
	return true;
}

typedef	chmpxman_lap<CHMPXMAN>	chmpxmanlap;

//---------------------------------------------------------
// For CHMINFO
//---------------------------------------------------------
template<typename T>
class chminfo_lap : public structure_lap<T>
{
	public:
		typedef T					st_type;
		typedef T*					st_ptr_type;
		typedef structure_lap<T>	basic_type;

	protected:
		bool SetMqFlagStatus(msgid_t msgid, bool is_assigned, bool is_activated);
		bool SetMergingClinetPid(pid_t pid);

	public:
		explicit chminfo_lap(st_ptr_type ptr = NULL, const void* shmbase = NULL, bool is_abs = true);

		virtual bool Initialize(void);
		virtual bool Dump(std::stringstream& sstream, const char* spacer) const;
		virtual bool Clear(void);
		st_ptr_type Dup(void);
		chmpxlap::st_ptr_type DupSelfChmpxSvr(void);
		void Free(st_ptr_type ptr) const;

		bool Close(int eqfd, int type = CLOSETG_BOTH);
		bool Initialize(const CHMCFGINFO* pchmcfg, PMQMSGHEADLIST rel_chmpxmsgarea, const CHMNODE_CFGINFO* pselfnode, const char* pselfname, PCHMPXLIST relchmpxlist, PCLTPROCLIST relcltproclist, PCHMSOCKLIST relchmsockarea, PCHMPX* pchmpxarrbase, PCHMPX* pchmpxarrpend);
		bool IsSafeCurrentVersion(void) const;
		bool ReloadConfiguration(const CHMCFGINFO* pchmcfg, int eqfd);

		CHMPXID_SEED_TYPE GetChmpxSeedType(void) const { return (basic_type::pAbsPtr ? basic_type::pAbsPtr->chmpxid_type : CHMPXID_SEED_NAME); }
		msgid_t GetBaseMsgId(void) const { return (basic_type::pAbsPtr ? basic_type::pAbsPtr->base_msgid : 0L ); }
		msgid_t GetRandomMsgId(bool is_chmpx, bool is_activated = true);						// get msgid from assigned list
		msgid_t AssignMsg(bool is_chmpx, bool is_activated);									// Assign msgid
		bool SetMqActivated(msgid_t msgid) { return SetMqFlagStatus(msgid, true, true); }		// change msgid status to activate
		bool SetMqDisactivated(msgid_t msgid) { return SetMqFlagStatus(msgid, true, false); }	// change msgid status to disactivate
		bool FreeMsg(msgid_t msgid);															// Free msgid
		bool IsMsgidActivated(msgid_t msgid) const;
		bool GetMsgidListByPid(pid_t pid, msgidlist_t& list, bool is_clear_list = false);		// Get msgid list by PID(from assigned and activated list)
		bool GetMsgidListByUniqPid(msgidlist_t& list, bool is_clear_list = false);				// Get msgid list for all uniq PID(from activated list)

		pid_t GetChmpxSvrPid(void) const { return (basic_type::pAbsPtr ? basic_type::pAbsPtr->pid : CHM_INVALID_PID); }
		// cppcheck-suppress clarifyCondition
		pid_t* GetChmpxSvrPidAddr(bool is_abs) { return (!basic_type::pAbsPtr ? NULL : is_abs ? &(basic_type::pAbsPtr->pid) : CHM_REL(basic_type::pShmBase, &(basic_type::pAbsPtr->pid), pid_t*)); }
		// cppcheck-suppress clarifyCondition
		void* GetClientPidListOffset(void) { return (!basic_type::pAbsPtr ? NULL : CHM_REL(basic_type::pShmBase, &(basic_type::pAbsPtr->client_pids), void*)); }

		bool GetSelfChmpxSvr(PCHMPXSVR chmpxsvr) const;
		bool GetChmpxSvr(chmpxid_t chmpxid, PCHMPXSVR chmpxsvr) const;
		bool GetChmpxSvrs(PCHMPXSVR* ppchmpxsvrs, long& count) const;
		bool MergeChmpxSvrs(PCHMPXSVR pchmpxsvrs, long count, bool is_remove = true, bool is_init_process = false, int eqfd = CHM_INVALID_HANDLE);

		bool GetGroup(std::string& group) const;
		bool IsRandomDeliver(void) const { return (basic_type::pAbsPtr ? basic_type::pAbsPtr->is_random_deliver : false); }
		bool IsAutoMerge(void) const { return (basic_type::pAbsPtr ? (!basic_type::pAbsPtr->is_auto_merge_suspend && basic_type::pAbsPtr->is_auto_merge) : false); }
		bool IsDoMerge(void) const { return (basic_type::pAbsPtr ? basic_type::pAbsPtr->is_do_merge : false); }
		bool SuspendAutoMerge(void);
		bool ResetAutoMerge(void);
		bool GetAutoMergeMode(void) const { return (basic_type::pAbsPtr ? basic_type::pAbsPtr->is_auto_merge_suspend : false); }
		chmss_ver_t GetSslMinVersion(void) const { return (basic_type::pAbsPtr ? basic_type::pAbsPtr->ssl_min_ver : CHM_SSLTLS_VER_DEFAULT); }
		const char* GetNssdbDir(void) const { return (basic_type::pAbsPtr ? basic_type::pAbsPtr->nssdb_dir : NULL); }
		int GetSocketThreadCount(void) const { return (basic_type::pAbsPtr ? basic_type::pAbsPtr->evsock_thread_cnt : 0); }
		int GetMQThreadCount(void) const { return (basic_type::pAbsPtr ? basic_type::pAbsPtr->evmq_thread_cnt : 0); }
		long GetMaxMQCount(void) const { return (basic_type::pAbsPtr ? basic_type::pAbsPtr->max_mqueue : -1); }
		long GetChmpxMQCount(void) const { return (basic_type::pAbsPtr ? basic_type::pAbsPtr->chmpx_mqueue : -1); }
		long GetMaxQueuePerChmpxMQ(void) const { return (basic_type::pAbsPtr ? basic_type::pAbsPtr->max_q_per_chmpxmq : -1); }
		long GetMaxQueuePerClientMQ(void) const { return (basic_type::pAbsPtr ? basic_type::pAbsPtr->max_q_per_cltmq : -1); }
		long GetMQPerClient(void) const { return (basic_type::pAbsPtr ? basic_type::pAbsPtr->max_mq_per_client : -1); }
		long GetMQPerAttach(void) const { return (basic_type::pAbsPtr ? basic_type::pAbsPtr->mq_per_attach : -1); }
		bool IsAckMQ(void) const { return (basic_type::pAbsPtr ? basic_type::pAbsPtr->mq_ack : true); }
		int GetMaxSockPool(void) const { return (basic_type::pAbsPtr ? (0 < basic_type::pAbsPtr->max_sock_pool ? basic_type::pAbsPtr->max_sock_pool : 1) : 1); }
		time_t GetSockPoolTimeout(void) const { return (basic_type::pAbsPtr ? (0 <= basic_type::pAbsPtr->sock_pool_timeout ? basic_type::pAbsPtr->sock_pool_timeout : 0) : 0); }
		int GetSockRetryCnt(void) const { return (basic_type::pAbsPtr ? basic_type::pAbsPtr->sock_retrycnt : -1); }
		suseconds_t GetSockTimeout(void) const { return (basic_type::pAbsPtr ? basic_type::pAbsPtr->timeout_wait_socket : 0); }
		suseconds_t GetConnectTimeout(void) const { return (basic_type::pAbsPtr ? basic_type::pAbsPtr->timeout_wait_connect : 0); }
		int GetMQRetryCnt(void) const { return (basic_type::pAbsPtr ? basic_type::pAbsPtr->mq_retrycnt : -1); }
		long GetMQTimeout(void) const { return (basic_type::pAbsPtr ? basic_type::pAbsPtr->timeout_wait_mq : 0); }
		time_t GetMergeTimeout(void) const { return (basic_type::pAbsPtr ? basic_type::pAbsPtr->timeout_merge : 0); }
		bool IsServerMode(void) const;
		bool IsSlaveMode(void) const;
		long GetReplicaCount(void) const;
		chmpxid_t GetSelfChmpxId(void) const;
		chmpxid_t GetNextRingChmpxId(chmpxid_t chmpxid = CHM_INVALID_CHMPXID) const;

		long GetServerCount(void) const;
		long GetSlaveCount(void) const;
		long GetUpServerCount(void) const;
		chmpxpos_t GetSelfServerPos(void) const;
		chmpxpos_t GetNextServerPos(chmpxpos_t startpos, chmpxpos_t nowpos, bool is_skip_self, bool is_cycle) const;
		bool CheckContainsChmpxSvrs(const char* hostname) const;
		bool CheckStrictlyContainsChmpxSvrs(const char* hostname, const short* pctlport, const char* pcuk, std::string* pnormalizedname, PCHMPXSSL pssl) const;

		bool IsOperating(void);
		bool IsServerChmpxId(chmpxid_t chmpxid) const;
		chmpxid_t GetChmpxIdBySock(int sock, int type = CLOSETG_BOTH) const;
		chmpxid_t GetChmpxIdByToServerName(const char* hostname, short ctlport, const char* cuk, const char* ctlendpoints, const char* custom_seed) const;
		chmpxid_t GetChmpxIdByStatus(chmpxsts_t status, bool part_match = false) const;
		chmpxid_t GetRandomServerChmpxId(bool without_suspend = false);
		chmpxid_t GetServerChmpxIdByHash(chmhash_t hash) const;
		bool GetServerChmHashsByHashs(chmhash_t hash, chmhashlist_t& basehashs, bool with_pending, bool without_down = true, bool without_suspend = true);
		bool GetServerChmpxIdByHashs(chmhash_t hash, chmpxidlist_t& chmpxids, bool with_pending, bool without_down = true, bool without_suspend = true);
		long GetServerChmpxIds(chmpxidlist_t& list, bool with_pending, bool without_down = true, bool without_suspend = true) const;
		bool GetServerBase(chmpxpos_t pos, std::string* pname = NULL, chmpxid_t* pchmpxid = NULL, short* pport = NULL, short* pctlport = NULL, hostport_list_t* pendpoints = NULL, hostport_list_t* pctlendpoints = NULL) const;
		bool GetServerBase(chmpxid_t chmpxid, std::string* pname = NULL, short* pport = NULL, short* pctlport = NULL, hostport_list_t* pendpoints = NULL, hostport_list_t* pctlendpoints = NULL) const;
		bool GetServerBase(chmpxid_t chmpxid, CHMPXSSL& ssl) const;
		bool GetServerSocks(chmpxid_t chmpxid, socklist_t& socklist, int& ctlsock) const;
		bool GetServerHash(chmpxid_t chmpxid, chmhash_t& base, chmhash_t& pending) const;
		bool GetMaxHashCount(chmhash_t& basehash, chmhash_t& pengindhash) const;
		chmpxsts_t GetServerStatus(chmpxid_t chmpxid) const;
		bool GetSelfPorts(short& port, short& ctlport) const;
		bool GetSelfSocks(int& sock, int& ctlsock) const;
		bool GetSelfHash(chmhash_t& base, chmhash_t& pending) const;
		chmpxsts_t GetSelfStatus(void) const;
		bool GetSelfSsl(CHMPXSSL& ssl) const;
		bool GetSelfBase(std::string* pname = NULL, short* pport = NULL, short* pctlport = NULL, std::string* pcuk = NULL, std::string* pcustom_seed = NULL, hostport_list_t* pendpoints = NULL, hostport_list_t* pctlendpoints = NULL, hostport_list_t* pforward_peers = NULL, hostport_list_t* preverse_peers = NULL) const;
		long GetSlaveChmpxIds(chmpxidlist_t& list) const;
		bool GetSlaveBase(chmpxid_t chmpxid, std::string* pname = NULL, short* pctlport = NULL, std::string* pcuk = NULL, std::string* pcustom_seed = NULL, hostport_list_t* pendpoints = NULL, hostport_list_t* pctlendpoints = NULL, hostport_list_t* pforward_peers = NULL, hostport_list_t* preverse_peers = NULL) const;
		bool GetSlaveSock(chmpxid_t chmpxid, socklist_t& socklist) const;
		chmpxsts_t GetSlaveStatus(chmpxid_t chmpxid) const;

		bool SetServerSocks(chmpxid_t chmpxid, int sock, int ctlsock, int type);
		bool SetServerHash(chmpxid_t chmpxid, chmhash_t base, chmhash_t pending, int type);
		bool SetServerStatus(chmpxid_t chmpxid, chmpxsts_t status);
		bool UpdateLastStatusTime(chmpxid_t chmpxid = CHM_INVALID_CHMPXID);
		bool RemoveServerSock(chmpxid_t chmpxid, int sock);
		bool UpdateHash(int type, bool is_allow_operating = true, bool is_allow_slave_mode = true);
		bool SetSelfSocks(int sock, int ctlsock);
		bool SetSelfHash(chmhash_t base, chmhash_t pending, int type);
		bool SetSelfStatus(chmpxsts_t status);
		bool SetSlaveBase(chmpxid_t chmpxid, const char* hostname, short ctlport, const char* cuk, const char* custom_seed, const hostport_list_t& endpoints, const hostport_list_t& ctlendpoints, const hostport_list_t& forward_peers, const hostport_list_t& reverse_peers, const PCHMPXSSL pssl);
		bool SetSlaveSock(chmpxid_t chmpxid, int sock);
		bool SetSlaveStatus(chmpxid_t chmpxid, chmpxsts_t status);
		bool RemoveSlaveSock(chmpxid_t chmpxid, int sock, bool is_remove_empty = true);
		bool RemoveSlave(chmpxid_t chmpxid, int eqfd);
		bool CheckSockInAllChmpx(int sock) const;

		bool AddStat(chmpxid_t chmpxid, bool is_sent, size_t length, const struct timespec& elapsed_time);
		bool GetStat(PCHMSTAT pserver, PCHMSTAT pslave) const;

		bool RetrieveClientPid(pid_t pid);
		bool AddClientPid(pid_t pid);
		bool GetAllPids(pidlist_t& list);
		bool IsClientPids(void) const;

		long GetMaxHistoryLogCount(void) const { return (basic_type::pAbsPtr ? basic_type::pAbsPtr->histlog_count : -1); }
};

template<typename T>
chminfo_lap<T>::chminfo_lap(st_ptr_type ptr, const void* shmbase, bool is_abs)
{
	structure_lap<T>::Reset(ptr, shmbase, is_abs);
}

template<typename T>
bool chminfo_lap<T>::Initialize(void)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}
	memset(basic_type::pAbsPtr->chminfo_version, 0, CHM_CHMINFO_VERSION_BUFLEN);
	strcpy(basic_type::pAbsPtr->chminfo_version, CHM_CHMINFO_CUR_VERSION_STR);
	memset(basic_type::pAbsPtr->nssdb_dir, 0, CHM_MAX_PATH_LEN);

	basic_type::pAbsPtr->chminfo_size			= sizeof(CHMINFO);
	basic_type::pAbsPtr->pid					= CHM_INVALID_PID;
	basic_type::pAbsPtr->start_time				= 0;
	basic_type::pAbsPtr->chmpxid_type			= CHMPXID_SEED_NAME;
	basic_type::pAbsPtr->is_random_deliver		= false;
	basic_type::pAbsPtr->is_auto_merge			= false;
	basic_type::pAbsPtr->is_auto_merge_suspend	= false;
	basic_type::pAbsPtr->is_do_merge			= false;
	basic_type::pAbsPtr->ssl_min_ver			= CHM_SSLTLS_VER_DEFAULT;
	basic_type::pAbsPtr->evsock_thread_cnt		= 0;
	basic_type::pAbsPtr->evmq_thread_cnt		= 0;
	basic_type::pAbsPtr->max_mqueue				= 0L;
	basic_type::pAbsPtr->chmpx_mqueue			= 0L;
	basic_type::pAbsPtr->max_q_per_chmpxmq		= 0L;
	basic_type::pAbsPtr->max_q_per_cltmq		= 0L;
	basic_type::pAbsPtr->max_mq_per_client		= 0L;
	basic_type::pAbsPtr->mq_per_attach			= 0L;
	basic_type::pAbsPtr->mq_ack					= true;
	basic_type::pAbsPtr->max_sock_pool			= 0;
	basic_type::pAbsPtr->sock_pool_timeout		= 0;
	basic_type::pAbsPtr->sock_retrycnt			= 0;
	basic_type::pAbsPtr->timeout_wait_socket	= 0L;
	basic_type::pAbsPtr->timeout_wait_connect	= 0L;
	basic_type::pAbsPtr->mq_retrycnt			= 0;
	basic_type::pAbsPtr->timeout_wait_mq		= 0;
	basic_type::pAbsPtr->timeout_merge			= 0;
	basic_type::pAbsPtr->base_msgid				= 0L;
	basic_type::pAbsPtr->chmpx_msg_count		= 0L;
	basic_type::pAbsPtr->chmpx_msgs				= NULL;
	basic_type::pAbsPtr->activated_msg_count	= 0L;
	basic_type::pAbsPtr->activated_msgs			= NULL;
	basic_type::pAbsPtr->assigned_msg_count		= 0L;
	basic_type::pAbsPtr->assigned_msgs			= NULL;
	basic_type::pAbsPtr->free_msg_count			= 0L;
	basic_type::pAbsPtr->free_msgs				= NULL;
	basic_type::pAbsPtr->last_msgid_chmpx		= CHM_INVALID_MSGID;
	basic_type::pAbsPtr->last_msgid_activated	= CHM_INVALID_MSGID;
	basic_type::pAbsPtr->last_msgid_assigned	= CHM_INVALID_MSGID;
	basic_type::pAbsPtr->rel_chmpxmsgarea		= NULL;
	basic_type::pAbsPtr->client_pids			= NULL;
	basic_type::pAbsPtr->free_pids				= NULL;
	basic_type::pAbsPtr->k2h_fullmap			= true;
	basic_type::pAbsPtr->k2h_mask_bitcnt		= 0;
	basic_type::pAbsPtr->k2h_cmask_bitcnt		= 0;
	basic_type::pAbsPtr->k2h_max_element		= 0;
	basic_type::pAbsPtr->histlog_count			= 0L;

	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	if(!tmpchmpxman.Initialize()){
		ERR_CHMPRN("Failed to initialize CHMPXMAN.");
		return false;
	}
	return true;
}

template<typename T>
bool chminfo_lap<T>::IsSafeCurrentVersion(void) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}

	// check prefix for version string
	if(0 == strncmp(basic_type::pAbsPtr->chminfo_version, CHM_CHMINFO_CUR_VERSION_STR, strlen(CHM_CHMINFO_CUR_VERSION_STR))){
		// CHMINFO structure is as same as current version.
		return true;
	}

	// check old version for messaging
	std::string	old_ver_1_0(CHM_CHMINFO_VERSION_PREFIX " " CHM_CHMINFO_OLD_VERSION_1_0);	// 1.0
	if(0 != strncmp(basic_type::pAbsPtr->chminfo_version, CHM_CHMINFO_VERSION_PREFIX, strlen(CHM_CHMINFO_VERSION_PREFIX))){
		ERR_CHMPRN("CHMINFO structure maybe old version before CHMPX version 1.0.59.");
	}else if(0 == strncmp(basic_type::pAbsPtr->chminfo_version, old_ver_1_0.c_str(), old_ver_1_0.length())){
		ERR_CHMPRN("CHMINFO structure is older version before CHMPX version 1.0.71.");
	}else{
		ERR_CHMPRN("CHMINFO structure meybe newer version than current CHMPX version.");
	}
	return false;
}

template<typename T>
bool chminfo_lap<T>::Dump(std::stringstream& sstream, const char* spacer) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}
	if(!IsSafeCurrentVersion()){
		return false;
	}
	std::string	tmpspacer = spacer ? spacer : "";
	tmpspacer += "  ";

	sstream << (spacer ? spacer : "") << "CHMINFO version      = " << basic_type::pAbsPtr->chminfo_version		<< std::endl;
	sstream << (spacer ? spacer : "") << "CHMINFO size         = " << basic_type::pAbsPtr->chminfo_size			<< std::endl;
	sstream << (spacer ? spacer : "") << "pid                  = " << basic_type::pAbsPtr->pid					<< std::endl;
	sstream << (spacer ? spacer : "") << "start_time           = " << basic_type::pAbsPtr->start_time			<< std::endl;
	sstream << (spacer ? spacer : "") << "chmpxid_type         = " << get_chmpxid_seed_type_string(basic_type::pAbsPtr->chmpxid_type)	<< std::endl;
	sstream << (spacer ? spacer : "") << "is_random_deliver    = " << (basic_type::pAbsPtr->is_random_deliver		? "true" : "false")	<< std::endl;
	sstream << (spacer ? spacer : "") << "automerge in conf    = " << (basic_type::pAbsPtr->is_auto_merge			? "yes" : "no")		<< std::endl;
	sstream << (spacer ? spacer : "") << "suspend automerge    = " << (basic_type::pAbsPtr->is_auto_merge_suspend	? "yes" : "no")		<< std::endl;
	sstream << (spacer ? spacer : "") << "domerge in conf      = " << (basic_type::pAbsPtr->is_do_merge				? "yes" : "no")		<< std::endl;
	sstream << (spacer ? spacer : "") << "SSL/TLS min version  = " << CHM_GET_STR_SSLTLS_VERSION(basic_type::pAbsPtr->ssl_min_ver)		<< std::endl;
	sstream << (spacer ? spacer : "") << "NSSDB dir path       = " << basic_type::pAbsPtr->nssdb_dir			<< std::endl;
	sstream << (spacer ? spacer : "") << "socket thread count  = " << basic_type::pAbsPtr->evsock_thread_cnt	<< std::endl;
	sstream << (spacer ? spacer : "") << "MQ thread count      = " << basic_type::pAbsPtr->evmq_thread_cnt		<< std::endl;
	sstream << (spacer ? spacer : "") << "max_mqueue           = " << basic_type::pAbsPtr->max_mqueue			<< std::endl;
	sstream << (spacer ? spacer : "") << "chmpx_mqueue         = " << basic_type::pAbsPtr->chmpx_mqueue			<< std::endl;
	sstream << (spacer ? spacer : "") << "max_q_per_chmpxmq    = " << basic_type::pAbsPtr->max_q_per_chmpxmq	<< std::endl;
	sstream << (spacer ? spacer : "") << "max_q_per_cltmq      = " << basic_type::pAbsPtr->max_q_per_cltmq		<< std::endl;
	sstream << (spacer ? spacer : "") << "max_mq_per_client    = " << basic_type::pAbsPtr->max_mq_per_client	<< std::endl;
	sstream << (spacer ? spacer : "") << "mq_per_attach        = " << basic_type::pAbsPtr->mq_per_attach		<< std::endl;
	sstream << (spacer ? spacer : "") << "mq_ack               = " << (basic_type::pAbsPtr->mq_ack ? "true" : "false")	<< std::endl;
	sstream << (spacer ? spacer : "") << "Max socket pool      = " << basic_type::pAbsPtr->max_sock_pool		<< std::endl;
	sstream << (spacer ? spacer : "") << "sock pool timeout    = " << basic_type::pAbsPtr->sock_pool_timeout	<< std::endl;
	sstream << (spacer ? spacer : "") << "sock retry count     = " << basic_type::pAbsPtr->sock_retrycnt		<< std::endl;
	sstream << (spacer ? spacer : "") << "sock timeout         = " << basic_type::pAbsPtr->timeout_wait_socket	<< std::endl;
	sstream << (spacer ? spacer : "") << "connect timeout      = " << basic_type::pAbsPtr->timeout_wait_connect	<< std::endl;
	sstream << (spacer ? spacer : "") << "MQ retry count       = " << basic_type::pAbsPtr->mq_retrycnt			<< std::endl;
	sstream << (spacer ? spacer : "") << "MQ timeout           = " << basic_type::pAbsPtr->timeout_wait_mq		<< std::endl;
	sstream << (spacer ? spacer : "") << "merge timeout        = " << basic_type::pAbsPtr->timeout_merge		<< std::endl;
	sstream << (spacer ? spacer : "") << "base_msgid           = " << basic_type::pAbsPtr->base_msgid			<< std::endl;
	sstream << (spacer ? spacer : "") << "chmpx_msg_count      = " << basic_type::pAbsPtr->chmpx_msg_count		<< std::endl;
	sstream << (spacer ? spacer : "") << "chmpx_msgs           = " << basic_type::pAbsPtr->chmpx_msgs			<< std::endl;

	if(0L < basic_type::pAbsPtr->chmpx_msg_count){
		sstream << (spacer ? spacer : "") << "{" << std::endl;
		mqmsgheadlistlap	chmpx_msgs(basic_type::pAbsPtr->chmpx_msgs, basic_type::pShmBase, false);		// From rel
		chmpx_msgs.Dump(sstream, tmpspacer.c_str());
		sstream << (spacer ? spacer : "") << "}" << std::endl;
	}

	sstream << (spacer ? spacer : "") << "activated_msg_count  = " << basic_type::pAbsPtr->activated_msg_count	<< std::endl;
	sstream << (spacer ? spacer : "") << "activated_msgs       = " << basic_type::pAbsPtr->activated_msgs		<< std::endl;

	if(0L < basic_type::pAbsPtr->activated_msg_count){
		sstream << (spacer ? spacer : "") << "{" << std::endl;
		mqmsgheadlistlap	activated_msgs(basic_type::pAbsPtr->activated_msgs, basic_type::pShmBase, false);	// From rel
		activated_msgs.Dump(sstream, tmpspacer.c_str());
		sstream << (spacer ? spacer : "") << "}" << std::endl;
	}

	sstream << (spacer ? spacer : "") << "assigned_msg_count   = " << basic_type::pAbsPtr->assigned_msg_count	<< std::endl;
	sstream << (spacer ? spacer : "") << "assigned_msgs        = " << basic_type::pAbsPtr->assigned_msgs		<< std::endl;

	if(0L < basic_type::pAbsPtr->assigned_msg_count){
		sstream << (spacer ? spacer : "") << "{" << std::endl;
		mqmsgheadlistlap	assigned_msgs(basic_type::pAbsPtr->assigned_msgs, basic_type::pShmBase, false);	// From rel
		assigned_msgs.Dump(sstream, tmpspacer.c_str());
		sstream << (spacer ? spacer : "") << "}" << std::endl;
	}

	sstream << (spacer ? spacer : "") << "free_msg_count       = " << basic_type::pAbsPtr->free_msg_count		<< std::endl;
	sstream << (spacer ? spacer : "") << "free_msgs            = " << basic_type::pAbsPtr->free_msgs			<< std::endl;
	sstream << (spacer ? spacer : "") << "last_msgid_chmpx     = 0x" << to_hexstring(basic_type::pAbsPtr->last_msgid_chmpx)		<< std::endl;
	sstream << (spacer ? spacer : "") << "last_msgid_activated = 0x" << to_hexstring(basic_type::pAbsPtr->last_msgid_activated)	<< std::endl;
	sstream << (spacer ? spacer : "") << "last_msgid_assigned  = 0x" << to_hexstring(basic_type::pAbsPtr->last_msgid_assigned)	<< std::endl;

	sstream << (spacer ? spacer : "") << "chmpx_man{"	<< std::endl;
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	tmpchmpxman.Dump(sstream, tmpspacer.c_str());
	sstream << (spacer ? spacer : "") << "}"			<< std::endl;

	sstream << (spacer ? spacer : "") << "rel_chmpxmsgarea     = " << basic_type::pAbsPtr->rel_chmpxmsgarea		<< std::endl;

	sstream << (spacer ? spacer : "") << "client_pids          = " << basic_type::pAbsPtr->client_pids			<< std::endl;
	if(basic_type::pAbsPtr->client_pids){
		sstream << (spacer ? spacer : "") << "{" << std::endl;
		cltproclistlap	cltproc_pids(basic_type::pAbsPtr->client_pids, basic_type::pShmBase, false);		// From rel
		cltproc_pids.Dump(sstream, tmpspacer.c_str());
		sstream << (spacer ? spacer : "") << "}" << std::endl;
	}
	sstream << (spacer ? spacer : "") << "free_pids            = " << basic_type::pAbsPtr->free_pids			<< std::endl;

	sstream << (spacer ? spacer : "") << "k2hash full map      = " << (basic_type::pAbsPtr->k2h_fullmap ? "yes" : "no")	<< std::endl;
	sstream << (spacer ? spacer : "") << "init k2h mask bit    = " << basic_type::pAbsPtr->k2h_mask_bitcnt		<< std::endl;
	sstream << (spacer ? spacer : "") << "init k2h cmask bit   = " << basic_type::pAbsPtr->k2h_cmask_bitcnt		<< std::endl;
	sstream << (spacer ? spacer : "") << "k2hash max element   = " << basic_type::pAbsPtr->k2h_max_element		<< std::endl;
	sstream << (spacer ? spacer : "") << "history log count    = " << basic_type::pAbsPtr->histlog_count		<< std::endl;

	return true;
}

template<typename T>
typename chminfo_lap<T>::st_ptr_type chminfo_lap<T>::Dup(void)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCLTPROCLIST does not set.");
		return NULL;
	}
	if(!IsSafeCurrentVersion()){
		return NULL;
	}

	// duplicate
	st_ptr_type	pdst;
	if(NULL == (pdst = reinterpret_cast<st_ptr_type>(calloc(1, sizeof(st_type))))){
		ERR_CHMPRN("Could not allocation memory.");
		return NULL;
	}
	memset(pdst->chminfo_version, 0, CHM_CHMINFO_VERSION_BUFLEN);
	strcpy(pdst->chminfo_version, basic_type::pAbsPtr->chminfo_version);
	memcpy(pdst->nssdb_dir, basic_type::pAbsPtr->nssdb_dir, CHM_MAX_PATH_LEN);

	pdst->chminfo_size			= basic_type::pAbsPtr->chminfo_size;
	pdst->pid					= basic_type::pAbsPtr->pid;
	pdst->start_time			= basic_type::pAbsPtr->start_time;
	pdst->chmpxid_type			= basic_type::pAbsPtr->chmpxid_type;
	pdst->is_random_deliver		= basic_type::pAbsPtr->is_random_deliver;
	pdst->is_auto_merge			= basic_type::pAbsPtr->is_auto_merge;
	pdst->is_auto_merge_suspend	= basic_type::pAbsPtr->is_auto_merge_suspend;
	pdst->is_do_merge			= basic_type::pAbsPtr->is_do_merge;
	pdst->evsock_thread_cnt		= basic_type::pAbsPtr->evsock_thread_cnt;
	pdst->evmq_thread_cnt		= basic_type::pAbsPtr->evmq_thread_cnt;
	pdst->ssl_min_ver			= basic_type::pAbsPtr->ssl_min_ver;
	pdst->max_mqueue			= basic_type::pAbsPtr->max_mqueue;
	pdst->chmpx_mqueue			= basic_type::pAbsPtr->chmpx_mqueue;
	pdst->max_q_per_chmpxmq		= basic_type::pAbsPtr->max_q_per_chmpxmq;
	pdst->max_q_per_cltmq		= basic_type::pAbsPtr->max_q_per_cltmq;
	pdst->max_mq_per_client		= basic_type::pAbsPtr->max_mq_per_client;
	pdst->mq_per_attach			= basic_type::pAbsPtr->mq_per_attach;
	pdst->mq_ack				= basic_type::pAbsPtr->mq_ack;
	pdst->max_sock_pool			= basic_type::pAbsPtr->max_sock_pool;
	pdst->sock_pool_timeout		= basic_type::pAbsPtr->sock_pool_timeout;
	pdst->sock_retrycnt			= basic_type::pAbsPtr->sock_retrycnt;
	pdst->timeout_wait_socket	= basic_type::pAbsPtr->timeout_wait_socket;
	pdst->timeout_wait_connect	= basic_type::pAbsPtr->timeout_wait_connect;
	pdst->mq_retrycnt			= basic_type::pAbsPtr->mq_retrycnt;
	pdst->timeout_wait_mq		= basic_type::pAbsPtr->timeout_wait_mq;
	pdst->timeout_merge			= basic_type::pAbsPtr->timeout_merge;
	pdst->base_msgid			= basic_type::pAbsPtr->base_msgid;
	pdst->chmpx_msg_count		= basic_type::pAbsPtr->chmpx_msg_count;
	pdst->activated_msg_count	= basic_type::pAbsPtr->activated_msg_count;
	pdst->assigned_msg_count	= basic_type::pAbsPtr->assigned_msg_count;
	pdst->free_msg_count		= basic_type::pAbsPtr->free_msg_count;
	pdst->free_msgs				= NULL;										// Always NULL
	pdst->last_msgid_chmpx		= basic_type::pAbsPtr->last_msgid_chmpx;
	pdst->last_msgid_activated	= basic_type::pAbsPtr->last_msgid_activated;
	pdst->last_msgid_assigned	= basic_type::pAbsPtr->last_msgid_assigned;
	pdst->rel_chmpxmsgarea		= NULL;										// Always NULL
	pdst->free_pids				= NULL;										// Always NULL
	pdst->k2h_fullmap			= basic_type::pAbsPtr->k2h_fullmap;
	pdst->k2h_mask_bitcnt		= basic_type::pAbsPtr->k2h_mask_bitcnt;
	pdst->k2h_cmask_bitcnt		= basic_type::pAbsPtr->k2h_cmask_bitcnt;
	pdst->k2h_max_element		= basic_type::pAbsPtr->k2h_max_element;
	pdst->histlog_count			= basic_type::pAbsPtr->histlog_count;

	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	if(!tmpchmpxman.Copy(&pdst->chmpx_man)){
		WAN_CHMPRN("Failed to copy chmpx_man structure, but continue...");
	}

	mqmsgheadlistlap			chmpx_msgs(basic_type::pAbsPtr->chmpx_msgs, basic_type::pShmBase, false);			// From rel
	pdst->chmpx_msgs			= chmpx_msgs.Dup();

	mqmsgheadlistlap			activated_msgs(basic_type::pAbsPtr->activated_msgs, basic_type::pShmBase, false);	// From rel
	pdst->activated_msgs		= activated_msgs.Dup();

	mqmsgheadlistlap			assigned_msgs(basic_type::pAbsPtr->assigned_msgs, basic_type::pShmBase, false);		// From rel
	pdst->assigned_msgs			= assigned_msgs.Dup();

	cltproclistlap				cltproc_pids(basic_type::pAbsPtr->client_pids, basic_type::pShmBase, false);		// From rel
	pdst->client_pids			= cltproc_pids.Dup();

	return pdst;
}

template<typename T>
chmpxlap::st_ptr_type chminfo_lap<T>::DupSelfChmpxSvr(void)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCLTPROCLIST does not set.");
		return NULL;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.DupSelfChmpxSvr();
}

template<typename T>
void chminfo_lap<T>::Free(st_ptr_type ptr) const
{
	if(ptr){
		chmpxmanlap			tmpchmpxman;			// [NOTE] pointer is not on Shm, but we use chmpxmanlap object. thus using only Free method.
		tmpchmpxman.Free(&ptr->chmpx_man);

		mqmsgheadlistlap	tmpchmpxmsgs;			// [NOTE] pointer is not on Shm, but we use mqmsgheadlistlap object. thus using only Free method.
		tmpchmpxmsgs.Free(ptr->chmpx_msgs);
		tmpchmpxmsgs.Free(ptr->activated_msgs);
		tmpchmpxmsgs.Free(ptr->assigned_msgs);

		cltproclistlap		tmpcltprocpids;			// [NOTE] pointer is not on Shm, but we use cltproclistlap object. thus using only Free method.
		tmpcltprocpids.Free(ptr->client_pids);

		K2H_Free(ptr);
	}
}

template<typename T>
bool chminfo_lap<T>::Initialize(const CHMCFGINFO* pchmcfg, PMQMSGHEADLIST rel_chmpxmsgarea, const CHMNODE_CFGINFO* pselfnode, const char* pselfname, PCHMPXLIST relchmpxlist, PCLTPROCLIST relcltproclist, PCHMSOCKLIST relchmsockarea, PCHMPX* pchmpxarrbase, PCHMPX* pchmpxarrpend)
{
	if(!pchmcfg || !rel_chmpxmsgarea || !relchmpxlist || !pselfnode || !relcltproclist || !relchmsockarea || !pchmpxarrbase || !pchmpxarrpend){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}
	if(MAX_MQUEUE_COUNT < (pchmcfg->max_server_mq_cnt + pchmcfg->max_client_mq_cnt)){
		ERR_CHMPRN("Configuration information are wrong.");
		return false;
	}
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}

	// node name
	std::string	strselfname = CHMEMPTYSTR(pselfname) ? pselfnode->name : pselfname;

	memset(basic_type::pAbsPtr->chminfo_version, 0, CHM_CHMINFO_VERSION_BUFLEN);
	strcpy(basic_type::pAbsPtr->chminfo_version, CHM_CHMINFO_CUR_VERSION_STR);
	strcpy(basic_type::pAbsPtr->nssdb_dir, pchmcfg->nssdb_dir.c_str());

	basic_type::pAbsPtr->chminfo_size			= sizeof(CHMINFO);
	basic_type::pAbsPtr->pid					= getpid();
	basic_type::pAbsPtr->start_time				= time(NULL);
	basic_type::pAbsPtr->chmpxid_type			= pchmcfg->chmpxid_type;
	basic_type::pAbsPtr->is_random_deliver		= pchmcfg->is_random_mode;
	basic_type::pAbsPtr->is_auto_merge			= pchmcfg->is_auto_merge;
	basic_type::pAbsPtr->is_auto_merge_suspend	= false;					// default at loading configuration
	basic_type::pAbsPtr->is_do_merge			= pchmcfg->is_do_merge;
	basic_type::pAbsPtr->ssl_min_ver			= pchmcfg->ssl_min_ver;
	basic_type::pAbsPtr->evsock_thread_cnt		= pchmcfg->sock_thread_cnt;
	basic_type::pAbsPtr->evmq_thread_cnt		= pchmcfg->mq_thread_cnt;
	basic_type::pAbsPtr->max_mqueue				= pchmcfg->max_server_mq_cnt + pchmcfg->max_client_mq_cnt;
	basic_type::pAbsPtr->chmpx_mqueue			= pchmcfg->max_server_mq_cnt;
	basic_type::pAbsPtr->max_q_per_chmpxmq		= pchmcfg->max_q_per_servermq;
	basic_type::pAbsPtr->max_q_per_cltmq		= pchmcfg->max_q_per_clientmq;
	basic_type::pAbsPtr->max_mq_per_client		= pchmcfg->max_mq_per_client;
	basic_type::pAbsPtr->mq_per_attach			= pchmcfg->mqcnt_per_attach;
	basic_type::pAbsPtr->mq_ack					= pchmcfg->mq_ack;
	basic_type::pAbsPtr->max_sock_pool			= pchmcfg->max_sock_pool;
	basic_type::pAbsPtr->sock_pool_timeout		= pchmcfg->sock_pool_timeout;
	basic_type::pAbsPtr->sock_retrycnt			= pchmcfg->retrycnt;
	basic_type::pAbsPtr->timeout_wait_socket	= static_cast<suseconds_t>(pchmcfg->timeout_wait_socket);
	basic_type::pAbsPtr->timeout_wait_connect	= static_cast<suseconds_t>(pchmcfg->timeout_wait_connect);
	basic_type::pAbsPtr->mq_retrycnt			= pchmcfg->mq_retrycnt;
	basic_type::pAbsPtr->timeout_wait_mq		= pchmcfg->timeout_wait_mq;
	basic_type::pAbsPtr->timeout_merge			= pchmcfg->timeout_merge;
	basic_type::pAbsPtr->base_msgid				= MakeBaseMsgId(pchmcfg->groupname.c_str(), pchmcfg->chmpxid_type, strselfname.c_str(), pselfnode->ctlport, pselfnode->cuk.c_str(), get_hostports_string(pselfnode->ctlendpoints).c_str(), pselfnode->custom_seed.c_str());
	basic_type::pAbsPtr->chmpx_msg_count		= 0L;
	basic_type::pAbsPtr->chmpx_msgs				= NULL;
	basic_type::pAbsPtr->activated_msg_count	= 0L;
	basic_type::pAbsPtr->activated_msgs			= NULL;
	basic_type::pAbsPtr->assigned_msg_count		= 0L;
	basic_type::pAbsPtr->assigned_msgs			= NULL;
	basic_type::pAbsPtr->free_msg_count			= pchmcfg->max_server_mq_cnt + pchmcfg->max_client_mq_cnt;
	basic_type::pAbsPtr->free_msgs				= rel_chmpxmsgarea;
	basic_type::pAbsPtr->last_msgid_chmpx		= CHM_INVALID_MSGID;
	basic_type::pAbsPtr->last_msgid_activated	= CHM_INVALID_MSGID;
	basic_type::pAbsPtr->last_msgid_assigned	= CHM_INVALID_MSGID;
	basic_type::pAbsPtr->rel_chmpxmsgarea		= rel_chmpxmsgarea;
	basic_type::pAbsPtr->client_pids			= NULL;
	basic_type::pAbsPtr->free_pids				= relcltproclist;
	basic_type::pAbsPtr->k2h_fullmap			= pchmcfg->k2h_fullmap;
	basic_type::pAbsPtr->k2h_mask_bitcnt		= pchmcfg->k2h_mask_bitcnt;
	basic_type::pAbsPtr->k2h_cmask_bitcnt		= pchmcfg->k2h_cmask_bitcnt;
	basic_type::pAbsPtr->k2h_max_element		= pchmcfg->k2h_max_element;
	basic_type::pAbsPtr->histlog_count			= pchmcfg->max_histlog_count;

	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	if(!tmpchmpxman.Initialize(pchmcfg, pselfnode, pselfname, relchmpxlist, relchmsockarea, pchmpxarrbase, pchmpxarrpend)){
		ERR_CHMPRN("Failed to initialize CHMPXMAN.");
		return false;
	}

	// Fill msgid
	mqmsgheadarrlap	mqmsgheadarr(basic_type::pAbsPtr->rel_chmpxmsgarea, basic_type::pShmBase, basic_type::pAbsPtr->max_mqueue, false);	// From Relative
	if(!mqmsgheadarr.FillMsgId(basic_type::pAbsPtr->base_msgid)){
		ERR_CHMPRN("Failed to fill msgid into MQMSGHEADLIST.");
		return false;
	}
	return true;
}

template<typename T>
bool chminfo_lap<T>::ReloadConfiguration(const CHMCFGINFO* pchmcfg, int eqfd)
{
	if(!pchmcfg){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}

	// check
	if(MAX_MQUEUE_COUNT < (pchmcfg->max_server_mq_cnt + pchmcfg->max_client_mq_cnt)){
		ERR_CHMPRN("Configuration information are wrong.");
		return false;
	}
	if(basic_type::pAbsPtr->is_random_deliver != pchmcfg->is_random_mode){
		WAN_CHMPRN("chmpx mode(random/hash) could not be changed.");
	}
	if(basic_type::pAbsPtr->chmpxid_type != pchmcfg->chmpxid_type){
		// Must be same chmpxid seed type.
		ERR_CHMPRN("CHMPXID Seed type is different(Current:%s vs New configuration:%s).", get_chmpxid_seed_type_string(basic_type::pAbsPtr->chmpxid_type), get_chmpxid_seed_type_string(pchmcfg->chmpxid_type));
		return false;
	}

	// reset
	//
	// [NOTE]
	// reset is_auto_merge_suspend flag at reloading configuration.
	//
	strcpy(basic_type::pAbsPtr->nssdb_dir, pchmcfg->nssdb_dir.c_str());
	basic_type::pAbsPtr->is_auto_merge			= pchmcfg->is_auto_merge;
	basic_type::pAbsPtr->is_auto_merge_suspend	= false;
	basic_type::pAbsPtr->is_do_merge			= pchmcfg->is_do_merge;
	basic_type::pAbsPtr->ssl_min_ver			= pchmcfg->ssl_min_ver;
	basic_type::pAbsPtr->evsock_thread_cnt		= pchmcfg->sock_thread_cnt;
	basic_type::pAbsPtr->evmq_thread_cnt		= pchmcfg->mq_thread_cnt;
	basic_type::pAbsPtr->max_mqueue				= pchmcfg->max_server_mq_cnt + pchmcfg->max_client_mq_cnt;
	basic_type::pAbsPtr->chmpx_mqueue			= pchmcfg->max_server_mq_cnt;
	basic_type::pAbsPtr->max_q_per_chmpxmq		= pchmcfg->max_q_per_servermq;
	basic_type::pAbsPtr->max_q_per_cltmq		= pchmcfg->max_q_per_clientmq;
	basic_type::pAbsPtr->max_mq_per_client		= pchmcfg->max_mq_per_client;
	basic_type::pAbsPtr->mq_per_attach			= pchmcfg->mqcnt_per_attach;
	basic_type::pAbsPtr->mq_ack					= pchmcfg->mq_ack;
	basic_type::pAbsPtr->max_sock_pool			= pchmcfg->max_sock_pool;
	basic_type::pAbsPtr->sock_pool_timeout		= pchmcfg->sock_pool_timeout;
	basic_type::pAbsPtr->sock_retrycnt			= pchmcfg->retrycnt;
	basic_type::pAbsPtr->timeout_wait_socket	= static_cast<suseconds_t>(pchmcfg->timeout_wait_socket);
	basic_type::pAbsPtr->timeout_wait_connect	= static_cast<suseconds_t>(pchmcfg->timeout_wait_connect);
	basic_type::pAbsPtr->mq_retrycnt			= pchmcfg->mq_retrycnt;
	basic_type::pAbsPtr->timeout_wait_mq		= pchmcfg->timeout_wait_mq;
	basic_type::pAbsPtr->timeout_merge			= pchmcfg->timeout_merge;
	basic_type::pAbsPtr->histlog_count			= pchmcfg->max_histlog_count;

	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	if(!tmpchmpxman.ReloadConfiguration(pchmcfg, eqfd)){
		ERR_CHMPRN("Failed to initialize CHMPXMAN.");
		return false;
	}
	return true;
}

template<typename T>
bool chminfo_lap<T>::Clear(void)
{
	return Initialize();
}

template<typename T>
bool chminfo_lap<T>::Close(int eqfd, int type)
{
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.Close(eqfd, type);
}

template<typename T>
bool chminfo_lap<T>::FreeMsg(msgid_t msgid)
{
	if(CHM_INVALID_MSGID == msgid){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}

	// Check last msgid cache and clean it if hits.
	if(msgid == basic_type::pAbsPtr->last_msgid_chmpx){
		basic_type::pAbsPtr->last_msgid_chmpx = CHM_INVALID_MSGID;
	}
	if(msgid == basic_type::pAbsPtr->last_msgid_activated){
		basic_type::pAbsPtr->last_msgid_activated = CHM_INVALID_MSGID;
	}
	if(msgid == basic_type::pAbsPtr->last_msgid_assigned){
		basic_type::pAbsPtr->last_msgid_assigned = CHM_INVALID_MSGID;
	}

	// Find
	mqmsgheadarrlap	msgheadarr(basic_type::pAbsPtr->rel_chmpxmsgarea, basic_type::pShmBase, basic_type::pAbsPtr->max_mqueue, false);	// From Relative
	PMQMSGHEADLIST	retrieved_list_ptr = msgheadarr.Find(msgid, true);									// To abs
	if(!retrieved_list_ptr){
		ERR_CHMPRN("Could not find msgid(0x%016" PRIx64 ").", msgid);
		return false;
	}

	// build list
	mqmsgheadlistlap	retrieved_list(retrieved_list_ptr, basic_type::pShmBase, true);					// From abs
	mqmsgheadlap		retrieved_msghead(retrieved_list.GetAbsMqMsgHeadPtr(), basic_type::pShmBase);	// From abs

	// check flag
	if(retrieved_msghead.IsNotAssigned()){
		WAN_CHMPRN("Already not assigned by msgid(0x%016" PRIx64 ").", msgid);
		return true;
	}

	// Retrieve
	if(retrieved_msghead.IsChmpxProc()){
		if(NULL == (retrieved_list_ptr = retrieved_list.Retrieve())){
			ERR_CHMPRN("Failed to remove msgid(0x%016" PRIx64 ") from chmpxlist.", msgid);
			return false;
		}
		if(0 < basic_type::pAbsPtr->chmpx_msg_count){
			basic_type::pAbsPtr->chmpx_msg_count--;
		}
		basic_type::pAbsPtr->chmpx_msgs = retrieved_list.GetFirstPtr(false);

	}else if(retrieved_msghead.IsClientProc()){
		if(retrieved_msghead.IsAssigned() && !retrieved_msghead.IsActivated()){
			if(NULL == (retrieved_list_ptr = retrieved_list.Retrieve())){
				ERR_CHMPRN("Failed to remove msgid(0x%016" PRIx64 ") from client assigned list.", msgid);
				return false;
			}
			if(0 < basic_type::pAbsPtr->assigned_msg_count){
				basic_type::pAbsPtr->assigned_msg_count--;
			}
			basic_type::pAbsPtr->assigned_msgs = retrieved_list.GetFirstPtr(false);


		}else if(retrieved_msghead.IsAssigned() && retrieved_msghead.IsActivated()){
			if(NULL == (retrieved_list_ptr = retrieved_list.Retrieve())){
				ERR_CHMPRN("Failed to remove msgid(0x%016" PRIx64 ") from client activated list.", msgid);
				return false;
			}
			if(0 < basic_type::pAbsPtr->activated_msg_count){
				basic_type::pAbsPtr->activated_msg_count--;
			}
			basic_type::pAbsPtr->activated_msgs = retrieved_list.GetFirstPtr(false);

		}else{
			ERR_CHMPRN("msgid(0x%016" PRIx64 ") status is something wrong.", msgid);
			return false;
		}
	}else{
		WAN_CHMPRN("msgid(0x%016" PRIx64 ") is already freed.", msgid);
		return true;
	}

	// Add freemsglist
	retrieved_list.Reset(retrieved_list_ptr, basic_type::pShmBase, true);				// From abs
	retrieved_msghead.Reset(retrieved_list.GetAbsMqMsgHeadPtr(), basic_type::pShmBase);	// From abs

	retrieved_msghead.NotAccountMqFlag();

	mqmsgheadlistlap	freed_msgs(basic_type::pAbsPtr->free_msgs, basic_type::pShmBase, false);	// From rel(allow NULL)
	if(!freed_msgs.Push(retrieved_list_ptr, true)){
		ERR_CHMPRN("Failed to add freed PMQMSGHEADLIST %p.", retrieved_list_ptr);
		return false;
	}
	basic_type::pAbsPtr->free_msg_count++;
	basic_type::pAbsPtr->free_msgs = freed_msgs.GetFirstPtr(false);

	return true;
}

template<typename T>
bool chminfo_lap<T>::IsMsgidActivated(msgid_t msgid) const
{
	if(CHM_INVALID_MSGID == msgid){
		return false;
	}
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}

	// get msghead
	mqmsgheadarrlap	msgheadarr(basic_type::pAbsPtr->rel_chmpxmsgarea, basic_type::pShmBase, basic_type::pAbsPtr->max_mqueue, false);	// From Relative
	PMQMSGHEADLIST	msg_list_ptr = msgheadarr.Find(msgid, true);							// To abs
	if(!msg_list_ptr){
		MSG_CHMPRN("Could not find msgid(0x%016" PRIx64 ").", msgid);
		return false;
	}
	mqmsgheadlistlap	msg_list(msg_list_ptr, basic_type::pShmBase, true);					// From abs
	mqmsgheadlap		msghead(msg_list.GetAbsMqMsgHeadPtr(), basic_type::pShmBase);		// From abs

	return msghead.IsActivated();
}

template<typename T>
msgid_t chminfo_lap<T>::AssignMsg(bool is_chmpx, bool is_activated)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return CHM_INVALID_MSGID;
	}

	// freed list
	mqmsgheadlistlap	freed_msgs1(basic_type::pAbsPtr->free_msgs, basic_type::pShmBase, false);		// From rel
	PMQMSGHEADLIST		new_msghlst_ptr = freed_msgs1.PopFront();										// Get abs
	if(!new_msghlst_ptr){
		ERR_CHMPRN("Could not get new msgheadlist.");
		return CHM_INVALID_MSGID;
	}
	// Re-set freed list
	//
	basic_type::pAbsPtr->free_msg_count--;
	basic_type::pAbsPtr->free_msgs = freed_msgs1.GetFirstPtr(false);

	// Initialize new msghead list
	mqmsgheadlistlap	new_msghlist(new_msghlst_ptr, basic_type::pShmBase);							// From abs
	if(!new_msghlist.Clear()){
		ERR_CHMPRN("Failed to clear new PMQMSGHEADLIST %p.", new_msghlst_ptr);
		return CHM_INVALID_MSGID;
	}
	// keep msgid
	mqmsgheadlap	new_mqmsghead(new_msghlist.GetAbsMqMsgHeadPtr(), basic_type::pShmBase);
	msgid_t			new_msgid = new_mqmsghead.GetMsgId();

	// change status(Assigned)
	if(!new_mqmsghead.InitializeMqFlag(is_chmpx, is_activated)){
		ERR_CHMPRN("Could not initialize msghead flag to Assigned.");

		// For recovering, add freed list
		new_mqmsghead.NotAccountMqFlag();
		mqmsgheadlistlap	freed_msgs2(basic_type::pAbsPtr->free_msgs, basic_type::pShmBase, false);	// From rel(allow NULL)
		if(!freed_msgs2.Push(new_msghlst_ptr, true)){
			ERR_CHMPRN("Failed to add freed PMQMSGHEADLIST %p.", new_msghlst_ptr);
		}else{
			basic_type::pAbsPtr->free_msg_count++;
			basic_type::pAbsPtr->free_msgs = freed_msgs2.GetFirstPtr(false);
		}
		return CHM_INVALID_MSGID;
	}

	// Add assigned list
	mqmsgheadlistlap	base_msgs((is_chmpx ? basic_type::pAbsPtr->chmpx_msgs : !is_activated ? basic_type::pAbsPtr->assigned_msgs : basic_type::pAbsPtr->activated_msgs), basic_type::pShmBase, false);	// From rel(allow NULL)
	if(!base_msgs.Push(new_msghlst_ptr, true)){
		ERR_CHMPRN("Failed to add freed PMQMSGHEADLIST %p.", new_msghlst_ptr);

		// For recovering, add freed list
		new_mqmsghead.NotAccountMqFlag();
		mqmsgheadlistlap	freed_msgs3(basic_type::pAbsPtr->free_msgs, basic_type::pShmBase, false);	// From rel(allow NULL)
		if(!freed_msgs3.Push(new_msghlst_ptr, true)){
			ERR_CHMPRN("Failed to add freed PMQMSGHEADLIST %p.", new_msghlst_ptr);
		}else{
			basic_type::pAbsPtr->free_msg_count++;
			basic_type::pAbsPtr->free_msgs	= freed_msgs3.GetFirstPtr(false);
		}
		return CHM_INVALID_MSGID;
	}
	if(is_chmpx){
		basic_type::pAbsPtr->chmpx_msg_count++;
		basic_type::pAbsPtr->chmpx_msgs		= base_msgs.GetFirstPtr(false);
	}else if(!is_activated){
		basic_type::pAbsPtr->assigned_msg_count++;
		basic_type::pAbsPtr->assigned_msgs	= base_msgs.GetFirstPtr(false);
	}else{	// is_activated
		basic_type::pAbsPtr->activated_msg_count++;
		basic_type::pAbsPtr->activated_msgs	= base_msgs.GetFirstPtr(false);
	}
	return new_msgid;
}

template<typename T>
msgid_t chminfo_lap<T>::GetRandomMsgId(bool is_chmpx, bool is_activated)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return CHM_INVALID_MSGID;
	}

	// first: lastest msgid
	msgid_t	msgid;
	if(is_chmpx){
		msgid = basic_type::pAbsPtr->last_msgid_chmpx;
	}else{	// !is_chmpx
		if(is_activated){
			msgid = basic_type::pAbsPtr->last_msgid_activated;
		}else{	// !is_activated
			if(CHM_INVALID_MSGID == basic_type::pAbsPtr->last_msgid_activated || basic_type::pAbsPtr->last_msgid_assigned < basic_type::pAbsPtr->last_msgid_activated){
				msgid = basic_type::pAbsPtr->last_msgid_assigned;
			}else{
				msgid = basic_type::pAbsPtr->last_msgid_activated;
			}
		}
	}

	// find lastest msgid and get next msgid
	if(CHM_INVALID_MSGID != msgid){
		// Find
		mqmsgheadarrlap	msgheadarr(basic_type::pAbsPtr->rel_chmpxmsgarea, basic_type::pShmBase, basic_type::pAbsPtr->max_mqueue, false);	// From Relative
		PMQMSGHEADLIST	list_ptr = msgheadarr.Find(msgid, true);								// To abs
		if(!list_ptr){
			ERR_CHMPRN("Could not find msgid(0x%016" PRIx64 "), but try to get first msgid in list.", msgid);
			msgid = CHM_INVALID_MSGID;
		}else{
			// build list
			mqmsgheadlistlap	msglist(list_ptr, basic_type::pShmBase, true);					// From abs
			if(!msglist.ToNext(true)){
				ERR_CHMPRN("Could not get next msgid by msgid(0x%016" PRIx64 "), but try to get first msgid in list.", msgid);
				msgid = CHM_INVALID_MSGID;
			}else{
				mqmsgheadlap	msghead(msglist.GetAbsMqMsgHeadPtr(), basic_type::pShmBase);	// From abs
				msgid = msghead.GetMsgId();
			}
		}
	}

	// when could not get next msgid yet.
	if(CHM_INVALID_MSGID == msgid){
		PMQMSGHEADLIST	base_ptr = NULL;
		if(is_chmpx){
			if(!basic_type::pAbsPtr->chmpx_msgs || 0L == basic_type::pAbsPtr->chmpx_msg_count){
				ERR_CHMPRN("There is no msgid in chmpx msg list.");
				return CHM_INVALID_MSGID;
			}
			base_ptr = basic_type::pAbsPtr->chmpx_msgs;

		}else{	// !is_chmpx
			if(is_activated){
				if(!basic_type::pAbsPtr->activated_msgs || 0L == basic_type::pAbsPtr->activated_msg_count){
					ERR_CHMPRN("There is no msgid in activated msg list.");
					return CHM_INVALID_MSGID;
				}
				base_ptr = basic_type::pAbsPtr->activated_msgs;

			}else{	// !is_activated
				if(!basic_type::pAbsPtr->assigned_msgs || 0L == basic_type::pAbsPtr->assigned_msg_count){
					if(!basic_type::pAbsPtr->activated_msgs || 0L == basic_type::pAbsPtr->activated_msg_count){
						ERR_CHMPRN("There is no msgid in activated/assigned msg list.");
						return CHM_INVALID_MSGID;
					}
					base_ptr = basic_type::pAbsPtr->activated_msgs;
				}else{
					base_ptr = basic_type::pAbsPtr->assigned_msgs;
				}
			}
		}
		mqmsgheadlistlap	msglist(base_ptr, basic_type::pShmBase, false);					// From Relative
		mqmsgheadlap		msghead(msglist.GetAbsMqMsgHeadPtr(), basic_type::pShmBase);	// From abs
		msgid = msghead.GetMsgId();
	}

	// set last msgid into cache
	if(is_chmpx){
		basic_type::pAbsPtr->last_msgid_chmpx = msgid;
	}else{	// !is_chmpx
		if(is_activated){
			basic_type::pAbsPtr->last_msgid_activated = msgid;
		}else{	// !is_activated
			basic_type::pAbsPtr->last_msgid_assigned = msgid;
		}
	}
	return msgid;
}

template<typename T>
bool chminfo_lap<T>::SetMqFlagStatus(msgid_t msgid, bool is_assigned, bool is_activated)
{
	if(CHM_INVALID_MSGID == msgid || (!is_assigned && is_activated)){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}

	// If not assigned(and activated), it means to free msg.
	if(!is_assigned && !is_activated){
		return FreeMsg(msgid);
	}
	// After here, assigned msg.
	//

	// get msghead
	mqmsgheadarrlap	msgheadarr(basic_type::pAbsPtr->rel_chmpxmsgarea, basic_type::pShmBase, basic_type::pAbsPtr->max_mqueue, false);	// From Relative
	PMQMSGHEADLIST	msg_list_ptr = msgheadarr.Find(msgid, true);							// To abs
	if(!msg_list_ptr){
		ERR_CHMPRN("Could not find msgid(0x%016" PRIx64 ").", msgid);
		return false;
	}
	mqmsgheadlistlap	msg_list(msg_list_ptr, basic_type::pShmBase, true);					// From abs
	mqmsgheadlap		msghead(msg_list.GetAbsMqMsgHeadPtr(), basic_type::pShmBase);		// From abs

	// Set
	if(msghead.IsChmpxProc()){
		if(is_assigned && !is_activated){
			ERR_CHMPRN("msgid(0x%016" PRIx64 ") is used chmpx process, but specified invalid status Assigned and NOT activated.", msgid);
			return false;
		}
		if(!msghead.SetMqFlagStatus(is_assigned, is_activated)){
			ERR_CHMPRN("Failed to set status Assigned and Activated to msgid(0x%016" PRIx64 ").", msgid);
			return false;
		}

	}else if(msghead.IsClientProc()){
		// Client msg
		if(is_assigned && !is_activated){
			// Activated -> NOT activated
			if(!msghead.IsActivated()){
				ERR_CHMPRN("msgid(0x%016" PRIx64 ") is already not activated.", msgid);
				return true;
			}

			// Set status
			if(!msghead.SetMqFlagStatus(true, false)){
				ERR_CHMPRN("Failed to set status Assigned and NOT activated to msgid(0x%016" PRIx64 ").", msgid);
				return false;
			}

			// retrieve msg from now(activated) list.
			if(NULL == (msg_list_ptr = msg_list.Retrieve())){
				WAN_CHMPRN("Failed to remove msgid(0x%016" PRIx64 ") from activated list.", msgid);
				return false;
			}
			if(0L < basic_type::pAbsPtr->activated_msg_count){
				basic_type::pAbsPtr->activated_msg_count--;
				if(0L < basic_type::pAbsPtr->activated_msg_count){
					basic_type::pAbsPtr->activated_msgs = msg_list.GetFirstPtr(false);
				}else{
					basic_type::pAbsPtr->activated_msgs = NULL;
				}
			}else{
				basic_type::pAbsPtr->activated_msgs = NULL;
			}

			// add msg to assigned list
			msg_list.Reset(basic_type::pAbsPtr->assigned_msgs, basic_type::pShmBase, false);	// From Relative(allow NULL)
			if(!msg_list.Push(msg_list_ptr, true)){
				ERR_CHMPRN("Failed to add msgid(0x%016" PRIx64 ") assigned msgs.", msgid);
				return false;
			}
			basic_type::pAbsPtr->assigned_msg_count++;
			basic_type::pAbsPtr->assigned_msgs = msg_list.GetFirstPtr(false);

		}else{	// is_assigned && is_activated
			// NOT activated -> Activated
			if(msghead.IsActivated()){
				ERR_CHMPRN("msgid(0x%016" PRIx64 ") is already activated.", msgid);
				return true;
			}

			// Set status
			if(!msghead.SetMqFlagStatus(true, true)){
				ERR_CHMPRN("Failed to set status Assigned and Activated to msgid(0x%016" PRIx64 ").", msgid);
				return false;
			}

			// retrieve msg from now(assigned) list.
			if(NULL == (msg_list_ptr = msg_list.Retrieve())){
				WAN_CHMPRN("Failed to remove msgid(0x%016" PRIx64 ") from assigned list.", msgid);
				return false;
			}
			if(0L < basic_type::pAbsPtr->assigned_msg_count){
				basic_type::pAbsPtr->assigned_msg_count--;
				if(0L < basic_type::pAbsPtr->assigned_msg_count){
					basic_type::pAbsPtr->assigned_msgs = msg_list.GetFirstPtr(false);
				}else{
					basic_type::pAbsPtr->assigned_msgs = NULL;
				}
			}else{
				basic_type::pAbsPtr->assigned_msgs = NULL;
			}

			// add msg to activated list
			msg_list.Reset(basic_type::pAbsPtr->activated_msgs, basic_type::pShmBase, false);	// From Relative(allow NULL)
			if(!msg_list.Push(msg_list_ptr, true)){
				ERR_CHMPRN("Failed to add msgid(0x%016" PRIx64 ") assigned msgs.", msgid);
				return false;
			}
			basic_type::pAbsPtr->activated_msg_count++;
			basic_type::pAbsPtr->activated_msgs = msg_list.GetFirstPtr(false);
		}
	}else{
		ERR_CHMPRN("msgid(0x%016" PRIx64 ") is not accounted.", msgid);
		return false;
	}
	return true;
}

//
// Get all msgid from Assigned, and Activated list by PID.
// (not get from chmpx list)
//
template<typename T>
bool chminfo_lap<T>::GetMsgidListByPid(pid_t pid, msgidlist_t& list, bool is_clear_list)
{
	if(CHM_INVALID_PID == pid){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}
	if(is_clear_list){
		list.clear();
	}

	// get
	mqmsgheadlistlap	msg_list;
	if(basic_type::pAbsPtr->activated_msgs){
		msg_list.Reset(basic_type::pAbsPtr->activated_msgs, basic_type::pShmBase, false);		// From rel
		if(!msg_list.GetMsgidListByPid(pid, list, false)){
			ERR_CHMPRN("Something error occured during getting pid list from activated list.");
			return false;
		}
	}
	if(basic_type::pAbsPtr->assigned_msgs){
		msg_list.Reset(basic_type::pAbsPtr->assigned_msgs, basic_type::pShmBase, false);		// From rel
		if(!msg_list.GetMsgidListByPid(pid, list, false)){
			ERR_CHMPRN("Something error occured during getting pid list from assigned list.");
			return false;
		}
	}
	return true;
}

//
// Get all msgid from Activated list for all PID.
// (not get from chmpx list)
//
template<typename T>
bool chminfo_lap<T>::GetMsgidListByUniqPid(msgidlist_t& list, bool is_clear_list)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}
	if(is_clear_list){
		list.clear();
	}

	// get
	mqmsgheadlistlap	msg_list;
	if(basic_type::pAbsPtr->activated_msgs){
		msg_list.Reset(basic_type::pAbsPtr->activated_msgs, basic_type::pShmBase, false);		// From rel
		if(!msg_list.GetMsgidListByUniqPid(list, false)){
			ERR_CHMPRN("Something error occured during getting pid list from activated list.");
			return false;
		}
	}
	return true;
}

template<typename T>
bool chminfo_lap<T>::GetSelfChmpxSvr(PCHMPXSVR chmpxsvr) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.GetSelfChmpxSvr(chmpxsvr);
}

template<typename T>
bool chminfo_lap<T>::GetChmpxSvr(chmpxid_t chmpxid, PCHMPXSVR chmpxsvr) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.GetChmpxSvr(chmpxid, chmpxsvr);
}

template<typename T>
bool chminfo_lap<T>::GetChmpxSvrs(PCHMPXSVR* ppchmpxsvrs, long& count) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.GetChmpxSvrs(ppchmpxsvrs, count);
}

template<typename T>
bool chminfo_lap<T>::MergeChmpxSvrs(PCHMPXSVR pchmpxsvrs, long count, bool is_remove, bool is_init_process, int eqfd)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.MergeChmpxSvrs(pchmpxsvrs, basic_type::pAbsPtr->chmpxid_type, count, (NULL != basic_type::pAbsPtr->client_pids), is_remove, is_init_process, eqfd);
}

template<typename T>
bool chminfo_lap<T>::GetGroup(std::string& group) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.GetGroup(group);
}

template<typename T>
bool chminfo_lap<T>::SuspendAutoMerge(void)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}
	basic_type::pAbsPtr->is_auto_merge_suspend = true;
	return true;
}

template<typename T>
bool chminfo_lap<T>::ResetAutoMerge(void)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}
	basic_type::pAbsPtr->is_auto_merge_suspend = false;
	return true;
}

template<typename T>
bool chminfo_lap<T>::IsServerMode(void) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.IsServerMode();
}

template<typename T>
bool chminfo_lap<T>::IsSlaveMode(void) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.IsSlaveMode();
}

template<typename T>
long chminfo_lap<T>::GetReplicaCount(void) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return DEFAULT_REPLICA_COUNT;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.GetReplicaCount();
}

template<typename T>
chmpxid_t chminfo_lap<T>::GetSelfChmpxId(void) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return CHM_INVALID_CHMPXID;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.GetSelfChmpxId();
}

template<typename T>
chmpxid_t chminfo_lap<T>::GetNextRingChmpxId(chmpxid_t chmpxid) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return CHM_INVALID_CHMPXID;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.GetNextRingChmpxId(chmpxid);
}

template<typename T>
long chminfo_lap<T>::GetServerCount(void) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return 0L;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.GetServerCount();
}

template<typename T>
long chminfo_lap<T>::GetSlaveCount(void) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return 0L;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.GetSlaveCount();
}

template<typename T>
long chminfo_lap<T>::GetUpServerCount(void) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return 0L;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.GetUpServerCount();
}

template<typename T>
chmpxpos_t chminfo_lap<T>::GetSelfServerPos(void) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return CHM_INVALID_CHMPXLISTPOS;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.GetSelfServerPos();
}

template<typename T>
chmpxpos_t chminfo_lap<T>::GetNextServerPos(chmpxpos_t startpos, chmpxpos_t nowpos, bool is_skip_self, bool is_cycle) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return CHM_INVALID_CHMPXLISTPOS;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.GetNextServerPos(startpos, nowpos, is_skip_self, is_cycle);
}

template<typename T>
bool chminfo_lap<T>::CheckContainsChmpxSvrs(const char* hostname) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.CheckContainsChmpxSvrs(hostname);
}

template<typename T>
bool chminfo_lap<T>::CheckStrictlyContainsChmpxSvrs(const char* hostname, const short* pctlport, const char* pcuk, std::string* pnormalizedname, PCHMPXSSL pssl) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.CheckStrictlyContainsChmpxSvrs(hostname, pctlport, pcuk, pnormalizedname, pssl);
}

template<typename T>
bool chminfo_lap<T>::IsServerChmpxId(chmpxid_t chmpxid) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.IsServerChmpxId(chmpxid);
}

template<typename T>

chmpxid_t chminfo_lap<T>::GetChmpxIdBySock(int sock, int type) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return CHM_INVALID_CHMPXID;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.GetChmpxIdBySock(sock, type);
}

template<typename T>
chmpxid_t chminfo_lap<T>::GetChmpxIdByToServerName(const char* hostname, short ctlport, const char* cuk, const char* ctlendpoints, const char* custom_seed) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return CHM_INVALID_CHMPXID;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.GetChmpxIdByToServerName(basic_type::pAbsPtr->chmpxid_type, hostname, ctlport, cuk, ctlendpoints, custom_seed);
}

//
// This method is linear search in list, then it is not good performance.
//
template<typename T>
chmpxid_t chminfo_lap<T>::GetChmpxIdByStatus(chmpxsts_t status, bool part_match) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return CHM_INVALID_CHMPXID;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.GetChmpxIdByStatus(status, part_match);
}

template<typename T>
chmpxid_t chminfo_lap<T>::GetRandomServerChmpxId(bool without_suspend)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return CHM_INVALID_CHMPXID;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.GetRandomServerChmpxId(without_suspend);
}

template<typename T>
chmpxid_t chminfo_lap<T>::GetServerChmpxIdByHash(chmhash_t hash) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return CHM_INVALID_CHMPXID;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.GetServerChmpxIdByHash(hash);
}

template<typename T>
bool chminfo_lap<T>::GetServerChmHashsByHashs(chmhash_t hash, chmhashlist_t& basehashs, bool with_pending, bool without_down, bool without_suspend)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.GetServerChmHashsByHashs(hash, basehashs, with_pending, without_down, without_suspend);
}

template<typename T>
bool chminfo_lap<T>::GetServerChmpxIdByHashs(chmhash_t hash, chmpxidlist_t& chmpxids, bool with_pending, bool without_down, bool without_suspend)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.GetServerChmpxIdByHashs(hash, chmpxids, with_pending, without_down, without_suspend);
}

template<typename T>
long chminfo_lap<T>::GetServerChmpxIds(chmpxidlist_t& list, bool with_pending, bool without_down, bool without_suspend) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return 0L;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.GetServerChmpxIds(list, with_pending, without_down, without_suspend);
}

template<typename T>
bool chminfo_lap<T>::GetServerBase(chmpxpos_t pos, std::string* pname, chmpxid_t* pchmpxid, short* pport, short* pctlport, hostport_list_t* pendpoints, hostport_list_t* pctlendpoints) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.GetServerBase(pos, pname, pchmpxid, pport, pctlport, pendpoints, pctlendpoints);
}

template<typename T>
bool chminfo_lap<T>::GetServerBase(chmpxid_t chmpxid, std::string* pname, short* pport, short* pctlport, hostport_list_t* pendpoints, hostport_list_t* pctlendpoints) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.GetServerBase(chmpxid, pname, pport, pctlport, pendpoints, pctlendpoints);
}

template<typename T>
bool chminfo_lap<T>::GetServerBase(chmpxid_t chmpxid, CHMPXSSL& ssl) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.GetServerBase(chmpxid, ssl);
}

template<typename T>
bool chminfo_lap<T>::GetServerSocks(chmpxid_t chmpxid, socklist_t& socklist, int& ctlsock) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.GetServerSocks(chmpxid, socklist, ctlsock);
}

template<typename T>
bool chminfo_lap<T>::GetServerHash(chmpxid_t chmpxid, chmhash_t& base, chmhash_t& pending) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.GetServerHash(chmpxid, base, pending);
}

template<typename T>
bool chminfo_lap<T>::GetMaxHashCount(chmhash_t& basehash, chmhash_t& pengindhash) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.GetMaxHashCount(basehash, pengindhash);
}

template<typename T>
chmpxsts_t chminfo_lap<T>::GetServerStatus(chmpxid_t chmpxid) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return CHMPXSTS_SRVOUT_DOWN_NORMAL;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.GetServerStatus(chmpxid);
}

template<typename T>
bool chminfo_lap<T>::GetSelfPorts(short& port, short& ctlport) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.GetSelfPorts(port, ctlport);
}

template<typename T>
bool chminfo_lap<T>::GetSelfSocks(int& sock, int& ctlsock) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.GetSelfSocks(sock, ctlsock);
}

template<typename T>
bool chminfo_lap<T>::GetSelfHash(chmhash_t& base, chmhash_t& pending) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.GetSelfHash(base, pending);
}

template<typename T>
chmpxsts_t chminfo_lap<T>::GetSelfStatus(void) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return CHMPXSTS_SLAVE_DOWN_NORMAL;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.GetSelfStatus();
}

template<typename T>
bool chminfo_lap<T>::GetSelfSsl(CHMPXSSL& ssl) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.GetSelfSsl(ssl);
}

template<typename T>
bool chminfo_lap<T>::GetSelfBase(std::string* pname, short* pport, short* pctlport, std::string* pcuk, std::string* pcustom_seed, hostport_list_t* pendpoints, hostport_list_t* pctlendpoints, hostport_list_t* pforward_peers, hostport_list_t* preverse_peers) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.GetSelfBase(pname, pport, pctlport, pcuk, pcustom_seed, pendpoints, pctlendpoints, pforward_peers, preverse_peers);
}

template<typename T>
long chminfo_lap<T>::GetSlaveChmpxIds(chmpxidlist_t& list) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return 0L;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.GetSlaveChmpxIds(list);
}

template<typename T>
bool chminfo_lap<T>::GetSlaveBase(chmpxid_t chmpxid, std::string* pname, short* pctlport, std::string* pcuk, std::string* pcustom_seed, hostport_list_t* pendpoints, hostport_list_t* pctlendpoints, hostport_list_t* pforward_peers, hostport_list_t* preverse_peers) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.GetSlaveBase(chmpxid, pname, pctlport, pcuk, pcustom_seed, pendpoints, pctlendpoints, pforward_peers, preverse_peers);
}

template<typename T>
bool chminfo_lap<T>::GetSlaveSock(chmpxid_t chmpxid, socklist_t& socklist) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.GetSlaveSock(chmpxid, socklist);
}

template<typename T>
chmpxsts_t chminfo_lap<T>::GetSlaveStatus(chmpxid_t chmpxid) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return CHMPXSTS_SLAVE_DOWN_NORMAL;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.GetSlaveStatus(chmpxid);
}

template<typename T>
bool chminfo_lap<T>::SetServerSocks(chmpxid_t chmpxid, int sock, int ctlsock, int type)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.SetServerSocks(chmpxid, sock, ctlsock, type);
}

template<typename T>
bool chminfo_lap<T>::RemoveServerSock(chmpxid_t chmpxid, int sock)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.RemoveServerSock(chmpxid, sock);
}

template<typename T>
bool chminfo_lap<T>::SetServerHash(chmpxid_t chmpxid, chmhash_t base, chmhash_t pending, int type)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.SetServerHash(chmpxid, base, pending, type);
}

template<typename T>
bool chminfo_lap<T>::SetServerStatus(chmpxid_t chmpxid, chmpxsts_t status)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.SetServerStatus(chmpxid, status, (NULL != basic_type::pAbsPtr->client_pids));
}

template<typename T>
bool chminfo_lap<T>::UpdateLastStatusTime(chmpxid_t chmpxid)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.UpdateLastStatusTime(chmpxid);
}

template<typename T>
bool chminfo_lap<T>::IsOperating(void)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.IsOperating();
}

template<typename T>
bool chminfo_lap<T>::UpdateHash(int type, bool is_allow_operating, bool is_allow_slave_mode)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.UpdateHash(type, is_allow_operating, is_allow_slave_mode);
}

template<typename T>
bool chminfo_lap<T>::SetSelfSocks(int sock, int ctlsock)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.SetSelfSocks(sock, ctlsock);
}

template<typename T>
bool chminfo_lap<T>::SetSelfHash(chmhash_t base, chmhash_t pending, int type)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.SetSelfHash(base, pending, type);
}

template<typename T>
bool chminfo_lap<T>::SetSelfStatus(chmpxsts_t status)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.SetSelfStatus(status, (NULL != basic_type::pAbsPtr->client_pids));
}

template<typename T>
bool chminfo_lap<T>::SetSlaveBase(chmpxid_t chmpxid, const char* hostname, short ctlport, const char* cuk, const char* custom_seed, const hostport_list_t& endpoints, const hostport_list_t& ctlendpoints, const hostport_list_t& forward_peers, const hostport_list_t& reverse_peers, const PCHMPXSSL pssl)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.SetSlaveBase(basic_type::pAbsPtr->chmpxid_type, chmpxid, hostname, ctlport, cuk, custom_seed, endpoints, ctlendpoints, forward_peers, reverse_peers, pssl);
}

template<typename T>
bool chminfo_lap<T>::SetSlaveSock(chmpxid_t chmpxid, int sock)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.SetSlaveSock(chmpxid, sock);
}

template<typename T>
bool chminfo_lap<T>::SetSlaveStatus(chmpxid_t chmpxid, chmpxsts_t status)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.SetSlaveStatus(chmpxid, status);
}

template<typename T>
bool chminfo_lap<T>::RemoveSlaveSock(chmpxid_t chmpxid, int sock, bool is_remove_empty)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}

	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	if(!tmpchmpxman.RemoveSlaveSock(chmpxid, sock)){
		WAN_CHMPRN("Failed to remove the sock(%d) from slave.", sock);
	}

	socklist_t	socklist;
	if(is_remove_empty && tmpchmpxman.GetSlaveSock(chmpxid, socklist) && socklist.empty()){
		// slave has no socket, so remove slave
		return tmpchmpxman.RemoveSlave(chmpxid, CHM_INVALID_HANDLE);	// do not need eqfd
	}
	return true;
}

//
// This method forces to close all socket.
//
template<typename T>
bool chminfo_lap<T>::RemoveSlave(chmpxid_t chmpxid, int eqfd)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.RemoveSlave(chmpxid, eqfd);
}

template<typename T>
bool chminfo_lap<T>::CheckSockInAllChmpx(int sock) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.CheckSockInAllChmpx(sock);
}

template<typename T>
bool chminfo_lap<T>::AddStat(chmpxid_t chmpxid, bool is_sent, size_t length, const struct timespec& elapsed_time)
{
	if(CHM_INVALID_CHMPXID == chmpxid){
		ERR_CHMPRN("Parameter are wrong.");
		return false;
	}
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.AddStat(chmpxid, is_sent, length, elapsed_time);
}

template<typename T>
bool chminfo_lap<T>::GetStat(PCHMSTAT pserver, PCHMSTAT pslave) const
{
	if(!pserver || !pslave){
		ERR_CHMPRN("Parameter are wrong.");
		return false;
	}
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}
	chmpxmanlap	tmpchmpxman(&basic_type::pAbsPtr->chmpx_man, basic_type::pShmBase);
	return tmpchmpxman.GetStat(pserver, pslave);
}

template<typename T>
bool chminfo_lap<T>::RetrieveClientPid(pid_t pid)
{
	if(CHM_INVALID_PID == pid){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}
	if(!basic_type::pAbsPtr->client_pids){
		ERR_CHMPRN("Could not free pid(%d), because not found it.", pid);
		return false;
	}

	// Retrieve
	cltproclistlap	cltproc_pids(basic_type::pAbsPtr->client_pids, basic_type::pShmBase, false);	// From Relative
	PCLTPROCLIST	ptgcltproc;
	if(NULL == (ptgcltproc = cltproc_pids.Retrieve(pid))){
		ERR_CHMPRN("Could not free pid(%d), maybe not found it.", pid);
		return false;
	}
	basic_type::pAbsPtr->client_pids = cltproc_pids.GetFirstPtr(false);								// To Rel

	// clear
	cltproclistlap	tgcltproc(ptgcltproc, basic_type::pShmBase, true);								// From abs
	tgcltproc.Clear();

	// set into free list
	if(basic_type::pAbsPtr->free_pids){
		cltproclistlap	free_pids(basic_type::pAbsPtr->free_pids, basic_type::pShmBase, false);		// From Relative
		free_pids.Insert(ptgcltproc, true);
	}else{
		basic_type::pAbsPtr->free_pids = tgcltproc.GetFirstPtr(false);
	}
	return true;
}

template<typename T>
bool chminfo_lap<T>::AddClientPid(pid_t pid)
{
	if(CHM_INVALID_PID == pid){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}

	// Check
	// if client_pids is NULL, but following logic works good.
	//
	cltproclistlap	cltproc_pids(basic_type::pAbsPtr->client_pids, basic_type::pShmBase, false);	// From Relative
	PCLTPROCLIST	ptgcltproc;
	if(basic_type::pAbsPtr->client_pids && (NULL != (ptgcltproc = cltproc_pids.Find(pid, true)))){	// To Abs
		MSG_CHMPRN("pid(%d) already set in pid list.", pid);
		return true;
	}

	// get one object from free list
	if(!basic_type::pAbsPtr->free_pids){
		ERR_CHMPRN("There is no space for set pid list.");
		return false;
	}
	cltproclistlap	free_pids(basic_type::pAbsPtr->free_pids, basic_type::pShmBase, false);			// From Relative
	ptgcltproc = free_pids.Retrieve();
	basic_type::pAbsPtr->free_pids = free_pids.GetFirstPtr(false);									// To Rel

	// initialize
	cltproclistlap	tgcltproc(ptgcltproc, basic_type::pShmBase, true);								// From abs
	tgcltproc.Initialize(pid);

	// set
	if(basic_type::pAbsPtr->client_pids){
		// set into list
		if(!cltproc_pids.Insert(ptgcltproc, true)){													// from abs
			ERR_CHMPRN("Failed to insert pid object to list.");
			// recover...
			tgcltproc.Clear();
			free_pids.Insert(ptgcltproc, true);		// do not check error
			basic_type::pAbsPtr->free_pids = free_pids.GetFirstPtr(false);							// to rel
			return false;
		}
	}else{
		basic_type::pAbsPtr->client_pids = tgcltproc.GetFirstPtr(false);							// to rel
	}
	return true;
}

template<typename T>
bool chminfo_lap<T>::GetAllPids(pidlist_t& list)
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}

	if(basic_type::pAbsPtr->client_pids){
		cltproclistlap	cltproc_pids(basic_type::pAbsPtr->client_pids, basic_type::pShmBase, false);	// From Relative
		if(!cltproc_pids.GetAllPids(list)){
			ERR_CHMPRN("Something error occured during getting all pid list.");
			return false;
		}
	}else{
		list.clear();
	}
	return true;
}

template<typename T>
bool chminfo_lap<T>::IsClientPids(void) const
{
	if(!basic_type::pAbsPtr || !basic_type::pShmBase){
		ERR_CHMPRN("PCHMINFO does not set.");
		return false;
	}
	return (NULL != basic_type::pAbsPtr->client_pids);
}

typedef	chminfo_lap<CHMINFO>	chminfolap;

#endif	// CHMSTRUCTURE_TCC

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noexpandtab sw=4 ts=4 fdm=marker
 * vim<600: noexpandtab sw=4 ts=4
 */
