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
#ifndef	CHMCONF_H
#define	CHMCONF_H

#include <k2hash/k2hshm.h>
#include <string>
#include <list>

#include "chmcommon.h"
#include "chmutil.h"
#include "chmstructure.h"
#include "chmeventbase.h"

//---------------------------------------------------------
// Symbols
//---------------------------------------------------------
#define	UNINITIALIZE_REVISION			(-1)

//---------------------------------------------------------
// Structures
//---------------------------------------------------------
//
// configuration information for host and port
//
typedef struct host_port_pair{
	std::string		host;
	short			port;

	host_port_pair() :
		host(""),
		port(0)
	{}

	explicit host_port_pair(const struct host_port_pair& other) :
		host(other.host),
		port(other.port)
	{}

	bool compare(const struct host_port_pair& other) const
	{
		return (host == other.host && port == other.port);
	}
	struct host_port_pair& operator=(const struct host_port_pair& other)
	{
		host	= other.host;
		port	= other.port;
		return *this;
	}
	bool operator==(const struct host_port_pair& other) const
	{
		return compare(other);
	}
	bool operator!=(const struct host_port_pair& other) const
	{
		return !compare(other);
	}
	std::string get_string(void) const
	{
		std::string	result(host);
		if(CHM_INVALID_PORT != port){
			result += ":";
			result += to_string(port);
		}
		return result;
	}
}HOSTPORTPAIR, *PHOSTPORTPAIR;

typedef std::list<HOSTPORTPAIR>	hostport_list_t;

inline std::string get_hostports_string(const hostport_list_t& list)
{
	std::string	result;
	for(hostport_list_t::const_iterator iter = list.begin(); list.end() != iter; ++iter){
		if(!result.empty()){
			result += ",";
		}
		result += iter->get_string();
	}
	return result;
}

inline bool compare_hostports(const hostport_list_t& src1, const hostport_list_t& src2)
{
	if(src1.size() != src2.size()){
		return false;
	}
	hostport_list_t::const_iterator	iter1;
	hostport_list_t::const_iterator	iter2;
	for(iter1 = src1.begin(), iter2 = src2.begin(); iter1 != src1.end() && iter2 != src2.end(); ++iter1, ++iter2){
		if(iter1->host != iter2->host || iter1->port != iter2->port){
			return false;
		}
	}
	return true;
}

