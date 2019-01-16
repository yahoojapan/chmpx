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

#include "chmcommon.h"
#include "chmopts.h"
#include "chmdbg.h"

using namespace	std;

//---------------------------------------------------------
// Symbols
//---------------------------------------------------------
#define	OPT_PARAM_SEP			"\r\n"

//---------------------------------------------------------
// Constructor/Destructor
//---------------------------------------------------------
ChmOpts::ChmOpts(int argc, char** argv, const char* strsepchars) : sepchars(CHMEMPTYSTR(strsepchars) ? OPT_PARAM_SEP : strsepchars)
{
	if(argv && 0 < argc){
		Initialize(argc, argv);
	}
}

ChmOpts::~ChmOpts(void)
{
}

//---------------------------------------------------------
// Methods
//---------------------------------------------------------
bool ChmOpts::Initialize(int argc, char** argv)
{
	if(!argv || 0 == argc){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}
	optmap.clear();

	for(int cnt = 0; cnt < argc; cnt++){
		string		option;
		strlst_t	paramlist;

		if('-' != argv[cnt][0]){
			option = "";
		}else{
			option = &(argv[cnt][1]);
		}
		option = upper(option);

		paramlist.clear();
		int	pcnt;
		for(pcnt = 0; (cnt + pcnt + 1) < argc; pcnt++){
			if('-' == argv[cnt + pcnt + 1][0]){
				break;
			}
			str_paeser(argv[cnt + pcnt + 1], paramlist, OPT_PARAM_SEP);
		}
		optmap[option] = paramlist;
		cnt += pcnt;
	}
	return true;
}

bool ChmOpts::Get(const char* popt, string& param)
{
	string	stropt;
	if(CHMEMPTYSTR(popt)){
		stropt = "";
	}else{
		stropt = upper(string(popt));
	}
	if(optmap.end() == optmap.find(stropt)){
		return false;
	}
	if(optmap[stropt].empty()){
		param = "";
	}else{
		param = optmap[stropt].front();
	}
	return true;
}

bool ChmOpts::Get(const char* popt, strlst_t& params)
{
	string	stropt;
	if(CHMEMPTYSTR(popt)){
		stropt = "";
	}else{
		stropt = upper(string(popt));
	}
	if(optmap.end() == optmap.find(stropt)){
		return false;
	}
	params = optmap[stropt];
	return true;
}

bool ChmOpts::Find(const char* popt) const
{
	if(CHMEMPTYSTR(popt)){
		return false;
	}
	string	stropt = upper(string(popt));
	if(optmap.end() == optmap.find(stropt)){
		return false;
	}
	return true;
}

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
