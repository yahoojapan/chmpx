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
#ifndef	CHMUTIL_H
#define	CHMUTIL_H

#include <string>
#include <vector>
#include <list>
#include <map>
#include <sstream>
#include <fullock/flckutil.h>

#include "chmcommon.h"

//---------------------------------------------------------
// String Utilities
//---------------------------------------------------------
// RFC date
std::string str_rfcdate(time_t date = -1);
time_t rfcdate_time(const char* rfcdate);

// is_xxx
bool is_string_alpha(const char* str);
bool is_string_number(const char* str);

// array / mapping
typedef std::list<std::string>				strlst_t;
typedef std::map<std::string, std::string>	strmap_t;
typedef std::vector<strmap_t>				strmaparr_t;
typedef std::map<std::string, strlst_t>		strlstmap_t;

struct strarr_sort
{
	bool operator()(const std::string& lstr, const std::string& rstr) const
    {
		return lstr < rstr;
    }
};

template<typename T1, typename T2>
struct merge_map
{
	typedef typename std::map<T1, T2>					merge_map_t;
	typedef typename std::map<T1, T2>::const_iterator	merge_map_iterator;

	merge_map(merge_map_t& map1, merge_map_t& map2)
	{
		for(merge_map_iterator iter = map2.begin(); iter != map2.end(); ++iter){
			map1[iter->first] = iter->second;
		}
	}
};
typedef merge_map<std::string, std::string>	merge_strmap;

// separate
bool str_paeser(const char* pbase, strlst_t& strarr, const char* psep = NULL, bool istrim = true);
bool str_split(const char* pbase, strlst_t& strarr, char sep, bool istrim = true);

// others
bool sorted_insert_strmaparr(strmaparr_t& sorted_smaps, strmap_t& smap, const char* pSortKey1, const char* pSortKey2 = NULL);

//---------------------------------------------------------
// Utility Macros
//---------------------------------------------------------
#define	CHM_Free(ptr)	\
		{ \
			if(ptr){ \
				free(ptr); \
				ptr = NULL; \
			} \
		}

#define	CHM_Delete(ptr)	\
		{ \
			if(ptr){ \
				delete ptr; \
				ptr = NULL; \
			} \
		}

#define	CHM_CLOSE(fd)	\
		{ \
			if(CHM_INVALID_HANDLE != fd){ \
				close(fd); \
				fd = CHM_INVALID_HANDLE; \
			} \
		}

#define	CHM_CLOSESOCK(sock)	\
		{ \
			if(CHM_INVALID_SOCK != sock){ \
				close(sock); \
				sock = CHM_INVALID_SOCK; \
			} \
		}

#define	CHM_MUMMAP(fd, base, length)	\
		{ \
			if(NULL != base){ \
				munmap(base, length); \
				base = NULL; \
			} \
			CHM_CLOSE(fd); \
		}

//---------------------------------------------------------
// timespec
//---------------------------------------------------------
#if defined(__cplusplus)
template<typename T> inline void SET_TIMESPEC(T* ptr, time_t sec, long nsec)
{
	(ptr)->tv_sec	= sec;
	if(nsec >= (1000 * 1000 * 1000)){
		(ptr)->tv_sec	+= (nsec / (1000 * 1000 * 1000));
		nsec			=  (nsec % (1000 * 1000 * 1000));
	}
	(ptr)->tv_nsec	= nsec;
}
template<typename T> inline void COPY_TIMESPEC(T* dest, const T* src)
{
	(dest)->tv_sec	= (src)->tv_sec;
	(dest)->tv_nsec	= (src)->tv_nsec;
}
template<typename T> inline int COMPARE_TIMESPEC(const T* data1, const T* data2)
{
	if((data1)->tv_sec > (data2)->tv_sec){
		return 1;
	}else if((data1)->tv_sec < (data2)->tv_sec){
		return -1;
	}else if((data1)->tv_nsec > (data2)->tv_nsec){
		return 1;
	}else if((data1)->tv_nsec < (data2)->tv_nsec){
		return -1;
	}
	return 0;
}
template<typename T> inline void ADD_TIMESPEC(T* base, const T* val)
{
	(base)->tv_sec	+= (val)->tv_sec;
	(base)->tv_nsec	+= (val)->tv_nsec;
	if(1 <= (((base)->tv_nsec) / (1000 * 1000 * 1000))){
		++((base)->tv_sec);
		((base)->tv_nsec) = ((base)->tv_nsec) % (1000 * 1000 * 1000);
	}
}
template<typename T> inline void SUB_TIMESPEC(T* base, const T* val)
{
	// Do not care down flow.
	//
	if((base)->tv_nsec < (val)->tv_nsec){
		--((base)->tv_sec);
		(base)->tv_nsec += 1000 * 1000 * 1000;
	}
	(base)->tv_nsec	-= (val)->tv_nsec;
	(base)->tv_sec	-= (val)->tv_sec;
}
template<typename T> inline void INIT_TIMESPEC(T* ptr)
{
	SET_TIMESPEC(ptr, 0, 0L);
}
template<typename T> inline bool RT_TIMESPEC(T* ptr)
{
	if(-1 == clock_gettime(CLOCK_REALTIME_COARSE, ptr)){
		return false;
	}
	return true;
}
template<typename T> inline void STR_MATE_TIMESPEC(T* ptr)
{
	if(!RT_TIMESPEC(ptr)){
		INIT_TIMESPEC(ptr);
	}
}
template<typename T> inline void FIN_MATE_TIMESPEC(T* ptr)
{
	T	ts;
	if(!RT_TIMESPEC(&ts)){
		INIT_TIMESPEC(&ts);
	}
	SUB_TIMESPEC(&ts, ptr);
	COPY_TIMESPEC(ptr, &ts);
}
template<typename T> inline void FIN_MATE_TIMESPEC2(T* start, T* fin, T* elapsed)
{
	if(!RT_TIMESPEC(fin)){
		INIT_TIMESPEC(fin);
	}
	COPY_TIMESPEC(elapsed, fin);
	SUB_TIMESPEC(elapsed, start);
}

