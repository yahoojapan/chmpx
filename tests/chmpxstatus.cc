/*
 * CHMPX
 *
 * Copyright 2016 Yahoo! JAPAN corporation.
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
 * CREATE:   Fri Sep 2 2016
 * REVISION:
 *
 */
#include <stdlib.h>
#include <stdio.h>

#include "chmcommon.h"
#include "chmstructure.h"
#include "chmutil.h"
#include "chmdbg.h"
#include "chmcntrl.h"
#include "chmopts.h"

using namespace std;

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
	char	szBuff[32];
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
	PRN("Usage: %s -conf <configration file path> [-ctlport <port>] [-self] [-d [slient|err|wan|msg|dump]] [-dfile <debug file path>]", progname ? programname(progname) : "program");
	PRN("Usage: %s -h", progname ? programname(progname) : "program");
	PRN("");
	PRN("Option");
	PRN("  -conf <file name>    configuration file( .ini / .json / .yaml ) path");
	PRN("  -json <json>         configuration JSON string\n");
	PRN("  -ctlport <port>      specify the self contrl port(*)");
	PRN("  -self                print only self chmpx");
	PRN("  -d <param>           specify the debugging output mode:");
	PRN("                        silent - no output");
	PRN("                        err    - output error level");
	PRN("                        wan    - output warning level");
	PRN("                        msg    - output debug(message) level");
	PRN("                        dump   - output communication debug level");
	PRN("  -dfile <path>        specify the file path which is put output");
	PRN("  -h(help)             display this usage.");
	PRN("");
	PRN("(*) if ctlport option is specified, chmpx searches same ctlport in configuration");
	PRN("    file and ignores \"CTLPORT\" directive in \"GLOBAL\" section. and chmpx will");
	PRN("    start in the mode indicated by the server entry that has beed detected.");
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
	PRN("k2hash file path                             = %s",				pInfo->k2hashpath);
	PRN("k2hash full mapping                          = %s",				pInfo->pchminfo->k2h_fullmap ? "yes" : "no");
	PRN("k2hash mask bit count                        = %d",				pInfo->pchminfo->k2h_mask_bitcnt);
	PRN("k2hash collision mask bit count              = %d",				pInfo->pchminfo->k2h_cmask_bitcnt);
	PRN("k2hash maximum element count                 = %d",				pInfo->pchminfo->k2h_max_element);
	PRN("random mode                                  = %s",				pInfo->pchminfo->is_random_deliver ? "yes" : "no");
	PRN("communication history count                  = %ld",				pInfo->pchminfo->histlog_count);
	PRN("auto merge                                   = %s",				pInfo->pchminfo->is_auto_merge ? "yes" : "no");
	PRN("merge processing(do merge)                   = %s",				pInfo->pchminfo->is_do_merge ? "yes" :"no");
	PRN("timeout for merge                            = %zd (s)",			pInfo->pchminfo->timeout_merge);
	PRN("thread count for socket                      = %d",				pInfo->pchminfo->evsock_thread_cnt);
	PRN("thread count for MQ                          = %d",				pInfo->pchminfo->evmq_thread_cnt);
	PRN("maximum socket count per chmpx(socket pool)  = %d",				pInfo->pchminfo->max_sock_pool);
	PRN("timeout for socket pool                      = %zd (s)",			pInfo->pchminfo->sock_pool_timeout);
	PRN("retry count on socket                        = %d",				pInfo->pchminfo->sock_retrycnt);
	PRN("timeout for send/recieve on socket           = %ld (us)",			pInfo->pchminfo->timeout_wait_socket);
	PRN("timeout for connect on socket                = %ld (us)",			pInfo->pchminfo->timeout_wait_connect);
	PRN("timeout for send/recieve on socket           = %ld (us)",			pInfo->pchminfo->timeout_wait_mq);
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
	PRN("configration file version                    = %ld",				pInfo->pchminfo->chmpx_man.cfg_revision);
	PRN("configration file date                       = %zu (unix time)",	pInfo->pchminfo->chmpx_man.cfg_date);
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
	PRN("joining client proces = {");
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
// Main
//---------------------------------------------------------
int main(int argc, char** argv)
{
	// parse parameters
	ChmOpts	opts((argc - 1), &argv[1]);

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

	// only self
	bool	is_only_self = opts.Find("self");

	// Initialize
	ChmCntrl	chmobj;
	if(!chmobj.OnlyAttachInitialize(config.c_str(), ctlport)){
		ERR("Could not initialize(attach) chmpx shared memory.");
		PRN("You can see detail about error, execute with \"-d\"(\"-g\") option.");
		exit(EXIT_FAILURE);
	}

	// Main processing
	bool	result;
	if(!is_only_self){
		result = PrintAllInfo(&chmobj);
	}else{
		result = PrintSelfInfo(&chmobj);
	}
	chmobj.Clean();

	exit(result ? EXIT_SUCCESS : EXIT_FAILURE);
}

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
