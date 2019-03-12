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
 * CREATE:   Thu Nor 17 2016
 * REVISION:
 *
 */

#include <fullock/flckutil.h>
#include <k2hash/k2hutil.h>

#include "chmconfutil.h"
#include "chmutil.h"
#include "chmdbg.h"

using namespace std;

//---------------------------------------------------------
// Utility Functions
//---------------------------------------------------------
// [NOTE]
// This function checks target string which is json format for not file path.
//
bool right_check_json_string(const char* target)
{
	if(CHMEMPTYSTR(target)){
		return false;
	}
	for(const char* ptr = target; ptr && '\0' != *ptr; ++ptr){
		if(0 == isspace(*ptr) && '\\' != *ptr){
			if('{' == *ptr || '[' == *ptr || ':' == *ptr || '}' == *ptr || ']' == *ptr || '\\' == *ptr || '\'' == *ptr || '\"' == *ptr){
				return true;
			}
			break;
		}
	}
	return false;
}

//
// extract for configuration ini file
//
bool extract_conf_value(string& value)
{
	value = trim(value);	// trim

	string		result("");
	bool		is_single_quart = false;
	bool		is_double_quart = false;
	bool		is_escape_char	= false;
	bool		is_before_space	= false;
	for(const char* ptr = value.c_str(); ptr && '\0' != *ptr; ++ptr){
		if(is_single_quart){
			if('\"' == *ptr){
				result			+= *ptr;
			}else if('\'' == *ptr){
				is_single_quart	= false;			// end of single quart area
			}else if('#' == *ptr){
				result			+= *ptr;			// not comment
			}else if('\\' == *ptr){
				result			+= *ptr;			// not to escape
			// cppcheck-suppress unmatchedSuppression
			// cppcheck-suppress duplicateBranch
			}else if(0 != isspace(*ptr)){
				result			+= *ptr;
			}else{
				result			+= *ptr;
			}
		}else if(is_double_quart){
			if('\"' == *ptr){
				if(is_escape_char){
					result			+= *ptr;
					is_escape_char	= false;
				}else{
					is_double_quart	= false;		// end of double quart area
				}
			}else if('\'' == *ptr){
				if(is_escape_char){
					result			+= '\\';
					is_escape_char	= false;
				}
				result				+= *ptr;
			}else if('#' == *ptr){
				if(is_escape_char){
					result			+= '\\';
					is_escape_char	= false;
				}
				result				+= *ptr;
			}else if('\\' == *ptr){
				if(is_escape_char){
					result			+= *ptr;
					is_escape_char	= false;
				}else{
					is_escape_char	= true;
				}
			// cppcheck-suppress unmatchedSuppression
			// cppcheck-suppress duplicateBranch
			}else if(0 != isspace(*ptr)){
				if(is_escape_char){
					result			+= '\\';
					is_escape_char	= false;
				}
				result				+= *ptr;
			}else{
				if(is_escape_char){
					result			+= '\\';
					is_escape_char	= false;
				}
				result				+= *ptr;
			}
		}else{
			if('\"' == *ptr){
				if(is_escape_char){
					result			+= *ptr;
					is_escape_char	= false;
				}else{
					is_double_quart	= true;
				}
			}else if('\'' == *ptr){
				if(is_escape_char){
					result			+= *ptr;
					is_escape_char	= false;
				}else{
					is_single_quart	= true;
				}
			}else if('#' == *ptr){
				if(!is_before_space){
					// need "not space" before '#'
					result += *ptr;
				}else{
					// after here is all comment.
					break;
				}
			}else if('\\' == *ptr){
				if(is_escape_char){
					result			+= *ptr;
					is_escape_char	= false;
				}else{
					is_escape_char	= true;
				}
			}else if(0 != isspace(*ptr)){
				result			+= *ptr;
				is_before_space	= true;
				is_escape_char	= false;
			}else{
				result			+= *ptr;
				is_before_space	= false;
				is_escape_char	= false;
			}
		}
	}
	if(is_single_quart || is_double_quart || is_escape_char){
		ERR_CHMPRN("Could not extract string(%s)", result.c_str());
		return false;
	}
	value = trim(result);
	return true;
}

