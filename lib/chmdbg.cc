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
#include <strings.h>
#include <errno.h>

#include "chmcommon.h"
#include "chmdbg.h"

//---------------------------------------------------------
// Class CHMDbgControl
//---------------------------------------------------------
class CHMDbgControl
{
	protected:
		static const char*		DBGENVNAME;
		static const char*		DBGENVFILE;
		static CHMDbgControl	singleton;

	public:
		static bool LoadEnv(void);
		static bool LoadEnvName(void);
		static bool LoadEnvFile(void);

		CHMDbgControl();
		virtual ~CHMDbgControl();
};

// Class valiables
const char*		CHMDbgControl::DBGENVNAME = "CHMDBGMODE";
const char*		CHMDbgControl::DBGENVFILE = "CHMDBGFILE";
CHMDbgControl	CHMDbgControl::singleton;

// Constructor / Destructor
CHMDbgControl::CHMDbgControl()
{
	CHMDbgControl::LoadEnv();
}
CHMDbgControl::~CHMDbgControl()
{
}

// Class Methods
bool CHMDbgControl::LoadEnv(void)
{
	if(!CHMDbgControl::LoadEnvName() || !CHMDbgControl::LoadEnvFile()){
		return false;
	}
	return true;
}

bool CHMDbgControl::LoadEnvName(void)
{
	char*	pEnvVal;
	if(NULL == (pEnvVal = getenv(CHMDbgControl::DBGENVNAME))){
		MSG_CHMPRN("%s ENV is not set.", CHMDbgControl::DBGENVNAME);
		return true;
	}
	if(0 == strcasecmp(pEnvVal, "SILENT")){
		SetChmDbgMode(CHMDBG_SILENT);
	}else if(0 == strcasecmp(pEnvVal, "ERR")){
		SetChmDbgMode(CHMDBG_ERR);
	}else if(0 == strcasecmp(pEnvVal, "WAN")){
		SetChmDbgMode(CHMDBG_WARN);
	}else if(0 == strcasecmp(pEnvVal, "INFO")){
		SetChmDbgMode(CHMDBG_MSG);
	}else if(0 == strcasecmp(pEnvVal, "DUMP")){
		SetChmDbgMode(CHMDBG_DUMP);
	}else{
		MSG_CHMPRN("%s ENV is not unknown string(%s).", CHMDbgControl::DBGENVNAME, pEnvVal);
		return false;
	}
	return true;
}

bool CHMDbgControl::LoadEnvFile(void)
{
	char*	pEnvVal;
	if(NULL == (pEnvVal = getenv(CHMDbgControl::DBGENVFILE))){
		MSG_CHMPRN("%s ENV is not set.", CHMDbgControl::DBGENVFILE);
		return true;
	}
	if(!SetChmDbgFile(pEnvVal)){
		MSG_CHMPRN("%s ENV is unsafe string(%s).", CHMDbgControl::DBGENVFILE, pEnvVal);
		return false;
	}
	return true;
}

//---------------------------------------------------------
// Global variable
//---------------------------------------------------------
ChmDbgMode	chm_debug_mode	= CHMDBG_SILENT;
FILE*		chm_dbg_fp	= NULL;

ChmDbgMode SetChmDbgMode(ChmDbgMode mode)
{
	ChmDbgMode oldmode = chm_debug_mode;
	chm_debug_mode = mode;
	return oldmode;
}

ChmDbgMode BumpupChmDbgMode(void)
{
	ChmDbgMode	mode = GetChmDbgMode();

	if(CHMDBG_SILENT == mode){
		mode = CHMDBG_ERR;
	}else if(CHMDBG_ERR == mode){
		mode = CHMDBG_WARN;
	}else if(CHMDBG_WARN == mode){
		mode = CHMDBG_MSG;
	}else if(CHMDBG_MSG == mode){
		mode = CHMDBG_DUMP;
	}else{	// CHMDBG_DUMP == mode
		mode = CHMDBG_SILENT;
	}
	return ::SetChmDbgMode(mode);
}

ChmDbgMode GetChmDbgMode(void)
{
	return chm_debug_mode;
}

bool LoadChmDbgEnv(void)
{
	return CHMDbgControl::LoadEnv();
}

bool SetChmDbgFile(const char* filepath)
{
	if(CHMEMPTYSTR(filepath)){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(!UnsetChmDbgFile()){
		return false;
	}
	FILE*	newfp;
	if(NULL == (newfp = fopen(filepath, "a+"))){
		ERR_CHMPRN("Could not open debug file(%s). errno = %d", filepath, errno);
		return false;
	}
	chm_dbg_fp = newfp;
	return true;
}

bool UnsetChmDbgFile(void)
{
	if(chm_dbg_fp){
		if(0 != fclose(chm_dbg_fp)){
			ERR_CHMPRN("Could not close debug file. errno = %d", errno);
			chm_dbg_fp = NULL;		// On this case, chm_dbg_fp is not correct pointer after error...
			return false;
		}
		chm_dbg_fp = NULL;
	}
	return true;
}

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
