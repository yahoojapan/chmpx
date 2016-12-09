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
#include <libgen.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <k2hash/k2hash.h>

#include <iostream>

#include "chmcommon.h"
#include "chmcntrl.h"
#include "chmsigcntrl.h"
#include "chmutil.h"
#include "chmopts.h"
#include "chmpx.h"
#include "chmconfutil.h"
#include "chmdbg.h"

using namespace	std;

//---------------------------------------------------------
// Symbols
//---------------------------------------------------------
#define	OPT_HELP1				"H"
#define	OPT_HELP2				"HELP"
#define	OPT_VERSION1			"V"
#define	OPT_VERSION2			"VER"
#define	OPT_VERSION3			"VERSION"
#define	OPT_CONFPATH			"CONF"
#define	OPT_CONFPATH2			"F"
#define	OPT_JSONCONF			"JSON"
#define	OPT_CTLPORT				"CTLPORT"
#define	OPT_CTLPORT2			"CNTLPORT"
#define	OPT_CTLPORT3			"CNTRLPORT"
#define	OPT_DBG					"D"
#define	OPT_DBG2				"G"
#define	OPT_DBGFILEPATH			"DFILE"
#define	OPT_DBGFILEPATH2		"GFILE"
#define	OPT_DBG_PARAM_SLT		"SLT"
#define	OPT_DBG_PARAM_SLIENT	"SILENT"
#define	OPT_DBG_PARAM_ERR		"ERR"
#define	OPT_DBG_PARAM_ERROR		"ERROR"
#define	OPT_DBG_PARAM_WAN		"WAN"
#define	OPT_DBG_PARAM_WARNING	"WARNING"
#define	OPT_DBG_PARAM_MSG		"MSG"
#define	OPT_DBG_PARAM_MESSAGE	"MESSAGE"
#define	OPT_DBG_PARAM_INFO		"INFO"
#define	OPT_DBG_PARAM_DUMP		"DUMP"
#define	OPT_DBG_PARAM_ALLOW_SC	"ALLOWSELFCERT"		// [Hidden option] Only SSL debugging

//---------------------------------------------------------
// Signal handler
//---------------------------------------------------------
static void SigUsr1handler(int signum)
{
	if(SIGUSR1 == signum){
		// Bumpup debug level.
		BumpupChmDbgMode();

		ChmDbgMode	chmdbgmode	= GetChmDbgMode();
		string		strmode		= ChmDbgMode_STR(chmdbgmode);
		cout << "MESSAGE: Caught signal SIGUSR1, bumpup the logging level to " << strmode << "." << endl;

		// If debug level is not same chmpx and k2hash, then it should be
		// set same level.
		if(CHMDBG_SILENT == chmdbgmode){
			k2h_set_debug_level_silent();
		}else if(CHMDBG_ERR == chmdbgmode){
			k2h_set_debug_level_error();
		}else if(CHMDBG_WARN == chmdbgmode){
			k2h_set_debug_level_warning();
		}else if(CHMDBG_MSG == chmdbgmode){
			k2h_set_debug_level_message();
		}else if(CHMDBG_DUMP == chmdbgmode){
			k2h_set_debug_level_message();		// k2hash does not have dump mode
		}
	}
}

