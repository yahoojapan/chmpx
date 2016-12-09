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
 * CREATE:   Thu Nor 17 2016
 * REVISION:
 *
 */

#ifndef	CHMCONFUTIL_H
#define	CHMCONFUTIL_H

#include <yaml.h>
#include <vector>
#include <string>
#include "chmcommon.h"

//---------------------------------------------------------
// Symbols
//---------------------------------------------------------
// Environment
#define	CHM_CONFFILE_ENV_NAME			"CHMCONFFILE"
#define	CHM_JSONCONF_ENV_NAME			"CHMJSONCONF"

//---------------------------------------------------------
// Enum
//---------------------------------------------------------
// enum for check_chmconf_type(_ex) functions
//
typedef enum chm_conf_type{
	CHMCONF_TYPE_UNKNOWN = -1,
	CHMCONF_TYPE_NULL = 0,
	CHMCONF_TYPE_INI_FILE,
	CHMCONF_TYPE_YAML_FILE,
	CHMCONF_TYPE_JSON_FILE,
	CHMCONF_TYPE_JSON_STRING
}CHMCONFTYPE;

//---------------------------------------------------------
// Utility Functions
//---------------------------------------------------------
DECL_EXTERN_C_START

bool right_check_json_string(const char* target);
bool have_env_chm_conf(void);

DECL_EXTERN_C_END

bool extract_conf_value(std::string& value);
bool getenv_chm_conffile(std::string& value);
bool getenv_chm_jsonconf(std::string& value);
CHMCONFTYPE check_chmconf_type(const char* config);
CHMCONFTYPE check_chmconf_type_ex(const char* config, const char* env_conf_name, const char* env_json_name, std::string* normalize_config);

//---------------------------------------------------------
// Structure for CHMYamlDataStack
//---------------------------------------------------------
typedef struct chm_yaml_data_pair_type{
	yaml_event_type_t	first;
	yaml_event_type_t	second;

	chm_yaml_data_pair_type(yaml_event_type_t first_type = YAML_NO_EVENT) : first(first_type), second(YAML_NO_EVENT) {}

}CHM_YAML_DATAPAIR;

typedef std::vector<CHM_YAML_DATAPAIR>	chm_yaml_datas_t;

//---------------------------------------------------------
// Class CHMYamlDataStack
//---------------------------------------------------------
class CHMYamlDataStack
{
	private:
		chm_yaml_datas_t	stack;

	public:
		CHMYamlDataStack() {}
		virtual ~CHMYamlDataStack() {}

		bool empty(void) const { return stack.empty(); }
		bool add(const yaml_event_type_t& type);
};

#endif	// CHMCONFUTIL_H

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