//
// configuration information for node
//
typedef struct chm_node_cfg_info{
	std::string		name;
	short			port;
	short			ctlport;
	bool			is_ssl;
	bool			verify_peer;			// verify ssl client peer on server
	bool			is_ca_file;
	std::string		capath;
	std::string		server_cert;
	std::string		server_prikey;
	std::string		slave_cert;
	std::string		slave_prikey;
	std::string		cuk;					// after v1.0.71
	hostport_list_t	endpoints;				// after v1.0.71
	hostport_list_t	ctlendpoints;			// after v1.0.71
	hostport_list_t	forward_peers;			// after v1.0.71
	hostport_list_t	reverse_peers;			// after v1.0.71
	std::string		custom_seed;			// after v1.0.71

	chm_node_cfg_info() :
		name(""),
		port(0),
		ctlport(0),
		is_ssl(false),
		verify_peer(false),
		is_ca_file(false),
		capath(""),
		server_cert(""),
		server_prikey(""),
		slave_cert(""),
		slave_prikey(""),
		cuk(""),							// after v1.0.71
		custom_seed("")						// after v1.0.71
	{}

	explicit chm_node_cfg_info(const struct chm_node_cfg_info& other) :
		name(other.name),
		port(other.port),
		ctlport(other.ctlport),
		is_ssl(other.is_ssl),
		verify_peer(other.verify_peer),
		is_ca_file(other.is_ca_file),
		capath(other.capath),
		server_cert(other.server_cert),
		server_prikey(other.server_prikey),
		slave_cert(other.slave_cert),
		slave_prikey(other.slave_prikey),
		cuk(other.cuk),						// after v1.0.71
		endpoints(other.endpoints),			// after v1.0.71
		ctlendpoints(other.ctlendpoints),	// after v1.0.71
		forward_peers(other.forward_peers),	// after v1.0.71
		reverse_peers(other.reverse_peers),	// after v1.0.71
		custom_seed(other.custom_seed)		// after v1.0.71
	{}

	bool compare(const struct chm_node_cfg_info& other) const
	{
		if(	name			== other.name			&&
			port			== other.port			&&
			ctlport			== other.ctlport		&&
			is_ssl			== other.is_ssl			&&
			verify_peer		== other.verify_peer	&&
			is_ca_file		== other.is_ca_file		&&
			capath			== other.capath			&&
			server_cert		== other.server_cert	&&
			server_prikey	== other.server_prikey	&&
			slave_cert		== other.slave_cert		&&
			slave_prikey	== other.slave_prikey	&&
			cuk				== other.cuk			&&		// after v1.0.71
			endpoints		== other.endpoints		&&		// after v1.0.71
			ctlendpoints	== other.ctlendpoints	&&		// after v1.0.71
			forward_peers	== other.forward_peers	&&		// after v1.0.71
			reverse_peers	== other.reverse_peers	&&		// after v1.0.71
			custom_seed		== other.custom_seed	)		// after v1.0.71
		{
			return true;
		}
		return false;
	}
	struct chm_node_cfg_info& operator=(const struct chm_node_cfg_info& other)
	{
		name			= other.name;
		port			= other.port;
		ctlport			= other.ctlport;
		is_ssl			= other.is_ssl;
		verify_peer		= other.verify_peer;
		is_ca_file		= other.is_ca_file;
		capath			= other.capath;
		server_cert		= other.server_cert;
		server_prikey	= other.server_prikey;
		slave_cert		= other.slave_cert;
		slave_prikey	= other.slave_prikey;
		cuk				= other.cuk;					// after v1.0.71
		endpoints		= other.endpoints;				// after v1.0.71
		ctlendpoints	= other.ctlendpoints;			// after v1.0.71
		forward_peers	= other.forward_peers;			// after v1.0.71
		reverse_peers	= other.reverse_peers;			// after v1.0.71
		custom_seed		= other.custom_seed;			// after v1.0.71

		return *this;
	}
	bool operator==(const struct chm_node_cfg_info& other) const
	{
		return compare(other);
	}
	bool operator!=(const struct chm_node_cfg_info& other) const
	{
		return !compare(other);
	}
}CHMNODE_CFGINFO, *PCHMNODE_CFGINFO;

typedef std::list<CHMNODE_CFGINFO>	chmnode_cfginfos_t;

struct chm_node_cfg_info_sort
{
	bool operator()(const CHMNODE_CFGINFO& lchmnodecfginfo, const CHMNODE_CFGINFO& rchmnodecfginfo) const
    {
		if(lchmnodecfginfo.name == rchmnodecfginfo.name){
			if(lchmnodecfginfo.port == rchmnodecfginfo.port){
				if(lchmnodecfginfo.ctlport == rchmnodecfginfo.ctlport){
					return lchmnodecfginfo.cuk < rchmnodecfginfo.cuk;
				}else{
					return lchmnodecfginfo.ctlport < rchmnodecfginfo.ctlport;
				}
			}else{
				return lchmnodecfginfo.port < rchmnodecfginfo.port;
			}
		}else{
			return lchmnodecfginfo.name < rchmnodecfginfo.name;
		}
    }
};

struct chm_node_cfg_info_same_name
{
	bool operator()(const CHMNODE_CFGINFO& lchmnodecfginfo, const CHMNODE_CFGINFO& rchmnodecfginfo) const
    {
		return lchmnodecfginfo.name == rchmnodecfginfo.name;
    }
};

struct chm_node_cfg_info_same_name_other
{
	bool operator()(const CHMNODE_CFGINFO& lchmnodecfginfo, const CHMNODE_CFGINFO& rchmnodecfginfo) const
    {
		return (lchmnodecfginfo.name == rchmnodecfginfo.name && lchmnodecfginfo.ctlport == rchmnodecfginfo.ctlport && lchmnodecfginfo.cuk == rchmnodecfginfo.cuk);
    }
};