//---------------------------------------------------------
// Functions
//---------------------------------------------------------
//	[Usage]
//	prgname -conf <configration file path> [-d [slient|err|wan|msg]]
//
static bool PrintUsage(const char* prgname)
{
	cout << "[Usage]" << endl;
	cout << (prgname ? prgname : "prgname") << " [-conf <file> | -json <json>] [-ctlport <port>] [-d [slient|err|wan|msg|dump]] [-dfile <debug file path>]" << endl;
	cout << (prgname ? prgname : "prgname") << " [ -h | -v ]" << endl;
	cout << endl;
	cout << "[option]" << endl;
	cout << "  -conf <path>         specify the configration file(.ini .yaml .json) path" << endl;
	cout << "  -json <json string>  specify the configration json string" << endl;
	cout << "  -ctlport <port>      specify the self contrl port(*)" << endl;
	cout << "  -d <param>           specify the debugging output mode:" << endl;
	cout << "                        silent - no output" << endl;
	cout << "                        err    - output error level" << endl;
	cout << "                        wan    - output warning level" << endl;
	cout << "                        msg    - output debug(message) level" << endl;
	cout << "                        dump   - output communication debug level" << endl;
	cout << "  -dfile <path>        specify the file path which is put debug output" << endl;
	cout << "  -h(help)             display this usage." << endl;
	cout << "  -v(version)          display version." << endl;
	cout << endl;
	cout << "[environments]" << endl;
	cout << "  CHMDBGMODE           debugging mode like \"-d\" option." << endl;
	cout << "  CHMDBGFILE           the file path for debugging output like \"-dfile\" option." << endl;
	cout << "  CHMCONFFILE          configuration file path like \"-conf\" option" << endl;
	cout << "  CHMJSONCONF          configuration json string like \"-json\" option" << endl;
	cout << endl;
	cout << "(*) if ctlport option is specified, chmpx searches same ctlport in configuration" << endl;
	cout << "    file and ignores \"CTLPORT\" directive in \"GLOBAL\" section. and chmpx will" << endl;
	cout << "    start in the mode indicated by the server entry that has beed detected." << endl;
	cout << endl;

	return true;
}