//
// Environment
//
bool have_env_chm_conf(void)
{
	string	value("");
	return ((k2h_getenv(CHM_CONFFILE_ENV_NAME, value) && !value.empty()) || (k2h_getenv(CHM_JSONCONF_ENV_NAME, value) && !value.empty()));
}

bool getenv_chm_conffile(string& value)
{
	value.clear();
	return (k2h_getenv(CHM_CONFFILE_ENV_NAME, value) && !value.empty());
}

bool getenv_chm_jsonconf(string& value)
{
	value.clear();
	return (k2h_getenv(CHM_JSONCONF_ENV_NAME, value) && !value.empty());
}

//
// Check configuration type
//
CHMCONFTYPE check_chmconf_type(const char* config)
{
	return check_chmconf_type_ex(config, NULL, NULL, NULL);
}

//
// Check configuration type with environments
//
CHMCONFTYPE check_chmconf_type_ex(const char* config, const char* env_conf_name, const char* env_json_name, string* normalize_config)
{
	string	tmpconfig("");
	if(CHMEMPTYSTR(config)){
		// check environment if needs.
		if(	!(!CHMEMPTYSTR(env_conf_name) && k2h_getenv(env_conf_name, tmpconfig) && !tmpconfig.empty()) &&
			!(!CHMEMPTYSTR(env_json_name) && k2h_getenv(env_json_name, tmpconfig) && !tmpconfig.empty()) )
		{
			return CHMCONF_TYPE_NULL;
		}
	}else{
		tmpconfig = config;
	}

	// check type(file or json string)
	CHMCONFTYPE	result_type = CHMCONF_TYPE_UNKNOWN;
	if(right_check_json_string(tmpconfig.c_str())){
		// json string type
		MSG_CHMPRN("configuration parameter is json string.");
		result_type = CHMCONF_TYPE_JSON_STRING;
	}else{
		// file type
		MSG_CHMPRN("configuration parameter(%s) is file path.", tmpconfig.c_str());

		if(!is_file_safe_exist(tmpconfig.c_str())){
			ERR_CHMPRN("configuration file %s is not existed.", tmpconfig.c_str());
			result_type = CHMCONF_TYPE_UNKNOWN;
		}else{
			char*	pTmp = strdup(tmpconfig.c_str());
			char*	base = basename(pTmp);
			char*	pos;
			if(CHMEMPTYSTR(base) || NULL == (pos = strrchr(base, '.'))){
				WAN_CHMPRN("configuration file %s does not have file extension, thus type is set default(INI).", tmpconfig.c_str());
				result_type = CHMCONF_TYPE_INI_FILE;
			}else{
				++pos;
				if(0 == strcasecmp(pos, "ini")){
					result_type = CHMCONF_TYPE_INI_FILE;
				}else if(0 == strcasecmp(pos, "json")){
					result_type = CHMCONF_TYPE_JSON_FILE;
				}else if(0 == strcasecmp(pos, "yaml") || 0 == strcasecmp(pos, "yml")){
					result_type = CHMCONF_TYPE_YAML_FILE;
				}else{
					ERR_CHMPRN("configuration file %s extension is unknown(should be .ini .json .yaml).", tmpconfig.c_str());
					result_type = CHMCONF_TYPE_UNKNOWN;
				}
			}
			CHM_Free(pTmp);
		}
	}

	if(CHMCONF_TYPE_UNKNOWN != result_type && normalize_config){
		(*normalize_config) = tmpconfig;
	}
	return result_type;
}

//---------------------------------------------------------
// Helper Class CHMYamlDataStack
//---------------------------------------------------------
//
// utility macros
//
#define	CHMYAML_STACK_IS_NOTYPE(type)				(	YAML_NO_EVENT				== type )

