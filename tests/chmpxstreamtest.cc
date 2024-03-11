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
#include <stdarg.h>
#include <libgen.h>
#include <string>
#include <iostream>

#include "chmcommon.h"
#include "chmstructure.h"
#include "chmutil.h"
#include "chmconfutil.h"
#include "chmdbg.h"
#include "chmcntrl.h"
#include "chmopts.h"
#include "chmstream.h"

using namespace std;

//---------------------------------------------------------
// Macros
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

//---------------------------------------------------------
// Test functions
//---------------------------------------------------------
#define	OSTREAM_KEY_STR				"noreply_ostream_key"
#define	OSTREAM_KEY_STR2			"noreply_ostream_key2"
#define	OSTREAM_DIR_KEY_STR			"noreply_ostream_dir_key"
#define	OSTREAM_DIR_KEY_STR2		"noreply_ostream_dir_key2"
#define	OSTREAM_DIR_HASH_VAL		0

#define	ISTREAM_DIR_KEY_STR			"noreply_istream_dir_key"
#define	ISTREAM_DIR_KEY_STR2		"noreply_istream_dir_key2"
#define	ISTREAM_DIR_HASH_VAL		0

#define	STREAM_KEY_STR				"reply_stream_key"
#define	STREAM_KEY_STR2				"reply_stream_key2"
#define	STREAM_DIR_KEY_STR			"reply_stream_dir_key"
#define	STREAM_DIR_KEY_STR2			"reply_stream_dir_key2"
#define	STREAM_DIR_HASH_VAL			1

#define	REQUEST32_KEY_STR			"reply_val32"
#define	REQUEST8192_KEY_STR			"reply_val8192"

#define	VALTYPE_TEST_KEY_CHR		"chkval_char"
#define	VALTYPE_TEST_KEY_INT		"chkval_int"
#define	VALTYPE_TEST_KEY_LNG		"chkval_long"
#define	VALTYPE_TEST_NKEY_SKP		"chkval_seekp***** NG AREA ******"		// last 20 word is NG
#define	VALTYPE_TEST_TKEY_SKP		"chkval_seekp"
#define	VALTYPE_TEST_PKEY_SKP		"chkval_s"
#define	VALTYPE_TEST_KEY_SKG		"chkval_seekg"

#define VALTYPE_TEST_VAL_CHR		'Z'
#define VALTYPE_TEST_VAL_INT		1000
#define VALTYPE_TEST_VAL_LNG		0xA5A5A5A5

#define	STREAM_REPLY_HASHKEY_STR	"reply_stream_hash"
#define	VALTYPE_REPLY_VAL_SKG		"*******************OK"					// begin 20 word is NG
#define	VALTYPE_REPLY_VAL_OK		"OK"
#define	VALTYPE_REPLY_VAL_NG		"NG"

#define	STREAM_VAL32_STR			"123456789+123456789+123456789+--"

static bool TestServerSide(ChmCntrl& chmobj)
{
	PRN("-------------------------------------------------------");
	PRN("TEST Receiver side");
	PRN("-------------------------------------------------------");

	chmstream	strm(&chmobj);
	string		strKey;
	string		strVal;
	string		strRVal;
	string		strRVal8192;
	for(int cnt = 0; cnt < 256; ++cnt){
		strRVal8192 += STREAM_VAL32_STR;
	}

	while(true){
		char	buff;
		int		ival;
		long	lval;
		bool	is_reply;
		bool	is_error;

		is_error = false;
		buff	= '\0';
		ival	= -1;
		lval	= -1;

		strKey.erase();
		strVal.erase();

		// read key & value
		strm.clear();
		strm >> strKey;
		if(strm.eof() && 0 == strKey.length()){
			//ERR("Could not read key data from stream, maybe timeout.");
			continue;
		}else{
			if(strKey == VALTYPE_TEST_KEY_CHR){
				strm >> buff;
				strVal	= buff;

			}else if(strKey == VALTYPE_TEST_KEY_INT){
				strm >> ival;
				strVal	= to_string(ival);

			}else if(strKey == VALTYPE_TEST_KEY_LNG){
				strm >> lval;
				strVal	= to_hexstring(lval);

			}else{
				strm >> strVal;
				if(strm.eof() && 0 == strVal.length()){
					// value is empty, so swap key and value.
					strVal = strKey;
					strKey = "";
				}
			}
		}

		// check
		if(0 == strKey.length()){
			if(STREAM_DIR_HASH_VAL == strm.receivedhash()){
				is_reply = true;
				strRVal	= VALTYPE_REPLY_VAL_OK;
			}else{
				is_reply = false;
			}

		}else if(strKey == OSTREAM_DIR_KEY_STR || strKey == OSTREAM_DIR_KEY_STR2){
			is_reply = false;

		}else if(strKey == OSTREAM_KEY_STR || strKey == OSTREAM_KEY_STR2){
			is_reply = false;

		}else if(strKey == ISTREAM_DIR_KEY_STR || strKey == ISTREAM_DIR_KEY_STR2){
			is_reply = false;

		}else if(strKey == STREAM_DIR_KEY_STR || strKey == STREAM_DIR_KEY_STR2){
			is_reply = true;
			strRVal	= strVal;

		}else if(strKey == STREAM_KEY_STR || strKey == STREAM_KEY_STR2){
			is_reply = true;
			strRVal	= strVal;

		}else if(strKey == REQUEST32_KEY_STR){
			is_reply = true;
			strRVal	= STREAM_VAL32_STR;

		}else if(strKey == REQUEST8192_KEY_STR){
			is_reply = true;
			strRVal	= strRVal8192;

		}else if(strKey == VALTYPE_TEST_KEY_CHR){
			is_reply= true;
			if(buff == VALTYPE_TEST_VAL_CHR){
				is_error = false;
				strRVal	= VALTYPE_REPLY_VAL_OK;
			}else{
				is_error = true;
				strRVal	= VALTYPE_REPLY_VAL_NG;
			}

		}else if(strKey == VALTYPE_TEST_KEY_INT){
			is_reply= true;
			if(ival == VALTYPE_TEST_VAL_INT){
				is_error = false;
				strRVal	= VALTYPE_REPLY_VAL_OK;
			}else{
				is_error = true;
				strRVal	= VALTYPE_REPLY_VAL_NG;
			}

		}else if(strKey == VALTYPE_TEST_KEY_LNG){
			is_reply= true;
			if(lval == VALTYPE_TEST_VAL_LNG){
				is_error = false;
				strRVal	= VALTYPE_REPLY_VAL_OK;
			}else{
				is_error = true;
				strRVal	= VALTYPE_REPLY_VAL_NG;
			}

		}else if(strKey == VALTYPE_TEST_KEY_SKG){
			is_reply = true;
			strRVal	= VALTYPE_REPLY_VAL_SKG;

		}else if(0 == strncmp(strKey.c_str(), VALTYPE_TEST_PKEY_SKP, strlen(VALTYPE_TEST_PKEY_SKP))){
			if(strKey == VALTYPE_TEST_TKEY_SKP){
				is_reply = true;
				strRVal	= VALTYPE_REPLY_VAL_OK;
			}else{
				is_reply = true;
				is_error = true;
				strRVal	= VALTYPE_REPLY_VAL_NG;
			}

		}else{
			is_reply = false;
			is_error = true;
		}

		// report
		if(is_error){
			PRN("receive(error): \"%s\" => \"%s\"", strKey.c_str(), strVal.c_str());
		}else{
			PRN("receive:        \"%s\" => \"%s\"", strKey.c_str(), strVal.c_str());
		}

		// reply
		if(is_reply){
			strm.clear();
			strm << strRVal << endl;
			PRN("reply:          \"value\" => \"%s\"", strRVal.c_str());
		}else{
			PRN("reply:          no replying");
		}
	}
	return true;
}