struct chm_node_cfg_info_same_eps
{
	bool operator()(const CHMNODE_CFGINFO& lchmnodecfginfo, const CHMNODE_CFGINFO& rchmnodecfginfo) const
    {
		return (lchmnodecfginfo.endpoints == rchmnodecfginfo.endpoints && lchmnodecfginfo.ctlendpoints == rchmnodecfginfo.ctlendpoints);
    }
};

struct chm_node_cfg_info_same_eps_peers
{
	bool operator()(const CHMNODE_CFGINFO& lchmnodecfginfo, const CHMNODE_CFGINFO& rchmnodecfginfo) const
    {
		return (lchmnodecfginfo.endpoints == rchmnodecfginfo.endpoints && lchmnodecfginfo.ctlendpoints == rchmnodecfginfo.ctlendpoints && lchmnodecfginfo.forward_peers == rchmnodecfginfo.forward_peers && lchmnodecfginfo.reverse_peers == rchmnodecfginfo.reverse_peers);
    }
};

//
// configuration information for all
//
typedef struct chm_cfg_info{
	std::string			groupname;
	long				revision;
	bool				is_server_mode;
	bool				is_random_mode;
	short				self_ctlport;
	std::string			self_cuk;					// after v1.0.71
	CHMPXID_SEED_TYPE	chmpxid_type;				// after v1.0.71
	long				max_chmpx_count;
	long				replica_count;
	long				max_server_mq_cnt;			// MQ count for server/slave node
	long				max_client_mq_cnt;			// MQ count for client process
	long				mqcnt_per_attach;			// MQ count at each attaching from each client process
	long				max_q_per_servermq;			// Queue count in one MQ on server/slave node
	long				max_q_per_clientmq;			// Queue count in one MQ on client process
	long				max_mq_per_client;			// max MQ count for one client process
	long				max_histlog_count;
	int					retrycnt;
	int					mq_retrycnt;
	bool				mq_ack;
	int					timeout_wait_socket;
	int					timeout_wait_connect;
	int					timeout_wait_mq;
	bool				is_auto_merge;
	bool				is_do_merge;
	time_t				timeout_merge;
	int					sock_thread_cnt;
	int					mq_thread_cnt;
	int					max_sock_pool;
	time_t				sock_pool_timeout;
	bool				k2h_fullmap;
	int					k2h_mask_bitcnt;
	int					k2h_cmask_bitcnt;
	int					k2h_max_element;
	time_t				date;
	chmss_ver_t			ssl_min_ver;				// SSL/TLS minimum version
	std::string			nssdb_dir;					// NSSDB directory path like "SSL_DIR" environment for NSS
	chmnode_cfginfos_t	servers;
	chmnode_cfginfos_t	slaves;

	chm_cfg_info() :
		groupname(""),
		revision(UNINITIALIZE_REVISION),
		is_server_mode(false),
		is_random_mode(false),
		self_ctlport(CHM_INVALID_PORT),
		self_cuk(""),								// after v1.0.71
		chmpxid_type(CHMPXID_SEED_NAME),
		max_chmpx_count(0L),
		replica_count(0L),
		max_server_mq_cnt(0L),
		max_client_mq_cnt(0L),
		mqcnt_per_attach(0L),
		max_q_per_servermq(0L),
		max_q_per_clientmq(0L),
		max_mq_per_client(0L),
		max_histlog_count(0L),
		retrycnt(-1),
		mq_retrycnt(-1),
		mq_ack(true),
		timeout_wait_socket(-1),
		timeout_wait_connect(-1),
		timeout_wait_mq(-1),
		is_auto_merge(false),
		is_do_merge(false),
		timeout_merge(0),
		sock_thread_cnt(0),
		mq_thread_cnt(0),
		max_sock_pool(1),
		sock_pool_timeout(0),
		k2h_fullmap(true),
		k2h_mask_bitcnt(K2HShm::DEFAULT_MASK_BITCOUNT),
		k2h_cmask_bitcnt(K2HShm::DEFAULT_COLLISION_MASK_BITCOUNT),
		k2h_max_element(K2HShm::DEFAULT_MAX_ELEMENT_CNT),
		date(0L),
		ssl_min_ver(CHM_SSLTLS_VER_DEFAULT),
		nssdb_dir("")
	{}

	explicit chm_cfg_info(const struct chm_cfg_info& other) :
		groupname(other.groupname),
		revision(other.revision),
		is_server_mode(other.is_server_mode),
		is_random_mode(other.is_random_mode),
		self_ctlport(other.self_ctlport),
		self_cuk(other.self_cuk),					// after v1.0.71
		chmpxid_type(other.chmpxid_type),			// after v1.0.71
		max_chmpx_count(other.max_chmpx_count),
		replica_count(other.replica_count),
		max_server_mq_cnt(other.max_server_mq_cnt),
		max_client_mq_cnt(other.max_client_mq_cnt),
		mqcnt_per_attach(other.mqcnt_per_attach),
		max_q_per_servermq(other.max_q_per_servermq),
		max_q_per_clientmq(other.max_q_per_clientmq),
		max_mq_per_client(other.max_mq_per_client),
		max_histlog_count(other.max_histlog_count),
		retrycnt(other.retrycnt),
		mq_retrycnt(other.mq_retrycnt),
		mq_ack(other.mq_ack),
		timeout_wait_socket(other.timeout_wait_socket),
		timeout_wait_connect(other.timeout_wait_connect),
		timeout_wait_mq(other.timeout_wait_mq),
		is_auto_merge(other.is_auto_merge),
		is_do_merge(other.is_do_merge),
		timeout_merge(other.timeout_merge),
		sock_thread_cnt(other.sock_thread_cnt),
		mq_thread_cnt(other.mq_thread_cnt),
		max_sock_pool(other.max_sock_pool),
		sock_pool_timeout(other.sock_pool_timeout),
		k2h_fullmap(other.k2h_fullmap),
		k2h_mask_bitcnt(other.k2h_mask_bitcnt),
		k2h_cmask_bitcnt(other.k2h_cmask_bitcnt),
		k2h_max_element(other.k2h_max_element),
		date(other.date),
		ssl_min_ver(other.ssl_min_ver),
		nssdb_dir(other.nssdb_dir)
	{
		chmnode_cfginfos_t::const_iterator	iter;
		for(iter = other.servers.begin(); other.servers.end() != iter; ++iter){
			servers.push_back(*iter);
		}
		for(iter = other.slaves.begin(); other.slaves.end() != iter; ++iter){
			slaves.push_back(*iter);
		}
	}

	bool compare(const struct chm_cfg_info& other) const
	{
		if(	groupname			== other.groupname				&&
			revision			== other.revision				&&
			is_server_mode		== other.is_server_mode			&&
			is_random_mode		== other.is_random_mode			&&
			self_ctlport		== other.self_ctlport			&&
			chmpxid_type		== other.chmpxid_type			&&		// after v1.0.71
			max_chmpx_count		== other.max_chmpx_count		&&
			replica_count		== other.replica_count			&&
			max_server_mq_cnt	== other.max_server_mq_cnt		&&
			max_client_mq_cnt	== other.max_client_mq_cnt		&&
			mqcnt_per_attach	== other.mqcnt_per_attach		&&
			max_q_per_servermq	== other.max_q_per_servermq		&&
			max_q_per_clientmq	== other.max_q_per_clientmq		&&
			max_mq_per_client	== other.max_mq_per_client		&&
			max_histlog_count	== other.max_histlog_count		&&
			retrycnt			== other.retrycnt				&&
			mq_retrycnt			== other.mq_retrycnt			&&
			mq_ack				== other.mq_ack					&&
			timeout_wait_socket	== other.timeout_wait_socket	&&
			timeout_wait_connect== other.timeout_wait_connect	&&
			timeout_wait_mq		== other.timeout_wait_mq		&&
			is_auto_merge		== other.is_auto_merge			&&
			is_do_merge			== other.is_do_merge			&&
			timeout_merge		== other.timeout_merge			&&
			sock_thread_cnt		== other.sock_thread_cnt		&&
			mq_thread_cnt		== other.mq_thread_cnt			&&
			max_sock_pool		== other.max_sock_pool			&&
			sock_pool_timeout	== other.sock_pool_timeout		&&
			//k2h_fullmap		== other.k2h_fullmap			&&	// [NOTICE] k2hash parameter is not compared
			//k2h_mask_bitcnt	== other.k2h_mask_bitcnt		&&
			//k2h_cmask_bitcnt	== other.k2h_cmask_bitcnt		&&
			//k2h_max_element	== other.k2h_max_element		&&
			//date				== other.date					&&	// [NOTICE] date is not compared
			ssl_min_ver			== other.ssl_min_ver			&&
			nssdb_dir			== other.nssdb_dir				&&
			servers				== other.servers				&&
			slaves				== other.slaves					)
		{
			return true;
		}
		return false;
	}
	struct chm_cfg_info& operator=(const struct chm_cfg_info& other)
	{
		groupname				= other.groupname;
		revision				= other.revision;
		is_server_mode			= other.is_server_mode;
		is_random_mode			= other.is_random_mode;
		self_ctlport			= other.self_ctlport;
		self_cuk				= other.self_cuk;
		chmpxid_type			= other.chmpxid_type;
		max_chmpx_count			= other.max_chmpx_count;
		replica_count			= other.replica_count;
		max_server_mq_cnt		= other.max_server_mq_cnt;
		max_client_mq_cnt		= other.max_client_mq_cnt;
		mqcnt_per_attach		= other.mqcnt_per_attach;
		max_q_per_servermq		= other.max_q_per_servermq;
		max_q_per_clientmq		= other.max_q_per_clientmq;
		max_mq_per_client		= other.max_mq_per_client;
		max_histlog_count		= other.max_histlog_count;
		retrycnt				= other.retrycnt;
		mq_retrycnt				= other.mq_retrycnt;
		mq_ack					= other.mq_ack;
		timeout_wait_socket		= other.timeout_wait_socket;
		timeout_wait_connect	= other.timeout_wait_connect;
		timeout_wait_mq			= other.timeout_wait_mq;
		is_auto_merge			= other.is_auto_merge;
		is_do_merge				= other.is_do_merge;
		timeout_merge			= other.timeout_merge;
		sock_thread_cnt			= other.sock_thread_cnt;
		mq_thread_cnt			= other.mq_thread_cnt;
		max_sock_pool			= other.max_sock_pool;
		sock_pool_timeout		= other.sock_pool_timeout;
		k2h_fullmap				= other.k2h_fullmap;
		k2h_mask_bitcnt			= other.k2h_mask_bitcnt;
		k2h_cmask_bitcnt		= other.k2h_cmask_bitcnt;
		k2h_max_element			= other.k2h_max_element;
		date					= other.date;
		ssl_min_ver				= other.ssl_min_ver;
		nssdb_dir				= other.nssdb_dir;

		chmnode_cfginfos_t::const_iterator	iter;
		for(iter = other.servers.begin(); other.servers.end() != iter; ++iter){
			servers.push_back(*iter);
		}
		for(iter = other.slaves.begin(); other.slaves.end() != iter; ++iter){
			slaves.push_back(*iter);
		}
		return *this;
	}
	bool operator==(const struct chm_cfg_info& other) const
	{
		return compare(other);
	}
	bool operator!=(const struct chm_cfg_info& other) const
	{
		return !compare(other);
	}
}CHMCFGINFO, *PCHMCFGINFO;

