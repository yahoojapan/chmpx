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
#include <unistd.h>
#include <errno.h>
#include <libgen.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/inotify.h>

#include <string>

#include "chmcommon.h"
#include "chmcntrl.h"
#include "chmimdata.h"
#include "chmeventmq.h"
#include "chmutil.h"
#include "chmdbg.h"

using namespace	std;

//---------------------------------------------------------
// Symbols
//---------------------------------------------------------
#define	PROCFS_PATH				"/proc/"
#define	CHMEVSHM_THREAD_NAME	"ChmEventShm"

//---------------------------------------------------------
// Class Methods
//---------------------------------------------------------
// [NOTICE]
// This class method is run on another worker thread.
//
bool ChmEventShm::CheckProcessRunning(void* common_param, chmthparam_t wp_param)
{
	if(!common_param){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	ChmEventShm*	pThis = reinterpret_cast<ChmEventShm*>(common_param);
	if(!pThis){
		ERR_CHMPRN("Internal error: pThis is NULL.");
		return false;
	}
	ChmIMData*	pImData	= pThis->pChmCntrl->GetImDataObj();
	ChmEventMq*	pMqObj	= pThis->pChmCntrl->GetEventMqObj();
	if(!pImData || !pMqObj){
		ERR_CHMPRN("Internal error: ChmImData or ChmEventMQ is NULL.");
		return false;
	}

	// get all pids
	pidlist_t	pidlist;
	if(!pImData->GetAllPids(pidlist)){
		WAN_CHMPRN("Failed to get pid list.");
		return true;	// finish.
	}
	if(0 == pidlist.size()){
		// why?(nothing to do)
		return true;
	}

	// get down client process.
	//
	pidlist_t	down_pids;
	for(int cnt = 0; true; ++cnt){
		for(pidlist_t::const_iterator iter = pidlist.begin(); iter != pidlist.end(); ++iter){
			if(CHM_INVALID_PID != *iter && !ChmEventShm::IsProcessRunning(*iter)){
				down_pids.push_back(*iter);
			}
		}
		if(0 != down_pids.size()){
			break;
		}
		// no down process
		if(1 < cnt){											// = 300ms
			return true;
		}
		// [NOTE]
		// For a case of that the process is downing before removing /proc/PID path.
		// So we wait a while.
		//
		struct timespec	sleeptime = {0, 100 * 1000 * 1000};		// 100ms
		nanosleep(&sleeptime, NULL);
	}

	// free msgids by pids.
	//
	msgidlist_t	freedmsgids;
	if(!pImData->FreeMsgs(down_pids, freedmsgids)){
		WAN_CHMPRN("Failed to free msgs, but continue for check process ids...");
	}
	if(0 == freedmsgids.size()){
		MSG_CHMPRN("There is no to free msgs, maybe already free it or failed FreeMsgs func. but continue for check process ids...");
	}

	// close MQ in MQ event object
	//
	for(msgidlist_t::const_iterator iter = freedmsgids.begin(); iter != freedmsgids.end(); ++iter){
		if(!pMqObj->CloseDestMQ(*iter)){
			ERR_CHMPRN("Failed to close internal destination MQ(0x%016" PRIx64 "), but continue...", *iter);
		}
	}

	// remove pids in shm.
	for(pidlist_t::const_iterator iter = down_pids.begin(); iter != down_pids.end(); ++iter){
		if(CHM_INVALID_PID != *iter && !pImData->RetriveClientPid(*iter)){
			ERR_CHMPRN("Failed to retrive pid(%d) from shm, but continue...", *iter);
		}
	}

	// check clients
	if(!pImData->IsClientPids()){
		// There is no client process, so send notification.
		pThis->pChmCntrl->is_close_notify = true;
	}
	return true;
}

bool ChmEventShm::IsProcessRunning(pid_t pid)
{
	if(CHM_INVALID_PID == pid){
		ERR_CHMPRN("Parameter is wrong");
		return false;
	}
	string	procfile = PROCFS_PATH;
	procfile += to_string(pid);

	return is_file_exist(procfile.c_str());
}

//---------------------------------------------------------
// ChmEventShm Class
//---------------------------------------------------------
ChmEventShm::ChmEventShm(int eventqfd, ChmCntrl* pcntrl, const char* file) : ChmEventBase(eventqfd, pcntrl), inotifyfd(CHM_INVALID_HANDLE), watchfd(CHM_INVALID_HANDLE), checkthread(CHMEVSHM_THREAD_NAME)
{
	if(!CHMEMPTYSTR(file)){
		chmshmfile = file;
	}
	// run check thread
	//	- sleep at starting
	//	- not at onece(not one shot)
	//	- sleep after every working
	//	- keep event count
	//
	if(!checkthread.CreateThreads(1, ChmEventShm::CheckProcessRunning, NULL, this, 0, true, false, false, true)){
		ERR_CHMPRN("Failed to create thread for checking, but continue...");
	}
}

ChmEventShm::~ChmEventShm()
{
	// exit thread
	if(!checkthread.ExitAllThreads()){
		ERR_CHMPRN("Failed to exit thread for checking.");
	}
	Clean();
}

bool ChmEventShm::Clean(void)
{
	if(IsWatching()){
		MSG_CHMPRN("Should call RemoveNotify function before calling this destructor.");
		UnsetEventQueue();
	}
	return ChmEventBase::Clean();
}

bool ChmEventShm::GetEventQueueFds(event_fds_t& fds)
{
	if(!IsWatching()){
		MSG_CHMPRN("There is no watching file.");
		return false;
	}
	fds.clear();
	fds.push_back(inotifyfd);
	return true;
}

bool ChmEventShm::SetEventQueue(void)
{
	if(0 == chmshmfile.c_str()){
		ERR_CHMPRN("This object does not have chmshm file path.");
		return false;
	}
	if(CHM_INVALID_HANDLE == eqfd){
		ERR_CHMPRN("event fd is invalid.");
		return false;
	}
	if(IsWatching()){
		if(!UnsetEventQueue()){
			return false;
		}
	}

	// create inotify
	if(-1 == (inotifyfd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC))){
		ERR_CHMPRN("Failed to create inotify, error %d", errno);
		return false;
	}

	// add inotify
	if(CHM_INVALID_HANDLE == (watchfd = inotify_add_watch(inotifyfd, chmshmfile.c_str(), IN_CLOSE))){
		ERR_CHMPRN("Could not watch file %s (errno=%d)", chmshmfile.c_str(), errno);
		CHM_CLOSE(inotifyfd);
		return false;
	}

	// add event
	struct epoll_event	epoolev;
	memset(&epoolev, 0, sizeof(struct epoll_event));
	epoolev.data.fd	= inotifyfd;
	epoolev.events	= EPOLLIN | EPOLLET | EPOLLRDHUP;			// EPOLLRDHUP is set
	if(-1 == epoll_ctl(eqfd, EPOLL_CTL_ADD, inotifyfd, &epoolev)){
		ERR_CHMPRN("Failed to add inotifyfd(%d)-watchfd(%d) to event fd(%d), error=%d", inotifyfd, watchfd, eqfd, errno);
		inotify_rm_watch(inotifyfd, watchfd);
		watchfd = CHM_INVALID_HANDLE;
		CHM_CLOSE(inotifyfd);
		return false;
	}
	return true;
}