static bool TestSlaveSide(ChmCntrl& chmobj)
{
	string		strKey;
	string		strVal;
	string		strVal32(STREAM_VAL32_STR);
	string		strVal8192;
	for(int cnt = 0; cnt < 256; ++cnt){
		strVal8192 += STREAM_VAL32_STR;
	}

	PRN("-------------------------------------------------------");
	PRN("TEST Slave side : ochmstream");
	PRN("-------------------------------------------------------");
	{
		{	// normal
			ochmstream	strm(&chmobj);
			strKey = OSTREAM_KEY_STR;

			strm << strKey << endl;
			strm << strVal32 << endl;

			PRN("ochmstream(normal 32):          \"%s\" => \"%s\"", strKey.c_str(), strVal32.c_str());
		}
		sleep(1);
		{	// normal(8192)
			ochmstream	strm(&chmobj);
			strKey = OSTREAM_KEY_STR2;

			strm << strKey << endl;
			strm << strVal8192 << endl;

			PRN("ochmstream(normal 8192):        \"%s\" => \"%s\"", strKey.c_str(), strVal8192.c_str());
		}
		sleep(1);
		{	// direct key
			ochmstream	strm(&chmobj, string(OSTREAM_DIR_KEY_STR));

			strm << strVal32 << endl;

			PRN("ochmstream(dir key 32):         \"%s\" => \"%s\"", OSTREAM_DIR_KEY_STR, strVal32.c_str());
		}
		sleep(1);
		{	// direct key(8192)
			ochmstream	strm(&chmobj, string(OSTREAM_DIR_KEY_STR2));

			strm << strVal8192 << endl;

			PRN("ochmstream(dir key 8192):       \"%s\" => \"%s\"", OSTREAM_DIR_KEY_STR2, strVal8192.c_str());
		}
		sleep(1);
		{	// hash
			ochmstream	strm(&chmobj, OSTREAM_DIR_HASH_VAL);

			strm << strVal32 << endl;

			PRN("ochmstream(hash 32):            \"%s\" => \"%s\"", OSTREAM_DIR_HASH_VAL, strVal32.c_str());
		}
		sleep(1);
		{	// hash(8192)
			ochmstream	strm(&chmobj, OSTREAM_DIR_HASH_VAL);

			strm << strVal8192 << endl;

			PRN("ochmstream(hash 8192):          \"%s\" => \"%s\"", OSTREAM_DIR_HASH_VAL, strVal8192.c_str());
		}
		sleep(1);
	}

	PRN("-------------------------------------------------------");
	PRN("TEST Slave side : ichmstream --> this case must be server side.");
	PRN("-------------------------------------------------------");
	PRN("SKIP THIS\n");

	PRN("-------------------------------------------------------");
	PRN("TEST Slave side : chmstream");
	PRN("-------------------------------------------------------");
	{
		{	// normal
			chmstream	strm(&chmobj);
			strKey = STREAM_KEY_STR;

			strm << strKey << endl;
			strm << strVal32 << endl;

			PRN("chmstream(normal 32):           \"%s\" => \"%s\"", strKey.c_str(), strVal32.c_str());

			while(true){
				strm.clear();
				strm >> strVal;
				if(0 < strVal.length() || strm.good()){
					break;
				}
			}
			PRN("chmstream(normal 32):           \"reply\" => \"%s\"", strVal.c_str());
		}
		sleep(1);
		{	// normal(8192)
			chmstream	strm(&chmobj);
			strKey = STREAM_KEY_STR2;

			strm << strKey << endl;
			strm << strVal8192 << endl;

			PRN("chmstream(normal 8192):         \"%s\" => \"%s\"", strKey.c_str(), strVal8192.c_str());

			while(true){
				strm.clear();
				strm >> strVal;
				if(0 < strVal.length() || strm.good()){
					break;
				}
			}
			PRN("chmstream(normal 8192):         \"reply\" => \"%s\"", strVal.c_str());
		}
		sleep(1);
		{	// direct key(this same as normal)
			chmstream	strm(&chmobj, string(STREAM_DIR_KEY_STR));

			strm << strVal32 << endl;

			PRN("chmstream(dir key 32):          \"%s\" => \"%s\"", STREAM_DIR_KEY_STR, strVal32.c_str());

			while(true){
				strm.clear();
				strm >> strVal;
				if(0 < strVal.length() || strm.good()){
					break;
				}
			}
			PRN("chmstream(dir key 32):          \"reply\" => \"%s\"", strVal.c_str());
		}
		sleep(1);
		{	// direct key 8192(this same as normal)
			chmstream	strm(&chmobj, string(STREAM_DIR_KEY_STR2));

			strm << strVal8192 << endl;

			PRN("chmstream(dir key 8192):        \"%s\" => \"%s\"", STREAM_DIR_KEY_STR, strVal8192.c_str());

			while(true){
				strm.clear();
				strm >> strVal;
				if(0 < strVal.length() || strm.good()){
					break;
				}
			}
			PRN("chmstream(dir key 8192):        \"reply\" => \"%s\"", strVal.c_str());
		}
		sleep(1);
		{	// hash(this same as normal)
			chmstream	strm(&chmobj, STREAM_DIR_HASH_VAL);

			strm << strVal32 << endl;

			PRN("chmstream(hash 32):             \"HASH\" => \"%s\"", strVal32.c_str());

			while(true){
				strm.clear();
				strm >> strVal;
				if(0 < strVal.length() || strm.good()){
					break;
				}
			}
			PRN("chmstream(hash 32):             \"reply\" => \"%s\"", strVal.c_str());
		}
		sleep(1);
		{	// hash 8192(this same as normal)
			chmstream	strm(&chmobj, STREAM_DIR_HASH_VAL);

			strm << strVal8192 << endl;

			PRN("chmstream(hash 8192):           \"HASH\" => \"%s\"", strVal8192.c_str());

			while(true){
				strm.clear();
				strm >> strVal;
				if(0 < strVal.length() || strm.good()){
					break;
				}
			}
			PRN("chmstream(hash 8192):           \"reply\" => \"%s\"", strVal.c_str());
		}
		sleep(1);
	}

	PRN("-------------------------------------------------------");
	PRN("TEST Slave side : value type");
	PRN("-------------------------------------------------------");
	{
		{	// char
			chmstream	strm(&chmobj);
			strKey		= VALTYPE_TEST_KEY_CHR;
			char	buff= VALTYPE_TEST_VAL_CHR;

			strm << strKey << endl;
			strm << buff << endl;

			PRN("chmstream(char):                \"%s\" => \"%c\"", strKey.c_str(), buff);

			strVal.erase();
			while(true){
				strm.clear();
				strm >> strVal;
				if(0 < strVal.length() || strm.good()){
					break;
				}
			}
			PRN("chmstream(char):                \"reply\" => \"%s\"", strVal.c_str());
		}
		sleep(1);
		{	// int
			chmstream	strm(&chmobj);
			strKey		= VALTYPE_TEST_KEY_INT;
			int		ival= VALTYPE_TEST_VAL_INT;

			strm << strKey << endl;
			strm << ival << endl;

			PRN("chmstream(int):                 \"%s\" => \"%d\"", strKey.c_str(), ival);

			strVal.erase();
			while(true){
				strm.clear();
				strm >> strVal;
				if(0 < strVal.length() || strm.good()){
					break;
				}
			}
			PRN("chmstream(int):                 \"reply\" => \"%s\"", strVal.c_str());
		}
		sleep(1);
		{	// long
			chmstream	strm(&chmobj);
			strKey		= VALTYPE_TEST_KEY_LNG;
			long	lval= VALTYPE_TEST_VAL_LNG;

			strm << strKey << endl;
			strm << lval << endl;

			PRN("chmstream(long):                \"%s\" => \"%ld\"", strKey.c_str(), lval);

			strVal.erase();
			while(true){
				strm.clear();
				strm >> strVal;
				if(0 < strVal.length() || strm.good()){
					break;
				}
			}
			PRN("chmstream(long):                \"reply\" => \"%s\"", strVal.c_str());
		}
		sleep(1);
		{	// seekg
			chmstream	strm(&chmobj);
			strKey		= VALTYPE_TEST_KEY_SKG;

			strm << strKey << endl;
			strm << strVal32 << endl;

			PRN("chmstream(seekg):               \"%s\" => \"%s\"", strKey.c_str(), strVal32.c_str());

			strVal.erase();
			while(true){
				char	buff = -1;				// tmp
				strm.clear();
				strm >> buff;
				if(buff != -1 || strm.good()){
					break;
				}
			}
			strm.seekg(20, std::ios_base::beg);
			strm >> strVal;

			PRN("chmstream(seekg):               \"reply\" => \"%s\"", strVal.c_str());
		}
		sleep(1);
		{	// seekp
			chmstream	strm(&chmobj);
			strKey		= VALTYPE_TEST_NKEY_SKP;

			strm << strKey << endl;
			strm.seekp(12, std::ios_base::beg);		// back to NG word
			strm << endl;							// set key end
			strm << strVal32 << endl;

			PRN("chmstream(seekp):               \"%s\" => \"%s\"", strKey.c_str(), strVal32.c_str());

			strVal.erase();
			while(true){
				strm.clear();
				strm >> strVal;
				if(0 < strVal.length() || strm.good()){
					break;
				}
			}
			PRN("chmstream(seekp):               \"reply\" => \"%s\"", strVal.c_str());
		}
	}
	return true;
}

