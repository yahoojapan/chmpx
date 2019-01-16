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
#ifndef	CHMOPTS_H
#define	CHMOPTS_H

#include "chmutil.h"

//---------------------------------------------------------
// ChmOpts Class
//---------------------------------------------------------
class ChmOpts
{
	protected:
		strlstmap_t	optmap;
		std::string	sepchars;

	public:
		ChmOpts(int argc = 0, char** argv = NULL, const char* strsepchars = NULL);
		virtual ~ChmOpts();

		bool Initialize(int argc, char** argv);
		bool Get(const char* popt, std::string& param);
		bool Get(const char* popt, strlst_t& params);
		bool Find(const char* popt) const;
		long Count(void) const { return static_cast<long>(optmap.size()); }
};

#endif	// CHMOPTS_H

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */

