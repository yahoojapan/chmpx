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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/inotify.h>
#include <sys/epoll.h>
#include <signal.h>
#include <map>
#include <string>

#include "chmcommon.h"
#include "chmstructure.h"
#include "chmutil.h"
#include "chmdbg.h"
#include "chmconf.h"
#include "chmopts.h"

using namespace std;

//---------------------------------------------------------
// Functions
//---------------------------------------------------------
// Parse parameters
//
static void Help(char* progname)
{
	printf("Usage: %s [options]\n", progname ? progname : "program");
	printf("Option  -conf [file name]       Configuration file( .ini / .json / .yaml ) path\n");
	printf("        -json [json string]     Configuration JSON string\n");
	printf("        -print_default          print default datas\n");
	printf("        -d [debug level]        \"ERR\" / \"WAN\" / \"INF\"\n");
	printf("        -h                      display help\n");
}

static bool LoadConfTest(CHMConf* pconfobj)
{
	if(!pconfobj){
		ERR_CHMPRN("parameter error.");
		return false;
	}

	const CHMCFGINFO*	pchmcfginfo = pconfobj->GetConfiguration(true);
	if(!pchmcfginfo){
		ERR_CHMPRN("Failed load configuration.");
		return false;
	}

	// Dump
	printf("configuration{\n");
	printf("\tGROUP           = %s\n", pchmcfginfo->groupname.c_str());
	printf("\tREVISION        = %ld\n", pchmcfginfo->revision);
	printf("\tMAXCHMPX        = %ld\n", pchmcfginfo->max_chmpx_count);
	printf("\tREPLICA         = %ld\n", pchmcfginfo->replica_count);
	printf("\tMAXMQSERVER     = %ld\n", pchmcfginfo->max_server_mq_cnt);
	printf("\tMAXMQCLIENT     = %ld\n", pchmcfginfo->max_client_mq_cnt);
	printf("\tMQPERATTACH     = %ld\n", pchmcfginfo->mqcnt_per_attach);
	printf("\tMAXQPERSERVERMQ = %ld\n", pchmcfginfo->max_q_per_servermq);
	printf("\tMAXQPERCLIENTMQ = %ld\n", pchmcfginfo->max_q_per_clientmq);
	printf("\tMAXMQPERCLIENT  = %ld\n", pchmcfginfo->max_mq_per_client);
	printf("\tMAXHISTLOG      = %ld\n", pchmcfginfo->max_histlog_count);
	printf("\tDATE            = %jd\n", static_cast<intmax_t>(pchmcfginfo->date));

	int count = 1;
	chmnode_cfginfos_t::const_iterator	iter;
	for(iter = pchmcfginfo->servers.begin(); iter != pchmcfginfo->servers.end(); ++iter, ++count){
		printf("\tserver[%d]{\n", count);
		printf("\t\tNAME          = %s\n", iter->name.c_str());
		printf("\t\tPORT          = %d\n", iter->port);
		printf("\t\tCTLPORT       = %d\n", iter->ctlport);
		printf("\t\tSSL           = %s\n", iter->is_ssl ? "yes" : "no");
		if(iter->is_ssl){
			printf("\t\tVERIFY_PEER   = %s\n", iter->verify_peer ? "yes" : "no");
			printf("\t\tCA PATH TYPE  = %s\n", iter->is_ca_file ? "file" : "dir");
			printf("\t\tCA PATH       = %s\n", iter->capath.c_str());
			printf("\t\tSERVER CERT   = %s\n", iter->server_cert.c_str());
			printf("\t\tSERVER PRIKEY = %s\n", iter->server_prikey.c_str());
			printf("\t\tSLAVE CERT    = %s\n", iter->slave_cert.c_str());
			printf("\t\tSLAVE PRIKEY  = %s\n", iter->slave_prikey.c_str());
		}
		printf("\t}\n");
	}

	count = 1;
	for(iter = pchmcfginfo->slaves.begin(); iter != pchmcfginfo->slaves.end(); ++iter, ++count){
		printf("\tserver[%d]{\n", count);
		printf("\t\tNAME          = %s\n", iter->name.c_str());
		printf("\t\tPORT          = %d\n", iter->port);
		printf("\t\tCTLPORT       = %d\n", iter->ctlport);
		printf("\t\tSSL           = %s\n", iter->is_ssl ? "yes" : "no");
		if(iter->is_ssl){
			printf("\t\tVERIFY_PEER   = %s\n", iter->verify_peer ? "yes" : "no");
			printf("\t\tCA PATH TYPE  = %s\n", iter->is_ca_file ? "file" : "dir");
			printf("\t\tCA PATH       = %s\n", iter->capath.c_str());
			printf("\t\tSERVER CERT   = %s\n", iter->server_cert.c_str());
			printf("\t\tSERVER PRIKEY = %s\n", iter->server_prikey.c_str());
			printf("\t\tSLAVE CERT    = %s\n", iter->slave_cert.c_str());
			printf("\t\tSLAVE PRIKEY  = %s\n", iter->slave_prikey.c_str());
		}
		printf("\t}\n");
	}
	printf("}\n");

	return true;
}

//
// For Checking for structure size
//
void print_initial_datas(void)
{
	printf("================================================================\n");
	printf(" Default datas and size for SHM\n");
	printf("----------------------------------------------------------------\n");
	printf("CHMPX                %zu bytes\n", sizeof(CHMPX));
	printf("CHMPXLIST            %zu bytes\n", sizeof(CHMPXLIST));
	printf("CHMSTAT              %zu bytes\n", sizeof(CHMSTAT));
	printf("CHMPXMAN             %zu bytes\n", sizeof(CHMPXMAN));
	printf("MQMSGHEAD            %zu bytes\n", sizeof(MQMSGHEAD));
	printf("MQMSGHEADLIST        %zu bytes\n", sizeof(MQMSGHEADLIST));
	printf("CHMINFO              %zu bytes\n", sizeof(CHMINFO));
	printf("CHMLOGRAW            %zu bytes\n", sizeof(CHMLOGRAW));
	printf("CHMLOG               %zu bytes\n", sizeof(CHMLOG));
	printf("CHMSHM               %zu bytes\n", sizeof(CHMSHM));
	printf("\n");
	printf("CHMPXLIST[def]       %zu bytes\n", sizeof(CHMPXLIST) * DEFAULT_CHMPX_COUNT);
	printf("MQMSGHEADLIST[def]   %zu bytes\n", sizeof(MQMSGHEADLIST) * DEFAULT_CLIENT_MQ_CNT);
	printf("CHMLOGRAW[def]       %zu bytes\n", sizeof(CHMLOGRAW) * DEFAULT_HISTLOG_COUNT);
	printf("\n");
	printf("CHMPXLIST[max]       %zu bytes\n", sizeof(CHMPXLIST) * MAX_CHMPX_COUNT);
	printf("MQMSGHEADLIST[max]   %zu bytes\n", sizeof(MQMSGHEADLIST) * MAX_CLIENT_MQ_CNT);
	printf("CHMLOGRAW[max]       %zu bytes\n", sizeof(CHMLOGRAW) * MAX_HISTLOG_COUNT);
	printf("\n");
	printf("total[def]           %zu bytes\n", sizeof(CHMSHM) + sizeof(CHMPXLIST) * DEFAULT_CHMPX_COUNT + sizeof(MQMSGHEADLIST) * DEFAULT_CLIENT_MQ_CNT + sizeof(CHMLOGRAW) * DEFAULT_HISTLOG_COUNT);
	printf("total[max]           %zu bytes\n", sizeof(CHMSHM) + sizeof(CHMPXLIST) * MAX_CHMPX_COUNT + sizeof(MQMSGHEADLIST) * MAX_CLIENT_MQ_CNT + sizeof(CHMLOGRAW) * MAX_HISTLOG_COUNT);
	printf("\n");

	PCHMSHM	shm = new CHMSHM;

	printf("----------------------------------------------------------------\n");
	printf("CHMSHM                 %p\n",	shm);
	printf(" CHMINFO               %p\n",	&(shm->info));
	printf("  structure version    %s\n",	CHM_CHMINFO_CUR_VERSION_STR);
	printf("  structure size       %s\n",	to_string(sizeof(CHMINFO)).c_str());
	printf("  pid                  %p\n",	&(shm->info.pid));
	printf("  start_time           %jd\n",	static_cast<intmax_t>(shm->info.start_time));
	printf("  chmpx_man            %p\n",	&(shm->info.chmpx_man));
	printf("  max_mqueue           %s\n",	to_string(shm->info.max_mqueue).c_str());
	printf("  chmpx_mqueue         %s\n",	to_string(shm->info.chmpx_mqueue).c_str());
	printf("  max_q_per_chmpxmq    %s\n",	to_string(shm->info.max_q_per_chmpxmq).c_str());
	printf("  max_q_per_cltmq      %s\n",	to_string(shm->info.max_q_per_cltmq).c_str());
	printf("  base_msgid           %s\n",	to_hexstring(shm->info.base_msgid).c_str());
	printf("  activated_msg_count  %s\n",	to_string(shm->info.activated_msg_count).c_str());
	printf("  activated_msgs       %p\n",	&(shm->info.activated_msgs));
	printf("  assigned_msg_count   %s\n",	to_string(shm->info.assigned_msg_count).c_str());
	printf("  assigned_msgs        %p\n",	&(shm->info.assigned_msgs));
	printf("  free_msg_count       %s\n",	to_string(shm->info.free_msg_count).c_str());
	printf("  free_msgs            %p\n",	&(shm->info.free_msgs));
	printf("  rel_chmpxmsgarea     %p\n",	&(shm->info.rel_chmpxmsgarea));
	printf(" PCHMPXLIST            %p\n",	&(shm->rel_chmpxarea));
	printf(" PCHMPX*               %p\n",	&(shm->rel_pchmpxarrarea));
	printf(" PMQMSGHEADLIST        %p\n",	&(shm->rel_chmpxmsgarea));
	printf(" LOGRAW                %p\n",	&(shm->chmpxlog));
	printf("   enable              %s\n",	shm->chmpxlog.enable ? "true" : "false");
	printf("   start_time          %jd\n",	static_cast<intmax_t>(shm->chmpxlog.start_time));
	printf("   stop_time           %jd\n",	static_cast<intmax_t>(shm->chmpxlog.stop_time));
	printf("   max_log_count       %s\n",	to_string(shm->chmpxlog.max_log_count).c_str());
	printf("   next_pos            %s\n",	to_string(shm->chmpxlog.next_pos).c_str());
	printf("   start_log_rel_area  %p\n",	&(shm->chmpxlog.start_log_rel_area));
	printf("----------------------------------------------------------------\n");
	printf("\n");

	delete shm;
}

