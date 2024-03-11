/*
 * CHMPX
 *
 * Copyright 2016 Yahoo Japan Corporation.
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
 * CREATE:   Fri Sep 2 2016
 * REVISION:
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <libgen.h>

#include "chmcommon.h"
#include "chmstructure.h"
#include "chmutil.h"
#include "chmdbg.h"
#include "chmcntrl.h"
#include "chmopts.h"

using namespace std;

//---------------------------------------------------------
// Symbols
//---------------------------------------------------------
#define	CHMPXSTATUS_LIVE_UP_STR			"UP"
#define	CHMPXSTATUS_LIVE_DOWN_STR		"DOWN"
#define	CHMPXSTATUS_RING_SVRIN_STR		"SERVICEIN"
#define	CHMPXSTATUS_RING_SVROUT_STR		"SERVICEOUT"
#define	CHMPXSTATUS_RING_SLAVE_STR		"SLAVE"

#define	CHMPXSTATUS_WAIT_INTERVAL		1

//---------------------------------------------------------
// Utility Functions
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

static inline void ERR(const char* format, ...)
{
	fprintf(stderr, "[ERR] ");
	if(format){
		va_list ap;
		va_start(ap, format);
		vfprintf(stderr, format, ap); 
		va_end(ap);
	}
	fprintf(stderr, "\n");
}

static inline char* programname(char* prgpath)
{
	if(!prgpath){
		return NULL;
	}
	char*	pprgname = basename(prgpath);
	if(0 == strncmp(pprgname, "lt-", strlen("lt-"))){
		pprgname = &pprgname[strlen("lt-")];
	}
	return pprgname;
}

inline std::string PRN_TIMESPEC(const timespec& ts)
{
	char	szBuff[64];
	string	strResult;

	if(0 < ts.tv_sec){
		sprintf(szBuff, "%zus ", ts.tv_sec);
		strResult += szBuff;
	}
	sprintf(szBuff, "%ldms ", (ts.tv_nsec % (1000 * 1000)));
	strResult += szBuff;

	sprintf(szBuff, "%ldus ", ((ts.tv_nsec % (1000 * 1000)) / 1000));
	strResult += szBuff;

	sprintf(szBuff, "%ldns ", (ts.tv_nsec % 1000));
	strResult += szBuff;

	if(0 < ts.tv_sec){
		sprintf(szBuff, "(%zu%09ldns)", ts.tv_sec, ts.tv_nsec);
	}else{
		sprintf(szBuff, "(%ldns)", ts.tv_nsec);
	}
	strResult += szBuff;

	return strResult;
}

//---------------------------------------------------------
// Utility Functions
//---------------------------------------------------------
static void Help(char* progname)
{
	PRN("");
	PRN("Usage: %s -conf <configuration file path> [-ctlport <port>] [-cuk <cuk>] [-self] [-d [silent|err|wan|msg|dump]] [-dfile <debug file path>]", progname ? programname(progname) : "program");
	PRN("Usage: %s -conf <configuration file path> [-ctlport <port>] [-cuk <cuk>] -wait -live [down|up] -ring [serviceout|servicein|slave] -(no)suspend [-timeout <second>] [-d [silent|err|wan|msg|dump]] [-dfile <debug file path>]", progname ? programname(progname) : "program");
	PRN("Usage: %s -h", progname ? programname(progname) : "program");
	PRN("");
	PRN("Option");
	PRN("  -conf <file name>    configuration file( .ini / .json / .yaml ) path");
	PRN("  -json <json>         configuration JSON string\n");
	PRN("  -ctlport <port>      specify the self control port(*)");
	PRN("  -cuk <cuk>           specify the self CUK");
	PRN("  -self                print only self chmpx");
	PRN("  -wait                to wait until the state changes to the specified value");
	PRN("  -live <param>        Specify live status by waiting mode");
	PRN("                        up         - chmpx process up");
	PRN("                        down       - chmpx process down");
	PRN("  -ring <param>        specify ring status by waiting mode");
	PRN("                        servicein  - server chmpx joined ring");
	PRN("                        serviceout - server chmpx NOT joined ring");
	PRN("                        slave      - slave chmpx");
	PRN("  -suspend             specify suspend status by waiting mode");
	PRN("  -nosuspend           specify nosuspend status by waiting mode");
	PRN("  -d <param>           specify the debugging output mode:");
	PRN("                        silent     - no output");
	PRN("                        err        - output error level");
	PRN("                        wan        - output warning level");
	PRN("                        msg        - output debug(message) level");
	PRN("                        dump       - output communication debug level");
	PRN("  -dfile <path>        specify the file path which is put output");
	PRN("  -h(help)             display this usage.");
	PRN("");
	PRN("(*) if ctlport option is specified, chmpx searches same ctlport in configuration");
	PRN("    file and ignores \"CTLPORT\" directive in \"GLOBAL\" section. and chmpx will");
	PRN("    start in the mode indicated by the server entry that has been detected.");
	PRN("");
}

//---------------------------------------------------------
// Print
//---------------------------------------------------------
static bool PrintAllInfo(ChmCntrl* pchmobj)
{
	if(!pchmobj){
		return false;
	}
	// Get information
	PCHMINFOEX	pInfo = pchmobj->DupAllChmInfo();
	if(!pInfo){
		ERR("Something error occurred in getting information.");
		return false;
	}
	if(!pInfo->pchminfo){
		ERR("Something error occurred in getting information.");
		ChmCntrl::FreeDupAllChmInfo(pInfo);
		return false;
	}

	int				counter;
	PCHMPXLIST		pchmpxlist;
	PMQMSGHEADLIST	pmsglisttmp;
	PCLTPROCLIST	pproclist;

	// chmpx info & info ex
	PRN("chmpx process id                             = %d",				pInfo->pchminfo->pid);
	PRN("process start time                           = %zu (unix time)",	pInfo->pchminfo->start_time);
	PRN("chmpx shared memory file path                = %s",				pInfo->shmpath);
	PRN("chmpx shared memory file size                = %zu (byte)",		pInfo->shmsize);
	PRN("chmpx info structure version                 = %s",				pInfo->pchminfo->chminfo_version);
	PRN("chmpx info structure size                    = %zu (byte)",		pInfo->pchminfo->chminfo_size);
	PRN("k2hash file path                             = %s",				pInfo->k2hashpath);
	PRN("k2hash full mapping                          = %s",				pInfo->pchminfo->k2h_fullmap ? "yes" : "no");
	PRN("k2hash mask bit count                        = %d",				pInfo->pchminfo->k2h_mask_bitcnt);
	PRN("k2hash collision mask bit count              = %d",				pInfo->pchminfo->k2h_cmask_bitcnt);
	PRN("k2hash maximum element count                 = %d",				pInfo->pchminfo->k2h_max_element);
	PRN("random mode                                  = %s",				pInfo->pchminfo->is_random_deliver ? "yes" : "no");
	PRN("communication history count                  = %ld",				pInfo->pchminfo->histlog_count);
	PRN("auto merge                                   = %s",				pInfo->pchminfo->is_auto_merge ? "yes" : "no");
	PRN("suspend auto merge                           = %s",				pInfo->pchminfo->is_auto_merge_suspend ? "yes" : "no");
	PRN("merge processing(do merge)                   = %s",				pInfo->pchminfo->is_do_merge ? "yes" :"no");
	PRN("SSL/TLS minimum version                      = %s",				CHM_GET_STR_SSLTLS_VERSION(pInfo->pchminfo->ssl_min_ver));
	PRN("NSSDB directory path                         = %s",				pInfo->pchminfo->nssdb_dir);
	PRN("timeout for merge                            = %zd (s)",			pInfo->pchminfo->timeout_merge);
	PRN("thread count for socket                      = %d",				pInfo->pchminfo->evsock_thread_cnt);
	PRN("thread count for MQ                          = %d",				pInfo->pchminfo->evmq_thread_cnt);
	PRN("maximum socket count per chmpx(socket pool)  = %d",				pInfo->pchminfo->max_sock_pool);
	PRN("timeout for socket pool                      = %zd (s)",			pInfo->pchminfo->sock_pool_timeout);
	PRN("retry count on socket                        = %d",				pInfo->pchminfo->sock_retrycnt);
	PRN("timeout for send/receive on socket           = %ld (us)",			pInfo->pchminfo->timeout_wait_socket);
	PRN("timeout for connect on socket                = %ld (us)",			pInfo->pchminfo->timeout_wait_connect);
	PRN("timeout for send/receive on socket           = %ld (us)",			pInfo->pchminfo->timeout_wait_mq);
	PRN("maximum MQ count                             = %ld",				pInfo->pchminfo->max_mqueue);
	PRN("chmpx process MQ count                       = %ld",				pInfo->pchminfo->chmpx_mqueue);
	PRN("maximum queue per chmpx process MQ           = %ld",				pInfo->pchminfo->max_q_per_chmpxmq);
	PRN("maximum queue per client process MQ          = %ld",				pInfo->pchminfo->max_q_per_cltmq);
	PRN("maximum MQ per client process                = %ld",				pInfo->pchminfo->max_mq_per_client);
	PRN("maximum MQ per attach                        = %ld",				pInfo->pchminfo->mq_per_attach);
	PRN("using MQ ACK                                 = %s",				pInfo->pchminfo->mq_ack ? "yes" : "no");
	PRN("retry count on MQ                            = %d",				pInfo->pchminfo->mq_retrycnt);
	PRN("base msgid                                   = 0x%016" PRIx64 ,	pInfo->pchminfo->base_msgid);
	PRN("");

	// chmpx info & info ex --> chmpx manager
	PRN("chmpx name                                   = %s",				pInfo->pchminfo->chmpx_man.group);
	PRN("configuration file version                   = %ld",				pInfo->pchminfo->chmpx_man.cfg_revision);
	PRN("configuration file date                      = %zu (unix time)",	pInfo->pchminfo->chmpx_man.cfg_date);
	PRN("replication count                            = %ld",				pInfo->pchminfo->chmpx_man.replica_count);
	PRN("maximum chmpx count                          = %ld",				pInfo->pchminfo->chmpx_man.chmpx_count);
	PRN("base hash count                              = %ld",				pInfo->pchminfo->chmpx_man.chmpx_bhash_count);
	PRN("now operating                                = %s",				pInfo->pchminfo->chmpx_man.is_operating ? "yes" : "no");
	PRN("last using random chmpxid                    = 0x%016" PRIx64 ,	pInfo->pchminfo->chmpx_man.last_chmpxid);

	// chmpx info & info ex --> chmpx manager --> self chmpx
	if(pInfo->pchminfo->chmpx_man.chmpx_self){
		PRN("self chmpx = {");
		PRN("  chmpxid                                    = 0x%016" PRIx64 ,pInfo->pchminfo->chmpx_man.chmpx_self->chmpx.chmpxid);
		PRN("  hostname                                   = %s",			pInfo->pchminfo->chmpx_man.chmpx_self->chmpx.name);
		PRN("  mode                                       = %s",			STRCHMPXMODE(pInfo->pchminfo->chmpx_man.chmpx_self->chmpx.mode));
		PRN("  base hash                                  = 0x%016" PRIx64 ,pInfo->pchminfo->chmpx_man.chmpx_self->chmpx.base_hash);
		PRN("  pending hash                               = 0x%016" PRIx64 ,pInfo->pchminfo->chmpx_man.chmpx_self->chmpx.pending_hash);
		PRN("  port                                       = %d",			pInfo->pchminfo->chmpx_man.chmpx_self->chmpx.port);
		PRN("  control port                               = %d",			pInfo->pchminfo->chmpx_man.chmpx_self->chmpx.ctlport);
		PRN("  cuk                                        = %s",			pInfo->pchminfo->chmpx_man.chmpx_self->chmpx.cuk);
		PRN("  custom id seed                             = %s",			pInfo->pchminfo->chmpx_man.chmpx_self->chmpx.custom_seed);
		PRN("  endpoints                                  = %s",			get_hostport_pairs_string(pInfo->pchminfo->chmpx_man.chmpx_self->chmpx.endpoints, EXTERNAL_EP_MAX).c_str());
		PRN("  control endpoints                          = %s",			get_hostport_pairs_string(pInfo->pchminfo->chmpx_man.chmpx_self->chmpx.ctlendpoints, EXTERNAL_EP_MAX).c_str());
		PRN("  forward peers                              = %s",			get_hostport_pairs_string(pInfo->pchminfo->chmpx_man.chmpx_self->chmpx.forward_peers, FORWARD_PEER_MAX).c_str());
		PRN("  reverse peers                              = %s",			get_hostport_pairs_string(pInfo->pchminfo->chmpx_man.chmpx_self->chmpx.reverse_peers, REVERSE_PEER_MAX).c_str());
		PRN("  ssl                                        = %s",			pInfo->pchminfo->chmpx_man.chmpx_self->chmpx.is_ssl ? "yes" : "no");

		if(pInfo->pchminfo->chmpx_man.chmpx_self->chmpx.is_ssl){
			PRN("  verify client peer                         = %s",		pInfo->pchminfo->chmpx_man.chmpx_self->chmpx.verify_peer ? "yes" : "no");
			PRN("  CA path is                                 = %s",		pInfo->pchminfo->chmpx_man.chmpx_self->chmpx.is_ca_file ? "file" : "directory");
			PRN("  CA path                                    = %s",		pInfo->pchminfo->chmpx_man.chmpx_self->chmpx.capath);
			PRN("  server cert path                           = %s",		pInfo->pchminfo->chmpx_man.chmpx_self->chmpx.server_cert);
			PRN("  server private key path                    = %s",		pInfo->pchminfo->chmpx_man.chmpx_self->chmpx.server_prikey);
			PRN("  slave cert path                            = %s",		pInfo->pchminfo->chmpx_man.chmpx_self->chmpx.slave_cert);
			PRN("  slave private key path                     = %s",		pInfo->pchminfo->chmpx_man.chmpx_self->chmpx.slave_prikey);
		}

		string	socks;
		for(PCHMSOCKLIST psocklist = pInfo->pchminfo->chmpx_man.chmpx_self->chmpx.socklist; psocklist; psocklist = psocklist->next){
			char	szBuff[32];
			sprintf(szBuff, "%d", psocklist->sock);
			if(!socks.empty()){
				socks += ",";
			}
			socks += szBuff;
		}
		PRN("  sockets                                    = %s",			socks.c_str());
		PRN("  listen socket                              = %d",			pInfo->pchminfo->chmpx_man.chmpx_self->chmpx.selfsock);
		PRN("  control socket                             = %d",			pInfo->pchminfo->chmpx_man.chmpx_self->chmpx.ctlsock);
		PRN("  listen control socket                      = %d",			pInfo->pchminfo->chmpx_man.chmpx_self->chmpx.selfctlsock);
		PRN("  last status update time                    = %zu (unix time)",	pInfo->pchminfo->chmpx_man.chmpx_self->chmpx.last_status_time);
		PRN("  status                                     = %s",			STR_CHMPXSTS_FULL(pInfo->pchminfo->chmpx_man.chmpx_self->chmpx.status).c_str());
		PRN("}");
	}

	// chmpx info & info ex --> chmpx manager --> server chmpxs
	PRN("server chmpxs [ %ld ] = {",										pInfo->pchminfo->chmpx_man.chmpx_server_count);
	for(counter = 0, pchmpxlist = pInfo->pchminfo->chmpx_man.chmpx_servers; pchmpxlist; pchmpxlist = pchmpxlist->next, ++counter){
		PRN("  [%d] = {",													counter);
		PRN("    chmpxid                                  = 0x%016" PRIx64 ,pchmpxlist->chmpx.chmpxid);
		PRN("    hostname                                 = %s",			pchmpxlist->chmpx.name);
		PRN("    mode                                     = %s",			STRCHMPXMODE(pchmpxlist->chmpx.mode));
		PRN("    base hash                                = 0x%016" PRIx64 ,pchmpxlist->chmpx.base_hash);
		PRN("    pending hash                             = 0x%016" PRIx64 ,pchmpxlist->chmpx.pending_hash);
		PRN("    port                                     = %d",			pchmpxlist->chmpx.port);
		PRN("    control port                             = %d",			pchmpxlist->chmpx.ctlport);
		PRN("    cuk                                      = %s",			pchmpxlist->chmpx.cuk);
		PRN("    custom id seed                           = %s",			pchmpxlist->chmpx.custom_seed);
		PRN("    endpoints                                = %s",			get_hostport_pairs_string(pchmpxlist->chmpx.endpoints, EXTERNAL_EP_MAX).c_str());
		PRN("    control endpoints                        = %s",			get_hostport_pairs_string(pchmpxlist->chmpx.ctlendpoints, EXTERNAL_EP_MAX).c_str());
		PRN("    forward peers                            = %s",			get_hostport_pairs_string(pchmpxlist->chmpx.forward_peers, FORWARD_PEER_MAX).c_str());
		PRN("    reverse peers                            = %s",			get_hostport_pairs_string(pchmpxlist->chmpx.reverse_peers, REVERSE_PEER_MAX).c_str());
		PRN("    ssl                                      = %s",			pchmpxlist->chmpx.is_ssl ? "yes" : "no");

		if(pchmpxlist->chmpx.is_ssl){
			PRN("    verify client peer                       = %s",		pchmpxlist->chmpx.verify_peer ? "yes" : "no");
			PRN("    CA path is                               = %s",		pchmpxlist->chmpx.is_ca_file ? "file" : "directory");
			PRN("    CA path                                  = %s",		pchmpxlist->chmpx.capath);
			PRN("    server cert path                         = %s",		pchmpxlist->chmpx.server_cert);
			PRN("    server private key path                  = %s",		pchmpxlist->chmpx.server_prikey);
			PRN("    slave cert path                          = %s",		pchmpxlist->chmpx.slave_cert);
			PRN("    slave private key path                   = %s",		pchmpxlist->chmpx.slave_prikey);
		}

		string	socks;
		for(PCHMSOCKLIST psocklist = pchmpxlist->chmpx.socklist; psocklist; psocklist = psocklist->next){
			char	szBuff[32];
			sprintf(szBuff, "%d", psocklist->sock);
			if(!socks.empty()){
				socks += ",";
			}
			socks += szBuff;
		}
		PRN("    sockets                                  = %s",			socks.c_str());
		PRN("    listen socket                            = %d",			pchmpxlist->chmpx.selfsock);
		PRN("    control socket                           = %d",			pchmpxlist->chmpx.ctlsock);
		PRN("    listen control socket                    = %d",			pchmpxlist->chmpx.selfctlsock);
		PRN("    last status update time                  = %zu (unix time)",	pchmpxlist->chmpx.last_status_time);
		PRN("    status                                   = %s",			STR_CHMPXSTS_FULL(pchmpxlist->chmpx.status).c_str());
		PRN("  }");
	}
	PRN("}");

	// chmpx info & info ex --> chmpx manager --> slave chmpxs
	PRN("slave chmpxs [ %ld ] = {",											pInfo->pchminfo->chmpx_man.chmpx_slave_count);
	for(counter = 0, pchmpxlist = pInfo->pchminfo->chmpx_man.chmpx_slaves; pchmpxlist; pchmpxlist = pchmpxlist->next, ++counter){
		PRN("  [%d] = {",													counter);
		PRN("    chmpxid                                  = 0x%016" PRIx64 ,pchmpxlist->chmpx.chmpxid);
		PRN("    hostname                                 = %s",			pchmpxlist->chmpx.name);
		PRN("    mode                                     = %s",			STRCHMPXMODE(pchmpxlist->chmpx.mode));
		PRN("    base hash                                = 0x%016" PRIx64 ,pchmpxlist->chmpx.base_hash);
		PRN("    pending hash                             = 0x%016" PRIx64 ,pchmpxlist->chmpx.pending_hash);
		PRN("    port                                     = %d",			pchmpxlist->chmpx.port);
		PRN("    control port                             = %d",			pchmpxlist->chmpx.ctlport);
		PRN("    cuk                                      = %s",			pchmpxlist->chmpx.cuk);
		PRN("    custom id seed                           = %s",			pchmpxlist->chmpx.custom_seed);
		PRN("    endpoints                                = %s",			get_hostport_pairs_string(pchmpxlist->chmpx.endpoints, EXTERNAL_EP_MAX).c_str());
		PRN("    control endpoints                        = %s",			get_hostport_pairs_string(pchmpxlist->chmpx.ctlendpoints, EXTERNAL_EP_MAX).c_str());
		PRN("    forward peers                            = %s",			get_hostport_pairs_string(pchmpxlist->chmpx.forward_peers, FORWARD_PEER_MAX).c_str());
		PRN("    reverse peers                            = %s",			get_hostport_pairs_string(pchmpxlist->chmpx.reverse_peers, REVERSE_PEER_MAX).c_str());
		PRN("    ssl                                      = %s",			pchmpxlist->chmpx.is_ssl ? "yes" : "no");

		if(pchmpxlist->chmpx.is_ssl){
			PRN("    verify client peer                       = %s",		pchmpxlist->chmpx.verify_peer ? "yes" : "no");
			PRN("    CA path is                               = %s",		pchmpxlist->chmpx.is_ca_file ? "file" : "directory");
			PRN("    CA path                                  = %s",		pchmpxlist->chmpx.capath);
			PRN("    server cert path                         = %s",		pchmpxlist->chmpx.server_cert);
			PRN("    server private key path                  = %s",		pchmpxlist->chmpx.server_prikey);
			PRN("    slave cert path                          = %s",		pchmpxlist->chmpx.slave_cert);
			PRN("    slave private key path                   = %s",		pchmpxlist->chmpx.slave_prikey);
		}

		string	socks;
		for(PCHMSOCKLIST psocklist = pchmpxlist->chmpx.socklist; psocklist; psocklist = psocklist->next){
			char	szBuff[32];
			sprintf(szBuff, "%d", psocklist->sock);
			if(!socks.empty()){
				socks += ",";
			}
			socks += szBuff;
		}
		PRN("    sockets                                  = %s",			socks.c_str());
		PRN("    listen socket                            = %d",			pchmpxlist->chmpx.selfsock);
		PRN("    control socket                           = %d",			pchmpxlist->chmpx.ctlsock);
		PRN("    listen control socket                    = %d",			pchmpxlist->chmpx.selfctlsock);
		PRN("    last status update time                  = %zu (unix time)",	pchmpxlist->chmpx.last_status_time);
		PRN("    status                                   = %s",			STR_CHMPXSTS_FULL(pchmpxlist->chmpx.status).c_str());
		PRN("  }");
	}
	PRN("}");

	// chmpx info & info ex --> chmpx manager --> stat
	PRN("server stat = {");
	PRN("    total send message count                 = %ld",				pInfo->pchminfo->chmpx_man.server_stat.total_sent_count);
	PRN("    total receive message count              = %ld",				pInfo->pchminfo->chmpx_man.server_stat.total_received_count);
	PRN("    total body size                          = %zu (bytes)",		pInfo->pchminfo->chmpx_man.server_stat.total_body_bytes);
	PRN("    minimum body size                        = %zu (bytes)",		pInfo->pchminfo->chmpx_man.server_stat.min_body_bytes);
	PRN("    maximum body size                        = %zu (bytes)",		pInfo->pchminfo->chmpx_man.server_stat.max_body_bytes);
	PRN("    total elapsed time                       = %s",				PRN_TIMESPEC(pInfo->pchminfo->chmpx_man.server_stat.total_elapsed_time).c_str());
	PRN("    minimum elapsed time                     = %s",				PRN_TIMESPEC(pInfo->pchminfo->chmpx_man.server_stat.min_elapsed_time).c_str());
	PRN("    maximum elapsed time                     = %s",				PRN_TIMESPEC(pInfo->pchminfo->chmpx_man.server_stat.max_elapsed_time).c_str());
	PRN("}");

	PRN("slave stat = {");
	PRN("    total send message count                 = %ld",				pInfo->pchminfo->chmpx_man.slave_stat.total_sent_count);
	PRN("    total receive message count              = %ld",				pInfo->pchminfo->chmpx_man.slave_stat.total_received_count);
	PRN("    total body size                          = %zu (bytes)",		pInfo->pchminfo->chmpx_man.slave_stat.total_body_bytes);
	PRN("    minimum body size                        = %zu (bytes)",		pInfo->pchminfo->chmpx_man.slave_stat.min_body_bytes);
	PRN("    maximum body size                        = %zu (bytes)",		pInfo->pchminfo->chmpx_man.slave_stat.max_body_bytes);
	PRN("    total elapsed time                       = %s",				PRN_TIMESPEC(pInfo->pchminfo->chmpx_man.slave_stat.total_elapsed_time).c_str());
	PRN("    minimum elapsed time                     = %s",				PRN_TIMESPEC(pInfo->pchminfo->chmpx_man.slave_stat.min_elapsed_time).c_str());
	PRN("    maximum elapsed time                     = %s",				PRN_TIMESPEC(pInfo->pchminfo->chmpx_man.slave_stat.max_elapsed_time).c_str());
	PRN("}");

	// chmpx info & info ex --> chmpx manager --> free chmpx/sock
	PRN("free chmpx area count                        = %ld",				pInfo->pchminfo->chmpx_man.chmpx_free_count);
	PRN("free sock area count                         = %ld",				pInfo->pchminfo->chmpx_man.sock_free_count);
	PRN("");

	// chmpx info & info ex --> mq list
	PRN("chmpx using MQ [ %ld ] = {",										pInfo->pchminfo->chmpx_msg_count);
	for(counter = 0, pmsglisttmp = pInfo->pchminfo->chmpx_msgs; pmsglisttmp; pmsglisttmp = pmsglisttmp->next, ++counter){
		PRN("  [%d] = {",													counter);
		PRN("    msgid                                    = 0x%016" PRIx64 ,pmsglisttmp->msghead.msgid);
		PRN("    flag                                     = %s%s%s",		STR_MQFLAG_ASSIGNED(pmsglisttmp->msghead.flag), STR_MQFLAG_KIND(pmsglisttmp->msghead.flag), STR_MQFLAG_ACTIVATED(pmsglisttmp->msghead.flag));
		PRN("    pid                                      = %d",			pmsglisttmp->msghead.pid);
		PRN("  }");
	}
	PRN("}");

	// chmpx info & info ex --> mq list
	PRN("client process using MQ [ %ld ] = {",								pInfo->pchminfo->activated_msg_count);
	for(counter = 0, pmsglisttmp = pInfo->pchminfo->activated_msgs; pmsglisttmp; pmsglisttmp = pmsglisttmp->next, ++counter){
		PRN("  [%d] = {",													counter);
		PRN("    msgid                                    = 0x%016" PRIx64 ,pmsglisttmp->msghead.msgid);
		PRN("    flag                                     = %s%s%s",		STR_MQFLAG_ASSIGNED(pmsglisttmp->msghead.flag), STR_MQFLAG_KIND(pmsglisttmp->msghead.flag), STR_MQFLAG_ACTIVATED(pmsglisttmp->msghead.flag));
		PRN("    pid                                      = %d",			pmsglisttmp->msghead.pid);
		PRN("  }");
	}
	PRN("}");

	// chmpx info & info ex --> mq list
	PRN("assigned MQ [ %ld ] = {",											pInfo->pchminfo->assigned_msg_count);
	for(counter = 0, pmsglisttmp = pInfo->pchminfo->assigned_msgs; pmsglisttmp; pmsglisttmp = pmsglisttmp->next, ++counter){
		PRN("  [%d] = {",													counter);
		PRN("    msgid                                    = 0x%016" PRIx64 ,pmsglisttmp->msghead.msgid);
		PRN("    flag                                     = %s%s%s",		STR_MQFLAG_ASSIGNED(pmsglisttmp->msghead.flag), STR_MQFLAG_KIND(pmsglisttmp->msghead.flag), STR_MQFLAG_ACTIVATED(pmsglisttmp->msghead.flag));
		PRN("    pid                                      = %d",			pmsglisttmp->msghead.pid);
		PRN("  }");
	}
	PRN("}");

	// chmpx info & info ex
	PRN("free MQ                                      = %ld",				pInfo->pchminfo->free_msg_count);
	PRN("last random msgid used by chmpx process      = 0x%016" PRIx64 ,	pInfo->pchminfo->last_msgid_chmpx);
	PRN("last activated msgid                         = 0x%016" PRIx64 ,	pInfo->pchminfo->last_msgid_activated);
	PRN("last assigned msgid                          = 0x%016" PRIx64 ,	pInfo->pchminfo->last_msgid_assigned);

	// chmpx info & info ex --> client process
	PRN("joining client process = {");
	for(counter = 0, pproclist = pInfo->pchminfo->client_pids; pproclist; pproclist = pproclist->next, ++counter){
		PRN("  [%d] pid                                   = %d",			counter, pproclist->pid);
	}
	PRN("}");

	ChmCntrl::FreeDupAllChmInfo(pInfo);

	return true;
}

static bool PrintSelfInfo(ChmCntrl* pchmobj)
{
	if(!pchmobj){
		return false;
	}
	// Get information
	PCHMPX	pInfo = pchmobj->DupSelfChmpxInfo();
	if(!pInfo){
		ERR("Something error occurred in getting information.");
		return false;
	}

	// chmpx info
	PRN("  chmpxid                                    = 0x%016" PRIx64 ,pInfo->chmpxid);
	PRN("  hostname                                   = %s",			pInfo->name);
	PRN("  mode                                       = %s",			STRCHMPXMODE(pInfo->mode));
	PRN("  base hash                                  = 0x%016" PRIx64 ,pInfo->base_hash);
	PRN("  pending hash                               = 0x%016" PRIx64 ,pInfo->pending_hash);
	PRN("  port                                       = %d",			pInfo->port);
	PRN("  control port                               = %d",			pInfo->ctlport);
	PRN("    cuk                                      = %s",			pInfo->cuk);
	PRN("    custom id seed                           = %s",			pInfo->custom_seed);
	PRN("    endpoints                                = %s",			get_hostport_pairs_string(pInfo->endpoints, EXTERNAL_EP_MAX).c_str());
	PRN("    control endpoints                        = %s",			get_hostport_pairs_string(pInfo->ctlendpoints, EXTERNAL_EP_MAX).c_str());
	PRN("    forward peers                            = %s",			get_hostport_pairs_string(pInfo->forward_peers, FORWARD_PEER_MAX).c_str());
	PRN("    reverse peers                            = %s",			get_hostport_pairs_string(pInfo->reverse_peers, REVERSE_PEER_MAX).c_str());
	PRN("  ssl                                        = %s",			pInfo->is_ssl ? "yes" : "no");

	if(pInfo->is_ssl){
		PRN("  verify client peer                         = %s",		pInfo->verify_peer ? "yes" : "no");
		PRN("  CA path is                                 = %s",		pInfo->is_ca_file ? "file" : "directory");
		PRN("  CA path                                    = %s",		pInfo->capath);
		PRN("  server cert path                           = %s",		pInfo->server_cert);
		PRN("  server private key path                    = %s",		pInfo->server_prikey);
		PRN("  slave cert path                            = %s",		pInfo->slave_cert);
		PRN("  slave private key path                     = %s",		pInfo->slave_prikey);
	}

	string	socks;
	for(PCHMSOCKLIST psocklist = pInfo->socklist; psocklist; psocklist = psocklist->next){
		char	szBuff[32];
		sprintf(szBuff, "%d", psocklist->sock);
		if(!socks.empty()){
			socks += ",";
		}
		socks += szBuff;
	}
	PRN("  sockets                                    = %s",			socks.c_str());
	PRN("  listen socket                              = %d",			pInfo->selfsock);
	PRN("  control socket                             = %d",			pInfo->ctlsock);
	PRN("  listen control socket                      = %d",			pInfo->selfctlsock);
	PRN("  last status update time                    = %zu (unix time)",	pInfo->last_status_time);
	PRN("  status                                     = %s",			STR_CHMPXSTS_FULL(pInfo->status).c_str());

	ChmCntrl::FreeDupSelfChmpxInfo(pInfo);

	return true;
}

//---------------------------------------------------------
// Wait up mode
//---------------------------------------------------------
static bool StatusWait(ChmCntrl* pchmobj, chmpxsts_t targetsts, time_t timeout)
{
	if(!pchmobj){
		return false;
	}
	string	strTargetsts = STR_CHMPXSTS_FULL(targetsts);

	// set timeout value
	bool			is_notimeout = (0L == timeout);
	struct timespec	sleeptime;
	SET_TIMESPEC(&sleeptime, CHMPXSTATUS_WAIT_INTERVAL, 0);

	int	cnt;
	for(cnt = 0; 0 < timeout || is_notimeout; ++cnt, timeout -= std::min(timeout, static_cast<time_t>(CHMPXSTATUS_WAIT_INTERVAL))){
		PCHMPX	pInfo;

		// Get information
		if(NULL == (pInfo = pchmobj->DupSelfChmpxInfo())){
			ERR("Something error occurred in getting information.");
			MSG_CHMPRN("[%d * %d(sec)] WAIT -> Could not set status info", cnt, CHMPXSTATUS_WAIT_INTERVAL);

		}else{
			// Compare status
			if(	(pInfo->status & CHMPXSTS_MASK_LIVE)	== (targetsts & CHMPXSTS_MASK_LIVE)		&&
				(pInfo->status & CHMPXSTS_MASK_RING)	== (targetsts & CHMPXSTS_MASK_RING)		&&
				(pInfo->status & CHMPXSTS_MASK_SUSPEND)	== (targetsts & CHMPXSTS_MASK_SUSPEND)	)
			{
				// Found!
				ChmCntrl::FreeDupSelfChmpxInfo(pInfo);
				MSG_CHMPRN("[%d * %d(sec)] BREAK WAITING -> Status(%s) is as same as %s", cnt, CHMPXSTATUS_WAIT_INTERVAL, STR_CHMPXSTS_FULL(pInfo->status).c_str(), strTargetsts.c_str());
				PRN("SUCCEED");
				return true;
			}

			// not same status, thus sleep
			ChmCntrl::FreeDupSelfChmpxInfo(pInfo);
			MSG_CHMPRN("[%d * %d(sec)] WAIT -> Status(%s) is not as same as %s", cnt, CHMPXSTATUS_WAIT_INTERVAL, STR_CHMPXSTS_FULL(pInfo->status).c_str(), strTargetsts.c_str());
		}
		// sleep
		nanosleep(&sleeptime, NULL);
	}
	MSG_CHMPRN("[%d * %d(sec)] TIMEOUT -> Status was not %s", cnt, CHMPXSTATUS_WAIT_INTERVAL, strTargetsts.c_str());
	PRN("FAILED");
	return false;
}

//---------------------------------------------------------
// Main
//---------------------------------------------------------
int main(int argc, char** argv)
{
	// parse parameters
	ChmOpts	opts((argc - 1), const_cast<const char**>(&argv[1]));

	// help
	if(opts.Find("h") || opts.Find("help")){
		Help(argv[0]);
		exit(EXIT_SUCCESS);
	}

	// DBG Mode
	{
		string	dbgfile;
		if(opts.Get("dfile", dbgfile) || opts.Get("gfile", dbgfile)){
			if(!SetChmDbgFile(dbgfile.c_str())){
				ERR("Could not set debugging log file(%s), but continue...", dbgfile.c_str());
			}
		}
		string	dbgmode;
		SetChmDbgMode(CHMDBG_SILENT);
		if(opts.Get("d", dbgmode) || opts.Get("g", dbgmode)){
			if(0 == strcasecmp(dbgmode.c_str(), "ERR") || 0 == strcasecmp(dbgmode.c_str(), "ERROR")){
				SetChmDbgMode(CHMDBG_ERR);
			}else if(0 == strcasecmp(dbgmode.c_str(), "WAN") || 0 == strcasecmp(dbgmode.c_str(), "WARNING")){
				SetChmDbgMode(CHMDBG_WARN);
			}else if(0 == strcasecmp(dbgmode.c_str(), "INF") || 0 == strcasecmp(dbgmode.c_str(), "INFO") || 0 == strcasecmp(dbgmode.c_str(), "MSG")){
				SetChmDbgMode(CHMDBG_MSG);
			}else if(0 == strcasecmp(dbgmode.c_str(), "DUMP")){
				SetChmDbgMode(CHMDBG_DUMP);
			}else{
				ERR("Wrong parameter value \"-d\"(\"-g\") %s.", dbgmode.c_str());
				exit(EXIT_FAILURE);
			}
		}
	}

	// configuration file or json
	string	config;
	if(!opts.Get("conf", config) && !opts.Get("f", config) && !opts.Get("json", config)){
		//PRN("There is no -conf and -json option, thus check environment automatically.");
	}

	// control port
	short	ctlport = CHM_INVALID_PORT;
	{
		string	strtmp;
		if(opts.Get("ctlport", strtmp) || opts.Get("cntlport", strtmp) || opts.Get("cntrlport", strtmp)){
			ctlport = static_cast<short>(atoi(strtmp.c_str()));
		}
	}

	// cuk
	string	strcuk;
	{
		if(!opts.Get("cuk", strcuk)){
			strcuk.clear();
		}
	}

	// Initialize
	ChmCntrl	chmobj;
	if(!chmobj.OnlyAttachInitialize(config.c_str(), ctlport, (strcuk.empty() ? NULL : strcuk.c_str()))){
		ERR("Could not initialize(attach) chmpx shared memory.");
		PRN("You can see detail about error, execute with \"-d\"(\"-g\") option.");
		exit(EXIT_FAILURE);
	}

	// check wait/print(self or all) mode
	bool	is_wait_mode = opts.Find("wait");
	bool	is_only_self = opts.Find("self");

	bool	result;
	if(is_wait_mode){
		// wait mode
		if(!opts.Find("live") || !opts.Find("ring") || (!opts.Find("nosuspend") && !opts.Find("suspend"))){
			ERR("wait mode(-wait) needs -live, -ring and -nosuspend(or -suspend) option.");
			chmobj.Clean();
			exit(EXIT_FAILURE);
		}
		string		strTmp;
		chmpxsts_t	targetsts = (CHMPXSTS_VAL_NOACT | CHMPXSTS_VAL_NOTHING);
		time_t		timeout;

		// live
		if(!opts.Get("live", strTmp)){
			ERR("\"-live\" option needs parameter(up or down).");
			chmobj.Clean();
			exit(EXIT_FAILURE);
		}else{
			if(0 == strcasecmp(CHMPXSTATUS_LIVE_UP_STR, strTmp.c_str())){
				targetsts |= CHMPXSTS_VAL_UP;
			}else if(0 == strcasecmp(CHMPXSTATUS_LIVE_DOWN_STR, strTmp.c_str())){
				targetsts |= CHMPXSTS_VAL_DOWN;
			}else{
				ERR("wrong parameter(%s) is specified for \"-live\" option.", strTmp.c_str());
				chmobj.Clean();
				exit(EXIT_FAILURE);
			}
		}
		// ring
		if(!opts.Get("ring", strTmp)){
			ERR("\"-ring\" option needs parameter(servicein or serviceout or slave).");
			chmobj.Clean();
			exit(EXIT_FAILURE);
		}else{
			if(0 == strcasecmp(CHMPXSTATUS_RING_SVRIN_STR, strTmp.c_str())){
				targetsts |= CHMPXSTS_VAL_SRVIN;
			}else if(0 == strcasecmp(CHMPXSTATUS_RING_SVROUT_STR, strTmp.c_str())){
				targetsts |= CHMPXSTS_VAL_SRVOUT;
			}else if(0 == strcasecmp(CHMPXSTATUS_RING_SLAVE_STR, strTmp.c_str())){
				targetsts |= CHMPXSTS_VAL_SLAVE;
			}else{
				ERR("wrong parameter(%s) is specified for \"-ring\" option.", strTmp.c_str());
				chmobj.Clean();
				exit(EXIT_FAILURE);
			}
		}
		// suspend or nosuspend
		if(opts.Find("nosuspend")){
			targetsts |= CHMPXSTS_VAL_NOSUP;
		}else{	// opts.Find("suspend")
			targetsts |= CHMPXSTS_VAL_SUSPEND;
		}
		// timeout
		if(opts.Find("timeout")){
			string	strTimeout;
			if(!opts.Get("timeout", strTimeout)){
				ERR("\"-timeout\" option specified for \"-wait\", but argument(second) is not specified.");
				chmobj.Clean();
				exit(EXIT_FAILURE);
			}
			timeout = static_cast<time_t>(atoi(strTimeout.c_str()));
		}else{
			timeout = 0L;
		}

		// do wait
		result = StatusWait(&chmobj, targetsts, timeout);

	}else{
		// print mode
		if(!is_only_self){
			result = PrintAllInfo(&chmobj);
		}else{
			result = PrintSelfInfo(&chmobj);
		}
	}
	chmobj.Clean();

	exit(result ? EXIT_SUCCESS : EXIT_FAILURE);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noexpandtab sw=4 ts=4 fdm=marker
 * vim<600: noexpandtab sw=4 ts=4
 */
