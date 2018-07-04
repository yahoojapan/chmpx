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
 * CREATE:   Mon Feb 15 2016
 * REVISION:
 *
 */

#ifndef	CHMLOCKMAP_TCC
#define	CHMLOCKMAP_TCC

#include <fullock/flckbaselist.tcc>
#include <map>
#include <list>

//---------------------------------------------------------
// chm_lock_map class template
//---------------------------------------------------------
template<typename key_type, typename val_type>
class chm_lock_map
{
	public:
		typedef typename std::map<key_type, val_type>	st_type;
		typedef typename std::vector<key_type>			st_key_list;
		typedef typename st_type::iterator				iterator;
		typedef typename st_type::const_iterator		const_iterator;
		typedef bool (*chm_lock_map_erase_cb)(iterator&, void*);

	public:											// public for using directly.
		st_type					basemap;
		volatile int			lockval;

	protected:
		chm_lock_map_erase_cb	pbasefunc;			// default callback function for erasing each element
		void*					pbaseparam;			// default parameter
		val_type				emptyval;

	protected:
		// do not use this
		chm_lock_map(void) : lockval(FLCK_NOSHARED_MUTEX_VAL_UNLOCKED), pbasefunc(NULL), pbaseparam(NULL) {}

	public:
		chm_lock_map(val_type initval, chm_lock_map_erase_cb pfunc = NULL, void* pparam = NULL) : lockval(FLCK_NOSHARED_MUTEX_VAL_UNLOCKED), pbasefunc(pfunc), pbaseparam(pparam), emptyval(initval) {}
		virtual ~chm_lock_map(void) { clear(); }

		inline size_t count(void);
		inline val_type operator[](const key_type& key);
		inline val_type get(const key_type& key);
		inline bool set(const key_type& key, val_type val, bool allow_ow = false);
		inline bool find(const key_type& key);
		inline void get_keys(st_key_list& list);
		inline bool erase(const key_type& key);
		inline bool erase(const key_type& key, chm_lock_map_erase_cb pfunc, void* pparam);
		inline bool clear(void);
		inline bool clear(chm_lock_map_erase_cb pfunc, void* pparam);
};

//---------------------------------------------------------
// chm_lock_map methods
//---------------------------------------------------------
template<typename key_type, typename val_type>
inline size_t chm_lock_map<key_type, val_type>::count(void)
{
	while(!fullock::flck_trylock_noshared_mutex(&lockval));
	size_t	count = basemap.size();
	fullock::flck_unlock_noshared_mutex(&lockval);
	return count;
}

//
// This method returns not referance to value, because if returns it,
// this methos must lock.
// So this methos returns copied value.
//
template<typename key_type, typename val_type>
inline val_type chm_lock_map<key_type, val_type>::operator[](const key_type& key)
{
	while(!fullock::flck_trylock_noshared_mutex(&lockval));

	iterator iter = basemap.find(key);
	if(basemap.end() == iter){
		fullock::flck_unlock_noshared_mutex(&lockval);
		return emptyval;
	}
	val_type	value = iter->second;
	fullock::flck_unlock_noshared_mutex(&lockval);
	return value;
}

//
// This method returns not referance to value, because if returns it,
// this methos must lock.
// So this methos returns copied value.
//
template<typename key_type, typename val_type>
inline val_type chm_lock_map<key_type, val_type>::get(const key_type& key)
{
	while(!fullock::flck_trylock_noshared_mutex(&lockval));

	iterator iter = basemap.find(key);
	if(basemap.end() == iter){
		fullock::flck_unlock_noshared_mutex(&lockval);
		return emptyval;
	}
	val_type	value = iter->second;
	fullock::flck_unlock_noshared_mutex(&lockval);
	return value;
}

template<typename key_type, typename val_type>
inline bool chm_lock_map<key_type, val_type>::set(const key_type& key, val_type val, bool allow_ow)
{
	while(!fullock::flck_trylock_noshared_mutex(&lockval));

	iterator iter = basemap.find(key);
	if(basemap.end() != iter){
		if(!allow_ow){
			fullock::flck_unlock_noshared_mutex(&lockval);
			return false;
		}
		if(pbasefunc && !pbasefunc(iter, pbaseparam)){
			fullock::flck_unlock_noshared_mutex(&lockval);
			return false;
		}
	}
	basemap[key] = val;
	fullock::flck_unlock_noshared_mutex(&lockval);
	return true;
}

template<typename key_type, typename val_type>
inline bool chm_lock_map<key_type, val_type>::find(const key_type& key)
{
	bool	result = true;

	while(!fullock::flck_trylock_noshared_mutex(&lockval));
	if(basemap.end() == basemap.find(key)){
		result = false;
	}
	fullock::flck_unlock_noshared_mutex(&lockval);

	return result;
}

template<typename key_type, typename val_type>
inline void chm_lock_map<key_type, val_type>::get_keys(st_key_list& list)
{
	while(!fullock::flck_trylock_noshared_mutex(&lockval));
	for(const_iterator iter = basemap.begin(); iter != basemap.end(); ++iter){
		list.push_back(iter->first);
	}
	fullock::flck_unlock_noshared_mutex(&lockval);
}

template<typename key_type, typename val_type>
inline bool chm_lock_map<key_type, val_type>::erase(const key_type& key)
{
	while(!fullock::flck_trylock_noshared_mutex(&lockval));

	iterator iter = basemap.find(key);
	if(basemap.end() == iter){
		fullock::flck_unlock_noshared_mutex(&lockval);
		return false;
	}
	if(pbasefunc && !pbasefunc(iter, pbaseparam)){
		fullock::flck_unlock_noshared_mutex(&lockval);
		return false;
	}
	basemap.erase(iter);

	fullock::flck_unlock_noshared_mutex(&lockval);

	return true;
}

template<typename key_type, typename val_type>
inline bool chm_lock_map<key_type, val_type>::erase(const key_type& key, chm_lock_map_erase_cb pfunc, void* pparam)
{
	while(!fullock::flck_trylock_noshared_mutex(&lockval));

	iterator iter = basemap.find(key);
	if(basemap.end() == iter){
		fullock::flck_unlock_noshared_mutex(&lockval);
		return false;
	}
	if(pfunc && !pfunc(iter, pparam)){
		fullock::flck_unlock_noshared_mutex(&lockval);
		return false;
	}
	basemap.erase(iter);

	fullock::flck_unlock_noshared_mutex(&lockval);

	return true;
}

template<typename key_type, typename val_type>
inline bool chm_lock_map<key_type, val_type>::clear(void)
{
	while(!fullock::flck_trylock_noshared_mutex(&lockval));
	for(iterator iter = basemap.begin(); iter != basemap.end(); basemap.erase(iter++)){
		if(pbasefunc && !pbasefunc(iter, pbaseparam)){
			fullock::flck_unlock_noshared_mutex(&lockval);
			return false;
		}
	}
	fullock::flck_unlock_noshared_mutex(&lockval);
	return true;
}

template<typename key_type, typename val_type>
inline bool chm_lock_map<key_type, val_type>::clear(chm_lock_map_erase_cb pfunc, void* pparam)
{
	while(!fullock::flck_trylock_noshared_mutex(&lockval));
	for(iterator iter = basemap.begin(); iter != basemap.end(); basemap.erase(iter++)){
		if(pfunc && !pfunc(iter, pparam)){
			fullock::flck_unlock_noshared_mutex(&lockval);
			return false;
		}
	}
	fullock::flck_unlock_noshared_mutex(&lockval);
	return true;
}

#endif	// CHMLOCKMAP_TCC

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