// raw all configuration
typedef struct cfg_raw{
	strmap_t	global;
	strmaparr_t	server_nodes;
	strmaparr_t	slave_nodes;
}CFGRAW, *PCFGRAW;

// For using temporary carry common values for loading
//
typedef struct chm_conf_common_carry_value{
	short		port;
	short		ctlport;
	bool		server_mode;
	bool		is_ssl;
	bool		verify_peer;
	bool		is_ca_file;
	std::string	capath;
	std::string	server_cert;
	std::string	server_prikey;
	std::string	slave_cert;
	std::string	slave_prikey;
	bool		server_mode_by_comp;			// for check server/slave mode by checking control port and cuk and server name.
	bool		found_ssl;
	bool		found_ssl_verify_peer;

	chm_conf_common_carry_value() :
		port(CHM_INVALID_PORT),
		ctlport(CHM_INVALID_PORT),
		server_mode(false),
		is_ssl(false),
		verify_peer(false),
		is_ca_file(false),
		capath(""),
		server_cert(""),
		server_prikey(""),
		slave_cert(""),
		slave_prikey(""),
		server_mode_by_comp(false),
		found_ssl(false),
		found_ssl_verify_peer(false)
	{}
}CHMCONF_CCV, *PCHMCONF_CCV;

//---------------------------------------------------------
// Class CHMConf
//---------------------------------------------------------
class CHMConf : public ChmEventBase
{
	public:
		enum ConfType{
			CONF_UNKNOWN = 0,
			CONF_INI,
			CONF_JSON,
			CONF_JSON_STR,	// JSON string type(not file), On this case, this class does not use inotify.
			CONF_YAML
		};

	protected:
		static int			lockval;			// like mutex

		std::string			cfgfile;
		std::string			strjson;
		short				ctlport_param;
		std::string			cuk_param;
		CHMConf::ConfType	type;
		int					inotifyfd;
		int					watchfd;
		PCHMCFGINFO			pchmcfginfo;

	protected:
		explicit CHMConf(int eventqfd = CHM_INVALID_HANDLE, ChmCntrl* pcntrl = NULL, const char* file = NULL, short ctlport = CHM_INVALID_PORT, const char* cuk = NULL, const char* pJson = NULL);

		virtual bool LoadConfiguration(CHMCFGINFO& chmcfginfo) const = 0;

		const CHMCFGINFO* GetConfiguration(bool is_check_update = false);		// thread unsafe
		bool RawCheckContainsNodeInfoList(const char* hostname, const short* pctlport, const char* cuk, bool with_forward, chmnode_cfginfos_t* pnodeinfos, strlst_t* pnormnames, bool is_server, bool is_check_update = false);
		bool CheckContainsServerInfoList(const char* hostname, chmnode_cfginfos_t* pnodeinfos, strlst_t* pnormnames, bool is_check_update = false);
		bool CheckContainsSlaveInfoList(const char* hostname, chmnode_cfginfos_t* pnodeinfos, strlst_t* pnormnames, bool is_check_update = false);
		bool RawCheckContainsNodeInfo(const char* hostname, const CHMNODE_CFGINFO& nodeinfos, std::string& normalizedname, bool is_server, bool with_forward);
		bool GetServerInfo(const char* hostname, short ctlport, const char* cuk, CHMNODE_CFGINFO& svrnodeinfo, std::string& normalizedname, bool is_check_update = false);
		bool GetSelfServerInfo(CHMNODE_CFGINFO& svrnodeinfo, std::string& normalizedname, bool is_check_update = false);
		bool GetSlaveInfo(const char* hostname, short ctlport, const char* cuk, CHMNODE_CFGINFO& slvnodeinfo, std::string& normalizedname, bool is_check_update = false);
		bool GetSelfSlaveInfo(CHMNODE_CFGINFO& slvnodeinfo, std::string& normalizedname, bool is_check_update = false);
		bool GetSelfReversePeer(hostport_list_t& reverse_peers, bool& is_server);

