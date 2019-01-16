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
#include <k2hash/k2hashfunc.h>

#include "chmcommon.h"
#include "chmpx.h"
#include "chmkvp.h"
#include "chmhash.h"
#include "chmutil.h"
#include "chmdbg.h"

using namespace	std;

//---------------------------------------------------------
// ChmBinData class
//---------------------------------------------------------
ChmBinData::ChmBinData(unsigned char* bydata, size_t bylength, bool is_duplicate) : byptr(NULL), length(0L), is_allocate(false)
{
	if(bydata && 0 < bylength){
		Set(bydata, bylength, is_duplicate);
	}
}

ChmBinData::ChmBinData(PCHMBIN pchmbin, bool is_duplicate) : byptr(NULL), length(0L), is_allocate(false)
{
	Set(pchmbin, is_duplicate);
}

ChmBinData::~ChmBinData()
{
	Clear();
}

void ChmBinData::Clear(void)
{
	if(is_allocate){
		CHM_Free(byptr);
	}
	byptr		= NULL;
	length		= 0L;
	is_allocate	= false;
}

const unsigned char* ChmBinData::Get(size_t* plength) const
{
	if(plength){
		*plength = length;
	}
	return byptr;
}

const char* ChmBinData::Get(void) const
{
	if(byptr && 0 < length && '\0' != byptr[length - 1]){
		// not string
		return NULL;
	}
	return reinterpret_cast<const char*>(byptr);
}

bool ChmBinData::Set(unsigned char* bydata, size_t bylength, bool is_duplicate)
{
	Clear();

	if(bydata && 0 < bylength){
		if(is_duplicate){
			if(NULL == (byptr = reinterpret_cast<unsigned char*>(malloc(bylength)))){
				ERR_CHMPRN("Could not allocate memory.");
				return false;
			}else{
				memcpy(byptr, bydata, bylength);
				length		= bylength;
				is_allocate	= true;
			}
		}else{
			byptr	= bydata;
			length	= bylength;
		}
	}
	return true;
}

bool ChmBinData::Set(PCHMBIN pchmbin, bool is_duplicate)
{
	Clear();

	if(!pchmbin || !pchmbin->byptr || 0L == pchmbin->length){
		return false;
	}
	return Set(pchmbin->byptr, pchmbin->length, is_duplicate);
}

//
// After calling this method, byptr is always allocated.
//
bool ChmBinData::Overwrite(unsigned char* bydata, size_t bylength, off_t offset)
{
	if(!bydata || 0 == bylength){
		MSG_CHMPRN("Parameter is wrong, so nothing copy.");
		return true;
	}

	if(0 >= (static_cast<off_t>(bylength) + offset)){
		MSG_CHMPRN("Parameters(offset + bylength) is short.");
		return true;
	}

	// new size
	size_t	newlen;
	if(length < static_cast<size_t>(offset + static_cast<off_t>(bylength))){
		newlen = static_cast<size_t>(offset) + bylength;
	}else{
		newlen = length;
	}

	// allocate
	if(newlen != length || !is_allocate){
		if(!is_allocate){
			unsigned char*	newptr;
			if(NULL == (newptr = reinterpret_cast<unsigned char*>(malloc(newlen)))){
				ERR_CHMPRN("Could not allocate memory.");
				return false;
			}
			memcpy(newptr, byptr, length);
			byptr		= newptr;
			is_allocate	= true;
		}else{
			if(NULL == (byptr = reinterpret_cast<unsigned char*>(realloc(byptr, newlen)))){
				ERR_CHMPRN("Could not allocate memory.");
				return false;
			}
		}
	}

	// copy
	if(0 <= offset){
		memcpy(&byptr[offset], bydata, bylength);
	}else{
		memcpy(byptr, &bydata[imaxabs(static_cast<intmax_t>(offset))], static_cast<off_t>(bylength) + offset);
	}
	return true;
}

bool ChmBinData::Overwrite(PCHMBIN pchmbin, off_t offset)
{
	if(!pchmbin){
		return false;
	}
	return Overwrite(pchmbin->byptr, pchmbin->length, offset);
}

bool ChmBinData::Load(unsigned char* bydata, bool is_cvt_ntoh, bool is_duplicate)
{
	Clear();

	if(!bydata){
		return true;
	}
	size_t	datapos	= sizeof(size_t);
	size_t*	ptmplen	= reinterpret_cast<size_t*>(bydata);

	if(is_cvt_ntoh){
		length		= be64toh(*ptmplen);			// To host byte order
	}else{
		length		= *ptmplen;
	}
	if(0 < length){
		if(is_duplicate){
			if(NULL == (byptr = reinterpret_cast<unsigned char*>(malloc(length)))){
				ERR_CHMPRN("Could not allocate memory.");
				length	= 0L;
				return false;
			}
			memcpy(byptr, &bydata[datapos], length);
			is_allocate	= true;
		}else{
			byptr		= &bydata[datapos];
			is_allocate	= false;
		}
	}else{
		byptr		= NULL;
		is_allocate	= false;
	}
	return true;
}

bool ChmBinData::Put(unsigned char* bydata, bool is_cvt_hton) const
{
	if(!bydata){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}

	size_t			datapos	= sizeof(size_t);
	size_t			tmplen	= (is_cvt_hton ? htobe64(length) : length);
	unsigned char*	bylen	= reinterpret_cast<unsigned char*>(&tmplen);

	for(size_t cnt = 0; cnt < datapos; ++cnt){
		bydata[cnt] = bylen[cnt];
	}
	if(0 < length){
		memcpy(&bydata[datapos], byptr, length);
	}
	return true;
}

