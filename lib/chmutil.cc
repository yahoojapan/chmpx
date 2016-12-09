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
 * CREATE:   Tue July 1 2014
 * REVISION:
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <string.h>
#include <locale.h>
#include <errno.h>

#include "chmutil.h"
#include "chmcommon.h"
#include "chmdbg.h"

using namespace std;

//---------------------------------------------------------
// String Utilities
//---------------------------------------------------------
// RFC 2822
string str_rfcdate(time_t date)
{
	if(-1 == date){
		date = time(NULL);
	}
	char*	oldlocale = setlocale(LC_TIME, "C");

	char	buff[128];
	strftime(buff, sizeof(buff), "%a, %d %b %Y %T %z", gmtime(&date));

	setlocale(LC_TIME, oldlocale);
	return buff;
}

// RFC 2822
time_t rfcdate_time(const char* rfcdate)
{
	if(CHMEMPTYSTR(rfcdate)){
		return 0L;		// ...
	}
	char*	oldlocale = setlocale(LC_TIME, "C");

	struct tm	tmdate;
	strptime(rfcdate, "%a, %d %b %Y %T %z", &tmdate);
	time_t	tdate = mktime(&tmdate);

	setlocale(LC_TIME, oldlocale);
	return tdate;
}

bool sorted_insert_strmaparr(strmaparr_t& sorted_smaps, strmap_t& smap, const char* pSortKey1, const char* pSortKey2)
{
	if(CHMEMPTYSTR(pSortKey1)){
		return false;
	}
	if(smap.end() == smap.find(pSortKey1)){
		// not found key, it is wrong smap data.
		return false;
	}

	bool	is_name2 = !CHMEMPTYSTR(pSortKey2);
	string	strname1 = smap[pSortKey1];
	string	strname2 = !is_name2 ? "" : (smap.end() == smap.find(pSortKey2) ? "" : smap[pSortKey2]);

	for(strmaparr_t::iterator iter = sorted_smaps.begin(); iter != sorted_smaps.end(); ++iter){
		if(strname1 == (*iter)[pSortKey1]){
			if(!is_name2){
				// found same key, merge data
				merge_strmap((*iter), smap);
				return true;
			}else{
				// check strname2
				for(; iter != sorted_smaps.end(); ++iter){
					if(strname1 == (*iter)[pSortKey1]){

						string	sorted_strname2 = iter->end() == iter->find(pSortKey2) ? "" : (*iter)[pSortKey2];
						if(strname2 == sorted_strname2){
							// found same key, merge data
							merge_strmap((*iter), smap);
							return true;
						}else if(strname2 < sorted_strname2){
							// over posission, insert data
							sorted_smaps.insert(iter, smap);
							return true;
						}
					}else{
						// over posission, insert data
						sorted_smaps.insert(iter, smap);
						return true;
					}
				}
			}
			break;

		}else if(strname1 < (*iter)[pSortKey1]){
			// over posission, insert data
			sorted_smaps.insert(iter, smap);
			return true;
		}
	}
	// not found, insert to lastest posision
	sorted_smaps.push_back(smap);

	return true;
}

bool is_string_alpha(const char* str)
{
	if(CHMEMPTYSTR(str)){
		return false;
	}
	for(; '\0' != *str; ++str){
		if(0 == isalpha(*str)){
			return false;
		}
	}
	return true;
}

bool is_string_number(const char* str)
{
	if(CHMEMPTYSTR(str)){
		return false;
	}
	for(; '\0' != *str; ++str){
		if(0 == isdigit(*str)){
			return false;
		}
	}
	return true;
}

bool str_paeser(const char* pbase, strlst_t& strarr, const char* psep, bool istrim)
{
	strarr.clear();

	if(CHMEMPTYSTR(pbase)){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}
	strarr.push_back(string(pbase));

	if(CHMEMPTYSTR(psep)){
		psep = " \t\r\n";
	}
	for(; psep && *psep; psep++){
		for(strlst_t::iterator iter = strarr.begin(); iter != strarr.end(); ){
			strlst_t	tmparr;
			tmparr.clear();
			if(str_split(iter->c_str(), tmparr, *psep, istrim)){
				iter = strarr.erase(iter);
				if(iter != strarr.end()){
					for(strlst_t::iterator iter2 = tmparr.begin(); iter2 != tmparr.end(); ++iter2){
						iter = strarr.insert(iter, *iter2);
						++iter;
					}
				}else{
					for(strlst_t::iterator iter2 = tmparr.begin(); iter2 != tmparr.end(); ++iter2){
						strarr.push_back(*iter2);
					}
					break;
				}
			}else{
				++iter;
			}
		}
	}
	return true;
}

bool str_split(const char* pbase, strlst_t& strarr, char sep, bool istrim)
{
	strarr.clear();

	if(CHMEMPTYSTR(pbase)){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}
	stringstream	ss(pbase);
	string			one;
	while(getline(ss, one, sep)){
		if(istrim){
			one = trim(one);
		}
		strarr.push_back(one);
	}
	return true;
}

//---------------------------------------------------------
// Utilities
//---------------------------------------------------------
#define	DEFAULT_READ_BUFF_SIZE		4096

ssize_t chm_pread(int fd, void *buf, size_t count, off_t offset)
{
	ssize_t	read_cnt;
	ssize_t	one_read;
	for(read_cnt = 0L, one_read = 0L; static_cast<size_t>(read_cnt) < count; read_cnt += one_read){
		if(-1 == (one_read = pread(fd, &(static_cast<unsigned char*>(buf))[read_cnt], (count - static_cast<size_t>(read_cnt)), (offset + read_cnt)))){
			WAN_CHMPRN("Failed to read from fd(%d:%zd:%zu), errno = %d", fd, static_cast<ssize_t>(offset) + read_cnt, count - static_cast<size_t>(read_cnt), errno);
			return -1;
		}
		if(0 == one_read){
			break;
		}
	}
	return read_cnt;
}

ssize_t chm_pwrite(int fd, const void *buf, size_t count, off_t offset)
{
	ssize_t	write_cnt;
	ssize_t	one_write;
	for(write_cnt = 0L, one_write = 0L; static_cast<size_t>(write_cnt) < count; write_cnt += one_write){
		if(-1 == (one_write = pwrite(fd, &(static_cast<const unsigned char*>(buf))[write_cnt], (count - static_cast<size_t>(write_cnt)), (offset + write_cnt)))){
			WAN_CHMPRN("Failed to write from fd(%d:%zd:%zu), errno = %d", fd, static_cast<ssize_t>(offset) + write_cnt, count - static_cast<size_t>(write_cnt), errno);
			return -1;
		}
	}
	return write_cnt;
}

unsigned char* chm_read(int fd, size_t* psize)
{
	if(CHM_INVALID_HANDLE == fd || !psize){
		ERR_CHMPRN("Parameter fd or psize is wrong.");
		return NULL;
	}

	size_t			buffsize = DEFAULT_READ_BUFF_SIZE;
	unsigned char*	pbuff;
	if(NULL == (pbuff = reinterpret_cast<unsigned char*>(malloc(buffsize * sizeof(unsigned char))))){
		ERR_CHMPRN("Could not allocation memory.");
		return NULL;
	}

	for(ssize_t pos = 0, readsize = 0; true; pos += readsize){
		if(-1 == (readsize = read(fd, &pbuff[pos], DEFAULT_READ_BUFF_SIZE))){
			if(EAGAIN == errno){
				MSG_CHMPRN("reading fd reached end(EAGAIN)");
				break;
			}else if(EINTR == errno){
				MSG_CHMPRN("break reading fd by signal, so retry to read.");
				readsize = 0;
				continue;
			}
			ERR_CHMPRN("Failed to read from fd(%d: %zd: %d). errno=%d", fd, pos, DEFAULT_READ_BUFF_SIZE, errno);
			CHM_Free(pbuff);
			return NULL;
		}
		if(0 <= readsize && readsize < DEFAULT_READ_BUFF_SIZE){
			buffsize = static_cast<size_t>(pos + readsize);
			break;
		}

		if(buffsize < static_cast<size_t>(pos + readsize + DEFAULT_READ_BUFF_SIZE)){
			// reallocate
			buffsize = static_cast<size_t>(pos + readsize + DEFAULT_READ_BUFF_SIZE);	// => DEFAULT_READ_BUFF_SIZE * X

			unsigned char*	ptmp;
			if(NULL == (ptmp = reinterpret_cast<unsigned char*>(realloc(pbuff, buffsize)))){
				ERR_CHMPRN("Could not allocation memory.");
				CHM_Free(pbuff);
				return NULL;
			}
			pbuff = ptmp;
		}
	}
	*psize = buffsize;
	return pbuff;
}

bool is_file_exist(const char* file)
{
	if(CHMEMPTYSTR(file)){
		ERR_CHMPRN("Parameter is NULL.");
		return false;
	}
	struct stat	st;
	if(-1 == stat(file, &st)){
		MSG_CHMPRN("Could not get stat for %s(errno:%d)", file, errno);
		return false;
	}
	return true;
}

bool is_file_safe_exist(const char* file)
{
	if(CHMEMPTYSTR(file)){
		ERR_CHMPRN("Parameter is NULL.");
		return false;
	}
	struct stat	st;
	if(-1 == stat(file, &st)){
		MSG_CHMPRN("Could not get stat for %s(errno:%d)", file, errno);
		return false;
	}
	if(!S_ISREG(st.st_mode) && !S_ISLNK(st.st_mode)){
		MSG_CHMPRN("file %s is not regular nor symbolic file", file);
		return false;
	}
	return true;
}

bool is_dir_exist(const char* path)
{
	if(CHMEMPTYSTR(path)){
		ERR_CHMPRN("Parameter is NULL.");
		return false;
	}
	struct stat	st;
	if(-1 == stat(path, &st)){
		MSG_CHMPRN("Could not get stat for %s(errno:%d)", path, errno);
		return false;
	}
	if(!S_ISDIR(st.st_mode)){
		MSG_CHMPRN("path %s is not directory", path);
		return false;
	}
	return true;
}

bool get_file_size(const char* file, size_t& length)
{
	if(CHMEMPTYSTR(file)){
		ERR_CHMPRN("Parameter is NULL.");
		return false;
	}
	struct stat	st;
	if(-1 == stat(file, &st)){
		MSG_CHMPRN("Could not get stat for %s(errno:%d)", file, errno);
		return false;
	}
	length = static_cast<size_t>(st.st_size);
	return true;
}

bool move_file_to_backup(const char* file, const char* pext)
{
	if(CHMEMPTYSTR(file) || CHMEMPTYSTR(pext)){
		ERR_CHMPRN("Parameter is NULL.");
		return false;
	}
	if(!is_file_exist(file)){
		ERR_CHMPRN("file %s does not exist.", file);
		return false;
	}

	string	oldpath = file;
	string	tmppath = oldpath + "." + pext;
	string	newpath;
	for(int cnt = -1; true; cnt++){
		newpath = tmppath;
		if(0 <= cnt){
			newpath += "." + to_string(cnt);
		}
		if(!is_file_exist(newpath.c_str())){
			break;
		}
		MSG_CHMPRN("backup file %s exists, change file extension.", newpath.c_str());
	}

	if(-1 == link(oldpath.c_str(), newpath.c_str())){
		ERR_CHMPRN("Failed to link from file %s to %s, errno=%d", oldpath.c_str(), newpath.c_str(), errno);
		return false;
	}
	if(-1 == unlink(oldpath.c_str())){
		ERR_CHMPRN("Failed to unlink file %s, errno=%d", oldpath.c_str(), errno);
		unlink(newpath.c_str());		// try to recover
		return false;
	}
	return true;
}

bool truncate_filling_zero(int fd, size_t length, int pagesize)
{
	if(CHM_INVALID_HANDLE == fd || 0 == length || 0 >= pagesize){
		ERR_CHMPRN("parameters are wrong.");
		return false;
	}
	// truncate
	if(0 != ftruncate(fd, length)){
		ERR_CHMPRN("Could not truncate file(fd=%d) to %zu bytes, errno = %d", fd, length, errno);
		return false;
	}
	// for buffer
	unsigned char	szBuff[pagesize];
	memset(szBuff, 0, pagesize);

	// initialize(fill zero)
	for(size_t wrote = 0, onewrote = 0; wrote < length; wrote += onewrote){
		onewrote = min(static_cast<size_t>(sizeof(unsigned char) * pagesize), (length - wrote));
		if(-1 == chm_pwrite(fd, szBuff, onewrote, static_cast<off_t>(wrote))){
			ERR_CHMPRN("Failed to write file(fd=%d), errno = %d", fd, errno);
			return false;
		}
	}
	return true;
}

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
