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
#ifndef	CHMKVP_H
#define	CHMKVP_H

#include "chmpx.h"
#include "chmhash.h"

//---------------------------------------------------------
// ChmBinData Class
//---------------------------------------------------------
class ChmKVPair;

class ChmBinData
{
	friend class ChmKVPair;

	protected:
		unsigned char*	byptr;
		size_t			length;
		bool			is_allocate;

	public:
		explicit ChmBinData(unsigned char* bydata = NULL, size_t bylength = 0L, bool is_duplicate = false);
		ChmBinData(PCHMBIN pchmbin, bool is_duplicate);
		virtual ~ChmBinData();

		void Clear(void);
		bool IsEmpty(void) const { return (NULL == byptr); }

		const unsigned char* Get(size_t* plength) const;
		const char* Get(void) const;

		bool Set(unsigned char* bydata, size_t bylength, bool is_duplicate = false);
		bool Set(PCHMBIN pchmbin, bool is_duplicate = false);
		bool Overwrite(unsigned char* bydata, size_t bylength, off_t offset);
		bool Overwrite(PCHMBIN pchmbin, off_t offset);
		bool Append(unsigned char* bydata, size_t bylength) { return Overwrite(bydata, bylength, static_cast<off_t>(length)); }
		bool Append(PCHMBIN pchmbin) { return Overwrite(pchmbin, static_cast<off_t>(length)); }

		bool Load(unsigned char* bydata, bool is_cvt_ntoh = true, bool is_duplicate = false);
		bool Put(unsigned char* bydata, bool is_cvt_hton = true) const;
		unsigned char* Put(size_t& bylength, bool is_cvt_hton = true) const;

		bool Duplicate(const ChmBinData& other);
		bool Copy(const ChmBinData& other);
		chmhash_t GetHash(void) const;
};

//---------------------------------------------------------
// ChmKVPair Class
//---------------------------------------------------------
class ChmKVPair
{
	protected:
		ChmBinData	Key;
		ChmBinData	Value;

	public:
		explicit ChmKVPair(unsigned char* bykey = NULL, size_t keylen = 0L, unsigned char* byval = NULL, size_t vallen = 0L, bool is_duplicate = false);
		ChmKVPair(const ChmBinData* pKey, const ChmBinData* pValue, bool is_duplicate = false);
		ChmKVPair(PCHMKVP pkvp, bool is_duplicate);
		virtual ~ChmKVPair();

		void Clear(void);

		const unsigned char* Get(size_t* plength, bool is_key) const;
		const char* Get(bool is_key) const;
		const unsigned char* GetKey(size_t* plength) const { return Get(plength, true); }
		const unsigned char* GetValue(size_t* plength) const { return Get(plength, false); }
		const char* GetKey(void) const { return Get(true); }
		const char* GetValue(void) const { return Get(false); }

		bool Set(unsigned char* bydata, size_t bylength, bool is_key, bool is_duplicate = false);
		bool Set(const ChmBinData& Data, bool is_key, bool is_duplicate = false);
		bool Set(PCHMKVP pkvp, bool is_duplicate = false);
		bool Overwrite(unsigned char* bydata, size_t bylength, bool is_key, off_t offset);
		bool Overwrite(const ChmBinData& Data, bool is_key, off_t offset);
		bool Append(unsigned char* bydata, size_t bylength, bool is_key) { return Overwrite(bydata, bylength, is_key, static_cast<off_t>(is_key ? Key.length : Value.length)); }
		bool Append(const ChmBinData& Data, bool is_key) { return Overwrite(Data, is_key, static_cast<off_t>(is_key ? Key.length : Value.length)); }

		bool SetKey(unsigned char* bydata, size_t bylength, bool is_duplicate = false) { return Set(bydata, bylength, true, is_duplicate); }
		bool SetKey(const ChmBinData& Data, bool is_duplicate = false) { return Set(Data, true, is_duplicate); }
		bool OverwriteKey(unsigned char* bydata, size_t bylength, off_t offset) { return Overwrite(bydata, bylength, true, offset); }
		bool OverwriteKey(const ChmBinData& Data, bool is_key, off_t offset) { return Overwrite(Data, true, offset); }
		bool AppendKey(unsigned char* bydata, size_t bylength) { return Overwrite(bydata, bylength, true, static_cast<off_t>(Key.length)); }
		bool AppendKey(const ChmBinData& Data) { return Overwrite(Data, true, static_cast<off_t>(Key.length)); }

		bool SetValue(unsigned char* bydata, size_t bylength, bool is_duplicate = false) { return Set(bydata, bylength, false, is_duplicate); }
		bool SetValue(const ChmBinData& Data, bool is_duplicate = false) { return Set(Data, false, is_duplicate); }
		bool OverwriteValue(unsigned char* bydata, size_t bylength, off_t offset) { return Overwrite(bydata, bylength, false, offset); }
		bool OverwriteValue(const ChmBinData& Data, bool is_key, off_t offset) { return Overwrite(Data, false, offset); }
		bool AppendValue(unsigned char* bydata, size_t bylength) { return Overwrite(bydata, bylength, false, static_cast<off_t>(Value.length)); }
		bool AppendValue(const ChmBinData& Data) { return Overwrite(Data, false, static_cast<off_t>(Value.length)); }

		bool Load(unsigned char* bydata, bool is_cvt_ntoh = true, bool is_duplicate = false);
		bool Put(unsigned char* bydata, bool is_cvt_hton = true) const;
		unsigned char* Put(size_t& bylength, bool is_cvt_hton = true) const;

		chmhash_t GetHash(void) const;
};

#endif	// CHMKVP_H

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noexpandtab sw=4 ts=4 fdm=marker
 * vim<600: noexpandtab sw=4 ts=4
 */