unsigned char* ChmBinData::Put(size_t& bylength, bool is_cvt_hton) const
{
	bylength = length + sizeof(size_t);
	unsigned char*	bydata;
	if(NULL == (bydata = reinterpret_cast<unsigned char*>(malloc(bylength)))){
		ERR_CHMPRN("Could not allocate memory.");
		bylength = 0L;
		return NULL;
	}

	if(!Put(bydata, is_cvt_hton)){
		CHM_Free(bydata);
		bylength = 0L;
		return NULL;
	}
	return bydata;
}

bool ChmBinData::Duplicate(const ChmBinData& other)
{
	return Set(const_cast<unsigned char*>(other.byptr), other.length, true);
}

bool ChmBinData::Copy(ChmBinData& other)
{
	return Set(other.byptr, other.length, false);
}

chmhash_t ChmBinData::GetHash(void) const
{
	if(!byptr || 0L == length){
		return 0L;
	}
	return K2H_HASH_FUNC(reinterpret_cast<const void*>(byptr), length);
}

//---------------------------------------------------------
// ChmKVPair class
//---------------------------------------------------------
ChmKVPair::ChmKVPair(unsigned char* bykey, size_t keylen, unsigned char* byval, size_t vallen, bool is_duplicate) : Key(bykey, keylen, is_duplicate), Value(byval, vallen, is_duplicate)
{
}

ChmKVPair::ChmKVPair(ChmBinData* pKey, ChmBinData* pValue, bool is_duplicate)
{
	if(pKey){
		Set(*pKey, true, is_duplicate);
	}
	if(pValue){
		Set(*pValue, false, is_duplicate);
	}
}

ChmKVPair::ChmKVPair(PCHMKVP pkvp, bool is_duplicate)
{
	Set(pkvp, is_duplicate);
}

ChmKVPair::~ChmKVPair()
{
	Clear();
}

void ChmKVPair::Clear(void)
{
	Key.Clear();
	Value.Clear();
}

const unsigned char* ChmKVPair::Get(size_t* plength, bool is_key) const
{
	const ChmBinData*	pGetObj = (is_key ? &Key : &Value);
	return pGetObj->Get(plength);
}

const char* ChmKVPair::Get(bool is_key) const
{
	const ChmBinData*	pGetObj = (is_key ? &Key : &Value);
	return pGetObj->Get();
}

bool ChmKVPair::Set(unsigned char* bydata, size_t bylength, bool is_key, bool is_duplicate)
{
	ChmBinData*	pSetObj = (is_key ? &Key : &Value);

	if(!bydata || 0L == bylength){
		pSetObj->Clear();
		return true;
	}
	return pSetObj->Set(bydata, bylength, is_duplicate);
}

bool ChmKVPair::Set(ChmBinData& Data, bool is_key, bool is_duplicate)
{
	ChmBinData*	pSetObj = (is_key ? &Key : &Value);

	if(is_duplicate){
		pSetObj->Duplicate(Data);
	}else{
		pSetObj->Copy(Data);
	}
	return true;
}

bool ChmKVPair::Set(PCHMKVP pkvp, bool is_duplicate)
{
	if(!pkvp){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(!Key.Set(&(pkvp->key), is_duplicate) || !Value.Set(&(pkvp->val), is_duplicate)){
		return false;
	}
	return true;
}

bool ChmKVPair::Overwrite(unsigned char* bydata, size_t bylength, bool is_key, off_t offset)
{
	if(!bydata || 0L == bylength){
		return true;
	}
	ChmBinData*	pSetObj = (is_key ? &Key : &Value);

	return pSetObj->Overwrite(bydata, bylength, offset);
}

bool ChmKVPair::Overwrite(ChmBinData& Data, bool is_key, off_t offset)
{
	ChmBinData*	pSetObj = (is_key ? &Key : &Value);

	return pSetObj->Overwrite(Data.byptr, Data.length, offset);
}

bool ChmKVPair::Load(unsigned char* bydata, bool is_cvt_ntoh, bool is_duplicate)
{
	Clear();

	if(!Key.Load(bydata, is_cvt_ntoh, is_duplicate)){
		Clear();
		return false;
	}
	if(!Value.Load((bydata ? &bydata[Key.length + sizeof(size_t)] : bydata), is_cvt_ntoh, is_duplicate)){
		Clear();
		return false;
	}
	return true;
}

bool ChmKVPair::Put(unsigned char* bydata, bool is_cvt_hton) const
{
	if(!bydata){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(!Key.Put(bydata, is_cvt_hton)){
		return false;
	}
	if(!Value.Put(&bydata[Key.length + sizeof(size_t)], is_cvt_hton)){
		return false;
	}
	return true;
}

unsigned char* ChmKVPair::Put(size_t& bylength, bool is_cvt_hton) const
{
	bylength = Key.length + Value.length + sizeof(size_t) * 2;
	unsigned char*	bydata;
	if(NULL == (bydata = reinterpret_cast<unsigned char*>(malloc(bylength)))){
		ERR_CHMPRN("Could not allocate memory.");
		bylength = 0L;
		return NULL;
	}
	if(!Put(bydata, is_cvt_hton)){
		CHM_Free(bydata);
		bylength = 0L;
		return NULL;
	}
	return bydata;
}

chmhash_t ChmKVPair::GetHash(void) const
{
	return Key.GetHash();
}

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
