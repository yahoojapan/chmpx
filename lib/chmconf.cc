/*
 * CHMPX
 *
 * Copyright 2014 Yahoo! JAPAN corporation.
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
 * the LICENSE file that was distributed with this source code.
 *
 * AUTHOR:   Takeshi Nakatani
 * CREATE:   Tue July 1 2014
 * REVISION:
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <libgen.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/inotify.h>
#include <yaml.h>

#include <fstream>
#include <string>

#include "chmcommon.h"
#include "chmconf.h"
#include "chmconfutil.h"
#include "chmcntrl.h"
#include "chmstructure.h"
#include "chmutil.h"
#include "chmnetdb.h"
#include "chmregex.h"
#include "chmeventsock.h"
#include "chmdbg.h"

using namespace	std;

//---------------------------------------------------------
// Symbols
//---------------------------------------------------------
// Keywards
#define	CFG_GLOBAL_SEC_STR				"GLOBAL"
#define	CFG_SVRNODE_SEC_STR				"SVRNODE"
#define	CFG_SLVNODE_SEC_STR				"SLVNODE"

#define	INICFG_COMMENT_CHAR				'#'
#define	INICFG_SEC_START_CHAR			'['
#define	INICFG_SEC_END_CHAR				']'
#define	INICFG_GLOBAL_SEC_STR			"[" CFG_GLOBAL_SEC_STR "]"
#define	INICFG_SVRNODE_SEC_STR			"[" CFG_SVRNODE_SEC_STR "]"
#define	INICFG_SLVNODE_SEC_STR			"[" CFG_SLVNODE_SEC_STR "]"
#define	INICFG_GROUP_STR				"GROUP"
#define	INICFG_VERSION_STR				"FILEVERSION"
#define	INICFG_MODE_STR					"MODE"
#define	INICFG_DELIVERMODE_STR			"DELIVERMODE"
#define	INICFG_REPLICA_STR				"REPLICA"
#define	INICFG_MAXCHMPX_STR				"MAXCHMPX"
#define	INICFG_MAXMQSERVER_STR			"MAXMQSERVER"
#define	INICFG_MAXMQCLIENT_STR			"MAXMQCLIENT"
#define	INICFG_MQPERATTACH_STR			"MQPERATTACH"
#define	INICFG_MAXQPERSERVERMQ_STR		"MAXQPERSERVERMQ"
#define	INICFG_MAXQPERCLIENTMQ_STR		"MAXQPERCLIENTMQ"
#define	INICFG_MAXMQPERCLIENT_STR		"MAXMQPERCLIENT"
#define	INICFG_MAXHISTLOG_STR			"MAXHISTLOG"
#define	INICFG_DATE_STR					"DATE"
#define	INICFG_PORT_STR					"PORT"
#define	INICFG_CTLPORT_STR				"CTLPORT"
#define	INICFG_SELFCTLPORT_STR			"SELFCTLPORT"
#define	INICFG_RWTIMEOUT_STR			"RWTIMEOUT"
#define	INICFG_RETRYCNT_STR				"RETRYCNT"
#define	INICFG_MQRWTIMEOUT_STR			"MQRWTIMEOUT"
#define	INICFG_MQRETRYCNT_STR			"MQRETRYCNT"
#define	INICFG_MQACK_STR				"MQACK"
#define	INICFG_CONTIMEOUT_STR			"CONTIMEOUT"
#define	INICFG_AUTOMERGE_STR			"AUTOMERGE"
#define	INICFG_DOMERGE_STR				"DOMERGE"
#define	INICFG_MERGETIMEOUT_STR			"MERGETIMEOUT"
#define	INICFG_SOCKTHREADCNT_STR		"SOCKTHREADCNT"
#define	INICFG_MQTHREADCNT_STR			"MQTHREADCNT"
#define	INICFG_MAXSOCKPOOL_STR			"MAXSOCKPOOL"
#define	INICFG_SOCKPOOLTIMEOUT_STR		"SOCKPOOLTIMEOUT"
#define	INICFG_SSL_STR					"SSL"
#define	INICFG_SSL_VERIFY_PEER_STR		"SSL_VERIFY_PEER"
#define	INICFG_CAPATH_STR				"CAPATH"
#define	INICFG_SERVER_CERT_STR			"SERVER_CERT"
#define	INICFG_SERVER_PRIKEY_STR		"SERVER_PRIKEY"
#define	INICFG_SLAVE_CERT_STR			"SLAVE_CERT"
#define	INICFG_SLAVE_PRIKEY_STR			"SLAVE_PRIKEY"
#define	INICFG_K2HFULLMAP_STR			"K2HFULLMAP"
#define	INICFG_K2HMASKBIT_STR			"K2HMASKBIT"
#define	INICFG_K2HCMASKBIT_STR			"K2HCMASKBIT"
#define	INICFG_K2HMAXELE_STR			"K2HMAXELE"
#define	INICFG_NAME_STR					"NAME"

#define	INICFG_INCLUDE_STR				"INCLUDE"
#define	INICFG_KV_SEP					"="

#define	INICFG_BOOL_ON					"ON"
#define	INICFG_BOOL_YES					"YES"
#define	INICFG_BOOL_OFF					"OFF"
#define	INICFG_BOOL_NO					"NO"
#define	INICFG_STRING_NULL				"NULL"
#define	INICFG_MODE_SERVER_STR			"server"
#define	INICFG_MODE_SLAVE_STR			"slave"
#define	INICFG_DELIVERMODE_RANDOM_STR	"random"
#define	INICFG_DELIVERMODE_HASH_STR		"hash"

// for analizing .ini file
#define	INICFG_INSEC_NOT				0
#define	INICFG_INSEC_GLOBAL				1
#define	INICFG_INSEC_SVRNODE			2
#define	INICFG_INSEC_SLVNODE			3
#define	INICFG_INSEC_UNKNOWN			1000

//---------------------------------------------------------
// CHMConf Class Factory
//---------------------------------------------------------
CHMConf* CHMConf::GetCHMConf(int eventqfd, ChmCntrl* pcntrl, const char* config, short ctlport, bool is_check_env, string* normalize_config)
{
	string		tmpconf("");
	CHMCONFTYPE	conftype = check_chmconf_type_ex(config, (is_check_env ? CHM_CONFFILE_ENV_NAME : NULL), (is_check_env ? CHM_JSONCONF_ENV_NAME : NULL), &tmpconf);

	CHMConf*	result = NULL;
	switch(conftype){
		case	CHMCONF_TYPE_INI_FILE:
			result = new CHMIniConf(eventqfd, pcntrl, tmpconf.c_str(), ctlport);
			break;
		case	CHMCONF_TYPE_YAML_FILE:
			result = new CHMYamlConf(eventqfd, pcntrl, tmpconf.c_str(), ctlport);
			break;
		case	CHMCONF_TYPE_JSON_FILE:
			result = new CHMJsonConf(eventqfd, pcntrl, tmpconf.c_str(), ctlport);
			break;
		case	CHMCONF_TYPE_JSON_STRING:
			result = new CHMJsonStringConf(eventqfd, pcntrl, tmpconf.c_str(), ctlport);
			break;
		case	CHMCONF_TYPE_UNKNOWN:
		case	CHMCONF_TYPE_NULL:
		default:
			ERR_CHMPRN("configuration \"%s\" is unknown type.", CHMEMPTYSTR(config) ? "empty" : config);
			break;
	}
	if(result && normalize_config){
		(*normalize_config) = tmpconf;
	}
	return result;
}

//---------------------------------------------------------
// CHMConf Class
//---------------------------------------------------------
CHMConf::CHMConf(int eventqfd, ChmCntrl* pcntrl, const char* file, short ctlport, const char* pJson) : ChmEventBase(eventqfd, pcntrl), ctlport_param(ctlport), type(CONF_UNKNOWN), inotifyfd(CHM_INVALID_HANDLE), watchfd(CHM_INVALID_HANDLE), pchmcfginfo(NULL)
{
	// [NOTE] Either file or json string.
	//
	if(!CHMEMPTYSTR(file)){
		cfgfile = file;
	}else if(!CHMEMPTYSTR(pJson)){
		strjson = pJson;
	}
}

CHMConf::~CHMConf()
{
	Clean();
}

bool CHMConf::Clean(void)
{
	if(IsWatching()){
		MSG_CHMPRN("Should call RemoveNotify function before calling this destructor.");
		UnsetEventQueue();
	}
	CHM_Delete(pchmcfginfo);

	return ChmEventBase::Clean();
}

bool CHMConf::GetEventQueueFds(event_fds_t& fds)
{
	if(!IsWatching()){
		if(IsJsonStringType()){
			// Json string type --> return true
			return true;
		}
		MSG_CHMPRN("There is no watching file.");
		return false;
	}
	fds.clear();
	fds.push_back(inotifyfd);
	return true;
}

bool CHMConf::SetEventQueue(void)
{
	if(cfgfile.empty()){
		if(IsJsonStringType()){
			// Json string type --> return true
			return true;
		}
		ERR_CHMPRN("This object does not have file path.");
		return false;
	}
	if(CHM_INVALID_HANDLE == eqfd){
		ERR_CHMPRN("event fd is invalid.");
		return false;
	}
	if(IsWatching()){
		if(!UnsetEventQueue()){
			return false;
		}
	}

	// create inotify
	if(-1 == (inotifyfd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC))){
		ERR_CHMPRN("Failed to create inotify, error %d", errno);
		return false;
	}

	// add inotify
	if(CHM_INVALID_HANDLE == (watchfd = inotify_add_watch(inotifyfd, cfgfile.c_str(), IN_DELETE_SELF | IN_MODIFY | IN_MOVE_SELF))){
		ERR_CHMPRN("Could not watch file %s (errno=%d)", cfgfile.c_str(), errno);
		CHM_CLOSE(inotifyfd);
		return false;
	}

	// add event
	struct epoll_event	epoolev;
	memset(&epoolev, 0, sizeof(struct epoll_event));
	epoolev.data.fd	= inotifyfd;
	epoolev.events	= EPOLLIN | EPOLLET | EPOLLRDHUP;			// EPOLLRDHUP is set

	if(-1 == epoll_ctl(eqfd, EPOLL_CTL_ADD, inotifyfd, &epoolev)){
		ERR_CHMPRN("Failed to add inotifyfd(%d)-watchfd(%d) to event fd(%d), error=%d", inotifyfd, watchfd, eqfd, errno);
		inotify_rm_watch(inotifyfd, watchfd);
		watchfd = CHM_INVALID_HANDLE;
		CHM_CLOSE(inotifyfd);
		return false;
	}
	return true;
}

bool CHMConf::UnsetEventQueue(void)
{
	if(!IsWatching()){
		if(IsJsonStringType()){
			// Json string type --> return true
			return true;
		}
		MSG_CHMPRN("There is no watching file.");
		return false;
	}
	bool	result = true;

	if(CHM_INVALID_HANDLE == inotifyfd){
		WAN_CHMPRN("inotifyfd is invalid.");
	}else{
		epoll_ctl(eqfd, EPOLL_CTL_DEL, inotifyfd, NULL);

		if(CHM_INVALID_HANDLE == inotify_rm_watch(inotifyfd, watchfd)){
			if(EINVAL == errno){
				WAN_CHMPRN("Failed to remove watching fd(%d) from inotify fd(%d), because watchfd is invalid. It maybe removed file, so continue...", watchfd, inotifyfd);
			}else{
				ERR_CHMPRN("Could not remove watching fd(%d) from inotify fd(%d), errno=%d", watchfd, inotifyfd, errno);
				result = false;
			}
		}
		CHM_CLOSE(inotifyfd);
	}
	inotifyfd	= CHM_INVALID_HANDLE;
	watchfd		= CHM_INVALID_HANDLE;

	return result;
}


bool CHMConf::IsEventQueueFd(int fd)
{
	if(CHM_INVALID_HANDLE == fd){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(!IsWatching()){
		if(!IsJsonStringType()){
			MSG_CHMPRN("There is no watching file.");
		}
		return false;
	}
	// check
	if(inotifyfd != fd){
		return false;
	}
	return true;
}

bool CHMConf::ResetEventQueue(void)
{
	if(IsWatching()){
		if(!UnsetEventQueue()){
			return false;
		}
	}
	return SetEventQueue();
}

bool CHMConf::Receive(int fd)
{
	if(!IsWatching()){
		if(!IsJsonStringType()){
			ERR_CHMPRN("There is no watching file.");
		}
		return false;
	}
	if(!IsEventQueueFd(fd)){
		ERR_CHMPRN("fd(%d) is not this object event fd.", fd);
		return false;
	}

	// get type
	bool	is_check_file	= false;
	bool	is_reload		= false;
	uint	type			= CheckNotifyEvent();
	if(type & IN_CLOSE_WRITE){
		is_reload = true;
	}
	if(type & IN_MODIFY){
		is_reload = true;
	}
	if(type & IN_DELETE_SELF){
		is_check_file = true;
	}
	if(type & IN_MOVE_SELF){
		is_check_file = true;
	}

	// configration file is moved or deleted
	if(is_check_file){
		struct timespec	sleeptime;
		SET_TIMESPEC(&sleeptime, 0, (1000 * 1000));		// 1ms

		// wait for 500ms
		for(int cnt = 0; !CheckConfFile() && cnt < 500; nanosleep(&sleeptime, NULL), cnt++);

		// try to reset event
		if(!ResetEventQueue()){
			ERR_CHMPRN("Failed to set inotify event for configuration file(%s), then no more watching it.", cfgfile.c_str());
			return false;
		}
		is_reload = true;
	}

	// reload file & check update
	if(is_reload){
		if(CheckUpdate()){
			// somthing changed, so update internal data.
			if(!pChmCntrl->ConfigrationUpdateNotify()){
				ERR_CHMPRN("Failed to reinitialize internal data.");
				return false;
			}
		}else{
			MSG_CHMPRN("Reloaded configuration file(%s), but it is not changed.", cfgfile.c_str());
		}
	}
	return true;
}

bool CHMConf::Send(PCOMPKT pComPkt, const unsigned char* pbody, size_t blength)
{
	MSG_CHMPRN("Nothing to do in this object for this event.(Not implement thie event in this class)");
	return true;
}

bool CHMConf::NotifyHup(int fd)
{
	if(!IsWatching()){
		if(!IsJsonStringType()){
			MSG_CHMPRN("There is no watching file.");
		}
		return false;
	}
	if(!IsEventQueueFd(fd)){
		ERR_CHMPRN("fd(%d) is not this object event fd.", fd);
		return false;
	}
	return UnsetEventQueue();
}

bool CHMConf::Processing(PCOMPKT pComPkt)
{
	MSG_CHMPRN("Nothing to do in this object for this event.(Not implement thie event in this class)");
	return true;
}

//
// Return:	0				- Nothing to do
//			IN_CLOSE_WRITE	- The configuration file is wrote, should reload it.
//			IN_DELETE_SELF	- The configuration file is deleted, check it and reload assap.
//			IN_MOVE_SELF	- The configuration file is moved, check it and reload assap.
//
// Other event type is not handled here, because handled event type in this method is caught
// after those event.
//
uint CHMConf::CheckNotifyEvent(void)
{
	if(!IsWatching()){
		if(!IsJsonStringType()){
			MSG_CHMPRN("There is no watching file.");
		}
		return 0;
	}

	// read from inotify event
	unsigned char*	pevent;
	size_t			bytes;
	if(NULL == (pevent = chm_read(inotifyfd, &bytes))){
		WAN_CHMPRN("read no inotify event, no more inotify event data.");
		return 0;
	}

	// analize event types
	struct inotify_event*	in_event	= NULL;
	uint					result		= 0;
	for(unsigned char* ptr = pevent; (ptr + sizeof(struct inotify_event)) <= (pevent + bytes); ptr += sizeof(struct inotify_event) + in_event->len){
		in_event = reinterpret_cast<struct inotify_event*>(ptr);

		if(watchfd != in_event->wd){
			continue;
		}
		if(in_event->mask & IN_CLOSE_WRITE){
			MSG_CHMPRN("Configration file %s is wrote(%d).", cfgfile.c_str(), in_event->mask);
			result |= IN_CLOSE_WRITE;
		}else if(in_event->mask & IN_DELETE_SELF){
			MSG_CHMPRN("Configration file %s is deleted(%d).", cfgfile.c_str(), in_event->mask);
			result |= IN_DELETE_SELF;
		}else if(in_event->mask & IN_MOVE_SELF){
			MSG_CHMPRN("Configration file %s is moved(%d).", cfgfile.c_str(), in_event->mask);
			result |= IN_MOVE_SELF;
		}else if(in_event->mask & IN_MODIFY){
			MSG_CHMPRN("Configration file %s is modified(%d).", cfgfile.c_str(), in_event->mask);
			result |= IN_MODIFY;
		}else{
			WAN_CHMPRN("inotify event type(%u) is not handled because of waiting another event after it.", in_event->mask);
		}
	}
	CHM_Free(pevent);
	return result;
}

bool CHMConf::CheckConfFile(void) const
{
	if(!IsFileType() || !is_file_safe_exist(cfgfile.c_str())){
		return false;
	}
	return true;
}

bool CHMConf::CheckUpdate(void)
{
	if(pchmcfginfo && !CheckConfFile()){
		return false;
	}
	PCHMCFGINFO	pnewinfo = new CHMCFGINFO;

	// Load configration file.
	if(!LoadConfigration(*pnewinfo)){
		ERR_CHMPRN("Failed to load configuration from %s.", cfgfile.c_str());
		CHM_Delete(pnewinfo);
		return false;
	}

	bool	result = false;
	if(pchmcfginfo){
		if(!pchmcfginfo->compare(*pnewinfo)){
			// not same
			result = true;
		}
		CHM_Delete(pchmcfginfo);
	}else{
		result = true;
	}
	pchmcfginfo = pnewinfo;

	return result;
}

const CHMCFGINFO* CHMConf::GetConfiguration(bool is_check_update)
{
	if(IsFileType() && !CheckConfFile()){
		return NULL;
	}
	// Load & Check	configration file.
	if(is_check_update || !pchmcfginfo){
		CheckUpdate();
	}
	return pchmcfginfo;
}

bool CHMConf::GetServerInfo(const char* hostname, short ctlport, CHMNODE_CFGINFO& svrnodeinfo, bool is_check_update)
{
	if(CHMEMPTYSTR(hostname)){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(IsFileType() && !CheckConfFile()){
		return false;
	}

	// Load & Check	configration file.
	if(is_check_update || !pchmcfginfo){
		CheckUpdate();
	}
	// change hostname to fqdn
	string	globalname;
	if(!ChmNetDb::Get()->GetHostname(hostname, globalname, true)){
		MSG_CHMPRN("Could not convert hostname to global hostanme.");
		return false;
	}
	for(chmnode_cfginfos_t::const_iterator iter = pchmcfginfo->servers.begin(); iter != pchmcfginfo->servers.end(); ++iter){
		if(globalname == iter->name){
			if(CHM_INVALID_PORT == ctlport || ctlport == iter->ctlport){
				// found
				svrnodeinfo = *iter;
				return true;
			}
		}
	}
	return false;
}

bool CHMConf::GetSelfServerInfo(CHMNODE_CFGINFO& svrnodeinfo, bool is_check_update)
{
	return GetServerInfo("localhost", pchmcfginfo->self_ctlport, svrnodeinfo, is_check_update);
}

bool CHMConf::GetSlaveInfo(const char* hostname, short ctlport, CHMNODE_CFGINFO& slvnodeinfo, bool is_check_update)
{
	if(CHMEMPTYSTR(hostname)){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(IsFileType() && !CheckConfFile()){
		return false;
	}

	// Load & Check	configration file.
	if(is_check_update || !pchmcfginfo){
		CheckUpdate();
	}

	string		globalname;
	strlst_t	slave_list;
	for(chmnode_cfginfos_t::const_iterator iter = pchmcfginfo->slaves.begin(); iter != pchmcfginfo->slaves.end(); ++iter){
		slave_list.clear();
		slave_list.push_back(iter->name);
		if(IsMatchHostname(hostname, slave_list, globalname)){
			if(CHM_INVALID_PORT == ctlport || ctlport == iter->ctlport){
				// found
				slvnodeinfo.name			= globalname;
				slvnodeinfo.port			= iter->port;
				slvnodeinfo.ctlport			= iter->ctlport;
				slvnodeinfo.is_ssl			= iter->is_ssl;
				slvnodeinfo.verify_peer		= iter->verify_peer;
				slvnodeinfo.is_ca_file		= iter->is_ca_file;
				slvnodeinfo.capath			= iter->capath;
				slvnodeinfo.server_cert		= iter->server_cert;
				slvnodeinfo.server_prikey	= iter->server_prikey;
				slvnodeinfo.slave_cert		= iter->slave_cert;
				slvnodeinfo.slave_prikey	= iter->slave_prikey;
				return true;
			}
		}
	}
	return false;
}

bool CHMConf::GetSelfSlaveInfo(CHMNODE_CFGINFO& slvnodeinfo, bool is_check_update)
{
	return GetServerInfo("localhost", pchmcfginfo->self_ctlport, slvnodeinfo, is_check_update);
}

bool CHMConf::GetNodeInfo(const char* hostname, short ctlport, CHMNODE_CFGINFO& nodeinfo, bool is_only_server, bool is_check_update)
{
	if(IsFileType() && !CheckConfFile()){
		return false;
	}
	// Load & Check	configration file.
	if(is_check_update || !pchmcfginfo){
		CheckUpdate();
	}

	if(GetServerInfo(hostname, ctlport, nodeinfo, false)){		// already checked update
		return true;
	}
	if(is_only_server){
		return false;
	}
	// if not only server, next check slave nodes.
	return GetSlaveInfo(hostname, ctlport, nodeinfo, false);	// already checked update
}

bool CHMConf::GetSelfNodeInfo(CHMNODE_CFGINFO& nodeinfo, bool is_check_update)
{
	return GetNodeInfo("localhost", pchmcfginfo->self_ctlport, nodeinfo, pchmcfginfo->is_server_mode, is_check_update);
}

bool CHMConf::GetServerList(strlst_t& server_list)
{
	if(NULL == GetConfiguration()){
		ERR_CHMPRN("Could not get configuration file(%s) contents.", cfgfile.c_str());
		return false;
	}
	server_list.clear();
	for(chmnode_cfginfos_t::const_iterator iter = pchmcfginfo->servers.begin(); iter != pchmcfginfo->servers.end(); ++iter){
		server_list.push_back(iter->name);
	}
	server_list.sort(strarr_sort());

	return true;
}

bool CHMConf::IsServerList(const char* hostname, string& fqdn)
{
	strlst_t	server_list;
	if(!GetServerList(server_list)){
		ERR_CHMPRN("Could not get server list.");
		return false;
	}
	if(!IsInHostnameList(hostname, server_list, fqdn)){
		return false;
	}
	return true;
}

bool CHMConf::IsServerList(string& fqdn)
{
	return IsServerList("locahost", fqdn);
}

bool CHMConf::GetSlaveList(strlst_t& slave_list)
{
	if(NULL == GetConfiguration()){
		ERR_CHMPRN("Could not get configuration file(%s) contents.", cfgfile.c_str());
		return false;
	}
	slave_list.clear();

	for(chmnode_cfginfos_t::const_iterator iter = pchmcfginfo->slaves.begin(); iter != pchmcfginfo->slaves.end(); ++iter){
		slave_list.push_back(iter->name);
	}
	slave_list.sort(strarr_sort());

	return true;
}

bool CHMConf::IsSlaveList(const char* hostname, string& fqdn)
{
	strlst_t	slave_list;
	if(!GetSlaveList(slave_list)){
		ERR_CHMPRN("Could not get slave list.");
		return false;
	}
	if(!IsMatchHostname(hostname, slave_list, fqdn)){
		return false;
	}
	return true;
}

bool CHMConf::IsSlaveList(string& fqdn)
{
	return IsSlaveList("locahost", fqdn);
}

bool CHMConf::IsSsl(void) const
{
	if(!pchmcfginfo){
		MSG_CHMPRN("This object does not loading configration file yet.");
		return false;
	}

	for(chmnode_cfginfos_t::const_iterator iter = pchmcfginfo->servers.begin(); iter != pchmcfginfo->servers.end(); ++iter){
		if(iter->is_ssl){
			return true;
		}
	}
	return false;
}

//---------------------------------------------------------
// CHMIniConf Class
//---------------------------------------------------------
CHMIniConf::CHMIniConf(int eventqfd, ChmCntrl* pcntrl, const char* file, short ctlport) : CHMConf(eventqfd, pcntrl, file, ctlport, NULL)
{
	type = CHMConf::CONF_INI;
}

CHMIniConf::~CHMIniConf()
{
}

bool CHMIniConf::ReadFileContents(const string& filename, strlst_t& linelst, strlst_t& allfiles) const
{
	if(0 == filename.length()){
		ERR_CHMPRN("Configuration file path is wrong.");
		return false;
	}

	ifstream	cfgstream(filename.c_str(), ios::in);
	if(!cfgstream.good()){
		ERR_CHMPRN("Could not open(read only) file(%s)", filename.c_str());
		return false;
	}

	string		line;
	int			lineno;
	for(lineno = 1; cfgstream.good() && getline(cfgstream, line); lineno++){
		line = trim(line);
		if(0 == line.length()){
			continue;
		}

		// check only include
		string::size_type	pos;
		if(string::npos != (pos = line.find(INICFG_INCLUDE_STR))){
			string	value	= trim(line.substr(pos + 1));
			string	key		= trim(line.substr(0, pos));
			if(key == INICFG_INCLUDE_STR){
				// found include.
				bool	found_same_file = false;
				for(strlst_t::const_iterator iter = allfiles.begin(); iter != allfiles.end(); ++iter){
					if(value == (*iter)){
						found_same_file = true;
						break;
					}
				}
				if(found_same_file){
					WAN_CHMPRN("%s keyword in %s(%d) is filepath(%s) which already read!", INICFG_INCLUDE_STR, filename.c_str(), lineno, value.c_str());
				}else{
					// reentrant
					allfiles.push_back(value);
					if(!ReadFileContents(value, linelst, allfiles)){
						ERR_CHMPRN("Failed to load include file(%s)", value.c_str());
						cfgstream.close();
						return false;
					}
				}
				continue;
			}
		}
		// add
		linelst.push_back(line);
	}
	cfgstream.close();

	return true;
}

bool CHMIniConf::LoadConfigrationRaw(CFGRAW& chmcfgraw) const
{
	if(0 == cfgfile.length()){
		ERR_CHMPRN("Configuration file path is not set.");
		return false;
	}

	// Load all file contents(with include file)
	strlst_t	linelst;
	strlst_t	allfiles;
	allfiles.push_back(cfgfile);
	if(!ReadFileContents(cfgfile, linelst, allfiles)){
		ERR_CHMPRN("Could not load configration file(%s) contents.", cfgfile.c_str());
		return false;
	}

	string		line;
	int			section = INICFG_INSEC_NOT;
	int			next_section;
	strmap_t	tmpmap;
	for(strlst_t::const_iterator iter = linelst.begin(); iter != linelst.end(); ++iter){
		line = trim((*iter));
		if(0 == line.length()){
			continue;
		}

		// check section keywords
		if(INICFG_COMMENT_CHAR == line[0]){
			continue;
		}else if(line == INICFG_GLOBAL_SEC_STR){
			next_section = INICFG_INSEC_GLOBAL;
		}else if(line == INICFG_SVRNODE_SEC_STR){
			next_section = INICFG_INSEC_SVRNODE;
		}else if(line == INICFG_SLVNODE_SEC_STR){
			next_section = INICFG_INSEC_SLVNODE;
		}else if(INICFG_SEC_START_CHAR == line[0] && INICFG_SEC_END_CHAR == line[line.length() - 1]){
			next_section = INICFG_INSEC_UNKNOWN;
		}else{
			next_section = INICFG_INSEC_NOT;
		}

		if(INICFG_INSEC_NOT == next_section && INICFG_INSEC_NOT == section){
			// found not section keywords, but now out of section.
			WAN_CHMPRN("cfg file(%s) invalide value(%s), no section.", cfgfile.c_str(), line.c_str());
			continue;
		}

		if(INICFG_INSEC_NOT != next_section){
			// start new section, closing before section
			if(INICFG_INSEC_GLOBAL == section){
				merge_strmap(chmcfgraw.global, tmpmap);
			}else if(INICFG_INSEC_SVRNODE == section){
				if(!sorted_insert_strmaparr(chmcfgraw.server_nodes, tmpmap, INICFG_NAME_STR, INICFG_CTLPORT_STR)){
					WAN_CHMPRN("cfg file(%s) invalide server node data, probably %s is not specified.", cfgfile.c_str(), INICFG_NAME_STR);
				}
			}else if(INICFG_INSEC_SLVNODE == section){
				if(!sorted_insert_strmaparr(chmcfgraw.slave_nodes, tmpmap, INICFG_NAME_STR, INICFG_CTLPORT_STR)){
					WAN_CHMPRN("cfg file(%s) invalide slave node data, probably %s is not specified.", cfgfile.c_str(), INICFG_NAME_STR);
				}
			}else{	// INICFG_INSEC_UNKNOWN == section
				// Unknown section, nothing to do.
			}
			section = next_section;
			tmpmap.clear();

		}else if(INICFG_INSEC_UNKNOWN != section){
			// not section keywords, continue in before section.
			string				value("");
			string::size_type	pos;
			if(string::npos != (pos = line.find(INICFG_KV_SEP))){
				value	= trim(line.substr(pos + 1));
				line	= trim(line.substr(0, pos));
			}
			if(!extract_conf_value(value) || !extract_conf_value(line)){
				WAN_CHMPRN("cfg file(%s) invalide data, could not extract key or value.", cfgfile.c_str());
			}else{
				if(0 == line.length()){
					WAN_CHMPRN("cfg file(%s) invalide data, key is not found.", cfgfile.c_str());
				}else{
					if(tmpmap.end() != tmpmap.find(line)){
						WAN_CHMPRN("cfg file(%s) invalide data, key(%s: value=%s) is already specified, and overwrited.", cfgfile.c_str(), line.c_str(), value.c_str());
					}
					tmpmap[line] = value;
				}
			}
		}
	}

	if(0 < tmpmap.size()){
		if(INICFG_INSEC_GLOBAL == section){
			merge_strmap(chmcfgraw.global, tmpmap);
		}else if(INICFG_INSEC_SVRNODE == section){
			if(!sorted_insert_strmaparr(chmcfgraw.server_nodes, tmpmap, INICFG_NAME_STR, INICFG_CTLPORT_STR)){
				WAN_CHMPRN("cfg file(%s) invalide server node data, probably %s is not specified.", cfgfile.c_str(), INICFG_NAME_STR);
			}
		}else if(INICFG_INSEC_SLVNODE == section){
			if(!sorted_insert_strmaparr(chmcfgraw.slave_nodes, tmpmap, INICFG_NAME_STR, INICFG_CTLPORT_STR)){
				WAN_CHMPRN("cfg file(%s) invalide slave node data, probably %s is not specified.", cfgfile.c_str(), INICFG_NAME_STR);
			}
		}else{	// INICFG_INSEC_NOT == section
			// before section is nothing(first section), nothing to do.
		}
	}
	return true;
}

bool CHMIniConf::LoadConfigration(CHMCFGINFO& chmcfginfo) const
{
	if(0 == cfgfile.length()){
		ERR_CHMPRN("Configuration file path is not set.");
		return false;
	}

	CFGRAW	chmcfgraw;
	if(!LoadConfigrationRaw(chmcfgraw)){
		ERR_CHMPRN("Failed to read configuration file(%s).", cfgfile.c_str());
		return false;
	}

	// Common values
	CHMCONF_CCV	ccvals;
	if(chmcfgraw.global.end() == chmcfgraw.global.find(INICFG_GROUP_STR)){
		ERR_CHMPRN("configuration file(%s) does not have \"%s\" in %s section.", cfgfile.c_str(), INICFG_GROUP_STR, INICFG_GLOBAL_SEC_STR);
		return false;
	}
	chmcfginfo.groupname = chmcfgraw.global[INICFG_GROUP_STR];

	if(chmcfgraw.global.end() == chmcfgraw.global.find(INICFG_VERSION_STR)){
		ERR_CHMPRN("configuration file(%s) does not have \"%s\" in %s section.", cfgfile.c_str(), INICFG_VERSION_STR, INICFG_GLOBAL_SEC_STR);
		return false;
	}
	chmcfginfo.revision = static_cast<long>(atoi(chmcfgraw.global[INICFG_VERSION_STR].c_str()));

	if(chmcfgraw.global.end() == chmcfgraw.global.find(INICFG_MODE_STR)){
		WAN_CHMPRN("configuration file(%s) does not have \"%s\" in %s section, but seaching automatically by contrl port.", cfgfile.c_str(), INICFG_MODE_STR, INICFG_GLOBAL_SEC_STR);
	}else{
		if(0 == strcasecmp(chmcfgraw.global[INICFG_MODE_STR].c_str(), INICFG_MODE_SERVER_STR)){
			chmcfginfo.is_server_mode = true;
		}else if(0 == strcasecmp(chmcfgraw.global[INICFG_MODE_STR].c_str(), INICFG_MODE_SLAVE_STR)){
			chmcfginfo.is_server_mode = false;
		}else{
			ERR_CHMPRN("configuration file(%s) have \"%s\" in %s section, but value %s does not defined.", cfgfile.c_str(), INICFG_MODE_STR, INICFG_GLOBAL_SEC_STR, chmcfgraw.global[INICFG_MODE_STR].c_str());
			return false;
		}
		ccvals.server_mode = true;
	}

	if(chmcfgraw.global.end() == chmcfgraw.global.find(INICFG_DELIVERMODE_STR)){
		ERR_CHMPRN("configuration file(%s) does not have \"%s\" in %s section.", cfgfile.c_str(), INICFG_DELIVERMODE_STR, INICFG_GLOBAL_SEC_STR);
		return false;
	}else{
		if(0 == strcasecmp(chmcfgraw.global[INICFG_DELIVERMODE_STR].c_str(), INICFG_DELIVERMODE_RANDOM_STR)){
			chmcfginfo.is_random_mode = true;
		}else if(0 == strcasecmp(chmcfgraw.global[INICFG_DELIVERMODE_STR].c_str(), INICFG_DELIVERMODE_HASH_STR)){
			chmcfginfo.is_random_mode = false;
		}else{
			ERR_CHMPRN("configuration file(%s) have \"%s\" in %s section, but value %s does not defined.", cfgfile.c_str(), INICFG_DELIVERMODE_STR, INICFG_GLOBAL_SEC_STR, chmcfgraw.global[INICFG_DELIVERMODE_STR].c_str());
			return false;
		}
	}

	if(chmcfgraw.global.end() == chmcfgraw.global.find(INICFG_MAXCHMPX_STR)){
		MSG_CHMPRN("configuration file(%s) does not have \"%s\" in %s section.", cfgfile.c_str(), INICFG_MAXCHMPX_STR, INICFG_GLOBAL_SEC_STR);
		chmcfginfo.max_chmpx_count = DEFAULT_CHMPX_COUNT;
	}else{
		chmcfginfo.max_chmpx_count = static_cast<long>(atoi(chmcfgraw.global[INICFG_MAXCHMPX_STR].c_str()));
		if(MAX_CHMPX_COUNT < chmcfginfo.max_chmpx_count){
			WAN_CHMPRN("\"%s\" value(%ld) is over upper limit(%d), so set upper limit.", INICFG_MAXCHMPX_STR, chmcfginfo.max_chmpx_count, MAX_CHMPX_COUNT);
			chmcfginfo.max_chmpx_count = MAX_CHMPX_COUNT;
		}
	}

	if(chmcfgraw.global.end() == chmcfgraw.global.find(INICFG_REPLICA_STR)){
		MSG_CHMPRN("configuration file(%s) does not have \"%s\" in %s section.", cfgfile.c_str(), INICFG_REPLICA_STR, INICFG_GLOBAL_SEC_STR);
		chmcfginfo.replica_count = DEFAULT_REPLICA_COUNT;
	}else{
		chmcfginfo.replica_count = static_cast<long>(atoi(chmcfgraw.global[INICFG_REPLICA_STR].c_str()));
		if(chmcfginfo.is_random_mode){
			if(DEFAULT_REPLICA_COUNT != chmcfginfo.replica_count){
				WAN_CHMPRN("\"%s\" value(%ld) is not set, because random mode does not do replication. This value should be %d on random mode.", INICFG_REPLICA_STR, chmcfginfo.replica_count, DEFAULT_REPLICA_COUNT);
				chmcfginfo.replica_count = DEFAULT_REPLICA_COUNT;
			}
		}else{
			if(chmcfginfo.max_chmpx_count < chmcfginfo.replica_count){
				WAN_CHMPRN("\"%s\" value(%ld) is over maximum chmpx count(%ld), so set value to maximum chmpx count.", INICFG_REPLICA_STR, chmcfginfo.replica_count, chmcfginfo.max_chmpx_count);
				chmcfginfo.replica_count = chmcfginfo.max_chmpx_count;
			}
		}
	}

	if(chmcfgraw.global.end() == chmcfgraw.global.find(INICFG_MAXMQSERVER_STR)){
		MSG_CHMPRN("configuration file(%s) does not have \"%s\" in %s section.", cfgfile.c_str(), INICFG_MAXMQSERVER_STR, INICFG_GLOBAL_SEC_STR);
		chmcfginfo.max_server_mq_cnt = DEFAULT_SERVER_MQ_CNT;
	}else{
		chmcfginfo.max_server_mq_cnt = static_cast<long>(atoi(chmcfgraw.global[INICFG_MAXMQSERVER_STR].c_str()));
		if(MAX_SERVER_MQ_CNT < chmcfginfo.max_server_mq_cnt){
			WAN_CHMPRN("\"%s\" value(%ld) is over upper limit(%d), so set upper limit.", INICFG_MAXMQSERVER_STR, chmcfginfo.max_server_mq_cnt, MAX_SERVER_MQ_CNT);
			chmcfginfo.max_server_mq_cnt = MAX_SERVER_MQ_CNT;
		}
	}

	if(chmcfgraw.global.end() == chmcfgraw.global.find(INICFG_MAXMQCLIENT_STR)){
		MSG_CHMPRN("configuration file(%s) does not have \"%s\" in %s section.", cfgfile.c_str(), INICFG_MAXMQCLIENT_STR, INICFG_GLOBAL_SEC_STR);
		chmcfginfo.max_client_mq_cnt = DEFAULT_CLIENT_MQ_CNT;
	}else{
		chmcfginfo.max_client_mq_cnt = static_cast<long>(atoi(chmcfgraw.global[INICFG_MAXMQCLIENT_STR].c_str()));
		if(MAX_CLIENT_MQ_CNT < chmcfginfo.max_client_mq_cnt){
			WAN_CHMPRN("\"%s\" value(%ld) is over upper limit(%d), so set upper limit.", INICFG_MAXMQCLIENT_STR, chmcfginfo.max_client_mq_cnt, MAX_CLIENT_MQ_CNT);
			chmcfginfo.max_client_mq_cnt = MAX_CLIENT_MQ_CNT;
		}
	}

	if(chmcfgraw.global.end() == chmcfgraw.global.find(INICFG_MQPERATTACH_STR)){
		MSG_CHMPRN("configuration file(%s) does not have \"%s\" in %s section.", cfgfile.c_str(), INICFG_MQPERATTACH_STR, INICFG_GLOBAL_SEC_STR);
		chmcfginfo.mqcnt_per_attach = DEFAULT_MQ_PER_ATTACH;
	}else{
		chmcfginfo.mqcnt_per_attach = static_cast<long>(atoi(chmcfgraw.global[INICFG_MQPERATTACH_STR].c_str()));
		if(MAX_MQ_PER_ATTACH < chmcfginfo.mqcnt_per_attach){
			WAN_CHMPRN("\"%s\" value(%ld) is over upper limit(%d), so set upper limit.", INICFG_MQPERATTACH_STR, chmcfginfo.mqcnt_per_attach, MAX_MQ_PER_ATTACH);
			chmcfginfo.mqcnt_per_attach = MAX_MQ_PER_ATTACH;
		}
	}

	if(chmcfgraw.global.end() == chmcfgraw.global.find(INICFG_MAXQPERSERVERMQ_STR)){
		MSG_CHMPRN("configuration file(%s) does not have \"%s\" in %s section.", cfgfile.c_str(), INICFG_MAXQPERSERVERMQ_STR, INICFG_GLOBAL_SEC_STR);
		chmcfginfo.max_q_per_servermq = DEFAULT_QUEUE_PER_SERVERMQ;
	}else{
		chmcfginfo.max_q_per_servermq = static_cast<long>(atoi(chmcfgraw.global[INICFG_MAXQPERSERVERMQ_STR].c_str()));
		if(MAX_QUEUE_PER_SERVERMQ < chmcfginfo.max_q_per_servermq){
			WAN_CHMPRN("\"%s\" value(%ld) is over upper limit(%d), so set upper limit.", INICFG_MAXQPERSERVERMQ_STR, chmcfginfo.max_q_per_servermq, MAX_QUEUE_PER_SERVERMQ);
			chmcfginfo.max_q_per_servermq = MAX_QUEUE_PER_SERVERMQ;
		}
	}

	if(chmcfgraw.global.end() == chmcfgraw.global.find(INICFG_MAXQPERCLIENTMQ_STR)){
		MSG_CHMPRN("configuration file(%s) does not have \"%s\" in %s section.", cfgfile.c_str(), INICFG_MAXQPERCLIENTMQ_STR, INICFG_GLOBAL_SEC_STR);
		chmcfginfo.max_q_per_clientmq = DEFAULT_QUEUE_PER_CLIENTMQ;
	}else{
		chmcfginfo.max_q_per_clientmq = static_cast<long>(atoi(chmcfgraw.global[INICFG_MAXQPERCLIENTMQ_STR].c_str()));
		if(MAX_QUEUE_PER_CLIENTMQ < chmcfginfo.max_q_per_clientmq){
			WAN_CHMPRN("\"%s\" value(%ld) is over upper limit(%d), so set upper limit.", INICFG_MAXQPERCLIENTMQ_STR, chmcfginfo.max_q_per_clientmq, MAX_QUEUE_PER_CLIENTMQ);
			chmcfginfo.max_q_per_clientmq = MAX_QUEUE_PER_CLIENTMQ;
		}
	}

	if(chmcfgraw.global.end() == chmcfgraw.global.find(INICFG_MAXMQPERCLIENT_STR)){
		MSG_CHMPRN("configuration file(%s) does not have \"%s\" in %s section.", cfgfile.c_str(), INICFG_MAXMQPERCLIENT_STR, INICFG_GLOBAL_SEC_STR);
		chmcfginfo.max_mq_per_client = DEFAULT_MQ_PER_CLIENT;
	}else{
		chmcfginfo.max_mq_per_client = static_cast<long>(atoi(chmcfgraw.global[INICFG_MAXMQPERCLIENT_STR].c_str()));
		if(MAX_MQ_PER_CLIENT < chmcfginfo.max_mq_per_client){
			WAN_CHMPRN("\"%s\" value(%ld) is over upper limit(%d), so set upper limit.", INICFG_MAXMQPERCLIENT_STR, chmcfginfo.max_mq_per_client, MAX_MQ_PER_CLIENT);
			chmcfginfo.max_mq_per_client = MAX_MQ_PER_CLIENT;
		}
	}

	if(chmcfgraw.global.end() == chmcfgraw.global.find(INICFG_MAXHISTLOG_STR)){
		MSG_CHMPRN("configuration file(%s) does not have \"%s\" in %s section.", cfgfile.c_str(), INICFG_MAXHISTLOG_STR, INICFG_GLOBAL_SEC_STR);
		chmcfginfo.max_histlog_count = DEFAULT_HISTLOG_COUNT;
	}else{
		chmcfginfo.max_histlog_count = static_cast<long>(atoi(chmcfgraw.global[INICFG_MAXHISTLOG_STR].c_str()));
		if(MAX_HISTLOG_COUNT < chmcfginfo.max_histlog_count){
			WAN_CHMPRN("\"%s\" value(%ld) is over upper limit(%d), so set upper limit.", INICFG_MAXHISTLOG_STR, chmcfginfo.max_histlog_count, MAX_HISTLOG_COUNT);
			chmcfginfo.max_histlog_count = MAX_HISTLOG_COUNT;
		}
	}

	if(chmcfgraw.global.end() == chmcfgraw.global.find(INICFG_DATE_STR)){
		MSG_CHMPRN("configuration file(%s) does not have \"%s\" in %s section.", cfgfile.c_str(), INICFG_DATE_STR, INICFG_GLOBAL_SEC_STR);
		chmcfginfo.date = 0L;
	}else{
		chmcfginfo.date = rfcdate_time(chmcfgraw.global[INICFG_DATE_STR].c_str());
	}

	if(chmcfgraw.global.end() == chmcfgraw.global.find(INICFG_PORT_STR)){
		MSG_CHMPRN("configuration file(%s) does not have \"%s\" in %s section.", cfgfile.c_str(), INICFG_PORT_STR, INICFG_GLOBAL_SEC_STR);
	}else{
		ccvals.port = static_cast<short>(atoi(chmcfgraw.global[INICFG_PORT_STR].c_str()));
	}

	if(chmcfgraw.global.end() == chmcfgraw.global.find(INICFG_CTLPORT_STR)){
		MSG_CHMPRN("configuration file(%s) does not have \"%s\" in %s section.", cfgfile.c_str(), INICFG_CTLPORT_STR, INICFG_GLOBAL_SEC_STR);
	}else{
		ccvals.ctlport = static_cast<short>(atoi(chmcfgraw.global[INICFG_CTLPORT_STR].c_str()));
	}

	if(CHM_INVALID_PORT == ctlport_param){
		if(chmcfgraw.global.end() == chmcfgraw.global.find(INICFG_SELFCTLPORT_STR)){
			MSG_CHMPRN("configuration file(%s) does not have \"%s\" in %s section.", cfgfile.c_str(), INICFG_SELFCTLPORT_STR, INICFG_GLOBAL_SEC_STR);
			chmcfginfo.self_ctlport = CHM_INVALID_PORT;
		}else{
			chmcfginfo.self_ctlport = static_cast<short>(atoi(chmcfgraw.global[INICFG_SELFCTLPORT_STR].c_str()));
		}
	}else{
		chmcfginfo.self_ctlport = ctlport_param;
	}

	if(chmcfgraw.global.end() == chmcfgraw.global.find(INICFG_RETRYCNT_STR)){
		MSG_CHMPRN("configuration file(%s) does not have \"%s\" in %s section.", cfgfile.c_str(), INICFG_RETRYCNT_STR, INICFG_GLOBAL_SEC_STR);
		chmcfginfo.retrycnt = CHMEVENTSOCK_RETRY_DEFAULT;
	}else{
		chmcfginfo.retrycnt = atoi(chmcfgraw.global[INICFG_RETRYCNT_STR].c_str());
	}

	if(chmcfgraw.global.end() == chmcfgraw.global.find(INICFG_MQRETRYCNT_STR)){
		MSG_CHMPRN("configuration file(%s) does not have \"%s\" in %s section.", cfgfile.c_str(), INICFG_MQRETRYCNT_STR, INICFG_GLOBAL_SEC_STR);
		chmcfginfo.mq_retrycnt = ChmEventMq::DEFAULT_RETRYCOUNT;
	}else{
		chmcfginfo.mq_retrycnt = atoi(chmcfgraw.global[INICFG_MQRETRYCNT_STR].c_str());
	}

	if(chmcfgraw.global.end() == chmcfgraw.global.find(INICFG_MQACK_STR)){
		MSG_CHMPRN("configuration file(%s) does not have \"%s\" in %s section.", cfgfile.c_str(), INICFG_MQACK_STR, INICFG_GLOBAL_SEC_STR);
		chmcfginfo.mq_ack = true;
	}else{
		if(0 == strcasecmp(chmcfgraw.global[INICFG_MQACK_STR].c_str(), INICFG_BOOL_ON) || 0 == strcasecmp(chmcfgraw.global[INICFG_MQACK_STR].c_str(), INICFG_BOOL_YES)){
			chmcfginfo.mq_ack = true;
		}else if(0 == strcasecmp(chmcfgraw.global[INICFG_MQACK_STR].c_str(), INICFG_BOOL_OFF) || 0 == strcasecmp(chmcfgraw.global[INICFG_MQACK_STR].c_str(), INICFG_BOOL_NO)){
			chmcfginfo.mq_ack = false;
		}else{
			ERR_CHMPRN("configuration file(%s) have wrong \"%s\" value(%s) in %s section.", cfgfile.c_str(), INICFG_MQACK_STR, chmcfgraw.global[INICFG_MQACK_STR].c_str(), INICFG_GLOBAL_SEC_STR);
			return false;
		}
	}

	if(chmcfgraw.global.end() == chmcfgraw.global.find(INICFG_RWTIMEOUT_STR)){
		MSG_CHMPRN("configuration file(%s) does not have \"%s\" in %s section.", cfgfile.c_str(), INICFG_RWTIMEOUT_STR, INICFG_GLOBAL_SEC_STR);
		chmcfginfo.timeout_wait_socket = CHMEVENTSOCK_TIMEOUT_DEFAULT;
	}else{
		chmcfginfo.timeout_wait_socket = atoi(chmcfgraw.global[INICFG_RWTIMEOUT_STR].c_str());
		if(chmcfginfo.timeout_wait_socket < CHMEVENTSOCK_TIMEOUT_DEFAULT){
			chmcfginfo.timeout_wait_socket = CHMEVENTSOCK_TIMEOUT_DEFAULT;
		}
	}

	if(chmcfgraw.global.end() == chmcfgraw.global.find(INICFG_CONTIMEOUT_STR)){
		MSG_CHMPRN("configuration file(%s) does not have \"%s\" in %s section.", cfgfile.c_str(), INICFG_CONTIMEOUT_STR, INICFG_GLOBAL_SEC_STR);
		chmcfginfo.timeout_wait_connect = CHMEVENTSOCK_TIMEOUT_DEFAULT;
	}else{
		chmcfginfo.timeout_wait_connect = atoi(chmcfgraw.global[INICFG_CONTIMEOUT_STR].c_str());
		if(chmcfginfo.timeout_wait_connect < CHMEVENTSOCK_TIMEOUT_DEFAULT){
			chmcfginfo.timeout_wait_connect = CHMEVENTSOCK_TIMEOUT_DEFAULT;
		}
	}

	if(chmcfgraw.global.end() == chmcfgraw.global.find(INICFG_MQRWTIMEOUT_STR)){
		MSG_CHMPRN("configuration file(%s) does not have \"%s\" in %s section.", cfgfile.c_str(), INICFG_MQRWTIMEOUT_STR, INICFG_GLOBAL_SEC_STR);
		chmcfginfo.timeout_wait_mq = CHMEVENTMQ_TIMEOUT_DEFAULT;
	}else{
		chmcfginfo.timeout_wait_mq = atoi(chmcfgraw.global[INICFG_MQRWTIMEOUT_STR].c_str());
		if(chmcfginfo.timeout_wait_mq < CHMEVENTMQ_TIMEOUT_DEFAULT){
			chmcfginfo.timeout_wait_mq = CHMEVENTMQ_TIMEOUT_DEFAULT;
		}
	}

	if(chmcfgraw.global.end() == chmcfgraw.global.find(INICFG_AUTOMERGE_STR)){
		MSG_CHMPRN("configuration file(%s) does not have \"%s\" in %s section.", cfgfile.c_str(), INICFG_AUTOMERGE_STR, INICFG_GLOBAL_SEC_STR);
		chmcfginfo.is_auto_merge = false;
	}else{
		if(0 == strcasecmp(chmcfgraw.global[INICFG_AUTOMERGE_STR].c_str(), INICFG_BOOL_ON) || 0 == strcasecmp(chmcfgraw.global[INICFG_AUTOMERGE_STR].c_str(), INICFG_BOOL_YES)){
			chmcfginfo.is_auto_merge = true;
		}else if(0 == strcasecmp(chmcfgraw.global[INICFG_AUTOMERGE_STR].c_str(), INICFG_BOOL_OFF) || 0 == strcasecmp(chmcfgraw.global[INICFG_AUTOMERGE_STR].c_str(), INICFG_BOOL_NO)){
			chmcfginfo.is_auto_merge = false;
		}else{
			ERR_CHMPRN("configuration file(%s) have wrong \"%s\" value(%s) in %s section.", cfgfile.c_str(), INICFG_AUTOMERGE_STR, chmcfgraw.global[INICFG_AUTOMERGE_STR].c_str(), INICFG_GLOBAL_SEC_STR);
			return false;
		}
	}

	if(chmcfgraw.global.end() == chmcfgraw.global.find(INICFG_DOMERGE_STR)){
		MSG_CHMPRN("configuration file(%s) does not have \"%s\" in %s section.", cfgfile.c_str(), INICFG_DOMERGE_STR, INICFG_GLOBAL_SEC_STR);
		chmcfginfo.is_do_merge = false;
	}else{
		if(0 == strcasecmp(chmcfgraw.global[INICFG_DOMERGE_STR].c_str(), INICFG_BOOL_ON) || 0 == strcasecmp(chmcfgraw.global[INICFG_DOMERGE_STR].c_str(), INICFG_BOOL_YES)){
			chmcfginfo.is_do_merge = true;
		}else if(0 == strcasecmp(chmcfgraw.global[INICFG_DOMERGE_STR].c_str(), INICFG_BOOL_OFF) || 0 == strcasecmp(chmcfgraw.global[INICFG_DOMERGE_STR].c_str(), INICFG_BOOL_NO)){
			chmcfginfo.is_do_merge = false;
		}else{
			ERR_CHMPRN("configuration file(%s) have wrong \"%s\" value(%s) in %s section.", cfgfile.c_str(), INICFG_DOMERGE_STR, chmcfgraw.global[INICFG_DOMERGE_STR].c_str(), INICFG_GLOBAL_SEC_STR);
			return false;
		}
	}

	if(chmcfgraw.global.end() == chmcfgraw.global.find(INICFG_MERGETIMEOUT_STR)){
		MSG_CHMPRN("configuration file(%s) does not have \"%s\" in %s section.", cfgfile.c_str(), INICFG_MERGETIMEOUT_STR, INICFG_GLOBAL_SEC_STR);
		chmcfginfo.timeout_merge = CHMEVENTMQ_TIMEOUT_DEFAULT;
	}else{
		chmcfginfo.timeout_merge = static_cast<time_t>(atoi(chmcfgraw.global[INICFG_MERGETIMEOUT_STR].c_str()));
		if(chmcfginfo.timeout_merge < CHMEVENTMQ_TIMEOUT_DEFAULT){
			chmcfginfo.timeout_merge = CHMEVENTMQ_TIMEOUT_DEFAULT;
		}
	}

	if(chmcfgraw.global.end() == chmcfgraw.global.find(INICFG_SOCKTHREADCNT_STR)){
		MSG_CHMPRN("configuration file(%s) does not have \"%s\" in %s section.", cfgfile.c_str(), INICFG_SOCKTHREADCNT_STR, INICFG_GLOBAL_SEC_STR);
		chmcfginfo.sock_thread_cnt = ChmEventSock::DEFAULT_SOCK_THREAD_CNT;
	}else{
		chmcfginfo.sock_thread_cnt = atoi(chmcfgraw.global[INICFG_SOCKTHREADCNT_STR].c_str());
		if(chmcfginfo.sock_thread_cnt < ChmEventSock::DEFAULT_SOCK_THREAD_CNT){
			ERR_CHMPRN("configuration file(%s) have wrong \"%s\" value(%s) in %s section.", cfgfile.c_str(), INICFG_SOCKTHREADCNT_STR, chmcfgraw.global[INICFG_SOCKTHREADCNT_STR].c_str(), INICFG_GLOBAL_SEC_STR);
			return false;
		}
	}

	if(chmcfgraw.global.end() == chmcfgraw.global.find(INICFG_MQTHREADCNT_STR)){
		MSG_CHMPRN("configuration file(%s) does not have \"%s\" in %s section.", cfgfile.c_str(), INICFG_MQTHREADCNT_STR, INICFG_GLOBAL_SEC_STR);
		chmcfginfo.mq_thread_cnt = ChmEventMq::DEFAULT_MQ_THREAD_CNT;
	}else{
		chmcfginfo.mq_thread_cnt = atoi(chmcfgraw.global[INICFG_MQTHREADCNT_STR].c_str());
		if(chmcfginfo.mq_thread_cnt < ChmEventMq::DEFAULT_MQ_THREAD_CNT){
			ERR_CHMPRN("configuration file(%s) have wrong \"%s\" value(%s) in %s section.", cfgfile.c_str(), INICFG_MQTHREADCNT_STR, chmcfgraw.global[INICFG_MQTHREADCNT_STR].c_str(), INICFG_GLOBAL_SEC_STR);
			return false;
		}
	}

	if(chmcfgraw.global.end() == chmcfgraw.global.find(INICFG_MAXSOCKPOOL_STR)){
		MSG_CHMPRN("configuration file(%s) does not have \"%s\" in %s section.", cfgfile.c_str(), INICFG_MAXSOCKPOOL_STR, INICFG_GLOBAL_SEC_STR);
		chmcfginfo.max_sock_pool = ChmEventSock::DEFAULT_MAX_SOCK_POOL;
	}else{
		chmcfginfo.max_sock_pool = static_cast<time_t>(atoi(chmcfgraw.global[INICFG_MAXSOCKPOOL_STR].c_str()));
		if(chmcfginfo.max_sock_pool < ChmEventSock::DEFAULT_MAX_SOCK_POOL){
			ERR_CHMPRN("configuration file(%s) have wrong \"%s\" value(%s) in %s section.", cfgfile.c_str(), INICFG_MAXSOCKPOOL_STR, chmcfgraw.global[INICFG_MAXSOCKPOOL_STR].c_str(), INICFG_GLOBAL_SEC_STR);
			return false;
		}
	}

	if(chmcfgraw.global.end() == chmcfgraw.global.find(INICFG_SOCKPOOLTIMEOUT_STR)){
		MSG_CHMPRN("configuration file(%s) does not have \"%s\" in %s section.", cfgfile.c_str(), INICFG_SOCKPOOLTIMEOUT_STR, INICFG_GLOBAL_SEC_STR);
		chmcfginfo.sock_pool_timeout = ChmEventSock::DEFAULT_SOCK_POOL_TIMEOUT;
	}else{
		chmcfginfo.sock_pool_timeout = atoi(chmcfgraw.global[INICFG_SOCKPOOLTIMEOUT_STR].c_str());
		if(chmcfginfo.sock_pool_timeout < ChmEventSock::NO_SOCK_POOL_TIMEOUT){
			ERR_CHMPRN("configuration file(%s) have wrong \"%s\" value(%s) in %s section.", cfgfile.c_str(), INICFG_SOCKPOOLTIMEOUT_STR, chmcfgraw.global[INICFG_SOCKPOOLTIMEOUT_STR].c_str(), INICFG_GLOBAL_SEC_STR);
			return false;
		}
	}

	// SSL
	if(chmcfgraw.global.end() == chmcfgraw.global.find(INICFG_SSL_STR)){
		MSG_CHMPRN("configuration file(%s) does not have \"%s\" in %s section.", cfgfile.c_str(), INICFG_SSL_STR, INICFG_GLOBAL_SEC_STR);
	}else{
		if(0 == strcasecmp(chmcfgraw.global[INICFG_SSL_STR].c_str(), INICFG_BOOL_ON) || 0 == strcasecmp(chmcfgraw.global[INICFG_SSL_STR].c_str(), INICFG_BOOL_YES)){
			ccvals.is_ssl = true;
		}else if(0 == strcasecmp(chmcfgraw.global[INICFG_SSL_STR].c_str(), INICFG_BOOL_OFF) || 0 == strcasecmp(chmcfgraw.global[INICFG_SSL_STR].c_str(), INICFG_BOOL_NO)){
			ccvals.is_ssl = false;
		}else{
			ERR_CHMPRN("configuration file(%s) have wrong \"%s\" value(%s) in %s section.", cfgfile.c_str(), INICFG_SSL_STR, chmcfgraw.global[INICFG_SSL_STR].c_str(), INICFG_GLOBAL_SEC_STR);
			return false;
		}
	}

	// SSL_VERIFY_PEER
	if(chmcfgraw.global.end() == chmcfgraw.global.find(INICFG_SSL_VERIFY_PEER_STR)){
		MSG_CHMPRN("configuration file(%s) does not have \"%s\" in %s section.", cfgfile.c_str(), INICFG_SSL_VERIFY_PEER_STR, INICFG_GLOBAL_SEC_STR);
	}else{
		if(0 == strcasecmp(chmcfgraw.global[INICFG_SSL_VERIFY_PEER_STR].c_str(), INICFG_BOOL_ON) || 0 == strcasecmp(chmcfgraw.global[INICFG_SSL_VERIFY_PEER_STR].c_str(), INICFG_BOOL_YES)){
			if(!ccvals.is_ssl){
				ERR_CHMPRN("configuration file(%s) have \"%s\" value(%s) in %s section, but \"%s\" is OFF.", cfgfile.c_str(), INICFG_SSL_VERIFY_PEER_STR, chmcfgraw.global[INICFG_SSL_VERIFY_PEER_STR].c_str(), INICFG_GLOBAL_SEC_STR, INICFG_SSL_STR);
				return false;
			}
			ccvals.verify_peer = true;
		}else if(0 == strcasecmp(chmcfgraw.global[INICFG_SSL_VERIFY_PEER_STR].c_str(), INICFG_BOOL_OFF) || 0 == strcasecmp(chmcfgraw.global[INICFG_SSL_VERIFY_PEER_STR].c_str(), INICFG_BOOL_NO)){
			ccvals.verify_peer = false;
		}else{
			ERR_CHMPRN("configuration file(%s) have wrong \"%s\" value(%s) in %s section.", cfgfile.c_str(), INICFG_SSL_VERIFY_PEER_STR, chmcfgraw.global[INICFG_SSL_VERIFY_PEER_STR].c_str(), INICFG_GLOBAL_SEC_STR);
			return false;
		}
	}

	// SSL_CAPATH
	if(chmcfgraw.global.end() == chmcfgraw.global.find(INICFG_CAPATH_STR) || 0 == strcasecmp(chmcfgraw.global[INICFG_CAPATH_STR].c_str(), INICFG_STRING_NULL)){
		MSG_CHMPRN("configuration file(%s) does not have \"%s\" in %s section.", cfgfile.c_str(), INICFG_CAPATH_STR, INICFG_GLOBAL_SEC_STR);
	}else{
		if(is_dir_exist(chmcfgraw.global[INICFG_CAPATH_STR].c_str())){
			ccvals.is_ca_file = false;
		}else{
			if(is_file_safe_exist(chmcfgraw.global[INICFG_CAPATH_STR].c_str())){
				ccvals.is_ca_file = true;
			}else{
				ERR_CHMPRN("configuration file(%s) have \"%s\" value(%s) in %s section, but it is not directory or file.", cfgfile.c_str(), INICFG_CAPATH_STR, chmcfgraw.global[INICFG_CAPATH_STR].c_str(), INICFG_GLOBAL_SEC_STR);
				return false;
			}
		}
		if(CHM_MAX_PATH_LEN <= chmcfgraw.global[INICFG_CAPATH_STR].length()){
			ERR_CHMPRN("configuration file(%s) have \"%s\" value(%s) in %s section, but it is length(%zd) is over max length(1024).", cfgfile.c_str(), INICFG_CAPATH_STR, chmcfgraw.global[INICFG_CAPATH_STR].c_str(), INICFG_GLOBAL_SEC_STR, chmcfgraw.global[INICFG_CAPATH_STR].length());
			return false;
		}
		ccvals.capath = chmcfgraw.global[INICFG_CAPATH_STR];
	}

	// SSL_SERVER_CERT
	if(chmcfgraw.global.end() == chmcfgraw.global.find(INICFG_SERVER_CERT_STR) || 0 == strcasecmp(chmcfgraw.global[INICFG_SERVER_CERT_STR].c_str(), INICFG_STRING_NULL)){
		MSG_CHMPRN("configuration file(%s) does not have \"%s\" in %s section.", cfgfile.c_str(), INICFG_SERVER_CERT_STR, INICFG_GLOBAL_SEC_STR);
	}else{
		if(!is_file_safe_exist(chmcfgraw.global[INICFG_SERVER_CERT_STR].c_str())){
			ERR_CHMPRN("configuration file(%s) have \"%s\" value(%s) in %s section, but it is not safe file.", cfgfile.c_str(), INICFG_SERVER_CERT_STR, chmcfgraw.global[INICFG_SERVER_CERT_STR].c_str(), INICFG_GLOBAL_SEC_STR);
			return false;
		}
		if(!ccvals.is_ssl){
			ERR_CHMPRN("configuration file(%s) have \"%s\" value(%s) in %s section, but \"%s\" is OFF.", cfgfile.c_str(), INICFG_SERVER_CERT_STR, chmcfgraw.global[INICFG_SERVER_CERT_STR].c_str(), INICFG_GLOBAL_SEC_STR, INICFG_SSL_STR);
			return false;
		}
		if(CHM_MAX_PATH_LEN <= chmcfgraw.global[INICFG_SERVER_CERT_STR].length()){
			ERR_CHMPRN("configuration file(%s) have \"%s\" value(%s) in %s section, but it is length(%zd) is over max length(1024).", cfgfile.c_str(), INICFG_SERVER_CERT_STR, chmcfgraw.global[INICFG_SERVER_CERT_STR].c_str(), INICFG_GLOBAL_SEC_STR, chmcfgraw.global[INICFG_SERVER_CERT_STR].length());
			return false;
		}
		ccvals.server_cert = chmcfgraw.global[INICFG_SERVER_CERT_STR];
	}

	// SSL_SERVER_PRIKEY
	if(chmcfgraw.global.end() == chmcfgraw.global.find(INICFG_SERVER_PRIKEY_STR) || 0 == strcasecmp(chmcfgraw.global[INICFG_SERVER_PRIKEY_STR].c_str(), INICFG_STRING_NULL)){
		MSG_CHMPRN("configuration file(%s) does not have \"%s\" in %s section.", cfgfile.c_str(), INICFG_SERVER_PRIKEY_STR, INICFG_GLOBAL_SEC_STR);
	}else{
		if(!is_file_safe_exist(chmcfgraw.global[INICFG_SERVER_PRIKEY_STR].c_str())){
			ERR_CHMPRN("configuration file(%s) have \"%s\" value(%s) in %s section, but it is not safe file.", cfgfile.c_str(), INICFG_SERVER_PRIKEY_STR, chmcfgraw.global[INICFG_SERVER_PRIKEY_STR].c_str(), INICFG_GLOBAL_SEC_STR);
			return false;
		}
		if(!ccvals.is_ssl){
			ERR_CHMPRN("configuration file(%s) have \"%s\" value(%s) in %s section, but \"%s\" is OFF.", cfgfile.c_str(), INICFG_SERVER_PRIKEY_STR, chmcfgraw.global[INICFG_SERVER_PRIKEY_STR].c_str(), INICFG_GLOBAL_SEC_STR, INICFG_SSL_STR);
			return false;
		}
		if(CHM_MAX_PATH_LEN <= chmcfgraw.global[INICFG_SERVER_PRIKEY_STR].length()){
			ERR_CHMPRN("configuration file(%s) have \"%s\" value(%s) in %s section, but it is length(%zd) is over max length(1024).", cfgfile.c_str(), INICFG_SERVER_PRIKEY_STR, chmcfgraw.global[INICFG_SERVER_PRIKEY_STR].c_str(), INICFG_GLOBAL_SEC_STR, chmcfgraw.global[INICFG_SERVER_PRIKEY_STR].length());
			return false;
		}
		ccvals.server_prikey = chmcfgraw.global[INICFG_SERVER_PRIKEY_STR];
	}

	// SSL_SLAVE_CERT
	if(chmcfgraw.global.end() == chmcfgraw.global.find(INICFG_SLAVE_CERT_STR) || 0 == strcasecmp(chmcfgraw.global[INICFG_SLAVE_CERT_STR].c_str(), INICFG_STRING_NULL)){
		MSG_CHMPRN("configuration file(%s) does not have \"%s\" in %s section.", cfgfile.c_str(), INICFG_SLAVE_CERT_STR, INICFG_GLOBAL_SEC_STR);
	}else{
		if(!is_file_safe_exist(chmcfgraw.global[INICFG_SLAVE_CERT_STR].c_str())){
			ERR_CHMPRN("configuration file(%s) have \"%s\" value(%s) in %s section, but it is not safe file.", cfgfile.c_str(), INICFG_SLAVE_CERT_STR, chmcfgraw.global[INICFG_SLAVE_CERT_STR].c_str(), INICFG_GLOBAL_SEC_STR);
			return false;
		}
		if(CHM_MAX_PATH_LEN <= chmcfgraw.global[INICFG_SLAVE_CERT_STR].length()){
			ERR_CHMPRN("configuration file(%s) have \"%s\" value(%s) in %s section, but it is length(%zd) is over max length(1024).", cfgfile.c_str(), INICFG_SLAVE_CERT_STR, chmcfgraw.global[INICFG_SLAVE_CERT_STR].c_str(), INICFG_GLOBAL_SEC_STR, chmcfgraw.global[INICFG_SLAVE_CERT_STR].length());
			return false;
		}
		ccvals.slave_cert = chmcfgraw.global[INICFG_SLAVE_CERT_STR];
	}

	// SSL_SLAVE_PRIKEY
	if(chmcfgraw.global.end() == chmcfgraw.global.find(INICFG_SLAVE_PRIKEY_STR) || 0 == strcasecmp(chmcfgraw.global[INICFG_SLAVE_PRIKEY_STR].c_str(), INICFG_STRING_NULL)){
		MSG_CHMPRN("configuration file(%s) does not have \"%s\" in %s section.", cfgfile.c_str(), INICFG_SLAVE_PRIKEY_STR, INICFG_GLOBAL_SEC_STR);
	}else{
		if(!is_file_safe_exist(chmcfgraw.global[INICFG_SLAVE_PRIKEY_STR].c_str())){
			ERR_CHMPRN("configuration file(%s) have \"%s\" value(%s) in %s section, but it is not safe file.", cfgfile.c_str(), INICFG_SLAVE_PRIKEY_STR, chmcfgraw.global[INICFG_SLAVE_PRIKEY_STR].c_str(), INICFG_GLOBAL_SEC_STR);
			return false;
		}
		if(CHM_MAX_PATH_LEN <= chmcfgraw.global[INICFG_SLAVE_PRIKEY_STR].length()){
			ERR_CHMPRN("configuration file(%s) have \"%s\" value(%s) in %s section, but it is length(%zd) is over max length(1024).", cfgfile.c_str(), INICFG_SLAVE_PRIKEY_STR, chmcfgraw.global[INICFG_SLAVE_PRIKEY_STR].c_str(), INICFG_GLOBAL_SEC_STR, chmcfgraw.global[INICFG_SLAVE_PRIKEY_STR].length());
			return false;
		}
		ccvals.slave_prikey = chmcfgraw.global[INICFG_SLAVE_PRIKEY_STR];
	}

	if(chmcfgraw.global.end() == chmcfgraw.global.find(INICFG_K2HFULLMAP_STR)){
		MSG_CHMPRN("configuration file(%s) does not have \"%s\" in %s section.", cfgfile.c_str(), INICFG_K2HFULLMAP_STR, INICFG_GLOBAL_SEC_STR);
		chmcfginfo.k2h_fullmap = true;
	}else{
		if(0 == strcasecmp(chmcfgraw.global[INICFG_K2HFULLMAP_STR].c_str(), INICFG_BOOL_ON) || 0 == strcasecmp(chmcfgraw.global[INICFG_K2HFULLMAP_STR].c_str(), INICFG_BOOL_YES)){
			chmcfginfo.k2h_fullmap = true;
		}else if(0 == strcasecmp(chmcfgraw.global[INICFG_K2HFULLMAP_STR].c_str(), INICFG_BOOL_OFF) || 0 == strcasecmp(chmcfgraw.global[INICFG_K2HFULLMAP_STR].c_str(), INICFG_BOOL_NO)){
			chmcfginfo.k2h_fullmap = false;
		}else{
			ERR_CHMPRN("configuration file(%s) have wrong \"%s\" value(%s) in %s section.", cfgfile.c_str(), INICFG_K2HFULLMAP_STR, chmcfgraw.global[INICFG_K2HFULLMAP_STR].c_str(), INICFG_GLOBAL_SEC_STR);
			return false;
		}
	}

	if(chmcfgraw.global.end() == chmcfgraw.global.find(INICFG_K2HMASKBIT_STR)){
		MSG_CHMPRN("configuration file(%s) does not have \"%s\" in %s section.", cfgfile.c_str(), INICFG_K2HMASKBIT_STR, INICFG_GLOBAL_SEC_STR);
		chmcfginfo.k2h_mask_bitcnt = K2HShm::DEFAULT_MASK_BITCOUNT;
	}else{
		chmcfginfo.k2h_mask_bitcnt = atoi(chmcfgraw.global[INICFG_K2HMASKBIT_STR].c_str());
	}

	if(chmcfgraw.global.end() == chmcfgraw.global.find(INICFG_K2HCMASKBIT_STR)){
		MSG_CHMPRN("configuration file(%s) does not have \"%s\" in %s section.", cfgfile.c_str(), INICFG_K2HCMASKBIT_STR, INICFG_GLOBAL_SEC_STR);
		chmcfginfo.k2h_cmask_bitcnt = K2HShm::DEFAULT_COLLISION_MASK_BITCOUNT;
	}else{
		chmcfginfo.k2h_cmask_bitcnt = atoi(chmcfgraw.global[INICFG_K2HCMASKBIT_STR].c_str());
	}

	if(chmcfgraw.global.end() == chmcfgraw.global.find(INICFG_K2HMAXELE_STR)){
		MSG_CHMPRN("configuration file(%s) does not have \"%s\" in %s section.", cfgfile.c_str(), INICFG_K2HMAXELE_STR, INICFG_GLOBAL_SEC_STR);
		chmcfginfo.k2h_max_element = K2HShm::DEFAULT_MAX_ELEMENT_CNT;
	}else{
		chmcfginfo.k2h_max_element = atoi(chmcfgraw.global[INICFG_K2HMAXELE_STR].c_str());
	}

	// server node section
	if(0 == chmcfgraw.server_nodes.size()){
		ERR_CHMPRN("configuration file(%s) does not have %s section.", cfgfile.c_str(), INICFG_SVRNODE_SEC_STR);
		return false;
	}

	string	localhost_hostname;
	ChmNetDb::GetLocalHostname(localhost_hostname);
	for(strmaparr_t::iterator iter = chmcfgraw.server_nodes.begin(); iter != chmcfgraw.server_nodes.end(); ++iter){
		CHMNODE_CFGINFO	svrnode;

		if(iter->end() == iter->find(INICFG_NAME_STR)){
			MSG_CHMPRN("configuration file(%s) does not have \"%s\" in %s section.", cfgfile.c_str(), INICFG_NAME_STR, INICFG_SVRNODE_SEC_STR);
			return false;
		}else{
			svrnode.name = (*iter)[INICFG_NAME_STR];
		}

		if(iter->end() == iter->find(INICFG_PORT_STR)){
			if(CHM_INVALID_PORT == ccvals.port){
				MSG_CHMPRN("configuration file(%s) does not have \"%s\" in %s server node in %s section.", cfgfile.c_str(), INICFG_PORT_STR, svrnode.name.c_str(), INICFG_SVRNODE_SEC_STR);
				return false;
			}
			svrnode.port = ccvals.port;
		}else{
			svrnode.port = static_cast<short>(atoi((*iter)[INICFG_PORT_STR].c_str()));
		}

		if(iter->end() == iter->find(INICFG_CTLPORT_STR)){
			if(CHM_INVALID_PORT == ccvals.ctlport){
				MSG_CHMPRN("configuration file(%s) does not have \"%s\" in %s server node in %s section.", cfgfile.c_str(), INICFG_CTLPORT_STR, svrnode.name.c_str(), INICFG_SVRNODE_SEC_STR);
				return false;
			}
			svrnode.ctlport = ccvals.ctlport;
		}else{
			svrnode.ctlport = static_cast<short>(atoi((*iter)[INICFG_CTLPORT_STR].c_str()));
		}

		// SSL
		//
		if(iter->end() == iter->find(INICFG_SSL_STR)){
			svrnode.is_ssl = ccvals.is_ssl;
		}else{
			if(0 == strcasecmp((*iter)[INICFG_SSL_STR].c_str(), INICFG_BOOL_ON) || 0 == strcasecmp((*iter)[INICFG_SSL_STR].c_str(), INICFG_BOOL_YES)){
				svrnode.is_ssl = true;
			}else if(0 == strcasecmp((*iter)[INICFG_SSL_STR].c_str(), INICFG_BOOL_OFF) || 0 == strcasecmp((*iter)[INICFG_SSL_STR].c_str(), INICFG_BOOL_NO)){
				svrnode.is_ssl = false;
			}else{
				ERR_CHMPRN("configuration file(%s) have wrong \"%s\" value(%s) in %s server node in %s section.", cfgfile.c_str(), INICFG_SSL_STR, (*iter)[INICFG_SSL_STR].c_str(), svrnode.name.c_str(), INICFG_SVRNODE_SEC_STR);
				return false;
			}
		}
		if(svrnode.is_ssl){
			ccvals.found_ssl = true;				// Keep SSL flag for checking after this loop
		}

		// SSL_VERIFY_PEER
		//
		if(iter->end() == iter->find(INICFG_SSL_VERIFY_PEER_STR)){
			svrnode.verify_peer = ccvals.verify_peer;
		}else{
			if(0 == strcasecmp((*iter)[INICFG_SSL_VERIFY_PEER_STR].c_str(), INICFG_BOOL_ON) || 0 == strcasecmp((*iter)[INICFG_SSL_VERIFY_PEER_STR].c_str(), INICFG_BOOL_YES)){
				svrnode.verify_peer = true;
			}else if(0 == strcasecmp((*iter)[INICFG_SSL_VERIFY_PEER_STR].c_str(), INICFG_BOOL_OFF) || 0 == strcasecmp((*iter)[INICFG_SSL_VERIFY_PEER_STR].c_str(), INICFG_BOOL_NO)){
				svrnode.verify_peer = false;
			}else{
				ERR_CHMPRN("configuration file(%s) have wrong \"%s\" value(%s) in %s server node in %s section.", cfgfile.c_str(), INICFG_SSL_VERIFY_PEER_STR, (*iter)[INICFG_SSL_VERIFY_PEER_STR].c_str(), svrnode.name.c_str(), INICFG_SVRNODE_SEC_STR);
				return false;
			}
		}
		if(!svrnode.is_ssl && svrnode.verify_peer){
			ERR_CHMPRN("configuration file(%s) have \"%s\" value(%s) in %s server node in %s section, but \"%s\" is OFF.", cfgfile.c_str(), INICFG_SSL_VERIFY_PEER_STR, (*iter)[INICFG_SSL_VERIFY_PEER_STR].c_str(), svrnode.name.c_str(), INICFG_SVRNODE_SEC_STR, INICFG_SSL_STR);
			return false;
		}
		if(svrnode.verify_peer){
			ccvals.found_ssl_verify_peer = true;	// Keep verify peer flag for checking after this loop
		}

		// SSL_CAPATH
		//
		// [NOTE] : This value is checked after this loop.
		//
		if(iter->end() == iter->find(INICFG_CAPATH_STR)){
			svrnode.is_ca_file	= ccvals.is_ca_file;
			svrnode.capath		= ccvals.capath;
		}else{
			if(0 == strcasecmp((*iter)[INICFG_CAPATH_STR].c_str(), INICFG_STRING_NULL)){
				svrnode.is_ca_file	= false;
				svrnode.capath		= "";
			}else{
				if(is_dir_exist((*iter)[INICFG_CAPATH_STR].c_str())){
					svrnode.is_ca_file = false;
				}else{
					if(is_file_safe_exist((*iter)[INICFG_CAPATH_STR].c_str())){
						svrnode.is_ca_file = true;
					}else{
						ERR_CHMPRN("configuration file(%s) have \"%s\" value(%s) in %s server node in %s section, but it is not directory or file.", cfgfile.c_str(), INICFG_CAPATH_STR, (*iter)[INICFG_CAPATH_STR].c_str(), svrnode.name.c_str(), INICFG_SVRNODE_SEC_STR);
						return false;
					}
				}
				if(CHM_MAX_PATH_LEN <= (*iter)[INICFG_CAPATH_STR].length()){
					ERR_CHMPRN("configuration file(%s) have \"%s\" value(%s) in %s section, but it is length(%zd) is over max length(1024).", cfgfile.c_str(), INICFG_CAPATH_STR, (*iter)[INICFG_CAPATH_STR].c_str(), INICFG_GLOBAL_SEC_STR, (*iter)[INICFG_CAPATH_STR].length());
					return false;
				}
				svrnode.capath = (*iter)[INICFG_CAPATH_STR].c_str();
			}
		}

		// SSL_SERVER_CERT, SSL_SERVER_PRIKEY
		//
		if(iter->end() == iter->find(INICFG_SERVER_CERT_STR)){
			svrnode.server_cert = ccvals.server_cert;
		}else{
			if(0 == strcasecmp((*iter)[INICFG_SERVER_CERT_STR].c_str(), INICFG_STRING_NULL)){
				svrnode.server_cert = "";
			}else{
				if(!is_file_safe_exist((*iter)[INICFG_SERVER_CERT_STR].c_str())){
					ERR_CHMPRN("configuration file(%s) have \"%s\" value(%s) in %s server node in %s section, but it is not safe file.", cfgfile.c_str(), INICFG_SERVER_CERT_STR, (*iter)[INICFG_SERVER_CERT_STR].c_str(), svrnode.name.c_str(), INICFG_SVRNODE_SEC_STR);
					return false;
				}
				if(CHM_MAX_PATH_LEN <= (*iter)[INICFG_SERVER_CERT_STR].length()){
					ERR_CHMPRN("configuration file(%s) have \"%s\" value(%s) in %s section, but it is length(%zd) is over max length(1024).", cfgfile.c_str(), INICFG_SERVER_CERT_STR, (*iter)[INICFG_SERVER_CERT_STR].c_str(), INICFG_GLOBAL_SEC_STR, (*iter)[INICFG_SERVER_CERT_STR].length());
					return false;
				}
				svrnode.server_cert = (*iter)[INICFG_SERVER_CERT_STR].c_str();
			}
		}
		if(svrnode.is_ssl == svrnode.server_cert.empty()){
			ERR_CHMPRN("configuration file(%s) is \"%s\"=value(%s) in %s server node in %s section, but \"%s\" is %s.", cfgfile.c_str(), INICFG_SERVER_CERT_STR, svrnode.server_cert.c_str(), svrnode.name.c_str(), INICFG_SVRNODE_SEC_STR, INICFG_SSL_STR, (svrnode.is_ssl ? "ON" : "OFF"));
			return false;
		}
		if(iter->end() == iter->find(INICFG_SERVER_PRIKEY_STR)){
			svrnode.server_prikey = ccvals.server_prikey;
		}else{
			if(0 == strcasecmp((*iter)[INICFG_SERVER_PRIKEY_STR].c_str(), INICFG_STRING_NULL)){
				svrnode.server_prikey = "";
			}else{
				if(!is_file_safe_exist((*iter)[INICFG_SERVER_PRIKEY_STR].c_str())){
					ERR_CHMPRN("configuration file(%s) have \"%s\" value(%s) in %s server node in %s section, but it is not safe file.", cfgfile.c_str(), INICFG_SERVER_PRIKEY_STR, (*iter)[INICFG_SERVER_PRIKEY_STR].c_str(), svrnode.name.c_str(), INICFG_SVRNODE_SEC_STR);
					return false;
				}
				if(CHM_MAX_PATH_LEN <= (*iter)[INICFG_SERVER_PRIKEY_STR].length()){
					ERR_CHMPRN("configuration file(%s) have \"%s\" value(%s) in %s section, but it is length(%zd) is over max length(1024).", cfgfile.c_str(), INICFG_SERVER_PRIKEY_STR, (*iter)[INICFG_SERVER_PRIKEY_STR].c_str(), INICFG_GLOBAL_SEC_STR, (*iter)[INICFG_SERVER_PRIKEY_STR].length());
					return false;
				}
				svrnode.server_prikey = (*iter)[INICFG_SERVER_PRIKEY_STR].c_str();
			}
		}
		if(svrnode.is_ssl == svrnode.server_prikey.empty()){
			ERR_CHMPRN("configuration file(%s) is \"%s\"=value(%s) in %s server node in %s section, but \"%s\" is %s.", cfgfile.c_str(), INICFG_SERVER_PRIKEY_STR, svrnode.server_prikey.c_str(), svrnode.name.c_str(), INICFG_SVRNODE_SEC_STR, INICFG_SSL_STR, (svrnode.is_ssl ? "ON" : "OFF"));
			return false;
		}

		// SSL_SLAVE_CERT, SSL_SLAVE_PRIKEY
		//
		// [NOTE] : This value is checked after this loop.
		//
		if(iter->end() == iter->find(INICFG_SLAVE_CERT_STR)){
			svrnode.slave_cert = ccvals.slave_cert;
		}else{
			if(0 == strcasecmp((*iter)[INICFG_SLAVE_CERT_STR].c_str(), INICFG_STRING_NULL)){
				svrnode.slave_cert = "";
			}else{
				if(!is_file_safe_exist((*iter)[INICFG_SLAVE_CERT_STR].c_str())){
					ERR_CHMPRN("configuration file(%s) have \"%s\" value(%s) in %s server node in %s section, but it is not safe file.", cfgfile.c_str(), INICFG_SLAVE_CERT_STR, (*iter)[INICFG_SLAVE_CERT_STR].c_str(), svrnode.name.c_str(), INICFG_SVRNODE_SEC_STR);
					return false;
				}
				if(CHM_MAX_PATH_LEN <= (*iter)[INICFG_SLAVE_CERT_STR].length()){
					ERR_CHMPRN("configuration file(%s) have \"%s\" value(%s) in %s section, but it is length(%zd) is over max length(1024).", cfgfile.c_str(), INICFG_SLAVE_CERT_STR, (*iter)[INICFG_SLAVE_CERT_STR].c_str(), INICFG_GLOBAL_SEC_STR, (*iter)[INICFG_SLAVE_CERT_STR].length());
					return false;
				}
				svrnode.slave_cert = (*iter)[INICFG_SLAVE_CERT_STR].c_str();
			}
		}

		if(iter->end() == iter->find(INICFG_SLAVE_PRIKEY_STR)){
			svrnode.slave_prikey = ccvals.slave_prikey;
		}else{
			if(0 == strcasecmp((*iter)[INICFG_SLAVE_PRIKEY_STR].c_str(), INICFG_STRING_NULL)){
				svrnode.slave_prikey = "";
			}else{
				if(!is_file_safe_exist((*iter)[INICFG_SLAVE_PRIKEY_STR].c_str())){
					ERR_CHMPRN("configuration file(%s) have \"%s\" value(%s) in %s server node in %s section, but it is not safe file.", cfgfile.c_str(), INICFG_SLAVE_PRIKEY_STR, (*iter)[INICFG_SLAVE_PRIKEY_STR].c_str(), svrnode.name.c_str(), INICFG_SVRNODE_SEC_STR);
					return false;
				}
				if(CHM_MAX_PATH_LEN <= (*iter)[INICFG_SLAVE_PRIKEY_STR].length()){
					ERR_CHMPRN("configuration file(%s) have \"%s\" value(%s) in %s section, but it is length(%zd) is over max length(1024).", cfgfile.c_str(), INICFG_SLAVE_PRIKEY_STR, (*iter)[INICFG_SLAVE_PRIKEY_STR].c_str(), INICFG_GLOBAL_SEC_STR, (*iter)[INICFG_SLAVE_PRIKEY_STR].length());
					return false;
				}
				svrnode.slave_prikey = (*iter)[INICFG_SLAVE_PRIKEY_STR].c_str();
			}
		}
		if(svrnode.slave_cert.empty() != svrnode.slave_prikey.empty()){
			// SSL_SLAVE_CERT, SSL_SLAVE_PRIKEY must be set or not set.
			ERR_CHMPRN("configuration file(%s) have \"%s\"=value(%s) and \"%s\"=value(%s) in %s server node in %s section.", cfgfile.c_str(), INICFG_SLAVE_CERT_STR, svrnode.slave_cert.c_str(), INICFG_SLAVE_PRIKEY_STR, svrnode.slave_prikey.c_str(), svrnode.name.c_str(), INICFG_SVRNODE_SEC_STR);
			return false;
		}

		// Expand name(simple regex)
		// Need to expand for server node, because node name is compared directly and is used by connecting.
		// So that, we expand server node name here.
		//
		strlst_t	expand_svrnodes;
		expand_svrnodes.clear();
		if(!ExpandSimpleRegxHostname(svrnode.name.c_str(), expand_svrnodes, true, true, false)){	// convert localhost to server name and query FQDN.
			MSG_CHMPRN("Failed to expand server node name(%s).", svrnode.name.c_str());
			return false;
		}

		// Add each expanded server node name.
		for(strlst_t::const_iterator svrnodeiter = expand_svrnodes.begin(); svrnodeiter != expand_svrnodes.end(); ++svrnodeiter){
			svrnode.name = (*svrnodeiter);
			chmcfginfo.servers.push_back(svrnode);

			// whichever server mode or not?
			if(!ccvals.is_server_by_ctlport && chmcfginfo.self_ctlport == svrnode.ctlport && 0 == strcasecmp(svrnode.name.c_str(), localhost_hostname.c_str())){
				ccvals.is_server_by_ctlport = true;
			}
		}
	}

	// Re-Check SSL_CAPATH, SSL_SLAVE_CERT, SSL_SLAVE_PRIKEY
	//
	for(chmnode_cfginfos_t::iterator iter = chmcfginfo.servers.begin(); iter != chmcfginfo.servers.end(); ++iter){
		if(!ccvals.found_ssl){
			// If there is no ssl server, all servers should not have CApath.
			if(!iter->capath.empty()){
				ERR_CHMPRN("configuration file(%s) have \"%s\"=value(%s) in %s server node in %s section.", cfgfile.c_str(), INICFG_CAPATH_STR, iter->capath.c_str(), iter->name.c_str(), INICFG_SVRNODE_SEC_STR);
				return false;
			}
			// If there is no ssl servers and no verify peer, any servers should not have client cert and private key.
			if(!iter->slave_cert.empty() || !iter->slave_prikey.empty()){
				ERR_CHMPRN("configuration file(%s) have \"%s\"=value(%s) and \"%s\"=value(%s) in %s server node in %s section.", cfgfile.c_str(), INICFG_SLAVE_CERT_STR, iter->slave_cert.c_str(), INICFG_SLAVE_PRIKEY_STR, iter->slave_prikey.c_str(), iter->name.c_str(), INICFG_SVRNODE_SEC_STR);
				return false;
			}
		}else{
			if(ccvals.found_ssl_verify_peer){
				// If there are ssl servers with verify peer, all servers must have client cert and private key.
				if(iter->slave_cert.empty() || iter->slave_prikey.empty()){
					ERR_CHMPRN("configuration file(%s) have \"%s\"=value(%s) and \"%s\"=value(%s) in %s server node in %s section.", cfgfile.c_str(), INICFG_SLAVE_CERT_STR, iter->slave_cert.c_str(), INICFG_SLAVE_PRIKEY_STR, iter->slave_prikey.c_str(), iter->name.c_str(), INICFG_SVRNODE_SEC_STR);
					return false;
				}
			}else{
				// If there are ssl servers without verify peer, any servers should not have client cert and private key.
				if(!iter->slave_cert.empty() || !iter->slave_prikey.empty()){
					ERR_CHMPRN("configuration file(%s) have \"%s\"=value(%s) and \"%s\"=value(%s) in %s server node in %s section.", cfgfile.c_str(), INICFG_SLAVE_CERT_STR, iter->slave_cert.c_str(), INICFG_SLAVE_PRIKEY_STR, iter->slave_prikey.c_str(), iter->name.c_str(), INICFG_SVRNODE_SEC_STR);
					return false;
				}

			}
		}
	}
	// [NOTE]
	// The server list might have duplicate server name & port.
	// Because the port number can not be specified in configration file, so if there is some server nodes on same server
	// and one specifies port and the other does not specify port(using default port).
	// On this case, the list have duplicate server.
	//
	chmcfginfo.servers.unique(chm_node_cfg_info_same_name_port());		// uniq about server node must be name and port
	chmcfginfo.servers.sort(chm_node_cfg_info_sort());

	// slave node section
	if(0 == chmcfgraw.slave_nodes.size()){
		ERR_CHMPRN("configuration file(%s) does not have %s section.", cfgfile.c_str(), INICFG_SLVNODE_SEC_STR);
		return false;
	}
	for(strmaparr_t::iterator iter = chmcfgraw.slave_nodes.begin(); iter != chmcfgraw.slave_nodes.end(); ++iter){
		CHMNODE_CFGINFO	slvnode;

		if(iter->end() == iter->find(INICFG_NAME_STR)){
			MSG_CHMPRN("configuration file(%s) does not have \"%s\" in %s section.", cfgfile.c_str(), INICFG_NAME_STR, INICFG_SLVNODE_SEC_STR);
			return false;
		}else{
			// slave server name is allowed regexed format.
			// so do not convert name to global name(can not convert regexed name).
			//
			slvnode.name = (*iter)[INICFG_NAME_STR];
		}
		if(iter->end() == iter->find(INICFG_CTLPORT_STR)){
			if(CHM_INVALID_PORT == ccvals.ctlport){
				MSG_CHMPRN("configuration file(%s) does not have \"%s\" in %s slave node in %s section.", cfgfile.c_str(), INICFG_CTLPORT_STR, slvnode.name.c_str(), INICFG_SLVNODE_SEC_STR);
				return false;
			}
			slvnode.ctlport = ccvals.ctlport;
		}else{
			slvnode.ctlport = static_cast<short>(atoi((*iter)[INICFG_CTLPORT_STR].c_str()));
		}
		slvnode.port		= CHM_INVALID_PORT;
		slvnode.is_ssl		= false;
		slvnode.verify_peer	= false;

		// SSL_CAPATH
		//
		if(iter->end() == iter->find(INICFG_CAPATH_STR)){
			slvnode.is_ca_file	= ccvals.is_ca_file;
			slvnode.capath		= ccvals.capath;
		}else{
			if(0 == strcasecmp((*iter)[INICFG_CAPATH_STR].c_str(), INICFG_STRING_NULL)){
				slvnode.is_ca_file	= false;
				slvnode.capath		= "";
			}else{
				if(is_dir_exist((*iter)[INICFG_CAPATH_STR].c_str())){
					slvnode.is_ca_file = false;
				}else{
					if(is_file_safe_exist((*iter)[INICFG_CAPATH_STR].c_str())){
						slvnode.is_ca_file = true;
					}else{
						ERR_CHMPRN("configuration file(%s) have \"%s\" value(%s) in %s server node in %s section, but it is not directory or file.", cfgfile.c_str(), INICFG_CAPATH_STR, (*iter)[INICFG_CAPATH_STR].c_str(), slvnode.name.c_str(), INICFG_SLVNODE_SEC_STR);
						return false;
					}
				}
				if(CHM_MAX_PATH_LEN <= (*iter)[INICFG_CAPATH_STR].length()){
					ERR_CHMPRN("configuration file(%s) have \"%s\" value(%s) in %s section, but it is length(%zd) is over max length(1024).", cfgfile.c_str(), INICFG_CAPATH_STR, (*iter)[INICFG_CAPATH_STR].c_str(), INICFG_SLVNODE_SEC_STR, (*iter)[INICFG_CAPATH_STR].length());
					return false;
				}
				slvnode.capath = (*iter)[INICFG_CAPATH_STR].c_str();
			}
		}
		if(!ccvals.found_ssl && !slvnode.capath.empty()){
			// There is not SSL, but CApath is set.
			ERR_CHMPRN("configuration file(%s) have \"%s\"=value(%s) in %s server node in %s section.", cfgfile.c_str(), INICFG_CAPATH_STR, slvnode.capath.c_str(), slvnode.name.c_str(), INICFG_SLVNODE_SEC_STR);
			return false;
		}

		// SSL_SLAVE_CERT, SSL_SLAVE_PRIKEY
		//
		if(iter->end() == iter->find(INICFG_SLAVE_CERT_STR)){
			slvnode.slave_cert = ccvals.slave_cert;
		}else{
			if(0 == strcasecmp((*iter)[INICFG_SLAVE_CERT_STR].c_str(), INICFG_STRING_NULL)){
				slvnode.slave_cert = "";
			}else{
				if(!is_file_safe_exist((*iter)[INICFG_SLAVE_CERT_STR].c_str())){
					ERR_CHMPRN("configuration file(%s) have \"%s\" value(%s) in %s server node in %s section, but it is not safe file.", cfgfile.c_str(), INICFG_SLAVE_CERT_STR, (*iter)[INICFG_SLAVE_CERT_STR].c_str(), slvnode.name.c_str(), INICFG_SLVNODE_SEC_STR);
					return false;
				}
				if(CHM_MAX_PATH_LEN <= (*iter)[INICFG_SLAVE_CERT_STR].length()){
					ERR_CHMPRN("configuration file(%s) have \"%s\" value(%s) in %s section, but it is length(%zd) is over max length(1024).", cfgfile.c_str(), INICFG_SLAVE_CERT_STR, (*iter)[INICFG_SLAVE_CERT_STR].c_str(), INICFG_SLVNODE_SEC_STR, (*iter)[INICFG_SLAVE_CERT_STR].length());
					return false;
				}
				slvnode.slave_cert = (*iter)[INICFG_SLAVE_CERT_STR].c_str();
			}
		}
		if(iter->end() == iter->find(INICFG_SLAVE_PRIKEY_STR)){
			slvnode.slave_prikey = ccvals.slave_prikey;
		}else{
			if(0 == strcasecmp((*iter)[INICFG_SLAVE_PRIKEY_STR].c_str(), INICFG_STRING_NULL)){
				slvnode.slave_prikey = "";
			}else{
				if(!is_file_safe_exist((*iter)[INICFG_SLAVE_PRIKEY_STR].c_str())){
					ERR_CHMPRN("configuration file(%s) have \"%s\" value(%s) in %s server node in %s section, but it is not safe file.", cfgfile.c_str(), INICFG_SLAVE_PRIKEY_STR, (*iter)[INICFG_SLAVE_PRIKEY_STR].c_str(), slvnode.name.c_str(), INICFG_SLVNODE_SEC_STR);
					return false;
				}
				if(CHM_MAX_PATH_LEN <= (*iter)[INICFG_SLAVE_PRIKEY_STR].length()){
					ERR_CHMPRN("configuration file(%s) have \"%s\" value(%s) in %s section, but it is length(%zd) is over max length(1024).", cfgfile.c_str(), INICFG_SLAVE_PRIKEY_STR, (*iter)[INICFG_SLAVE_PRIKEY_STR].c_str(), INICFG_SLVNODE_SEC_STR, (*iter)[INICFG_SLAVE_PRIKEY_STR].length());
					return false;
				}
				slvnode.slave_prikey = (*iter)[INICFG_SLAVE_PRIKEY_STR].c_str();
			}
		}
		if(slvnode.slave_cert.empty() != slvnode.slave_prikey.empty()){
			// SSL_SLAVE_CERT, SSL_SLAVE_PRIKEY must be set or not set.
			ERR_CHMPRN("configuration file(%s) have \"%s\"=value(%s) and \"%s\"=value(%s) in %s server node in %s section.", cfgfile.c_str(), INICFG_SLAVE_CERT_STR, slvnode.slave_cert.c_str(), INICFG_SLAVE_PRIKEY_STR, slvnode.slave_prikey.c_str(), slvnode.name.c_str(), INICFG_SLVNODE_SEC_STR);
			return false;
		}
		if(ccvals.found_ssl_verify_peer == slvnode.slave_cert.empty()){
			// If There is SSL_VERIFY_PEER, but client cert(and private key) must be set.(nor so on)
			ERR_CHMPRN("configuration file(%s) have \"%s\"=value(%s) and \"%s\"=value(%s) in %s server node in %s section.", cfgfile.c_str(), INICFG_SLAVE_CERT_STR, slvnode.slave_cert.c_str(), INICFG_SLAVE_PRIKEY_STR, slvnode.slave_prikey.c_str(), slvnode.name.c_str(), INICFG_SLVNODE_SEC_STR);
			return false;
		}

		chmcfginfo.slaves.push_back(slvnode);
	}
	chmcfginfo.slaves.unique(chm_node_cfg_info_same_name_port());		// uniq about slave node must be name and port
	chmcfginfo.slaves.sort(chm_node_cfg_info_sort());

	// Re-check for server/slave mode by self control port number.
	//
	if(ccvals.is_server_by_ctlport){
		if(!ccvals.server_mode){
			WAN_CHMPRN("configuration file(%s) does not have \"%s\" in %s section, but self control port(%d) found in server list, so run server mode.", cfgfile.c_str(), INICFG_MODE_STR, INICFG_GLOBAL_SEC_STR, chmcfginfo.self_ctlport);
			chmcfginfo.is_server_mode = true;
		}else{
			if(!chmcfginfo.is_server_mode){
				ERR_CHMPRN("configuration file(%s) have \"%s\" as slave mode in %s section, but self control port(%d) found in server list.", cfgfile.c_str(), INICFG_MODE_STR, INICFG_GLOBAL_SEC_STR, chmcfginfo.self_ctlport);
				return false;
			}
		}
	}else{
		if(!ccvals.server_mode){
			WAN_CHMPRN("configuration file(%s) does not have \"%s\" in %s section, but self control port(%d) found in slave list, so run slave mode.", cfgfile.c_str(), INICFG_MODE_STR, INICFG_GLOBAL_SEC_STR, chmcfginfo.self_ctlport);
			chmcfginfo.is_server_mode = false;
		}else{
			if(chmcfginfo.is_server_mode){
				ERR_CHMPRN("configuration file(%s) have \"%s\" as server mode in %s section, but self control port(%d) not found in server list.", cfgfile.c_str(), INICFG_MODE_STR, INICFG_GLOBAL_SEC_STR, chmcfginfo.self_ctlport);
				return false;
			}
		}
	}

	// check merge flags
	if(chmcfginfo.is_random_mode){
		if(!chmcfginfo.is_auto_merge){
			WAN_CHMPRN("Specified %s=%s and %s=OFF. These options can not be specified at the same time, so SET %s=ON.", INICFG_DELIVERMODE_STR, INICFG_DELIVERMODE_RANDOM_STR, INICFG_AUTOMERGE_STR, INICFG_AUTOMERGE_STR);
			chmcfginfo.is_auto_merge = true;
		}
		if(chmcfginfo.is_do_merge){
			WAN_CHMPRN("Specified %s=%s and %s=ON. These options can not be specified at the same time, so SET %s=OFF.", INICFG_DELIVERMODE_STR, INICFG_DELIVERMODE_RANDOM_STR, INICFG_DOMERGE_STR, INICFG_DOMERGE_STR);
			chmcfginfo.is_do_merge = false;
		}
	}else{
		if(!chmcfginfo.is_do_merge && chmcfginfo.is_auto_merge){
			WAN_CHMPRN("Specified %s=OFF and %s=ON on %s=%s. These options can not be specified at the same time, so SET %s=OFF.", INICFG_DOMERGE_STR, INICFG_AUTOMERGE_STR, INICFG_DELIVERMODE_STR, INICFG_DELIVERMODE_HASH_STR, INICFG_AUTOMERGE_STR);
			chmcfginfo.is_auto_merge = false;
		}
	}

	return true;
}

//---------------------------------------------------------
// Loading Yaml Utilities for CHMYamlBaseConf Class
//---------------------------------------------------------
static bool ChmYamlLoadConfigrationGlobalSec(yaml_parser_t& yparser, CHMCFGINFO& chmcfginfo, CHMCONF_CCV& ccvals, short default_ctlport)
{
	// Must start yaml mapping event.
	yaml_event_t	yevent;
	if(!yaml_parser_parse(&yparser, &yevent)){
		ERR_CHMPRN("Could not parse event. errno = %d", errno);
		return false;
	}
	if(YAML_MAPPING_START_EVENT != yevent.type){
		ERR_CHMPRN("Parsed event type is not start mapping(%d)", yevent.type);
		yaml_event_delete(&yevent);
		return false;
	}
	yaml_event_delete(&yevent);

	// Set default value in GLOBAL section
	chmcfginfo.max_chmpx_count		= DEFAULT_CHMPX_COUNT;
	chmcfginfo.replica_count		= DEFAULT_REPLICA_COUNT;
	chmcfginfo.max_server_mq_cnt	= DEFAULT_SERVER_MQ_CNT;
	chmcfginfo.max_client_mq_cnt	= DEFAULT_CLIENT_MQ_CNT;
	chmcfginfo.mqcnt_per_attach		= DEFAULT_MQ_PER_ATTACH;
	chmcfginfo.max_q_per_servermq	= DEFAULT_QUEUE_PER_SERVERMQ;
	chmcfginfo.max_q_per_clientmq	= DEFAULT_QUEUE_PER_CLIENTMQ;
	chmcfginfo.max_mq_per_client	= DEFAULT_MQ_PER_CLIENT;
	chmcfginfo.max_histlog_count	= DEFAULT_HISTLOG_COUNT;
	chmcfginfo.date					= 0L;
	chmcfginfo.self_ctlport			= default_ctlport;
	chmcfginfo.retrycnt				= CHMEVENTSOCK_RETRY_DEFAULT;
	chmcfginfo.mq_retrycnt			= ChmEventMq::DEFAULT_RETRYCOUNT;
	chmcfginfo.mq_ack				= true;
	chmcfginfo.timeout_wait_socket	= CHMEVENTSOCK_TIMEOUT_DEFAULT;
	chmcfginfo.timeout_wait_connect	= CHMEVENTSOCK_TIMEOUT_DEFAULT;
	chmcfginfo.timeout_wait_mq		= CHMEVENTMQ_TIMEOUT_DEFAULT;
	chmcfginfo.is_auto_merge		= false;
	chmcfginfo.is_do_merge			= false;
	chmcfginfo.timeout_merge		= CHMEVENTMQ_TIMEOUT_DEFAULT;
	chmcfginfo.sock_thread_cnt		= ChmEventSock::DEFAULT_SOCK_THREAD_CNT;
	chmcfginfo.mq_thread_cnt		= ChmEventMq::DEFAULT_MQ_THREAD_CNT;
	chmcfginfo.max_sock_pool		= ChmEventSock::DEFAULT_MAX_SOCK_POOL;
	chmcfginfo.sock_pool_timeout	= ChmEventSock::DEFAULT_SOCK_POOL_TIMEOUT;
	chmcfginfo.k2h_fullmap			= true;
	chmcfginfo.k2h_mask_bitcnt		= K2HShm::DEFAULT_MASK_BITCOUNT;
	chmcfginfo.k2h_cmask_bitcnt		= K2HShm::DEFAULT_COLLISION_MASK_BITCOUNT;
	chmcfginfo.k2h_max_element		= K2HShm::DEFAULT_MAX_ELEMENT_CNT;

	// Clear default values
	ccvals.port						= CHM_INVALID_PORT;
	ccvals.ctlport					= CHM_INVALID_PORT;
	ccvals.server_mode				= false;
	ccvals.is_ssl					= false;
	ccvals.verify_peer				= false;
	ccvals.is_ca_file				= false;
	ccvals.capath					= "";
	ccvals.server_cert				= "";
	ccvals.server_prikey			= "";
	ccvals.slave_cert				= "";
	ccvals.slave_prikey				= "";

	// Loading
	string	key("");
	bool	result = true;
	for(bool is_loop = true; is_loop && result; ){
		// get event
		if(!yaml_parser_parse(&yparser, &yevent)){
			ERR_CHMPRN("Could not parse event. errno = %d", errno);
			result = false;
			continue;
		}

		// check event
		if(YAML_MAPPING_END_EVENT == yevent.type){
			// End of mapping event
			is_loop = false;

		}else if(YAML_SCALAR_EVENT == yevent.type){
			// Load key & value
			if(key.empty()){
				key = reinterpret_cast<const char*>(yevent.data.scalar.value);
			}else{
				//
				// Compare key and set value
				//
				if(0 == strcasecmp(INICFG_GROUP_STR, key.c_str())){
					chmcfginfo.groupname = reinterpret_cast<const char*>(yevent.data.scalar.value);

				}else if(0 == strcasecmp(INICFG_VERSION_STR, key.c_str())){
					chmcfginfo.revision = static_cast<long>(atoi(reinterpret_cast<const char*>(yevent.data.scalar.value)));

				}else if(0 == strcasecmp(INICFG_MODE_STR, key.c_str())){
					if(0 == strcasecmp(reinterpret_cast<const char*>(yevent.data.scalar.value), INICFG_MODE_SERVER_STR)){
						chmcfginfo.is_server_mode = true;
					}else if(0 == strcasecmp(reinterpret_cast<const char*>(yevent.data.scalar.value), INICFG_MODE_SLAVE_STR)){
						chmcfginfo.is_server_mode = false;
					}else{
						ERR_CHMPRN("Found %s in %s section, but value %s does not defined.", INICFG_MODE_STR, CFG_GLOBAL_SEC_STR, reinterpret_cast<const char*>(yevent.data.scalar.value));
						result = false;
					}
					ccvals.server_mode = true;

				}else if(0 == strcasecmp(INICFG_DELIVERMODE_STR, key.c_str())){
					if(0 == strcasecmp(reinterpret_cast<const char*>(yevent.data.scalar.value), INICFG_DELIVERMODE_RANDOM_STR)){
						chmcfginfo.is_random_mode = true;
					}else if(0 == strcasecmp(reinterpret_cast<const char*>(yevent.data.scalar.value), INICFG_DELIVERMODE_HASH_STR)){
						chmcfginfo.is_random_mode = false;
					}else{
						ERR_CHMPRN("Found %s in %s section, but value %s does not defined.", INICFG_DELIVERMODE_STR, CFG_GLOBAL_SEC_STR, reinterpret_cast<const char*>(yevent.data.scalar.value));
						result = false;
					}

				}else if(0 == strcasecmp(INICFG_MAXCHMPX_STR, key.c_str())){
					chmcfginfo.max_chmpx_count = static_cast<long>(atoi(reinterpret_cast<const char*>(yevent.data.scalar.value)));
					if(MAX_CHMPX_COUNT < chmcfginfo.max_chmpx_count){
						WAN_CHMPRN("\"%s\" value(%ld) is over upper limit(%d), so set upper limit.", INICFG_MAXCHMPX_STR, chmcfginfo.max_chmpx_count, MAX_CHMPX_COUNT);
						chmcfginfo.max_chmpx_count = MAX_CHMPX_COUNT;
					}

				}else if(0 == strcasecmp(INICFG_REPLICA_STR, key.c_str())){
					chmcfginfo.replica_count = static_cast<long>(atoi(reinterpret_cast<const char*>(yevent.data.scalar.value)));
					if(chmcfginfo.is_random_mode){
						if(DEFAULT_REPLICA_COUNT != chmcfginfo.replica_count){
							WAN_CHMPRN("\"%s\" value(%ld) is not set, because random mode does not do replication. This value should be %d on random mode.", INICFG_REPLICA_STR, chmcfginfo.replica_count, DEFAULT_REPLICA_COUNT);
							chmcfginfo.replica_count = DEFAULT_REPLICA_COUNT;
						}
					}else{
						if(chmcfginfo.max_chmpx_count < chmcfginfo.replica_count){
							WAN_CHMPRN("\"%s\" value(%ld) is over maximum chmpx count(%ld), so set value to maximum chmpx count.", INICFG_REPLICA_STR, chmcfginfo.replica_count, chmcfginfo.max_chmpx_count);
							chmcfginfo.replica_count = chmcfginfo.max_chmpx_count;
						}
					}

				}else if(0 == strcasecmp(INICFG_MAXMQSERVER_STR, key.c_str())){
					chmcfginfo.max_server_mq_cnt = static_cast<long>(atoi(reinterpret_cast<const char*>(yevent.data.scalar.value)));
					if(MAX_SERVER_MQ_CNT < chmcfginfo.max_server_mq_cnt){
						WAN_CHMPRN("\"%s\" value(%ld) is over upper limit(%d), so set upper limit.", INICFG_MAXMQSERVER_STR, chmcfginfo.max_server_mq_cnt, MAX_SERVER_MQ_CNT);
						chmcfginfo.max_server_mq_cnt = MAX_SERVER_MQ_CNT;
					}

				}else if(0 == strcasecmp(INICFG_MAXMQCLIENT_STR, key.c_str())){
					chmcfginfo.max_client_mq_cnt = static_cast<long>(atoi(reinterpret_cast<const char*>(yevent.data.scalar.value)));
					if(MAX_CLIENT_MQ_CNT < chmcfginfo.max_client_mq_cnt){
						WAN_CHMPRN("\"%s\" value(%ld) is over upper limit(%d), so set upper limit.", INICFG_MAXMQCLIENT_STR, chmcfginfo.max_client_mq_cnt, MAX_CLIENT_MQ_CNT);
						chmcfginfo.max_client_mq_cnt = MAX_CLIENT_MQ_CNT;
					}

				}else if(0 == strcasecmp(INICFG_MQPERATTACH_STR, key.c_str())){
					chmcfginfo.mqcnt_per_attach = static_cast<long>(atoi(reinterpret_cast<const char*>(yevent.data.scalar.value)));
					if(MAX_MQ_PER_ATTACH < chmcfginfo.mqcnt_per_attach){
						WAN_CHMPRN("\"%s\" value(%ld) is over upper limit(%d), so set upper limit.", INICFG_MQPERATTACH_STR, chmcfginfo.mqcnt_per_attach, MAX_MQ_PER_ATTACH);
						chmcfginfo.mqcnt_per_attach = MAX_MQ_PER_ATTACH;
					}

				}else if(0 == strcasecmp(INICFG_MAXQPERSERVERMQ_STR, key.c_str())){
					chmcfginfo.max_q_per_servermq = static_cast<long>(atoi(reinterpret_cast<const char*>(yevent.data.scalar.value)));
					if(MAX_QUEUE_PER_SERVERMQ < chmcfginfo.max_q_per_servermq){
						WAN_CHMPRN("\"%s\" value(%ld) is over upper limit(%d), so set upper limit.", INICFG_MAXQPERSERVERMQ_STR, chmcfginfo.max_q_per_servermq, MAX_QUEUE_PER_SERVERMQ);
						chmcfginfo.max_q_per_servermq = MAX_QUEUE_PER_SERVERMQ;
					}

				}else if(0 == strcasecmp(INICFG_MAXQPERCLIENTMQ_STR, key.c_str())){
					chmcfginfo.max_q_per_clientmq = static_cast<long>(atoi(reinterpret_cast<const char*>(yevent.data.scalar.value)));
					if(MAX_QUEUE_PER_CLIENTMQ < chmcfginfo.max_q_per_clientmq){
						WAN_CHMPRN("\"%s\" value(%ld) is over upper limit(%d), so set upper limit.", INICFG_MAXQPERCLIENTMQ_STR, chmcfginfo.max_q_per_clientmq, MAX_QUEUE_PER_CLIENTMQ);
						chmcfginfo.max_q_per_clientmq = MAX_QUEUE_PER_CLIENTMQ;
					}

				}else if(0 == strcasecmp(INICFG_MAXMQPERCLIENT_STR, key.c_str())){
					chmcfginfo.max_mq_per_client = static_cast<long>(atoi(reinterpret_cast<const char*>(yevent.data.scalar.value)));
					if(MAX_MQ_PER_CLIENT < chmcfginfo.max_mq_per_client){
						WAN_CHMPRN("\"%s\" value(%ld) is over upper limit(%d), so set upper limit.", INICFG_MAXMQPERCLIENT_STR, chmcfginfo.max_mq_per_client, MAX_MQ_PER_CLIENT);
						chmcfginfo.max_mq_per_client = MAX_MQ_PER_CLIENT;
					}

				}else if(0 == strcasecmp(INICFG_MAXHISTLOG_STR, key.c_str())){
					chmcfginfo.max_histlog_count = static_cast<long>(atoi(reinterpret_cast<const char*>(yevent.data.scalar.value)));
					if(MAX_HISTLOG_COUNT < chmcfginfo.max_histlog_count){
						WAN_CHMPRN("\"%s\" value(%ld) is over upper limit(%d), so set upper limit.", INICFG_MAXHISTLOG_STR, chmcfginfo.max_histlog_count, MAX_HISTLOG_COUNT);
						chmcfginfo.max_histlog_count = MAX_HISTLOG_COUNT;
					}

				}else if(0 == strcasecmp(INICFG_DATE_STR, key.c_str())){
					chmcfginfo.date = rfcdate_time(reinterpret_cast<const char*>(yevent.data.scalar.value));

				}else if(0 == strcasecmp(INICFG_PORT_STR, key.c_str())){
					ccvals.port = static_cast<short>(atoi(reinterpret_cast<const char*>(yevent.data.scalar.value)));

				}else if(0 == strcasecmp(INICFG_CTLPORT_STR, key.c_str())){
					ccvals.ctlport = static_cast<short>(atoi(reinterpret_cast<const char*>(yevent.data.scalar.value)));

				}else if(0 == strcasecmp(INICFG_SELFCTLPORT_STR, key.c_str())){
					if(CHM_INVALID_PORT == default_ctlport){
						chmcfginfo.self_ctlport = static_cast<short>(atoi(reinterpret_cast<const char*>(yevent.data.scalar.value)));
					}

				}else if(0 == strcasecmp(INICFG_RETRYCNT_STR, key.c_str())){
					chmcfginfo.retrycnt = atoi(reinterpret_cast<const char*>(yevent.data.scalar.value));

				}else if(0 == strcasecmp(INICFG_MQRETRYCNT_STR, key.c_str())){
					chmcfginfo.mq_retrycnt = atoi(reinterpret_cast<const char*>(yevent.data.scalar.value));

				}else if(0 == strcasecmp(INICFG_MQACK_STR, key.c_str())){
					if(0 == strcasecmp(reinterpret_cast<const char*>(yevent.data.scalar.value), INICFG_BOOL_ON) || 0 == strcasecmp(reinterpret_cast<const char*>(yevent.data.scalar.value), INICFG_BOOL_YES)){
						chmcfginfo.mq_ack = true;
					}else if(0 == strcasecmp(reinterpret_cast<const char*>(yevent.data.scalar.value), INICFG_BOOL_OFF) || 0 == strcasecmp(reinterpret_cast<const char*>(yevent.data.scalar.value), INICFG_BOOL_NO)){
						chmcfginfo.mq_ack = false;
					}else{
						ERR_CHMPRN("Found %s in %s section, but value %s does not defined.", INICFG_MQACK_STR, CFG_GLOBAL_SEC_STR, reinterpret_cast<const char*>(yevent.data.scalar.value));
						result = false;
					}

				}else if(0 == strcasecmp(INICFG_RWTIMEOUT_STR, key.c_str())){
					chmcfginfo.timeout_wait_socket = atoi(reinterpret_cast<const char*>(yevent.data.scalar.value));
					if(chmcfginfo.timeout_wait_socket < CHMEVENTSOCK_TIMEOUT_DEFAULT){
						chmcfginfo.timeout_wait_socket = CHMEVENTSOCK_TIMEOUT_DEFAULT;
					}

				}else if(0 == strcasecmp(INICFG_CONTIMEOUT_STR, key.c_str())){
					chmcfginfo.timeout_wait_connect = atoi(reinterpret_cast<const char*>(yevent.data.scalar.value));
					if(chmcfginfo.timeout_wait_connect < CHMEVENTSOCK_TIMEOUT_DEFAULT){
						chmcfginfo.timeout_wait_connect = CHMEVENTSOCK_TIMEOUT_DEFAULT;
					}

				}else if(0 == strcasecmp(INICFG_MQRWTIMEOUT_STR, key.c_str())){
					chmcfginfo.timeout_wait_mq = atoi(reinterpret_cast<const char*>(yevent.data.scalar.value));
					if(chmcfginfo.timeout_wait_mq < CHMEVENTMQ_TIMEOUT_DEFAULT){
						chmcfginfo.timeout_wait_mq = CHMEVENTMQ_TIMEOUT_DEFAULT;
					}

				}else if(0 == strcasecmp(INICFG_AUTOMERGE_STR, key.c_str())){
					if(0 == strcasecmp(reinterpret_cast<const char*>(yevent.data.scalar.value), INICFG_BOOL_ON) || 0 == strcasecmp(reinterpret_cast<const char*>(yevent.data.scalar.value), INICFG_BOOL_YES)){
						chmcfginfo.is_auto_merge = true;
					}else if(0 == strcasecmp(reinterpret_cast<const char*>(yevent.data.scalar.value), INICFG_BOOL_OFF) || 0 == strcasecmp(reinterpret_cast<const char*>(yevent.data.scalar.value), INICFG_BOOL_NO)){
						chmcfginfo.is_auto_merge = false;
					}else{
						ERR_CHMPRN("Found %s in %s section, but value %s does not defined.", INICFG_AUTOMERGE_STR, CFG_GLOBAL_SEC_STR, reinterpret_cast<const char*>(yevent.data.scalar.value));
						result = false;
					}

				}else if(0 == strcasecmp(INICFG_DOMERGE_STR, key.c_str())){
					if(0 == strcasecmp(reinterpret_cast<const char*>(yevent.data.scalar.value), INICFG_BOOL_ON) || 0 == strcasecmp(reinterpret_cast<const char*>(yevent.data.scalar.value), INICFG_BOOL_YES)){
						chmcfginfo.is_do_merge = true;
					}else if(0 == strcasecmp(reinterpret_cast<const char*>(yevent.data.scalar.value), INICFG_BOOL_OFF) || 0 == strcasecmp(reinterpret_cast<const char*>(yevent.data.scalar.value), INICFG_BOOL_NO)){
						chmcfginfo.is_do_merge = false;
					}else{
						ERR_CHMPRN("Found %s in %s section, but value %s does not defined.", INICFG_DOMERGE_STR, CFG_GLOBAL_SEC_STR, reinterpret_cast<const char*>(yevent.data.scalar.value));
						result = false;
					}

				}else if(0 == strcasecmp(INICFG_MERGETIMEOUT_STR, key.c_str())){
					chmcfginfo.timeout_merge = static_cast<time_t>(atoi(reinterpret_cast<const char*>(yevent.data.scalar.value)));
					if(chmcfginfo.timeout_merge < CHMEVENTMQ_TIMEOUT_DEFAULT){
						chmcfginfo.timeout_merge = CHMEVENTMQ_TIMEOUT_DEFAULT;
					}

				}else if(0 == strcasecmp(INICFG_SOCKTHREADCNT_STR, key.c_str())){
					chmcfginfo.sock_thread_cnt = atoi(reinterpret_cast<const char*>(yevent.data.scalar.value));
					if(chmcfginfo.sock_thread_cnt < ChmEventSock::DEFAULT_SOCK_THREAD_CNT){
						ERR_CHMPRN("Found %s in %s section, but value %s is wrong.", INICFG_SOCKTHREADCNT_STR, CFG_GLOBAL_SEC_STR, reinterpret_cast<const char*>(yevent.data.scalar.value));
						result = false;
					}

				}else if(0 == strcasecmp(INICFG_MQTHREADCNT_STR, key.c_str())){
					chmcfginfo.mq_thread_cnt = atoi(reinterpret_cast<const char*>(yevent.data.scalar.value));
					if(chmcfginfo.mq_thread_cnt < ChmEventMq::DEFAULT_MQ_THREAD_CNT){
						ERR_CHMPRN("Found %s in %s section, but value %s is wrong.", INICFG_MQTHREADCNT_STR, CFG_GLOBAL_SEC_STR, reinterpret_cast<const char*>(yevent.data.scalar.value));
						result = false;
					}

				}else if(0 == strcasecmp(INICFG_MAXSOCKPOOL_STR, key.c_str())){
					chmcfginfo.max_sock_pool = static_cast<time_t>(atoi(reinterpret_cast<const char*>(yevent.data.scalar.value)));
					if(chmcfginfo.max_sock_pool < ChmEventSock::DEFAULT_MAX_SOCK_POOL){
						ERR_CHMPRN("Found %s in %s section, but value %s is wrong.", INICFG_MAXSOCKPOOL_STR, CFG_GLOBAL_SEC_STR, reinterpret_cast<const char*>(yevent.data.scalar.value));
						result = false;
					}

				}else if(0 == strcasecmp(INICFG_SOCKPOOLTIMEOUT_STR, key.c_str())){
					chmcfginfo.sock_pool_timeout = atoi(reinterpret_cast<const char*>(yevent.data.scalar.value));
					if(chmcfginfo.sock_pool_timeout < ChmEventSock::NO_SOCK_POOL_TIMEOUT){
						ERR_CHMPRN("Found %s in %s section, but value %s is wrong.", INICFG_SOCKPOOLTIMEOUT_STR, CFG_GLOBAL_SEC_STR, reinterpret_cast<const char*>(yevent.data.scalar.value));
						result = false;
					}

				}else if(0 == strcasecmp(INICFG_SSL_STR, key.c_str())){
					if(0 == strcasecmp(reinterpret_cast<const char*>(yevent.data.scalar.value), INICFG_BOOL_ON) || 0 == strcasecmp(reinterpret_cast<const char*>(yevent.data.scalar.value), INICFG_BOOL_YES)){
						ccvals.is_ssl = true;
					}else if(0 == strcasecmp(reinterpret_cast<const char*>(yevent.data.scalar.value), INICFG_BOOL_OFF) || 0 == strcasecmp(reinterpret_cast<const char*>(yevent.data.scalar.value), INICFG_BOOL_NO)){
						ccvals.is_ssl = false;
					}else{
						ERR_CHMPRN("Found %s in %s section, but value %s does not defined.", INICFG_SSL_STR, CFG_GLOBAL_SEC_STR, reinterpret_cast<const char*>(yevent.data.scalar.value));
						result = false;
					}

				}else if(0 == strcasecmp(INICFG_SSL_VERIFY_PEER_STR, key.c_str())){
					if(0 == strcasecmp(reinterpret_cast<const char*>(yevent.data.scalar.value), INICFG_BOOL_ON) || 0 == strcasecmp(reinterpret_cast<const char*>(yevent.data.scalar.value), INICFG_BOOL_YES)){
						if(!ccvals.is_ssl){
							ERR_CHMPRN("Found %s in %s section with value(%s), but %s is OFF.", INICFG_SSL_VERIFY_PEER_STR, CFG_GLOBAL_SEC_STR, reinterpret_cast<const char*>(yevent.data.scalar.value), INICFG_SSL_STR);
							result = false;
						}else{
							ccvals.verify_peer = true;
						}
					}else if(0 == strcasecmp(reinterpret_cast<const char*>(yevent.data.scalar.value), INICFG_BOOL_OFF) || 0 == strcasecmp(reinterpret_cast<const char*>(yevent.data.scalar.value), INICFG_BOOL_NO)){
						ccvals.verify_peer = false;
					}else{
						ERR_CHMPRN("Found %s in %s section, but value %s is wrong.", INICFG_SSL_VERIFY_PEER_STR, CFG_GLOBAL_SEC_STR, reinterpret_cast<const char*>(yevent.data.scalar.value));
						result = false;
					}

				}else if(0 == strcasecmp(INICFG_CAPATH_STR, key.c_str())){
					if(0 != strcasecmp(INICFG_STRING_NULL, reinterpret_cast<const char*>(yevent.data.scalar.value))){		// set only when value is not "NULL"
						if(is_dir_exist(reinterpret_cast<const char*>(yevent.data.scalar.value))){
							ccvals.is_ca_file = false;
						}else{
							if(is_file_safe_exist(reinterpret_cast<const char*>(yevent.data.scalar.value))){
								ccvals.is_ca_file = true;
							}else{
								ERR_CHMPRN("Found %s in %s section with value %s, but it is not directory or file.", INICFG_CAPATH_STR, CFG_GLOBAL_SEC_STR, reinterpret_cast<const char*>(yevent.data.scalar.value));
								result = false;
							}
						}
						if(CHM_MAX_PATH_LEN <= strlen(reinterpret_cast<const char*>(yevent.data.scalar.value))){
							ERR_CHMPRN("Found %s in %s section with value %s, but its length(%zd) is over max length(1024).", INICFG_CAPATH_STR, CFG_GLOBAL_SEC_STR, reinterpret_cast<const char*>(yevent.data.scalar.value), strlen(reinterpret_cast<const char*>(yevent.data.scalar.value)));
							result = false;
						}else{
							ccvals.capath = reinterpret_cast<const char*>(yevent.data.scalar.value);
						}
					}

				}else if(0 == strcasecmp(INICFG_SERVER_CERT_STR, key.c_str())){
					if(0 != strcasecmp(INICFG_STRING_NULL, reinterpret_cast<const char*>(yevent.data.scalar.value))){		// set only when value is not "NULL"
						if(!is_file_safe_exist(reinterpret_cast<const char*>(yevent.data.scalar.value))){
							ERR_CHMPRN("Found %s in %s section with value %s, but it is not safe file.", INICFG_SERVER_CERT_STR, CFG_GLOBAL_SEC_STR, reinterpret_cast<const char*>(yevent.data.scalar.value));
							result = false;
						}else{
							if(!ccvals.is_ssl){
								ERR_CHMPRN("Found %s in %s section with value(%s), but %s is OFF.", INICFG_SERVER_CERT_STR, CFG_GLOBAL_SEC_STR, reinterpret_cast<const char*>(yevent.data.scalar.value), INICFG_SSL_STR);
								result = false;
							}else{
								if(CHM_MAX_PATH_LEN <= strlen(reinterpret_cast<const char*>(yevent.data.scalar.value))){
									ERR_CHMPRN("Found %s in %s section with value %s, but its length(%zd) is over max length(1024).", INICFG_SERVER_CERT_STR, CFG_GLOBAL_SEC_STR, reinterpret_cast<const char*>(yevent.data.scalar.value), strlen(reinterpret_cast<const char*>(yevent.data.scalar.value)));
									result = false;
								}else{
									ccvals.server_cert = reinterpret_cast<const char*>(yevent.data.scalar.value);
								}
							}
						}
					}

				}else if(0 == strcasecmp(INICFG_SERVER_PRIKEY_STR, key.c_str())){
					if(0 != strcasecmp(INICFG_STRING_NULL, reinterpret_cast<const char*>(yevent.data.scalar.value))){		// set only when value is not "NULL"
						if(!is_file_safe_exist(reinterpret_cast<const char*>(yevent.data.scalar.value))){
							ERR_CHMPRN("Found %s in %s section with value %s, but it is not safe file.", INICFG_SERVER_PRIKEY_STR, CFG_GLOBAL_SEC_STR, reinterpret_cast<const char*>(yevent.data.scalar.value));
							result = false;
						}else{
							if(!ccvals.is_ssl){
								ERR_CHMPRN("Found %s in %s section with value(%s), but %s is OFF.", INICFG_SERVER_PRIKEY_STR, CFG_GLOBAL_SEC_STR, reinterpret_cast<const char*>(yevent.data.scalar.value), INICFG_SSL_STR);
								result = false;
							}else{
								if(CHM_MAX_PATH_LEN <= strlen(reinterpret_cast<const char*>(yevent.data.scalar.value))){
									ERR_CHMPRN("Found %s in %s section with value %s, but its length(%zd) is over max length(1024).", INICFG_SERVER_PRIKEY_STR, CFG_GLOBAL_SEC_STR, reinterpret_cast<const char*>(yevent.data.scalar.value), strlen(reinterpret_cast<const char*>(yevent.data.scalar.value)));
									result = false;
								}else{
									ccvals.server_prikey = reinterpret_cast<const char*>(yevent.data.scalar.value);
								}
							}
						}
					}

				}else if(0 == strcasecmp(INICFG_SLAVE_CERT_STR, key.c_str())){
					if(0 != strcasecmp(INICFG_STRING_NULL, reinterpret_cast<const char*>(yevent.data.scalar.value))){		// set only when value is not "NULL"
						if(!is_file_safe_exist(reinterpret_cast<const char*>(yevent.data.scalar.value))){
							ERR_CHMPRN("Found %s in %s section with value %s, but it is not safe file.", INICFG_SLAVE_CERT_STR, CFG_GLOBAL_SEC_STR, reinterpret_cast<const char*>(yevent.data.scalar.value));
							result = false;
						}else{
							if(CHM_MAX_PATH_LEN <= strlen(reinterpret_cast<const char*>(yevent.data.scalar.value))){
								ERR_CHMPRN("Found %s in %s section with value %s, but its length(%zd) is over max length(1024).", INICFG_SLAVE_CERT_STR, CFG_GLOBAL_SEC_STR, reinterpret_cast<const char*>(yevent.data.scalar.value), strlen(reinterpret_cast<const char*>(yevent.data.scalar.value)));
								result = false;
							}else{
								ccvals.slave_cert = reinterpret_cast<const char*>(yevent.data.scalar.value);
							}
						}
					}

				}else if(0 == strcasecmp(INICFG_SLAVE_PRIKEY_STR, key.c_str())){
					if(0 != strcasecmp(INICFG_STRING_NULL, reinterpret_cast<const char*>(yevent.data.scalar.value))){		// set only when value is not "NULL"
						if(!is_file_safe_exist(reinterpret_cast<const char*>(yevent.data.scalar.value))){
							ERR_CHMPRN("Found %s in %s section with value %s, but it is not safe file.", INICFG_SLAVE_PRIKEY_STR, CFG_GLOBAL_SEC_STR, reinterpret_cast<const char*>(yevent.data.scalar.value));
							result = false;
						}else{
							if(CHM_MAX_PATH_LEN <= strlen(reinterpret_cast<const char*>(yevent.data.scalar.value))){
								ERR_CHMPRN("Found %s in %s section with value %s, but its length(%zd) is over max length(1024).", INICFG_SLAVE_PRIKEY_STR, CFG_GLOBAL_SEC_STR, reinterpret_cast<const char*>(yevent.data.scalar.value), strlen(reinterpret_cast<const char*>(yevent.data.scalar.value)));
								result = false;
							}else{
								ccvals.slave_prikey = reinterpret_cast<const char*>(yevent.data.scalar.value);
							}
						}
					}

				}else if(0 == strcasecmp(INICFG_K2HFULLMAP_STR, key.c_str())){
					if(0 == strcasecmp(reinterpret_cast<const char*>(yevent.data.scalar.value), INICFG_BOOL_ON) || 0 == strcasecmp(reinterpret_cast<const char*>(yevent.data.scalar.value), INICFG_BOOL_YES)){
						chmcfginfo.k2h_fullmap = true;
					}else if(0 == strcasecmp(reinterpret_cast<const char*>(yevent.data.scalar.value), INICFG_BOOL_OFF) || 0 == strcasecmp(reinterpret_cast<const char*>(yevent.data.scalar.value), INICFG_BOOL_NO)){
						chmcfginfo.k2h_fullmap = false;
					}else{
						ERR_CHMPRN("Found %s in %s section, but value %s is wrong.", INICFG_K2HFULLMAP_STR, CFG_GLOBAL_SEC_STR, reinterpret_cast<const char*>(yevent.data.scalar.value));
						result = false;
					}

				}else if(0 == strcasecmp(INICFG_K2HMASKBIT_STR, key.c_str())){
					chmcfginfo.k2h_mask_bitcnt = atoi(reinterpret_cast<const char*>(yevent.data.scalar.value));

				}else if(0 == strcasecmp(INICFG_K2HCMASKBIT_STR, key.c_str())){
					chmcfginfo.k2h_cmask_bitcnt = atoi(reinterpret_cast<const char*>(yevent.data.scalar.value));

				}else if(0 == strcasecmp(INICFG_K2HMAXELE_STR, key.c_str())){
					chmcfginfo.k2h_max_element = atoi(reinterpret_cast<const char*>(yevent.data.scalar.value));

				}else{
					WAN_CHMPRN("Found unexpected key(%s) in %s section, thus skip this key and value.", key.c_str(), CFG_GLOBAL_SEC_STR);
				}
				key.clear();
			}
		}else{
			// [TODO] Now not support alias(anchor) event
			//
			ERR_CHMPRN("Found unexpected yaml event(%d) in %s section.", yevent.type, CFG_GLOBAL_SEC_STR);
			result = false;
		}

		// delete event
		if(is_loop){
			is_loop = yevent.type != YAML_STREAM_END_EVENT;
		}
		yaml_event_delete(&yevent);
	}

	return result;
}

static bool ChmYamlLoadConfigrationSvrnodeSec(yaml_parser_t& yparser, CHMCFGINFO& chmcfginfo, CHMCONF_CCV& ccvals)
{
	// Must start yaml sequence(for mapping array) -> mapping event.
	yaml_event_t	yevent;
	{
		// sequence
		if(!yaml_parser_parse(&yparser, &yevent)){
			ERR_CHMPRN("Could not parse event. errno = %d", errno);
			return false;
		}
		if(YAML_SEQUENCE_START_EVENT != yevent.type){
			ERR_CHMPRN("Parsed event type is not start sequence(%d)", yevent.type);
			yaml_event_delete(&yevent);
			return false;
		}
		yaml_event_delete(&yevent);
	}

	// Clear default values
	ccvals.is_server_by_ctlport		= false;	// for check server/slave mode by checking control port and server name.
	ccvals.found_ssl				= false;
	ccvals.found_ssl_verify_peer	= false;

	// temporary data
	CHMNODE_CFGINFO	svrnode;
	string			localhost_hostname;
	ChmNetDb::GetLocalHostname(localhost_hostname);

	// Loading
	string	key("");
	bool	result = true;
	for(bool is_loop = true, in_mapping = false; is_loop && result; ){
		// get event
		if(!yaml_parser_parse(&yparser, &yevent)){
			ERR_CHMPRN("Could not parse event. errno = %d", errno);
			result = false;
			continue;				// break loop assap.
		}

		// check event
		if(YAML_MAPPING_START_EVENT == yevent.type){
			// Start mapping event
			if(in_mapping){
				ERR_CHMPRN("Already start yaml mapping event in %s section loop.", CFG_SVRNODE_SEC_STR);
				result = false;
			}else{
				in_mapping = true;

				// Set default value for each server node
				svrnode.name.clear();
				svrnode.port			= ccvals.port;
				svrnode.ctlport			= ccvals.ctlport;
				svrnode.is_ssl			= ccvals.is_ssl;
				svrnode.verify_peer		= ccvals.verify_peer;
				svrnode.is_ca_file		= ccvals.is_ca_file;
				svrnode.capath			= ccvals.capath;
				svrnode.server_cert		= ccvals.server_cert;
				svrnode.server_prikey	= ccvals.server_prikey;
				svrnode.slave_cert		= ccvals.slave_cert;
				svrnode.slave_prikey	= ccvals.slave_prikey;
			}

		}else if(YAML_MAPPING_END_EVENT == yevent.type){
			// End mapping event
			if(!in_mapping){
				ERR_CHMPRN("Already stop yaml mapping event in %s section loop.", CFG_SVRNODE_SEC_STR);
				result = false;
			}else{
				// Finish one server node configuration.
				//
				if(svrnode.is_ssl){
					ccvals.found_ssl = true;				// Keep SSL flag for checking after this methods
				}
				if(svrnode.verify_peer){
					ccvals.found_ssl_verify_peer = true;	// Keep verify peer flag for checking after this loop
				}

				// check values
				if(svrnode.name.empty()){
					ERR_CHMPRN("Found some value in %s section, but NAME is empty.", CFG_SVRNODE_SEC_STR);
					result = false;
				}
				if(CHM_INVALID_PORT == svrnode.port){
					ERR_CHMPRN("Invalid port number for %s server node.", svrnode.name.c_str());
					result = false;
				}
				if(CHM_INVALID_PORT == svrnode.ctlport){
					ERR_CHMPRN("Invalid control port number for %s server node.", svrnode.name.c_str());
					result = false;
				}
				if(!svrnode.is_ssl && svrnode.verify_peer){
					ERR_CHMPRN("Found %s with value(ON) in %s section, but %s is OFF.", INICFG_SSL_VERIFY_PEER_STR, CFG_SVRNODE_SEC_STR, INICFG_SSL_STR);
					result = false;
				}
				if(svrnode.is_ssl == svrnode.server_cert.empty()){
					ERR_CHMPRN("Found %s with value(%s) in %s section, but %s is %s.", INICFG_SERVER_CERT_STR, svrnode.server_cert.c_str(), CFG_SVRNODE_SEC_STR, INICFG_SSL_STR, (svrnode.is_ssl ? "ON" : "OFF"));
					result = false;
				}
				if(svrnode.is_ssl == svrnode.server_prikey.empty()){
					ERR_CHMPRN("Found %s with value(%s) in %s section, but %s is %s.", INICFG_SERVER_PRIKEY_STR, svrnode.server_prikey.c_str(), CFG_SVRNODE_SEC_STR, INICFG_SSL_STR, (svrnode.is_ssl ? "ON" : "OFF"));
					result = false;
				}
				if(svrnode.slave_cert.empty() != svrnode.slave_prikey.empty()){
					// SSL_SLAVE_CERT, SSL_SLAVE_PRIKEY must be set or not set.
					ERR_CHMPRN("Found %s with value(%s) and %s with(%s) in %s server node in %s section.", INICFG_SLAVE_CERT_STR, svrnode.slave_cert.c_str(), INICFG_SLAVE_PRIKEY_STR, svrnode.slave_prikey.c_str(), svrnode.name.c_str(), INICFG_SVRNODE_SEC_STR);
					result = false;
				}

				// Expand name(simple regex)
				// Need to expand for server node, because node name is compared directly and is used by connecting.
				// So that, we expand server node name here.
				//
				strlst_t	expand_svrnodes;
				if(!ExpandSimpleRegxHostname(svrnode.name.c_str(), expand_svrnodes, true, true, false)){	// convert localhost to server name and query FQDN.
					ERR_CHMPRN("Failed to expand server node name(%s).", svrnode.name.c_str());
					result = false;

				}else{
					// Add each expanded server node name.
					for(strlst_t::const_iterator svrnodeiter = expand_svrnodes.begin(); svrnodeiter != expand_svrnodes.end(); ++svrnodeiter){
						svrnode.name = (*svrnodeiter);
						chmcfginfo.servers.push_back(svrnode);

						// whichever server mode or not?
						if(!ccvals.is_server_by_ctlport && chmcfginfo.self_ctlport == svrnode.ctlport && 0 == strcasecmp(svrnode.name.c_str(), localhost_hostname.c_str())){
							ccvals.is_server_by_ctlport = true;
						}
					}
				}

				in_mapping = false;
			}

		}else if(YAML_SEQUENCE_END_EVENT == yevent.type){
			// End sequence(for mapping) event
			if(in_mapping){
				ERR_CHMPRN("Found yaml sequence event, but not stop yaml mapping event in %s section loop.", CFG_SVRNODE_SEC_STR);
				result = false;
			}else{
				// Finish loop without error.
				//
				is_loop = false;
			}

		}else if(YAML_SCALAR_EVENT == yevent.type){
			// Load key & value
			if(key.empty()){
				key = reinterpret_cast<const char*>(yevent.data.scalar.value);
			}else{
				//
				// Compare key and set value
				//
				if(0 == strcasecmp(INICFG_NAME_STR, key.c_str())){
					svrnode.name = reinterpret_cast<const char*>(yevent.data.scalar.value);

				}else if(0 == strcasecmp(INICFG_PORT_STR, key.c_str())){
					svrnode.port = static_cast<short>(atoi(reinterpret_cast<const char*>(yevent.data.scalar.value)));

				}else if(0 == strcasecmp(INICFG_CTLPORT_STR, key.c_str())){
					svrnode.ctlport = static_cast<short>(atoi(reinterpret_cast<const char*>(yevent.data.scalar.value)));

				}else if(0 == strcasecmp(INICFG_SSL_STR, key.c_str())){
					if(0 == strcasecmp(reinterpret_cast<const char*>(yevent.data.scalar.value), INICFG_BOOL_ON) || 0 == strcasecmp(reinterpret_cast<const char*>(yevent.data.scalar.value), INICFG_BOOL_YES)){
						svrnode.is_ssl = true;
					}else if(0 == strcasecmp(reinterpret_cast<const char*>(yevent.data.scalar.value), INICFG_BOOL_OFF) || 0 == strcasecmp(reinterpret_cast<const char*>(yevent.data.scalar.value), INICFG_BOOL_NO)){
						svrnode.is_ssl = false;
					}else{
						ERR_CHMPRN("Found %s in %s section, but value %s does not defined.", INICFG_SSL_STR, CFG_SVRNODE_SEC_STR, reinterpret_cast<const char*>(yevent.data.scalar.value));
						result = false;
					}

				}else if(0 == strcasecmp(INICFG_SSL_VERIFY_PEER_STR, key.c_str())){
					if(0 == strcasecmp(reinterpret_cast<const char*>(yevent.data.scalar.value), INICFG_BOOL_ON) || 0 == strcasecmp(reinterpret_cast<const char*>(yevent.data.scalar.value), INICFG_BOOL_YES)){
						svrnode.verify_peer = true;
					}else if(0 == strcasecmp(reinterpret_cast<const char*>(yevent.data.scalar.value), INICFG_BOOL_OFF) || 0 == strcasecmp(reinterpret_cast<const char*>(yevent.data.scalar.value), INICFG_BOOL_NO)){
						svrnode.verify_peer = false;
					}else{
						ERR_CHMPRN("Found %s in %s section, but value %s does not defined.", INICFG_SSL_VERIFY_PEER_STR, CFG_SVRNODE_SEC_STR, reinterpret_cast<const char*>(yevent.data.scalar.value));
						result = false;
					}

				}else if(0 == strcasecmp(INICFG_CAPATH_STR, key.c_str())){
					if(0 == strcasecmp(reinterpret_cast<const char*>(yevent.data.scalar.value), INICFG_STRING_NULL)){
						svrnode.is_ca_file	= false;
						svrnode.capath		= "";
					}else{
						if(is_dir_exist(reinterpret_cast<const char*>(yevent.data.scalar.value))){
							svrnode.is_ca_file = false;
						}else{
							if(is_file_safe_exist(reinterpret_cast<const char*>(yevent.data.scalar.value))){
								svrnode.is_ca_file = true;
							}else{
								ERR_CHMPRN("Found %s in %s section with value %s, but it is not directory or file.", INICFG_CAPATH_STR, CFG_SVRNODE_SEC_STR, reinterpret_cast<const char*>(yevent.data.scalar.value));
								result = false;
							}
						}
						if(CHM_MAX_PATH_LEN <= strlen(reinterpret_cast<const char*>(yevent.data.scalar.value))){
							ERR_CHMPRN("Found %s in %s section with value %s, but its length(%zd) is over max length(1024).", INICFG_CAPATH_STR, CFG_SVRNODE_SEC_STR, reinterpret_cast<const char*>(yevent.data.scalar.value), strlen(reinterpret_cast<const char*>(yevent.data.scalar.value)));
							result = false;
						}else{
							svrnode.capath = reinterpret_cast<const char*>(yevent.data.scalar.value);
						}
					}

				}else if(0 == strcasecmp(INICFG_SERVER_CERT_STR, key.c_str())){
					if(0 == strcasecmp(reinterpret_cast<const char*>(yevent.data.scalar.value), INICFG_STRING_NULL)){
						svrnode.server_cert = "";
					}else{
						if(!is_file_safe_exist(reinterpret_cast<const char*>(yevent.data.scalar.value))){
							ERR_CHMPRN("Found %s in %s section with value %s, but it is not directory or file.", INICFG_SERVER_CERT_STR, CFG_SVRNODE_SEC_STR, reinterpret_cast<const char*>(yevent.data.scalar.value));
							result = false;
						}else{
							if(CHM_MAX_PATH_LEN <= strlen(reinterpret_cast<const char*>(yevent.data.scalar.value))){
								ERR_CHMPRN("Found %s in %s section with value %s, but its length(%zd) is over max length(1024).", INICFG_SERVER_CERT_STR, CFG_SVRNODE_SEC_STR, reinterpret_cast<const char*>(yevent.data.scalar.value), strlen(reinterpret_cast<const char*>(yevent.data.scalar.value)));
								result = false;
							}else{
								svrnode.server_cert = reinterpret_cast<const char*>(yevent.data.scalar.value);
							}
						}
					}

				}else if(0 == strcasecmp(INICFG_SERVER_PRIKEY_STR, key.c_str())){
					if(0 == strcasecmp(reinterpret_cast<const char*>(yevent.data.scalar.value), INICFG_STRING_NULL)){
						svrnode.server_prikey = "";
					}else{
						if(!is_file_safe_exist(reinterpret_cast<const char*>(yevent.data.scalar.value))){
							ERR_CHMPRN("Found %s in %s section with value %s, but it is not directory or file.", INICFG_SERVER_PRIKEY_STR, CFG_SVRNODE_SEC_STR, reinterpret_cast<const char*>(yevent.data.scalar.value));
							result = false;
						}else{
							if(CHM_MAX_PATH_LEN <= strlen(reinterpret_cast<const char*>(yevent.data.scalar.value))){
								ERR_CHMPRN("Found %s in %s section with value %s, but its length(%zd) is over max length(1024).", INICFG_SERVER_PRIKEY_STR, CFG_SVRNODE_SEC_STR, reinterpret_cast<const char*>(yevent.data.scalar.value), strlen(reinterpret_cast<const char*>(yevent.data.scalar.value)));
								result = false;
							}else{
								svrnode.server_prikey = reinterpret_cast<const char*>(yevent.data.scalar.value);
							}
						}
					}

				}else if(0 == strcasecmp(INICFG_SLAVE_CERT_STR, key.c_str())){
					if(0 == strcasecmp(reinterpret_cast<const char*>(yevent.data.scalar.value), INICFG_STRING_NULL)){
						svrnode.slave_cert = "";
					}else{
						if(!is_file_safe_exist(reinterpret_cast<const char*>(yevent.data.scalar.value))){
							ERR_CHMPRN("Found %s in %s section with value %s, but it is not directory or file.", INICFG_SLAVE_CERT_STR, CFG_SVRNODE_SEC_STR, reinterpret_cast<const char*>(yevent.data.scalar.value));
							result = false;
						}else{
							if(CHM_MAX_PATH_LEN <= strlen(reinterpret_cast<const char*>(yevent.data.scalar.value))){
								ERR_CHMPRN("Found %s in %s section with value %s, but its length(%zd) is over max length(1024).", INICFG_SLAVE_CERT_STR, CFG_SVRNODE_SEC_STR, reinterpret_cast<const char*>(yevent.data.scalar.value), strlen(reinterpret_cast<const char*>(yevent.data.scalar.value)));
								result = false;
							}else{
								svrnode.slave_cert = reinterpret_cast<const char*>(yevent.data.scalar.value);
							}
						}
					}

				}else if(0 == strcasecmp(INICFG_SLAVE_PRIKEY_STR, key.c_str())){
					if(0 == strcasecmp(reinterpret_cast<const char*>(yevent.data.scalar.value), INICFG_STRING_NULL)){
						svrnode.slave_prikey = "";
					}else{
						if(!is_file_safe_exist(reinterpret_cast<const char*>(yevent.data.scalar.value))){
							ERR_CHMPRN("Found %s in %s section with value %s, but it is not directory or file.", INICFG_SLAVE_PRIKEY_STR, CFG_SVRNODE_SEC_STR, reinterpret_cast<const char*>(yevent.data.scalar.value));
							result = false;
						}else{
							if(CHM_MAX_PATH_LEN <= strlen(reinterpret_cast<const char*>(yevent.data.scalar.value))){
								ERR_CHMPRN("Found %s in %s section with value %s, but its length(%zd) is over max length(1024).", INICFG_SLAVE_PRIKEY_STR, CFG_SVRNODE_SEC_STR, reinterpret_cast<const char*>(yevent.data.scalar.value), strlen(reinterpret_cast<const char*>(yevent.data.scalar.value)));
								result = false;
							}else{
								svrnode.slave_prikey = reinterpret_cast<const char*>(yevent.data.scalar.value);
							}
						}
					}

				}else{
					WAN_CHMPRN("Found unexpected key(%s) in %s section, thus skip this key and value.", key.c_str(), CFG_SVRNODE_SEC_STR);
				}
				key.clear();
			}
		}else{
			// [TODO] Now not support alias(anchor) event
			//
			ERR_CHMPRN("Found unexpected yaml event(%d) in %s section.", yevent.type, CFG_GLOBAL_SEC_STR);
			result = false;
		}

		// delete event
		if(is_loop){
			is_loop = yevent.type != YAML_STREAM_END_EVENT;
		}
		yaml_event_delete(&yevent);
	}

	if(result){
		// Re-Check SSL_CAPATH, SSL_SLAVE_CERT, SSL_SLAVE_PRIKEY
		//
		for(chmnode_cfginfos_t::iterator iter = chmcfginfo.servers.begin(); iter != chmcfginfo.servers.end(); ++iter){
			if(!ccvals.found_ssl){
				// If there is no ssl server, all servers should not have CApath.
				if(!iter->capath.empty()){
					ERR_CHMPRN("Found %s with value(%s) in %s server node in %s section.", INICFG_CAPATH_STR, iter->capath.c_str(), iter->name.c_str(), CFG_SVRNODE_SEC_STR);
					result = false;
					break;
				}
				// If there is no ssl servers and no verify peer, any servers should not have client cert and private key.
				if(!iter->slave_cert.empty() || !iter->slave_prikey.empty()){
					ERR_CHMPRN("Found %s with value(%s) and %s with value(%s) in %s server node in %s section.", INICFG_SLAVE_CERT_STR, iter->slave_cert.c_str(), INICFG_SLAVE_PRIKEY_STR, iter->slave_prikey.c_str(), iter->name.c_str(), INICFG_SVRNODE_SEC_STR);
					result = false;
					break;
				}
			}else{
				if(ccvals.found_ssl_verify_peer){
					// If there are ssl servers with verify peer, all servers must have client cert and private key.
					if(iter->slave_cert.empty() || iter->slave_prikey.empty()){
						ERR_CHMPRN("Found %s with value(%s) and %s with value(%s) in %s server node in %s section.", INICFG_SLAVE_CERT_STR, iter->slave_cert.c_str(), INICFG_SLAVE_PRIKEY_STR, iter->slave_prikey.c_str(), iter->name.c_str(), INICFG_SVRNODE_SEC_STR);
						result = false;
						break;
					}
				}else{
					// If there are ssl servers without verify peer, any servers should not have client cert and private key.
					if(!iter->slave_cert.empty() || !iter->slave_prikey.empty()){
						ERR_CHMPRN("Found %s with value(%s) and %s with value(%s) in %s server node in %s section.", INICFG_SLAVE_CERT_STR, iter->slave_cert.c_str(), INICFG_SLAVE_PRIKEY_STR, iter->slave_prikey.c_str(), iter->name.c_str(), INICFG_SVRNODE_SEC_STR);
						result = false;
						break;
					}

				}
			}
		}
		if(result){
			// [NOTE]
			// The server list might have duplicate server name & port.
			// Because the port number can not be specified in configration file, so if there is some server nodes on same server
			// and one specifies port and the other does not specify port(using default port).
			// On this case, the list have duplicate server.
			//
			chmcfginfo.servers.unique(chm_node_cfg_info_same_name_port());		// uniq about server node must be name and port
			chmcfginfo.servers.sort(chm_node_cfg_info_sort());
		}
	}

	return result;
}

static bool ChmYamlLoadConfigrationSlvnodeSec(yaml_parser_t& yparser, CHMCFGINFO& chmcfginfo, CHMCONF_CCV& ccvals)
{
	// Must start yaml sequence(for mapping array) -> mapping event.
	yaml_event_t	yevent;
	{
		// sequence
		if(!yaml_parser_parse(&yparser, &yevent)){
			ERR_CHMPRN("Could not parse event. errno = %d", errno);
			return false;
		}
		if(YAML_SEQUENCE_START_EVENT != yevent.type){
			ERR_CHMPRN("Parsed event type is not start sequence(%d)", yevent.type);
			yaml_event_delete(&yevent);
			return false;
		}
		yaml_event_delete(&yevent);
	}

	// temporary data
	CHMNODE_CFGINFO	slvnode;

	// Loading
	string	key("");
	bool	result = true;
	for(bool is_loop = true, in_mapping = false; is_loop && result; ){
		// get event
		if(!yaml_parser_parse(&yparser, &yevent)){
			ERR_CHMPRN("Could not parse event. errno = %d", errno);
			result = false;
			continue;				// break loop assap.
		}

		// check event
		if(YAML_MAPPING_START_EVENT == yevent.type){
			// Start mapping event
			if(in_mapping){
				ERR_CHMPRN("Already start yaml mapping event in %s section loop.", CFG_SLVNODE_SEC_STR);
				result = false;
			}else{
				in_mapping = true;

				// Set default value for each server node
				slvnode.name.clear();
				slvnode.server_cert.clear();
				slvnode.server_prikey.clear();
				slvnode.port			= CHM_INVALID_PORT;
				slvnode.ctlport			= ccvals.ctlport;
				slvnode.is_ssl			= false;
				slvnode.verify_peer		= false;
				slvnode.is_ca_file		= ccvals.is_ca_file;
				slvnode.capath			= ccvals.capath;
				slvnode.slave_cert		= ccvals.slave_cert;
				slvnode.slave_prikey	= ccvals.slave_prikey;
			}

		}else if(YAML_MAPPING_END_EVENT == yevent.type){
			// End mapping event
			if(!in_mapping){
				ERR_CHMPRN("Already stop yaml mapping event in %s section loop.", CFG_SLVNODE_SEC_STR);
				result = false;
			}else{
				// Finish one server node configuration.
				//

				// check values
				if(slvnode.name.empty()){
					ERR_CHMPRN("Found some value in %s section, but NAME is empty.", CFG_SLVNODE_SEC_STR);
					result = false;
				}
				if(!ccvals.found_ssl && !slvnode.capath.empty()){
					// There is not SSL, but CApath is set.
					ERR_CHMPRN("Found %s with value(%s) in %s slave node in %s section.", INICFG_CAPATH_STR, slvnode.capath.c_str(), slvnode.name.c_str(), INICFG_SLVNODE_SEC_STR);
					result = false;
				}
				if(slvnode.slave_cert.empty() != slvnode.slave_prikey.empty()){
					// SSL_SLAVE_CERT, SSL_SLAVE_PRIKEY must be set or not set.
					ERR_CHMPRN("Found %s with value(%s) and %s with(%s) in %s slave node in %s section.", INICFG_SLAVE_CERT_STR, slvnode.slave_cert.c_str(), INICFG_SLAVE_PRIKEY_STR, slvnode.slave_prikey.c_str(), slvnode.name.c_str(), INICFG_SLVNODE_SEC_STR);
					result = false;
				}
				if(ccvals.found_ssl_verify_peer == slvnode.slave_cert.empty()){
					// If There is SSL_VERIFY_PEER, but client cert(and private key) must be set.(nor so on)
					ERR_CHMPRN("Found %s with value(%s) and %s with(%s) in %s slave node in %s section.", INICFG_SLAVE_CERT_STR, slvnode.slave_cert.c_str(), INICFG_SLAVE_PRIKEY_STR, slvnode.slave_prikey.c_str(), slvnode.name.c_str(), INICFG_SLVNODE_SEC_STR);
					result = false;
				}

				// set value
				chmcfginfo.slaves.push_back(slvnode);

				in_mapping = false;
			}

		}else if(YAML_SEQUENCE_END_EVENT == yevent.type){
			// End sequence(for mapping) event
			if(in_mapping){
				ERR_CHMPRN("Found yaml sequence event, but not stop yaml mapping event in %s section loop.", CFG_SLVNODE_SEC_STR);
				result = false;
			}else{
				// Finish loop without error.
				//
				is_loop = false;
			}

		}else if(YAML_SCALAR_EVENT == yevent.type){
			// Load key & value
			if(key.empty()){
				key = reinterpret_cast<const char*>(yevent.data.scalar.value);
			}else{
				//
				// Compare key and set value
				//
				if(0 == strcasecmp(INICFG_NAME_STR, key.c_str())){
					// slave server name is allowed regexed format.
					// so do not convert name to global name(can not convert regexed name).
					//
					slvnode.name = reinterpret_cast<const char*>(yevent.data.scalar.value);

				}else if(0 == strcasecmp(INICFG_CTLPORT_STR, key.c_str())){
					slvnode.ctlport = static_cast<short>(atoi(reinterpret_cast<const char*>(yevent.data.scalar.value)));

				}else if(0 == strcasecmp(INICFG_CAPATH_STR, key.c_str())){
					if(0 == strcasecmp(reinterpret_cast<const char*>(yevent.data.scalar.value), INICFG_STRING_NULL)){
						slvnode.is_ca_file	= false;
						slvnode.capath		= "";
					}else{
						if(is_dir_exist(reinterpret_cast<const char*>(yevent.data.scalar.value))){
							slvnode.is_ca_file = false;
						}else{
							if(is_file_safe_exist(reinterpret_cast<const char*>(yevent.data.scalar.value))){
								slvnode.is_ca_file = true;
							}else{
								ERR_CHMPRN("Found %s in %s section with value %s, but it is not directory or file.", INICFG_CAPATH_STR, CFG_SLVNODE_SEC_STR, reinterpret_cast<const char*>(yevent.data.scalar.value));
								result = false;
							}
						}
						if(CHM_MAX_PATH_LEN <= strlen(reinterpret_cast<const char*>(yevent.data.scalar.value))){
							ERR_CHMPRN("Found %s in %s section with value %s, but its length(%zd) is over max length(1024).", INICFG_CAPATH_STR, CFG_SLVNODE_SEC_STR, reinterpret_cast<const char*>(yevent.data.scalar.value), strlen(reinterpret_cast<const char*>(yevent.data.scalar.value)));
							result = false;
						}else{
							slvnode.capath = reinterpret_cast<const char*>(yevent.data.scalar.value);
						}
					}

				}else if(0 == strcasecmp(INICFG_SLAVE_CERT_STR, key.c_str())){
					if(0 == strcasecmp(reinterpret_cast<const char*>(yevent.data.scalar.value), INICFG_STRING_NULL)){
						slvnode.slave_cert = "";
					}else{
						if(!is_file_safe_exist(reinterpret_cast<const char*>(yevent.data.scalar.value))){
							ERR_CHMPRN("Found %s in %s section with value %s, but it is not directory or file.", INICFG_SLAVE_CERT_STR, CFG_SLVNODE_SEC_STR, reinterpret_cast<const char*>(yevent.data.scalar.value));
							result = false;
						}else{
							if(CHM_MAX_PATH_LEN <= strlen(reinterpret_cast<const char*>(yevent.data.scalar.value))){
								ERR_CHMPRN("Found %s in %s section with value %s, but its length(%zd) is over max length(1024).", INICFG_SLAVE_CERT_STR, CFG_SLVNODE_SEC_STR, reinterpret_cast<const char*>(yevent.data.scalar.value), strlen(reinterpret_cast<const char*>(yevent.data.scalar.value)));
								result = false;
							}else{
								slvnode.slave_cert = reinterpret_cast<const char*>(yevent.data.scalar.value);
							}
						}
					}

				}else if(0 == strcasecmp(INICFG_SLAVE_PRIKEY_STR, key.c_str())){
					if(0 == strcasecmp(reinterpret_cast<const char*>(yevent.data.scalar.value), INICFG_STRING_NULL)){
						slvnode.slave_prikey = "";
					}else{
						if(!is_file_safe_exist(reinterpret_cast<const char*>(yevent.data.scalar.value))){
							ERR_CHMPRN("Found %s in %s section with value %s, but it is not directory or file.", INICFG_SLAVE_PRIKEY_STR, CFG_SLVNODE_SEC_STR, reinterpret_cast<const char*>(yevent.data.scalar.value));
							result = false;
						}else{
							if(CHM_MAX_PATH_LEN <= strlen(reinterpret_cast<const char*>(yevent.data.scalar.value))){
								ERR_CHMPRN("Found %s in %s section with value %s, but its length(%zd) is over max length(1024).", INICFG_SLAVE_PRIKEY_STR, CFG_SLVNODE_SEC_STR, reinterpret_cast<const char*>(yevent.data.scalar.value), strlen(reinterpret_cast<const char*>(yevent.data.scalar.value)));
								result = false;
							}else{
								slvnode.slave_prikey = reinterpret_cast<const char*>(yevent.data.scalar.value);
							}
						}
					}

				}else{
					WAN_CHMPRN("Found unexpected key(%s) in %s section, thus skip this key and value.", key.c_str(), CFG_SLVNODE_SEC_STR);
				}
				key.clear();
			}
		}else{
			// [TODO] Now not support alias(anchor) event
			//
			ERR_CHMPRN("Found unexpected yaml event(%d) in %s section.", yevent.type, CFG_GLOBAL_SEC_STR);
			result = false;
		}

		// delete event
		if(is_loop){
			is_loop = yevent.type != YAML_STREAM_END_EVENT;
		}
		yaml_event_delete(&yevent);
	}

	if(result){
		chmcfginfo.slaves.unique(chm_node_cfg_info_same_name_port());		// uniq about slave node must be name and port
		chmcfginfo.slaves.sort(chm_node_cfg_info_sort());
	}

	return result;
}

static bool ChmYamlLoadConfigrationTopLevel(yaml_parser_t& yparser, CHMCFGINFO& chmcfginfo, short default_ctlport)
{
	CHMYamlDataStack	other_stack;
	CHMCONF_CCV			ccvals;
	bool				is_set_global	= false;
	bool				is_set_svrnode	= false;
	bool				is_set_slvnode	= false;
	bool				result			= true;
	for(bool is_loop = true, in_stream = false, in_document = false, in_toplevel = false; is_loop && result; ){
		// get event
		yaml_event_t	yevent;
		if(!yaml_parser_parse(&yparser, &yevent)){
			ERR_CHMPRN("Could not parse event. errno = %d", errno);
			result = false;
			continue;
		}

		// check event
		switch(yevent.type){
			case YAML_NO_EVENT:
				MSG_CHMPRN("There is no yaml event in loop");
				break;

			case YAML_STREAM_START_EVENT:
				if(!other_stack.empty()){
					MSG_CHMPRN("Found start yaml stream event in skipping event loop");
					if(!other_stack.add(yevent.type)){
						result = false;
					}
				}else if(in_stream){
					MSG_CHMPRN("Already start yaml stream event in loop, Thus stacks this event.");
					if(!other_stack.add(yevent.type)){
						result = false;
					}
				}else{
					MSG_CHMPRN("Start yaml stream event in loop");
					in_stream = true;
				}
				break;

			case YAML_STREAM_END_EVENT:
				if(!other_stack.empty()){
					MSG_CHMPRN("Found stop yaml stream event in skipping event loop");
					if(!other_stack.add(yevent.type)){
						result = false;
					}
				}else if(!in_stream){
					MSG_CHMPRN("Already stop yaml stream event in loop, Thus stacks this event.");
					if(!other_stack.add(yevent.type)){
						result = false;
					}
				}else{
					MSG_CHMPRN("Stop yaml stream event in loop");
					in_stream = false;
				}
				break;

			case YAML_DOCUMENT_START_EVENT:
				if(!other_stack.empty()){
					MSG_CHMPRN("Found start yaml document event in skipping event loop");
					if(!other_stack.add(yevent.type)){
						result = false;
					}
				}else if(!in_stream){
					MSG_CHMPRN("Found start yaml document event before yaml stream event in loop, Thus stacks this event.");
					if(!other_stack.add(yevent.type)){
						result = false;
					}
				}else if(in_document){
					MSG_CHMPRN("Already start yaml document event in loop, Thus stacks this event.");
					if(!other_stack.add(yevent.type)){
						result = false;
					}
				}else{
					MSG_CHMPRN("Start yaml document event in loop");
					in_document = true;
				}
				break;

			case YAML_DOCUMENT_END_EVENT:
				if(!other_stack.empty()){
					MSG_CHMPRN("Found stop yaml document event in skipping event loop");
					if(!other_stack.add(yevent.type)){
						result = false;
					}
				}else if(!in_document){
					MSG_CHMPRN("Already stop yaml document event in loop, Thus stacks this event.");
					if(!other_stack.add(yevent.type)){
						result = false;
					}
				}else{
					MSG_CHMPRN("Stop yaml document event in loop");
					in_document = false;
				}
				break;

			case YAML_MAPPING_START_EVENT:
				if(!other_stack.empty()){
					MSG_CHMPRN("Found start yaml mapping event in skipping event loop");
					if(!other_stack.add(yevent.type)){
						result = false;
					}
				}else if(!in_stream){
					MSG_CHMPRN("Found start yaml mapping event before yaml stream event in loop, Thus stacks this event.");
					if(!other_stack.add(yevent.type)){
						result = false;
					}
				}else if(!in_document){
					MSG_CHMPRN("Found start yaml mapping event before yaml document event in loop, Thus stacks this event.");
					if(!other_stack.add(yevent.type)){
						result = false;
					}
				}else if(in_toplevel){
					MSG_CHMPRN("Already start yaml mapping event in loop, Thus stacks this event.");
					if(!other_stack.add(yevent.type)){
						result = false;
					}
				}else{
					MSG_CHMPRN("Start yaml mapping event in loop");
					in_toplevel = true;
				}
				break;

			case YAML_MAPPING_END_EVENT:
				if(!other_stack.empty()){
					MSG_CHMPRN("Found stop yaml mapping event in skipping event loop");
					if(!other_stack.add(yevent.type)){
						result = false;
					}
				}else if(!in_toplevel){
					MSG_CHMPRN("Already stop yaml mapping event in loop, Thus stacks this event.");
					if(!other_stack.add(yevent.type)){
						result = false;
					}
				}else{
					MSG_CHMPRN("Stop yaml mapping event in loop");
					in_toplevel = false;
				}
				break;

			case YAML_SEQUENCE_START_EVENT:
				// always stacking
				//
				if(!other_stack.empty()){
					MSG_CHMPRN("Found start yaml sequence event in skipping event loop");
					if(!other_stack.add(yevent.type)){
						result = false;
					}
				}else{
					MSG_CHMPRN("Found start yaml sequence event before top level event in loop, Thus stacks this event.");
					if(!other_stack.add(yevent.type)){
						result = false;
					}
				}
				break;

			case YAML_SEQUENCE_END_EVENT:
				// always stacking
				//
				if(!other_stack.empty()){
					MSG_CHMPRN("Found stop yaml sequence event in skipping event loop");
					if(!other_stack.add(yevent.type)){
						result = false;
					}
				}else{
					MSG_CHMPRN("Found stop yaml sequence event before top level event in loop, Thus stacks this event.");
					if(!other_stack.add(yevent.type)){
						result = false;
					}
				}
				break;

			case YAML_SCALAR_EVENT:
				if(!other_stack.empty()){
					MSG_CHMPRN("Got yaml scalar event in skipping event loop");
					if(!other_stack.add(yevent.type)){
						result = false;
					}
				}else if(!in_stream){
					MSG_CHMPRN("Got yaml scalar event before yaml stream event in loop, Thus stacks this event.");
					if(!other_stack.add(yevent.type)){
						result = false;
					}
				}else if(!in_document){
					MSG_CHMPRN("Got yaml scalar event before yaml document event in loop, Thus stacks this event.");
					if(!other_stack.add(yevent.type)){
						result = false;
					}
				}else if(!in_toplevel){
					MSG_CHMPRN("Got yaml scalar event before yaml mapping event in loop, Thus stacks this event.");
					if(!other_stack.add(yevent.type)){
						result = false;
					}
				}else{
					// Found Top Level Keywards, start to loading
					if(0 == strcasecmp(CFG_GLOBAL_SEC_STR, reinterpret_cast<const char*>(yevent.data.scalar.value))){
						if(is_set_global){
							MSG_CHMPRN("Got yaml scalar event in loop, but already loading %s top level. Thus stacks this event.", CFG_GLOBAL_SEC_STR);
							if(!other_stack.add(yevent.type)){
								result = false;
							}
						}else{
							// Load GLOBAL section
							if(!ChmYamlLoadConfigrationGlobalSec(yparser, chmcfginfo, ccvals, default_ctlport)){
								ERR_CHMPRN("Something error occured in loading %s section.", CFG_GLOBAL_SEC_STR);
								result = false;
							}
						}

					}else if(0 == strcasecmp(CFG_SVRNODE_SEC_STR, reinterpret_cast<const char*>(yevent.data.scalar.value))){
						if(is_set_svrnode){
							MSG_CHMPRN("Got yaml scalar event in loop, but already loading SVRNODE top level. Thus stacks this event.");
							if(!other_stack.add(yevent.type)){
								result = false;
							}
						}else{
							// Load SVRNODE section
							if(!ChmYamlLoadConfigrationSvrnodeSec(yparser, chmcfginfo, ccvals)){
								ERR_CHMPRN("Something error occured in loading %s section.", CFG_SVRNODE_SEC_STR);
								result = false;
							}
						}

					}else if(0 == strcasecmp(CFG_SLVNODE_SEC_STR, reinterpret_cast<const char*>(yevent.data.scalar.value))){
						if(is_set_slvnode){
							MSG_CHMPRN("Got yaml scalar event in loop, but already loading SLVNODE top level. Thus stacks this event.");
							if(!other_stack.add(yevent.type)){
								result = false;
							}
						}else{
							// Load SLVNODE section
							if(!ChmYamlLoadConfigrationSlvnodeSec(yparser, chmcfginfo, ccvals)){
								ERR_CHMPRN("Something error occured in loading %s section.", CFG_SLVNODE_SEC_STR);
								result = false;
							}
						}

					}else{
						MSG_CHMPRN("Got yaml scalar event in loop, but unknown keyward(%s) for top level target. Thus stacks this event.", reinterpret_cast<const char*>(yevent.data.scalar.value));
						if(!other_stack.add(yevent.type)){
							result = false;
						}
					}
				}
				break;

			case YAML_ALIAS_EVENT:
				// [TODO]
				// Now we do not supports alias(anchor) event.
				//
				MSG_CHMPRN("Got yaml alias(anchor) event in loop, but we does not support this event. Thus skip this event.");
				break;
		}

		// delete event
		is_loop = yevent.type != YAML_STREAM_END_EVENT;
		yaml_event_delete(&yevent);
	}

	if(result){
		// Re-check for server/slave mode by self control port number.
		//
		if(ccvals.is_server_by_ctlport){
			if(!ccvals.server_mode){
				WAN_CHMPRN("There is no \"%s\" in %s section, but self control port(%d) found in server list, so run server mode.", INICFG_MODE_STR, INICFG_GLOBAL_SEC_STR, chmcfginfo.self_ctlport);
				chmcfginfo.is_server_mode = true;
			}else{
				if(!chmcfginfo.is_server_mode){
					ERR_CHMPRN("Found \"%s\" as slave mode in %s section, but self control port(%d) found in server list.", INICFG_MODE_STR, INICFG_GLOBAL_SEC_STR, chmcfginfo.self_ctlport);
					result = false;
				}
			}
		}else{
			if(!ccvals.server_mode){
				WAN_CHMPRN("There is no \"%s\" in %s section, but self control port(%d) found in slave list, so run slave mode.", INICFG_MODE_STR, INICFG_GLOBAL_SEC_STR, chmcfginfo.self_ctlport);
				chmcfginfo.is_server_mode = false;
			}else{
				if(chmcfginfo.is_server_mode){
					ERR_CHMPRN("Found \"%s\" as server mode in %s section, but self control port(%d) not found in server list.", INICFG_MODE_STR, INICFG_GLOBAL_SEC_STR, chmcfginfo.self_ctlport);
					result = false;
				}
			}
		}

		// check merge flags
		if(chmcfginfo.is_random_mode){
			if(!chmcfginfo.is_auto_merge){
				WAN_CHMPRN("Found %s=%s and %s=OFF. These options can not be specified at the same time, so SET %s=ON.", INICFG_DELIVERMODE_STR, INICFG_DELIVERMODE_RANDOM_STR, INICFG_AUTOMERGE_STR, INICFG_AUTOMERGE_STR);
				chmcfginfo.is_auto_merge = true;
			}
			if(chmcfginfo.is_do_merge){
				WAN_CHMPRN("Found %s=%s and %s=ON. These options can not be specified at the same time, so SET %s=OFF.", INICFG_DELIVERMODE_STR, INICFG_DELIVERMODE_RANDOM_STR, INICFG_DOMERGE_STR, INICFG_DOMERGE_STR);
				chmcfginfo.is_do_merge = false;
			}
		}else{
			if(!chmcfginfo.is_do_merge && chmcfginfo.is_auto_merge){
				WAN_CHMPRN("Found %s=OFF and %s=ON on %s=%s. These options can not be specified at the same time, so SET %s=OFF.", INICFG_DOMERGE_STR, INICFG_AUTOMERGE_STR, INICFG_DELIVERMODE_STR, INICFG_DELIVERMODE_HASH_STR, INICFG_AUTOMERGE_STR);
				chmcfginfo.is_auto_merge = false;
			}
		}
	}
	return result;
}

//---------------------------------------------------------
// CHMYamlBaseConf Class
//---------------------------------------------------------
CHMYamlBaseConf::CHMYamlBaseConf(int eventqfd, ChmCntrl* pcntrl, const char* file, short ctlport, const char* pJson) : CHMConf(eventqfd, pcntrl, file, ctlport, pJson)
{
}

CHMYamlBaseConf::~CHMYamlBaseConf()
{
}

bool CHMYamlBaseConf::LoadConfigration(CHMCFGINFO& chmcfginfo) const
{
	if(CONF_JSON != type && CONF_JSON_STR != type && CONF_YAML != type){
		ERR_CHMPRN("Class type(%d) does not JSON/JSON_STR/YAML.", type);
		return false;
	}
	if(CONF_JSON == type || CONF_YAML == type){
		if(cfgfile.empty()){
			ERR_CHMPRN("Configuration file path is not set.");
			return false;
		}
	}else{	// JSON_STR
		if(strjson.empty()){
			ERR_CHMPRN("JSON string is not set.");
			return false;
		}
	}

	// initialize yaml parser
	yaml_parser_t	yparser;
	if(!yaml_parser_initialize(&yparser)){
		ERR_CHMPRN("Failed to initialize yaml parser");
		return false;
	}

	FILE*	fp = NULL;
	if(CONF_JSON == type || CONF_YAML == type){
		// open configuration file
		if(NULL == (fp = fopen(cfgfile.c_str(), "r"))){
			ERR_CHMPRN("Could not open configuration file(%s). errno = %d", cfgfile.c_str(), errno);
			return false;
		}

		// set file to parser
		yaml_parser_set_input_file(&yparser, fp);

	}else{	// JSON_STR
		// set string to parser
		yaml_parser_set_input_string(&yparser, reinterpret_cast<const unsigned char*>(strjson.c_str()), strjson.length());
	}

	// Do parsing
	bool	result = ChmYamlLoadConfigrationTopLevel(yparser, chmcfginfo, ctlport_param);

	yaml_parser_delete(&yparser);
	if(fp){
		fclose(fp);
	}
	return result;
}

//---------------------------------------------------------
// CHMJsonConf Class
//---------------------------------------------------------
CHMJsonConf::CHMJsonConf(int eventqfd, ChmCntrl* pcntrl, const char* file, short ctlport) : CHMYamlBaseConf(eventqfd, pcntrl, file, ctlport, NULL)
{
	type = CHMConf::CONF_JSON;
}

CHMJsonConf::~CHMJsonConf()
{
}

//---------------------------------------------------------
// CHMJsonStringConf Class
//---------------------------------------------------------
CHMJsonStringConf::CHMJsonStringConf(int eventqfd, ChmCntrl* pcntrl, const char* pJson, short ctlport) : CHMYamlBaseConf(eventqfd, pcntrl, NULL, ctlport, pJson)
{
	type = CHMConf::CONF_JSON_STR;		// default
}

CHMJsonStringConf::~CHMJsonStringConf()
{
}

//---------------------------------------------------------
// CHMYamlConf Class
//---------------------------------------------------------
CHMYamlConf::CHMYamlConf(int eventqfd, ChmCntrl* pcntrl, const char* file, short ctlport) : CHMYamlBaseConf(eventqfd, pcntrl, file, ctlport, NULL)
{
	type = CHMConf::CONF_YAML;
}

CHMYamlConf::~CHMYamlConf()
{
}

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