//---------------------------------------------------------
// Main
//---------------------------------------------------------
int main(int argc, char** argv)
{
	if(0 == argc || !argv){
		exit(EXIT_FAILURE);
	}
	string	prgname = basename(argv[0]);
	if(0 == strncmp(prgname.c_str(), "lt-", strlen("lt-"))){
		prgname = prgname.substr(3);
	}

	// limit
	struct rlimit rl;
	getrlimit(RLIMIT_NOFILE, &rl);
	if(rl.rlim_cur < rl.rlim_max){
		rl.rlim_cur = rl.rlim_max;
		if(0 != setrlimit(RLIMIT_NOFILE, &rl)){
			cout << "ERROR: Could not set RLIMIT_NOFILE from " << rl.rlim_max << " to " << rl.rlim_cur << ", but continue..." << endl;
		}
	}

	ChmOpts	opts((argc - 1), &argv[1]);

	// help
	if(opts.Find(OPT_HELP1) || opts.Find(OPT_HELP2)){
		PrintUsage(prgname.c_str());
		exit(EXIT_SUCCESS);
	}

	// help
	if(opts.Find(OPT_VERSION1) || opts.Find(OPT_VERSION2) || opts.Find(OPT_VERSION3)){
		chmpx_print_version(stdout);
		exit(EXIT_SUCCESS);
	}

	// parameter - configration path
	string	config;
	if(!opts.Get(OPT_CONFPATH, config) && !opts.Get(OPT_CONFPATH2, config) && !opts.Get(OPT_JSONCONF, config)){
		if(!have_env_chm_conf()){
			cout << "ERROR: Must specify option \"-conf\" or \"-json\"" << endl;
			PrintUsage(prgname.c_str());
			exit(EXIT_FAILURE);
		}
		// Has configuration environment.
	}

	// parameter - control port
	short	ctlport = CHM_INVALID_PORT;
	string	strctlport;
	if(opts.Get(OPT_CTLPORT, strctlport) || opts.Get(OPT_CTLPORT2, strctlport) || opts.Get(OPT_CTLPORT3, strctlport)){
		ctlport = static_cast<short>(atoi(strctlport.c_str()));
		if(CHM_INVALID_PORT == ctlport || 0 == ctlport){
			cout << "ERROR: option \"-ctlport\" is specified with invalid value." << endl;
			exit(EXIT_FAILURE);
		}
	}

	// parameter - debug
	bool		is_dbgopt	= false;
	ChmDbgMode	dbgmode		= CHMDBG_SILENT;
	string		dbgfile		= "";
	{
		string	strDbgMode;
		if(opts.Get(OPT_DBG, strDbgMode) || opts.Get(OPT_DBG2, strDbgMode)){
			if(strDbgMode.empty()){
				cout << "ERROR: \"-d\" option must be specified with parameter." << endl;
				PrintUsage(prgname.c_str());
				exit(EXIT_FAILURE);
			}
			strDbgMode = upper(strDbgMode);

			if(OPT_DBG_PARAM_SLT == strDbgMode || OPT_DBG_PARAM_SLIENT == strDbgMode){
				dbgmode = CHMDBG_SILENT;
			}else if(OPT_DBG_PARAM_ERR == strDbgMode || OPT_DBG_PARAM_ERROR == strDbgMode){
				dbgmode = CHMDBG_ERR;
			}else if(OPT_DBG_PARAM_WAN == strDbgMode || OPT_DBG_PARAM_WARNING == strDbgMode){
				dbgmode = CHMDBG_WARN;
			}else if(OPT_DBG_PARAM_MSG == strDbgMode || OPT_DBG_PARAM_MESSAGE == strDbgMode || OPT_DBG_PARAM_INFO == strDbgMode){
				dbgmode = CHMDBG_MSG;
			}else if(OPT_DBG_PARAM_DUMP == strDbgMode){
				dbgmode = CHMDBG_DUMP;
			}else{
				cout << "ERROR: Unknown \"-d\" option parameter(" << strDbgMode << ") is specified." << endl;
				PrintUsage(prgname.c_str());
				exit(EXIT_FAILURE);
			}
			is_dbgopt = true;
		}
		if(!opts.Get(OPT_DBGFILEPATH, dbgfile)){
			opts.Get(OPT_DBGFILEPATH2, dbgfile);
		}
	}

	// Set debug mode
	if(!LoadChmDbgEnv()){
		cout << "WARNING: Something error occured while loading debugging mode/file from environment." << endl;
	}
	if(!dbgfile.empty()){
		if(!SetChmDbgFile(dbgfile.c_str())){
			cout << "WARNING: Something error occured while dispatching debugging file(" << dbgfile << ")." << endl;
		}
	}
	if(is_dbgopt){
		SetChmDbgMode(dbgmode);
	}

	// signal mask
	ChmSigCntrl	sigcntrl;
	int			sigunmask[] = {SIGUSR1, SIGHUP, SIGINT};	// we have only USR1/HUP/INT handler now.
	if(	!sigcntrl.Initialize(sigunmask, (sizeof(sigunmask) / sizeof(int)))	||
		!sigcntrl.SetSignalProcMask()										||
		!sigcntrl.SetHandler(SIGUSR1, SigUsr1handler)						||
		!sigcntrl.SetHandler(SIGHUP, ChmCntrl::LoopBreakHandler)			||
		!sigcntrl.SetHandler(SIGINT, ChmCntrl::LoopBreakHandler)			)
	{
		cout << "ERROR: Could not set signal procmask, handler, etc..." << endl;
		exit(EXIT_SUCCESS);
	}

	// Initialize
	ChmCntrl	chmobj;
	if(!chmobj.InitializeOnChmpx(config.c_str(), ctlport)){
		cout << "ERROR: Could not initialize process." << endl;
		cout << "You can see detail about error, execute with \"-d\" option." << endl;
		exit(EXIT_FAILURE);
	}
	if(opts.Find(OPT_DBG_PARAM_ALLOW_SC)){
		// Hidden option.
		chmobj.AllowSelfCert();
	}

	// print process information.
	stringstream	sstream;
	if(!chmobj.DumpSelfChmpxSvr(sstream)){
		cout << "ERROR: Could not get chmpx process information, maybe failed to initialize." << endl;
		exit(EXIT_FAILURE);
	}
	cout << sstream.str() << endl;

	// main loop
	if(!chmobj.EventLoop()){
		cout << "ERROR: Something error occured in main event loop." << endl;
		chmobj.Clean();
		exit(EXIT_FAILURE);
	}
	chmobj.Clean();

	exit(EXIT_SUCCESS);
}

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