//---------------------------------------------------------
// Functions
//---------------------------------------------------------
// Parse parameters
//
// -f [file name]    Configuration file path
// -d [debug level]  "ERR" / "WAN" / "INF" / "DUMP"
// -h                display help
//
static void Help(const char* progname)
{
	printf("Usage: %s [options]\n", progname ? progname : "program");
	printf("Option  -s|-c                   Specify server side or slave side process.\n");
	printf("        -conf [file name]       Configuration file( .ini / .json / .yaml ) path\n");
	printf("        -json [json string]     Configuration JSON string\n");
	printf("        -d [debug level]        \"ERR\" / \"WAN\" / \"INF\" / \"DUMP\"\n");
	printf("        -h                      display help\n");
}

//---------------------------------------------------------
// Main
//---------------------------------------------------------
int main(int argc, char** argv)
{
	ChmOpts		opts((argc - 1), const_cast<const char**>(&argv[1]));
	ChmCntrl	chmobj;

	// help
	if(opts.Find("h") || opts.Find("help")){
		const char*	pprgname = basename(argv[0]);
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
		}else if(0 == strcasecmp(dbgmode.c_str(), "DUMP")){
			SetChmDbgMode(CHMDBG_DUMP);
		}else{
			ERR_CHMPRN("Wrong parameter value \"-d\"(\"-g\") %s.", dbgmode.c_str());
			exit(EXIT_FAILURE);
		}
	}

	// configuration file or json
	string	config;
	if(!opts.Get("conf", config) && !opts.Get("f", config) && !opts.Get("json", config)){
		//PRN("There is no -conf and -json option, thus check environment automatically.");
		if(!getenv_chm_conffile(config) && !getenv_chm_jsonconf(config)){
			ERR_CHMPRN("There is no option(-conf or -json).");
		}
	}

	// Mode
	bool	is_server_side = false;
	if(opts.Find("s")){
		is_server_side = true;
	}else if(opts.Find("c")){
		is_server_side = false;
	}else{
		ERR("Parameter \"-s\" or \"-c\" must be set.");
		exit(EXIT_FAILURE);
	}

	// Message
	PRN("-------------------------------------------------------");
	PRN("    Test chmstream class");
	PRN("-------------------------------------------------------");
	PRN(" *** NOTICE ***");
	PRN(" You MUST set REPLICA as 0 in all server configuration file.");
	PRN("");
	PRN("-------------------------------------------------------");
	PRN("Test process run on:                    %s",		is_server_side ? "server side" : "slave side");
	PRN("Configuration file or json:             %s",	config.c_str());
	PRN("-------------------------------------------------------");

	// Initialize
	bool	result;
	if(is_server_side){
		result = chmobj.InitializeOnServer(config.c_str(), true);		// auto rejoin
	}else{
		result = chmobj.InitializeOnSlave(config.c_str(), true);		// auto rejoin
	}
	if(!result){
		ERR_CHMPRN("Could not initialize internal object/data/etc.");
		chmobj.Clean();
		exit(EXIT_FAILURE);
	}

	// Run test
	if(is_server_side){
		result = TestServerSide(chmobj);
	}else{
		result = TestSlaveSide(chmobj);
	}

	// Result
	PRN("-------------------------------------------------------");
	PRN("Test Result:                            %s", result ? "Succeed" : "Failed");
	PRN("-------------------------------------------------------");
	if(!result){
		ERR_CHMPRN("Failed test.");
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