		bool IsWatching(void) const { return (CHM_INVALID_HANDLE != watchfd); }
		uint CheckNotifyEvent(void);
		bool ResetEventQueue(void);

		bool IsFileType(void) const { return (CONF_INI == type || CONF_YAML == type || CONF_JSON == type); }
		bool IsJsonStringType(void) const { return (CONF_JSON_STR == type); }

	public:
		static CHMConf* GetCHMConf(int eventqfd, ChmCntrl* pcntrl, const char* config, short ctlport = CHM_INVALID_PORT, const char* cuk = NULL, bool is_check_env = true, std::string* normalize_config = NULL);	// Class Factory
		static std::string GetChmpxidTypeString(CHMPXID_SEED_TYPE type);
		static std::string GetSslVersionString(chmss_ver_t version);

		virtual ~CHMConf();

		virtual bool Clean(void);

		virtual bool GetEventQueueFds(event_fds_t& fds);
		virtual bool SetEventQueue(void);
		virtual bool UnsetEventQueue(void);
		virtual bool IsEventQueueFd(int fd);

		virtual bool Send(PCOMPKT pComPkt, const unsigned char* pbody, size_t blength);
		virtual bool Receive(int fd);
		virtual bool NotifyHup(int fd);
		virtual bool Processing(PCOMPKT pComPkt);