#define	CHMYAML_STACK_IS_OPENED_TYPE(type)			(	YAML_STREAM_START_EVENT		== type || \
														YAML_DOCUMENT_START_EVENT	== type || \
														YAML_MAPPING_START_EVENT	== type || \
														YAML_SEQUENCE_START_EVENT	== type )


#define	CHMYAML_STACK_IS_CLOSED_TYPE(type)			(	YAML_STREAM_END_EVENT		== type || \
														YAML_DOCUMENT_END_EVENT		== type || \
														YAML_MAPPING_END_EVENT		== type || \
														YAML_SEQUENCE_END_EVENT		== type || \
														YAML_SCALAR_EVENT			== type || \
														YAML_ALIAS_EVENT			== type )

#define	CHMYAML_STACK_GET_SYMMETRY_TYPE(type)		(	YAML_NO_EVENT				== type ? YAML_NO_EVENT				: \
														YAML_STREAM_START_EVENT		== type ? YAML_STREAM_END_EVENT		: \
														YAML_STREAM_END_EVENT		== type ? YAML_STREAM_START_EVENT	: \
														YAML_DOCUMENT_START_EVENT	== type ? YAML_DOCUMENT_END_EVENT	: \
														YAML_DOCUMENT_END_EVENT		== type ? YAML_DOCUMENT_START_EVENT	: \
														YAML_MAPPING_START_EVENT	== type ? YAML_MAPPING_END_EVENT	: \
														YAML_MAPPING_END_EVENT		== type ? YAML_MAPPING_START_EVENT	: \
														YAML_SEQUENCE_START_EVENT	== type ? YAML_SEQUENCE_END_EVENT	: \
														YAML_SEQUENCE_END_EVENT		== type ? YAML_SEQUENCE_START_EVENT	: \
														YAML_SCALAR_EVENT			== type ? YAML_SCALAR_EVENT			: \
														YAML_ALIAS_EVENT			== type ? YAML_ALIAS_EVENT			: YAML_NO_EVENT	)

#define	CHMYAML_STACK_CHECK_SYMMETRY_TYPE(type1, type2)	(type1 == CHMYAML_STACK_GET_SYMMETRY_TYPE(type2))

#define	CHMYAML_STACK_IS_EMPTY							stack.empty()
#define	CHMYAML_STACK_FIRST								(stack[stack.size() - 1].first)
#define	CHMYAML_STACK_SECOND							(stack[stack.size() - 1].second)
#define	CHMYAML_STACK_SET_FIRST(type)					stack[stack.size() - 1].first	= type
#define	CHMYAML_STACK_SET_SECOND(type)					stack[stack.size() - 1].second	= type