#else	// __cplusplus

#define	SET_TIMESPEC(ptr, sec, nsec)		\
		{ \
			(ptr)->tv_sec	= sec; \
			if(nsec >= (1000 * 1000 * 1000)){ \
				(ptr)->tv_sec	+= (nsec / (1000 * 1000 * 1000)); \
				nsec			=  (nsec % (1000 * 1000 * 1000)); \
			} \
			(ptr)->tv_nsec	= nsec; \
		}
#define	COPY_TIMESPEC(dest, src)			\
		{ \
			(dest)->tv_sec	= (src)->tv_sec; \
			(dest)->tv_nsec	= (src)->tv_nsec; \
		}
#define	COMPARE_TIMESPEC(data1, data2)		(	(data1)->tv_sec		> (data2)->tv_sec 	?	1	: \
												(data1)->tv_sec		< (data2)->tv_sec	?	-1	: \
												(data1)->tv_nsec	> (data2)->tv_nsec	?	1	: \
												(data1)->tv_nsec	< (data2)->tv_nsec	?	-1	:	0	)
#define	ADD_TIMESPEC(base, val)				\
		{ \
			(base)->tv_sec	= (val)->tv_sec; \
			(base)->tv_nsec	= (val)->tv_nsec; \
			if(1 <= (((base)->tv_nsec) / (1000 * 1000 * 1000))){ \
				((base)->tv_sec)++; \
				((base)->tv_nsec) = ((base)->tv_nsec) % (1000 * 1000 * 1000); \
			} \
		}
#define SUB_TIMESPEC(base, val)				\
		{ \
			if((base)->tv_nsec < (val)->tv_nsec){ \
				--((base)->tv_sec); \
				(base)->tv_nsec += 1000 * 1000 * 1000; \
			} \
			(base)->tv_nsec	-= (val)->tv_nsec; \
			(base)->tv_sec	-= (val)->tv_sec; \
		}
#define	INIT_TIMESPEC(ptr)					SET_TIMESPEC(ptr, 0, 0L)
#define RT_TIMESPEC(ptr)					(0 == clock_gettime(CLOCK_REALTIME_COARSE, ptr))
#define STR_MATE_TIMESPEC(ptr)				\
		{ \
			if(!RT_TIMESPEC(ptr)){ \
				INIT_TIMESPEC(ptr); \
			} \
		}
#define FIN_MATE_TIMESPEC(ptr)				\
		{ \
			struct timespec	ts; \
			if(!RT_TIMESPEC(&ts)){ \
				INIT_TIMESPEC(&ts); \
			} \
			SUB_TIMESPEC(&ts, ptr); \
			COPY_TIMESPEC(ptr, &ts); \
		}
#define FIN_MATE_TIMESPEC2(start, fin, elapsed)	\
		{ \
			if(!RT_TIMESPEC(fin)){ \
				INIT_TIMESPEC(fin); \
			} \
			COPY_TIMESPEC(elapsed, fin); \
			SUB_TIMESPEC(elapsed, start); \
		}

#endif	// __cplusplus

//---------------------------------------------------------
// Utility Functions
//---------------------------------------------------------
DECL_EXTERN_C_START

// File 
ssize_t chm_pread(int fd, void *buf, size_t count, off_t offset);
ssize_t chm_pwrite(int fd, const void *buf, size_t count, off_t offset);
unsigned char* chm_read(int fd, size_t* psize);
bool is_file_exist(const char* file);
bool is_file_safe_exist(const char* file);
bool is_file_safe_exist_ex(const char* file, bool is_msg);
bool is_dir_exist(const char* path);
bool get_file_size(const char* file, size_t& length);
bool move_file_to_backup(const char* file, const char* pext);
bool truncate_filling_zero(int fd, size_t length, int pagesize);

DECL_EXTERN_C_END

#endif	// CHMUTIL_H

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