//---------------------------------------------------------
// Main
//---------------------------------------------------------
int main(int argc, char** argv)
{
	ChmOpts	opts((argc - 1), &argv[1]);

	// help
	if(opts.Find("h") || opts.Find("help")){
		char*	pprgname = basename(argv[0]);
		Help(pprgname);
		exit(EXIT_SUCCESS);
	}

	// DBG Mode
	string	dbgmode;
	if(opts.Get("g", dbgmode) || opts.Get("d", dbgmode)){
		if(0 == strcasecmp(dbgmode.c_str(), "ERR")){
			SetChmDbgMode(CHMDBG_ERR);
		}else if(0 == strcasecmp(dbgmode.c_str(), "WAN")){
			SetChmDbgMode(CHMDBG_WARN);
		}else if(0 == strcasecmp(dbgmode.c_str(), "INF")){
			SetChmDbgMode(CHMDBG_MSG);
		}else{
			ERR_CHMPRN("Wrong parameter value \"-d\"(\"-g\") %s.", dbgmode.c_str());
			exit(EXIT_FAILURE);
		}
	}

	// print default data
	if(opts.Find("print_default")){
		print_initial_datas();
		exit(EXIT_SUCCESS);
	}

	// configuration option
	string	config("");
	if(opts.Get("conf", config) || opts.Get("f", config)){
		MSG_CHMPRN("Configuration file is %s.", config.c_str());
	}else if(opts.Get("json", config)){
		MSG_CHMPRN("Configuration JSON is \"%s\"", config.c_str());
	}

	// create epoll event fd
	int		eventfd;
	if(CHM_INVALID_HANDLE == (eventfd = epoll_create1(EPOLL_CLOEXEC))){				// EPOLL_CLOEXEC is OK?
		ERR_CHMPRN("epoll_create: error %d", errno);
		exit(EXIT_FAILURE);
	}

	// get conf object
	CHMConf*	pconfobj = CHMConf::GetCHMConf(eventfd, NULL, config.c_str(), CHM_INVALID_PORT, true, &config);
	if(!pconfobj){
		ERR_CHMPRN("Could not build configuration object.");
		close(eventfd);
		exit(EXIT_FAILURE);
	}

	// conf
	if(!LoadConfTest(pconfobj)){
		WAN_CHMPRN("Failed dump configuration file.");
	}

	// inotify set
	if(!pconfobj->SetEventQueueFd(eventfd) || pconfobj->SetEventQueue()){
		ERR_CHMPRN("Failed to set eventfd for inotify.");
		pconfobj->UnsetEventQueue();
		delete pconfobj;
		close(eventfd);
		exit(EXIT_FAILURE);
	}

	// Signal block test
	sigset_t		sigset;
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGUSR1);
	sigprocmask(SIG_SETMASK, &sigset, NULL);

	// Loop
	while(true){
		struct epoll_event	events[32];
		int					max_events	= 32;
		int					timeout		= 1000;					// 1s (ex, another is 100ms)

		int evcount = epoll_pwait(eventfd, events, max_events, timeout, &sigset);
		if(-1 == evcount){
			ERR_CHMPRN("epoll_pwait: error %d", errno);
			break;
		}else if(0 == evcount){
			MSG_CHMPRN("epoll_pwait timeouted.");
		}else{
			for(int ecnt = 0; ecnt < evcount; ecnt++){
				if(pconfobj->IsEventQueueFd(events[ecnt].data.fd)){
					if(!pconfobj->Receive(events[ecnt].data.fd)){
						ERR_CHMPRN("Failed to check inotify fd.");
						break;
					}
				}else{
					ERR_CHMPRN("unknown event for fd(%d).\n", events[ecnt].data.fd);
				}
			}
		}
	}
	pconfobj->UnsetEventQueue();
	delete pconfobj;
	close(eventfd);

	exit(EXIT_SUCCESS);
}

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