bool ChmEventShm::UnsetEventQueue(void)
{
	if(!IsWatching()){
		MSG_CHMPRN("There is no watching file.");
		return false;
	}
	bool	result = true;

	if(CHM_INVALID_HANDLE == inotifyfd){
		WAN_CHMPRN("inotifyfd is invalid.");
	}else{
		epoll_ctl(eqfd, EPOLL_CTL_DEL, inotifyfd, NULL);

		if(CHM_INVALID_HANDLE == inotify_rm_watch(inotifyfd, watchfd)){
			if(EINVAL == errno){
				WAN_CHMPRN("Failed to remove watching fd(%d) from inotify fd(%d), because watchfd is invalid. It maybe removed file, so continue...", watchfd, inotifyfd);
			}else{
				ERR_CHMPRN("Could not remove watching fd(%d) from inotify fd(%d), errno=%d", watchfd, inotifyfd, errno);
				result = false;
			}
		}
		CHM_CLOSE(inotifyfd);
	}
	inotifyfd	= CHM_INVALID_HANDLE;
	watchfd		= CHM_INVALID_HANDLE;

	return result;
}


bool ChmEventShm::IsEventQueueFd(int fd)
{
	if(CHM_INVALID_HANDLE == fd){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(!IsWatching()){
		MSG_CHMPRN("There is no watching file.");
		return false;
	}
	// check
	if(inotifyfd != fd){
		return false;
	}
	return true;
}

bool ChmEventShm::ResetEventQueue(void)
{
	if(IsWatching()){
		if(!UnsetEventQueue()){
			return false;
		}
	}
	return SetEventQueue();
}

//
// If the event is IN_CLOSE which is for this obejct, this method returns true.
//
bool ChmEventShm::CheckNotifyEvent(void) const
{
	if(!IsWatching()){
		MSG_CHMPRN("There is no watching file.");
		return false;
	}

	// read from inotify event
	unsigned char*	pevent;
	size_t			bytes;
	if(NULL == (pevent = chm_read(inotifyfd, &bytes))){
		WAN_CHMPRN("read no inotify event, no more inotify event data.");
		return false;
	}

	// analize event types
	struct inotify_event*	in_event	= NULL;
	bool					result		= false;
	for(unsigned char* ptr = pevent; (ptr + sizeof(struct inotify_event)) <= (pevent + bytes); ptr += sizeof(struct inotify_event) + in_event->len){
		in_event = reinterpret_cast<struct inotify_event*>(ptr);

		if(watchfd != in_event->wd){
			continue;
		}
		if(in_event->mask & IN_CLOSE_WRITE){
			MSG_CHMPRN("chmshm file %s is wrote(%d).", chmshmfile.c_str(), in_event->mask);
			result = true;
		}else if(in_event->mask & IN_CLOSE_NOWRITE){
			MSG_CHMPRN("chmshm file %s is read(%d).", chmshmfile.c_str(), in_event->mask);
			result = true;
		}else{
			WAN_CHMPRN("inotify event type(%u) is not handled because of waiting another event after it.", in_event->mask);
		}
	}
	CHM_Free(pevent);
	return result;
}

bool ChmEventShm::Receive(int fd)
{
	if(!IsWatching()){
		ERR_CHMPRN("There is no watching file.");
		return false;
	}
	if(!IsEventQueueFd(fd)){
		ERR_CHMPRN("fd(%d) is not this object event fd.", fd);
		return false;
	}
	if(!CheckNotifyEvent()){
		MSG_CHMPRN("There is no event for checking processes running by this object.");
		return false;
	}

	// check client process running.
	//
	// Checking client processes running by worker thread instead of COM_C2PX communication,
	// becasue working in main thread is not good for performance.
	//
	if(!checkthread.DoWorkThread()){
		ERR_CHMPRN("Failed to wake up thread for checking.");
		return false;
	}

	// Nothing to dispatch event in this class.

	return true;
}

bool ChmEventShm::Send(PCOMPKT pComPkt, const unsigned char* pbody, size_t blength)
{
	MSG_CHMPRN("Nothing to do in this object for this event.(Not implement thie event in this class)");
	return true;
}

bool ChmEventShm::NotifyHup(int fd)
{
	if(!IsWatching()){
		MSG_CHMPRN("There is no watching file.");
		return false;
	}
	if(!IsEventQueueFd(fd)){
		ERR_CHMPRN("fd(%d) is not this object event fd.", fd);
		return false;
	}
	return UnsetEventQueue();
}

bool ChmEventShm::Processing(PCOMPKT pComPkt)
{
	MSG_CHMPRN("Nothing to do in this object for this event.(Not implement thie event in this class)");
	return true;
}

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