		bool CheckConfFile(void) const;
		bool CheckUpdate(void);
		bool GetConfiguration(CHMCFGINFO& config, bool is_check_update = false);

		bool CheckContainsNodeInfoList(const char* hostname, chmnode_cfginfos_t* pnodeinfos, strlst_t* pnormnames, bool is_check_update = false);
		bool GetNodeInfo(const char* hostname, short ctlport, const char* cuk, CHMNODE_CFGINFO& nodeinfo, std::string& normalizedname, bool is_only_server, bool is_check_update = false);
		bool GetSelfNodeInfo(CHMNODE_CFGINFO& nodeinfo, std::string& normalizedname, bool is_check_update = false);
		bool IsSelfReversePeer(const char* hostname);
		bool SearchContainsNodeInfoList(short ctlport, const char* cuk, CHMNODE_CFGINFO& nodeinfo, std::string& normalizedname, bool is_server, bool is_check_update = false);

		bool GetServerList(strlst_t& server_list);
		bool IsServerList(const char* hostname, std::string& fqdn);
		bool IsServerList(std::string& fqdn);

		bool GetSlaveList(strlst_t& slave_list);
		bool IsSlaveList(const char* hostname, std::string& fqdn);
		bool IsSlaveList(std::string& fqdn);
		bool IsSsl(void) const;
};

//---------------------------------------------------------
// Class CHMIniConf
//---------------------------------------------------------
class CHMIniConf : public CHMConf
{
	friend class CHMConf;

	protected:
		explicit CHMIniConf(int eventqfd = CHM_INVALID_HANDLE, ChmCntrl* pcntrl = NULL, const char* file = NULL, short ctlport = CHM_INVALID_PORT, const char* cuk = NULL);

		virtual bool LoadConfiguration(CHMCFGINFO& chmcfginfo) const;
		bool LoadConfigurationRaw(CFGRAW& chmcfgraw) const;
		bool ReadFileContents(const std::string& filename, strlst_t& linelst, strlst_t& allfiles) const;

	public:
		virtual ~CHMIniConf();
};

//---------------------------------------------------------
// Class CHMYamlBaseConf
//---------------------------------------------------------
class CHMYamlBaseConf : public CHMConf
{
	friend class CHMConf;

	protected:
		explicit CHMYamlBaseConf(int eventqfd = CHM_INVALID_HANDLE, ChmCntrl* pcntrl = NULL, const char* file = NULL, short ctlport = CHM_INVALID_PORT, const char* cuk = NULL, const char* pJson = NULL);

		virtual bool LoadConfiguration(CHMCFGINFO& chmcfginfo) const;

	public:
		virtual ~CHMYamlBaseConf();
};

//---------------------------------------------------------
// Class CHMJsonConf
//---------------------------------------------------------
class CHMJsonConf : public CHMYamlBaseConf
{
	friend class CHMConf;

	protected:
		explicit CHMJsonConf(int eventqfd = CHM_INVALID_HANDLE, ChmCntrl* pcntrl = NULL, const char* file = NULL, short ctlport = CHM_INVALID_PORT, const char* cuk = NULL);

	public:
		virtual ~CHMJsonConf();
};

//---------------------------------------------------------
// Class CHMJsonStringConf
//---------------------------------------------------------
class CHMJsonStringConf : public CHMYamlBaseConf
{
	friend class CHMConf;

	protected:
		explicit CHMJsonStringConf(int eventqfd = CHM_INVALID_HANDLE, ChmCntrl* pcntrl = NULL, const char* pJson = NULL, short ctlport = CHM_INVALID_PORT, const char* cuk = NULL);

	public:
		virtual ~CHMJsonStringConf();
};

//---------------------------------------------------------
// Class CHMYamlConf
//---------------------------------------------------------
class CHMYamlConf : public CHMYamlBaseConf
{
	friend class CHMConf;

	protected:
		explicit CHMYamlConf(int eventqfd = CHM_INVALID_HANDLE, ChmCntrl* pcntrl = NULL, const char* file = NULL, short ctlport = CHM_INVALID_PORT, const char* cuk = NULL);

	public:
		virtual ~CHMYamlConf();
};

#endif	// CHMCONF_H

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
