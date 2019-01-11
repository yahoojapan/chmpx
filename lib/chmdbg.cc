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
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <errno.h>

#include "chmcommon.h"
#include "chmdbg.h"

//---------------------------------------------------------
// Global variable
//---------------------------------------------------------
ChmDbgMode	chm_debug_mode	= CHMDBG_SILENT;
FILE*		chm_dbg_fp		= NULL;

//---------------------------------------------------------
// Class CHMDbgControl
//---------------------------------------------------------
// [NOTE]
// To avoid static object initialization order problem(SIOF)
//
class CHMDbgControl
{
	protected:
		static const char*		DBGENVNAME;
		static const char*		DBGENVFILE;

		ChmDbgMode*				pchm_debug_mode;
		FILE**					pchm_dbg_fp;

	protected:
		CHMDbgControl() : pchm_debug_mode(&chm_debug_mode), pchm_dbg_fp(&chm_dbg_fp)
		{
			*pchm_debug_mode	= CHMDBG_SILENT;
			*pchm_dbg_fp		= NULL;
			LoadDbgCntlEnv();
		}

		virtual ~CHMDbgControl()
		{
			UnsetChmDbgCntlFile();
		}

		bool LoadDbgCntlEnvName(void);
		bool LoadDbgCntlEnvFile(void);

	public:
		bool LoadDbgCntlEnv(void);

		ChmDbgMode SetChmDbgCntlMode(ChmDbgMode mode);
		ChmDbgMode BumpupChmDbgCntlMode(void);
		ChmDbgMode GetChmDbgCntlMode(void);

		bool SetChmDbgCntlFile(const char* filepath);
		bool UnsetChmDbgCntlFile(void);

		static CHMDbgControl& GetCHMDbgCntrl(void)
		{
			static CHMDbgControl	singleton;			// singleton
			return singleton;
		}
};

//
// Class variables
//
const char*		CHMDbgControl::DBGENVNAME = "CHMDBGMODE";
const char*		CHMDbgControl::DBGENVFILE = "CHMDBGFILE";

//
// Methods
//
bool CHMDbgControl::LoadDbgCntlEnv(void)
{
	if(!LoadDbgCntlEnvName() || !LoadDbgCntlEnvFile()){
		return false;
	}
	return true;
}

bool CHMDbgControl::LoadDbgCntlEnvName(void)
{
	char*	pEnvVal;
	if(NULL == (pEnvVal = getenv(CHMDbgControl::DBGENVNAME))){
		MSG_CHMPRN("%s ENV is not set.", CHMDbgControl::DBGENVNAME);
		return true;
	}
	if(0 == strcasecmp(pEnvVal, "SILENT")){
		SetChmDbgCntlMode(CHMDBG_SILENT);
	}else if(0 == strcasecmp(pEnvVal, "ERR")){
		SetChmDbgCntlMode(CHMDBG_ERR);
	}else if(0 == strcasecmp(pEnvVal, "WAN")){
		SetChmDbgCntlMode(CHMDBG_WARN);
	}else if(0 == strcasecmp(pEnvVal, "INFO")){
		SetChmDbgCntlMode(CHMDBG_MSG);
	}else if(0 == strcasecmp(pEnvVal, "DUMP")){
		SetChmDbgCntlMode(CHMDBG_DUMP);
	}else{
		MSG_CHMPRN("%s ENV is not unknown string(%s).", CHMDbgControl::DBGENVNAME, pEnvVal);
		return false;
	}
	return true;
}

bool CHMDbgControl::LoadDbgCntlEnvFile(void)
{
	char*	pEnvVal;
	if(NULL == (pEnvVal = getenv(CHMDbgControl::DBGENVFILE))){
		MSG_CHMPRN("%s ENV is not set.", CHMDbgControl::DBGENVFILE);
		return true;
	}
	if(!SetChmDbgCntlFile(pEnvVal)){
		MSG_CHMPRN("%s ENV is unsafe string(%s).", CHMDbgControl::DBGENVFILE, pEnvVal);
		return false;
	}
	return true;
}

ChmDbgMode CHMDbgControl::SetChmDbgCntlMode(ChmDbgMode mode)
{
	ChmDbgMode oldmode = *pchm_debug_mode;
	*pchm_debug_mode = mode;
	return oldmode;
}

ChmDbgMode CHMDbgControl::BumpupChmDbgCntlMode(void)
{
	ChmDbgMode	mode = GetChmDbgCntlMode();

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
	return SetChmDbgCntlMode(mode);
}

ChmDbgMode CHMDbgControl::GetChmDbgCntlMode(void)
{
	return *pchm_debug_mode;
}

bool CHMDbgControl::SetChmDbgCntlFile(const char* filepath)
{
	if(CHMEMPTYSTR(filepath)){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(!UnsetChmDbgCntlFile()){
		return false;
	}
	FILE*	newfp;
	if(NULL == (newfp = fopen(filepath, "a+"))){
		ERR_CHMPRN("Could not open debug file(%s). errno = %d", filepath, errno);
		return false;
	}
	*pchm_dbg_fp = newfp;
	return true;
}

bool CHMDbgControl::UnsetChmDbgCntlFile(void)
{
	if(*pchm_dbg_fp){
		if(0 != fclose(*pchm_dbg_fp)){
			ERR_CHMPRN("Could not close debug file. errno = %d", errno);
			*pchm_dbg_fp = NULL;		// On this case, chm_dbg_fp is not correct pointer after error...
			return false;
		}
		*pchm_dbg_fp = NULL;
	}
	return true;
}

//---------------------------------------------------------
// Global Functions
//---------------------------------------------------------
ChmDbgMode SetChmDbgMode(ChmDbgMode mode)
{
	return CHMDbgControl::GetCHMDbgCntrl().SetChmDbgCntlMode(mode);
}

ChmDbgMode BumpupChmDbgMode(void)
{
	return CHMDbgControl::GetCHMDbgCntrl().BumpupChmDbgCntlMode();
}

ChmDbgMode GetChmDbgMode(void)
{
	return CHMDbgControl::GetCHMDbgCntrl().GetChmDbgCntlMode();
}

bool LoadChmDbgEnv(void)
{
	return CHMDbgControl::GetCHMDbgCntrl().LoadDbgCntlEnv();
}

bool SetChmDbgFile(const char* filepath)
{
	return CHMDbgControl::GetCHMDbgCntrl().SetChmDbgCntlFile(filepath);
}

bool UnsetChmDbgFile(void)
{
	return CHMDbgControl::GetCHMDbgCntrl().UnsetChmDbgCntlFile();
}

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