bool CHMYamlDataStack::add(const yaml_event_type_t& type)
{
	switch(type){ 
		case YAML_NO_EVENT:
			break;

		case YAML_DOCUMENT_START_EVENT:
		case YAML_STREAM_START_EVENT:
			stack.push_back(CHM_YAML_DATAPAIR(type));
			break;

		case YAML_DOCUMENT_END_EVENT:
		case YAML_STREAM_END_EVENT:
			if(CHMYAML_STACK_IS_EMPTY){
				ERR_CHMPRN("Yaml parser stack : there is no start section before end section type.")
				return false;
			}else{
				if(CHMYAML_STACK_CHECK_SYMMETRY_TYPE(type, CHMYAML_STACK_FIRST)){
					ERR_CHMPRN("Yaml parser stack : there is no symmetry section for type.")
					return false;
				}
				stack.pop_back();
			}
			break;

		case YAML_MAPPING_START_EVENT:
		case YAML_SEQUENCE_START_EVENT:
			if(CHMYAML_STACK_IS_EMPTY){
				stack.push_back(CHM_YAML_DATAPAIR(type));

			}else if(!CHMYAML_STACK_IS_NOTYPE(CHMYAML_STACK_SECOND)){
				if(CHMYAML_STACK_IS_CLOSED_TYPE(CHMYAML_STACK_SECOND)){
					WAN_CHMPRN("Yaml parser stack : internal warning - why come here, try to recover...")
					stack.pop_back();
					return add(type);							// Recursive call
				}else{											// CHMYAML_STACK_SECOND is opened
					stack.push_back(CHM_YAML_DATAPAIR(type));
				}
			}else{												// CHMYAML_STACK_SECOND is empty
				if(CHMYAML_STACK_IS_NOTYPE(CHMYAML_STACK_FIRST)){
					WAN_CHMPRN("Yaml parser stack : internal warning - why come here, try to recover...")
					CHMYAML_STACK_SET_FIRST(type);				// overwrite

				}else if(CHMYAML_STACK_IS_CLOSED_TYPE(CHMYAML_STACK_FIRST)){
					CHMYAML_STACK_SET_SECOND(type);

				}else{											// CHMYAML_STACK_FIRST is opened
					stack.push_back(CHM_YAML_DATAPAIR(type));
				}
			}
			break;

		case YAML_MAPPING_END_EVENT:
		case YAML_SEQUENCE_END_EVENT:
			if(CHMYAML_STACK_IS_EMPTY){
				ERR_CHMPRN("Yaml parser stack : there is no start section before end section type.")
				return false;

			}else if(!CHMYAML_STACK_IS_NOTYPE(CHMYAML_STACK_SECOND)){
				if(CHMYAML_STACK_CHECK_SYMMETRY_TYPE(type, CHMYAML_STACK_SECOND)){
					stack.pop_back();

				}else if(CHMYAML_STACK_IS_CLOSED_TYPE(CHMYAML_STACK_SECOND)){
					WAN_CHMPRN("Yaml parser stack : internal warning - why come here, try to recover...")
					stack.pop_back();
					return add(type);							// Recursive call

				}else{											// CHMYAML_STACK_SECOND is opened
					ERR_CHMPRN("Yaml parser stack : internal error - why come here")
					return false;
				}
			}else{												// CHMYAML_STACK_SECOND is empty
				if(CHMYAML_STACK_CHECK_SYMMETRY_TYPE(type, CHMYAML_STACK_FIRST)){
					CHMYAML_STACK_SET_FIRST(type);

				}else if(CHMYAML_STACK_IS_CLOSED_TYPE(CHMYAML_STACK_FIRST)){
					// should close this stack level.
					stack.pop_back();
					return add(type);							// Recursive call

				}else{											// CHMYAML_STACK_FIRST is opened
					ERR_CHMPRN("Yaml parser stack : internal error - why come here")
					return false;
				}
			}
			break;

		case YAML_SCALAR_EVENT:
		case YAML_ALIAS_EVENT:									// not support ALIAS now.
			if(CHMYAML_STACK_IS_EMPTY){
				stack.push_back(CHM_YAML_DATAPAIR(type));

			}else if(!CHMYAML_STACK_IS_NOTYPE(CHMYAML_STACK_SECOND)){
				if(CHMYAML_STACK_IS_CLOSED_TYPE(CHMYAML_STACK_SECOND)){
					WAN_CHMPRN("Yaml parser stack : internal warning - why come here, try to recover...")
					stack.pop_back();
					return add(type);							// Recursive call

				}else{											// CHMYAML_STACK_SECOND is opened
					stack.push_back(CHM_YAML_DATAPAIR(type));
				}
			}else{												// CHMYAML_STACK_SECOND is empty
				if(!CHMYAML_STACK_IS_NOTYPE(CHMYAML_STACK_FIRST)){
					if(CHMYAML_STACK_IS_CLOSED_TYPE(CHMYAML_STACK_FIRST)){
						stack.pop_back();
					}else{										// CHMYAML_STACK_FIRST is opened
						stack.push_back(CHM_YAML_DATAPAIR(type));
					}
				}else{											// CHMYAML_STACK_FIRST is empty
					WAN_CHMPRN("Yaml parser stack : internal warning - why come here, try to recover...")
					CHMYAML_STACK_SET_FIRST(type);
				}
			}
			break;
	}
	return true;
}

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
