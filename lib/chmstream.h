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
#ifndef	CHMSTREAM_H
#define	CHMSTREAM_H

#include <iostream>

#include "chmcommon.h"
#include "chmpx.h"
#include "chmutil.h"
#include "chmdbg.h"
#include "chmcntrl.h"
#include "chmkvp.h"

//---------------------------------------------------------
// Template basic_chmstreambuf
//---------------------------------------------------------
template<typename CharT, typename Traits>
class basic_chmstreambuf : public std::basic_streambuf<CharT, Traits>
{
	public:
		typedef CharT								char_type;
		typedef Traits								traits_type;
		typedef typename traits_type::int_type		int_type;
		typedef typename traits_type::pos_type		pos_type;
		typedef typename traits_type::off_type		off_type;
		typedef std::ios_base::openmode				open_mode;
		typedef std::basic_streambuf<CharT, Traits>	streambuf_type;

	private:
		static const size_t	pagesize		= 4096;
		static const int	default_timeout	= 1 * 10;		// 10ms

		ChmCntrl*			pchmcntrl;
		PCOMPKT				plastpkt;
		msgid_t				msgid;
		open_mode			mode;
		chmhash_t			init_hash;
		bool				is_init_hash;
		bool				is_init_key;
		size_t				input_buff_size;
		size_t				output_buff_size;
		char_type*			input_buff;
		char_type*			output_buff;
		char_type*			output_val_pos;
		int					read_timeout;

	protected:
		virtual int sync(void);
		virtual int_type overflow(int_type ch = traits_type::eof());
		virtual int_type pbackfail(int_type ch = traits_type::eof());
		virtual int_type underflow(void);

		virtual pos_type seekoff(off_type offset, std::ios_base::seekdir bpostype, open_mode opmode = std::ios_base::in | std::ios_base::out);
		virtual pos_type seekpos(pos_type abspos, open_mode opmode = std::ios_base::in | std::ios_base::out);

	private:
		bool init_output_buff(void);
		bool send_sync(void);
		bool receive_sync(void);

	public:
		basic_chmstreambuf(open_mode opmode = std::ios_base::in | std::ios_base::out);							// Do not call this
		basic_chmstreambuf(ChmCntrl* pchmobj, open_mode opmode = std::ios_base::in | std::ios_base::out);
		basic_chmstreambuf(ChmCntrl* pchmobj, chmhash_t hash, open_mode opmode = std::ios_base::in | std::ios_base::out);
		basic_chmstreambuf(ChmCntrl* pchmobj, const std::string& strkey, open_mode opmode = std::ios_base::in | std::ios_base::out);
		virtual ~basic_chmstreambuf();

		bool reset(void);
		bool init(ChmCntrl* pchmobj, open_mode opmode = std::ios_base::in | std::ios_base::out);
		bool init(ChmCntrl* pchmobj, chmhash_t hash, open_mode opmode = std::ios_base::in | std::ios_base::out);
		bool init(ChmCntrl* pchmobj, const std::string& strkey, open_mode opmode = std::ios_base::in | std::ios_base::out);

		chmhash_t receivedhash(void) const { return (plastpkt ? plastpkt->head.hash : CHM_INVALID_HASHVAL); }
};

//---------------------------------------------------------
// template basic_chmstreambuf
//---------------------------------------------------------
template<typename CharT, typename Traits>
basic_chmstreambuf<CharT, Traits>::basic_chmstreambuf(open_mode opmode) : pchmcntrl(NULL), plastpkt(NULL), msgid(CHM_INVALID_MSGID), mode(opmode), init_hash(CHM_INVALID_HASHVAL), is_init_hash(false), is_init_key(false), input_buff_size(0), output_buff_size(0), input_buff(reinterpret_cast<char_type*>(NULL)), output_buff(reinterpret_cast<char_type*>(NULL)), output_val_pos(reinterpret_cast<char_type*>(NULL))
{
}

template<typename CharT, typename Traits>
basic_chmstreambuf<CharT, Traits>::basic_chmstreambuf(ChmCntrl* pchmobj, open_mode opmode) : pchmcntrl(NULL), plastpkt(NULL), msgid(CHM_INVALID_MSGID), mode(opmode), init_hash(CHM_INVALID_HASHVAL), is_init_hash(false), is_init_key(false), input_buff_size(0), output_buff_size(0), input_buff(reinterpret_cast<char_type*>(NULL)), output_buff(reinterpret_cast<char_type*>(NULL)), output_val_pos(reinterpret_cast<char_type*>(NULL))
{
	if(!pchmobj){
		ERR_CHMPRN("Parameter is wrong.");
		return;
	}
	if(!init(pchmobj, opmode)){
		WAN_CHMPRN("Parameter is something wrong");
		return;
	}
}

template<typename CharT, typename Traits>
basic_chmstreambuf<CharT, Traits>::basic_chmstreambuf(ChmCntrl* pchmobj, chmhash_t hash, open_mode opmode) : pchmcntrl(NULL), plastpkt(NULL), msgid(CHM_INVALID_MSGID), mode(opmode), init_hash(hash), is_init_hash(false), is_init_key(false), input_buff_size(0), output_buff_size(0), input_buff(reinterpret_cast<char_type*>(NULL)), output_buff(reinterpret_cast<char_type*>(NULL)), output_val_pos(reinterpret_cast<char_type*>(NULL))
{
	if(!pchmobj){
		ERR_CHMPRN("Parameter is wrong.");
		return;
	}
	if(!init(pchmobj, hash, opmode)){
		WAN_CHMPRN("Parameter is something wrong");
		return;
	}
}

template<typename CharT, typename Traits>
basic_chmstreambuf<CharT, Traits>::basic_chmstreambuf(ChmCntrl* pchmobj, const std::string& strkey, open_mode opmode) : pchmcntrl(NULL), plastpkt(NULL), msgid(CHM_INVALID_MSGID), mode(opmode), init_hash(CHM_INVALID_HASHVAL), is_init_hash(false), is_init_key(false), input_buff_size(0), output_buff_size(0), input_buff(reinterpret_cast<char_type*>(NULL)), output_buff(reinterpret_cast<char_type*>(NULL)), output_val_pos(reinterpret_cast<char_type*>(NULL))
{
	if(!pchmobj){
		ERR_CHMPRN("Parameter is wrong.");
		return;
	}
	if(!init(pchmobj, strkey, opmode)){
		WAN_CHMPRN("Parameter is something wrong");
		return;
	}
}

template<typename CharT, typename Traits>
basic_chmstreambuf<CharT, Traits>::~basic_chmstreambuf(void)
{
	reset();
}

template<typename CharT, typename Traits>
bool basic_chmstreambuf<CharT, Traits>::reset(void)
{
	// If there is no-flushed write buffer, flush it here.
	//
	if(pchmcntrl && streambuf_type::pbase() < streambuf_type::pptr()){
		// If there are left data, send it.
		// Be careful, if is_init_key is true, current position must be over start os value data.
		//
		if(!is_init_key || (reinterpret_cast<char_type*>(&output_buff[strlen(output_buff) + 1]) < streambuf_type::pptr())){
			if(!send_sync()){
				WAN_CHMPRN("Could not put left write buffer into.");
			}
		}
	}

	if(pchmcntrl && CHM_INVALID_MSGID != msgid){
		// close msgid
		if(!pchmcntrl->Close(msgid)){
			WAN_CHMPRN("Could not close MQ.");
		}
	}
	pchmcntrl		= NULL;
	msgid			= CHM_INVALID_MSGID;

	mode			= std::ios_base::in | std::ios_base::out;
	init_hash		= CHM_INVALID_HASHVAL;
	is_init_hash	= false;
	is_init_key		= false;
	input_buff_size	= 0;
	output_buff_size= 0;
	output_val_pos	= reinterpret_cast<char_type*>(NULL);
	read_timeout	= basic_chmstreambuf<CharT, Traits>::default_timeout;

	CHM_Free(input_buff);
	CHM_Free(output_buff);
	CHM_Free(plastpkt);

	return true;
}

template<typename CharT, typename Traits>
bool basic_chmstreambuf<CharT, Traits>::init(ChmCntrl* pchmobj, open_mode opmode)
{
	reset();

	if(!pchmobj){
		return true;
	}
	if(pchmobj->IsChmpxType()){
		ERR_CHMPRN("ChmCntrl type does not server(slave) side.");
		return false;
	}
	pchmcntrl	= pchmobj;
	mode		= (opmode & (std::ios_base::in | std::ios_base::out));

	if(opmode & std::ios_base::out){
		output_buff_size = basic_chmstreambuf<CharT, Traits>::pagesize;

		if(reinterpret_cast<char_type*>(NULL) == (output_buff = reinterpret_cast<char_type*>(calloc(output_buff_size, sizeof(char_type))))){
			ERR_CHMPRN("Could not allocate memory.");
			reset();
			return false;
		}
		streambuf_type::setp(output_buff, reinterpret_cast<char_type*>(reinterpret_cast<off_type>(output_buff) + static_cast<off_type>(output_buff_size)));
	}
	if(opmode & std::ios_base::in){
		// input_buff = NULL
		streambuf_type::setg(input_buff, input_buff, reinterpret_cast<char_type*>(reinterpret_cast<off_type>(input_buff) + static_cast<off_type>(input_buff_size)));
	}
	return true;
}

template<typename CharT, typename Traits>
bool basic_chmstreambuf<CharT, Traits>::init(ChmCntrl* pchmobj, chmhash_t hash, open_mode opmode)
{
	reset();

	if(!pchmobj){
		return true;
	}
	if(pchmobj->IsChmpxType()){
		ERR_CHMPRN("ChmCntrl type does not server(slave) side.");
		return false;
	}
	if(0 == (opmode & std::ios_base::out)){
		ERR_CHMPRN("open_mode does not have std::ios_base::out.");
		return false;
	}
	pchmcntrl	= pchmobj;
	mode		= (opmode & (std::ios_base::in | std::ios_base::out));
	init_hash	= hash;
	is_init_hash= true;

	if(opmode & std::ios_base::out){
		output_buff_size = basic_chmstreambuf<CharT, Traits>::pagesize;

		if(reinterpret_cast<char_type*>(NULL) == (output_buff = reinterpret_cast<char_type*>(calloc(output_buff_size, sizeof(char_type))))){
			ERR_CHMPRN("Could not allocate memory.");
			reset();
			return false;
		}
		streambuf_type::setp(output_buff, reinterpret_cast<char_type*>(reinterpret_cast<off_type>(output_buff) + static_cast<off_type>(output_buff_size)));
	}
	if(opmode & std::ios_base::in){
		// input_buff = NULL
		streambuf_type::setg(input_buff, input_buff, reinterpret_cast<char_type*>(reinterpret_cast<off_type>(input_buff) + static_cast<off_type>(input_buff_size)));
	}
	return true;
}

template<typename CharT, typename Traits>
bool basic_chmstreambuf<CharT, Traits>::init(ChmCntrl* pchmobj, const std::string& strkey, open_mode opmode)
{
	reset();

	if(!pchmobj){
		return true;
	}
	if(pchmobj->IsChmpxType()){
		ERR_CHMPRN("ChmCntrl type does not server(slave) side.");
		return false;
	}
	if(0 == (opmode & std::ios_base::out)){
		ERR_CHMPRN("open_mode does not have std::ios_base::out.");
		return false;
	}
	pchmcntrl	= pchmobj;
	mode		= (opmode & (std::ios_base::in | std::ios_base::out));
	is_init_key	= true;

	if(opmode & std::ios_base::out){
		output_buff_size = ((strkey.length() + 1/* for \0 */ + 1/* for new adding buffer */) / basic_chmstreambuf<CharT, Traits>::pagesize + 1) * basic_chmstreambuf<CharT, Traits>::pagesize;

		if(reinterpret_cast<char_type*>(NULL) == (output_buff = reinterpret_cast<char_type*>(calloc(output_buff_size, sizeof(char_type))))){
			ERR_CHMPRN("Could not allocate memory.");
			reset();
			return false;
		}
		strcpy(output_buff, strkey.c_str());

		// start position is after strkey string.
		streambuf_type::setp(output_buff, reinterpret_cast<char_type*>(reinterpret_cast<off_type>(output_buff) + static_cast<off_type>(output_buff_size)));
		streambuf_type::pbump(strlen(output_buff) + 1);

		// set value start pos
		output_val_pos = &output_buff[strlen(output_buff) + 1];
	}
	if(opmode & std::ios_base::in){
		// input_buff = NULL
		streambuf_type::setg(input_buff, input_buff, reinterpret_cast<char_type*>(reinterpret_cast<off_type>(input_buff) + static_cast<off_type>(input_buff_size)));
	}
	return true;
}

template<typename CharT, typename Traits>
bool basic_chmstreambuf<CharT, Traits>::init_output_buff(void)
{
	if(!pchmcntrl){
		ERR_CHMPRN("This object did not initialized.");
		return false;
	}

	if(output_buff && is_init_key){
		// do not erase key area, so only reset value pos.
		output_val_pos = &output_buff[strlen(output_buff) + 1];

		// start position is after strkey string.
		streambuf_type::setp(output_buff, reinterpret_cast<char_type*>(reinterpret_cast<off_type>(output_buff) + static_cast<off_type>(output_buff_size)));
		streambuf_type::pbump(strlen(output_buff) + 1);

	}else{
		// clear
		if(!output_buff || 0 == output_buff_size){
			CHM_Free(output_buff);

			output_buff_size = basic_chmstreambuf<CharT, Traits>::pagesize;
			if(reinterpret_cast<char_type*>(NULL) == (output_buff = reinterpret_cast<char_type*>(calloc(output_buff_size, sizeof(char_type))))){
				ERR_CHMPRN("Could not allocate memory.");
				reset();
				return false;
			}
		}
		*output_buff	= static_cast<char_type>('\0');
		output_val_pos	= reinterpret_cast<char_type*>(NULL);

		streambuf_type::setp(output_buff, reinterpret_cast<char_type*>(reinterpret_cast<off_type>(output_buff) + static_cast<off_type>(output_buff_size)));
	}
	return true;
}

template<typename CharT, typename Traits>
bool basic_chmstreambuf<CharT, Traits>::send_sync(void)
{
	if(!pchmcntrl){
		ERR_CHMPRN("This object did not initialized.");
		return false;
	}

	// raw data
	chmhash_t		local_hash	= (is_init_hash ? init_hash : CHM_INVALID_HASHVAL);
	char_type*		val_pos		= output_val_pos ? output_val_pos : output_buff;		// backup value before reinitializing
	size_t			rawlength	= 0L;
	unsigned char*	rawdata		= NULL;
	{
		if(!output_buff || 0 == output_buff_size){
			ERR_CHMPRN("There is no data for sending.");
			init_output_buff();
			return false;
		}

		// make key and value structure
		ChmBinData	Key;
		ChmBinData	Value;
		if((output_buff != val_pos && !Key.Set(reinterpret_cast<unsigned char*>(output_buff), strlen(output_buff) + 1)) || !Value.Set(reinterpret_cast<unsigned char*>(val_pos), strlen(val_pos) + 1)){
			ERR_CHMPRN("Could not set key and value to chmbindata.");
			init_output_buff();
			return false;
		}

		// make chmkvp
		ChmKVPair	chmkvp;
		if(!chmkvp.SetKey(Key) || !chmkvp.SetValue(Value)){
			ERR_CHMPRN("Could not set key and value to chmkvp.");
			init_output_buff();
			return false;
		}

		// check hash value(for only slave)
		if(pchmcntrl->IsClientOnSlvType() && !is_init_hash){
			if(output_buff == val_pos){
				ERR_CHMPRN("Could not make hash value for sending on slave because of empty key value.");
				init_output_buff();
				return false;
			}
			local_hash = chmkvp.GetHash();
		}

		// make raw data
		if(NULL == (rawdata = chmkvp.Put(rawlength))){
			ERR_CHMPRN("Could not convert raw data for sending.");
			init_output_buff();
			return false;
		}

		// reset buffer
		init_output_buff();
	}

	// sending
	if(pchmcntrl->IsClientOnSvrType()){
		if(!plastpkt){
			ERR_CHMPRN("There is no sending terminal.");
			CHM_Free(rawdata);
			return false;
		}

		if(!pchmcntrl->Reply(plastpkt, rawdata, rawlength)){
			ERR_CHMPRN("Failed replying to server chmpx.");
			CHM_Free(rawdata);
			return false;
		}

	}else{
		if(CHM_INVALID_MSGID == msgid){
			if(CHM_INVALID_MSGID == (msgid = pchmcntrl->Open())){
				ERR_CHMPRN("Could not open MQ.");
				CHM_Free(rawdata);
				return false;
			}
		}

		if(!pchmcntrl->Send(msgid, rawdata, rawlength, local_hash)){
			ERR_CHMPRN("Failed sending to slave chmpx.");
			CHM_Free(rawdata);
			return false;
		}
	}
	CHM_Free(rawdata);

	return true;
}

template<typename CharT, typename Traits>
bool basic_chmstreambuf<CharT, Traits>::receive_sync(void)
{
	if(!pchmcntrl){
		MSG_CHMPRN("This object did not initialized or could not open key because of not existing.");
		return false;
	}

	// receiving
	PCOMPKT			pComPkt = NULL;
	unsigned char*	pbody	= NULL;
	size_t			length	= 0L;

	if(pchmcntrl->IsClientOnSvrType()){
		// receive
		if(!pchmcntrl->Receive(&pComPkt, &pbody, &length, read_timeout) || !pbody || 0 == length){
			//MSG_CHMPRN("Failed to receive from server chmpx.");
			CHM_Free(pbody);
			return false;
		}
	}else{
		if(CHM_INVALID_MSGID == msgid){
			ERR_CHMPRN("There is no receiving terminal.");
			return false;
		}
		if(!pchmcntrl->Receive(msgid, &pComPkt, &pbody, &length, read_timeout) || !pbody || 0 == length){
			//MSG_CHMPRN("Failed to receive from slave chmpx.");
			CHM_Free(pbody);
			return false;
		}
	}

	// convert
	ChmKVPair	chmkvp;
	if(!chmkvp.Load(pbody)){
		ERR_CHMPRN("Could not load message body to chmkvp.");
		CHM_Free(pComPkt);
		CHM_Free(pbody);
		return false;
	}

	// make buffer
	const char*	pkey	= chmkvp.GetKey();
	const char*	pval	= chmkvp.GetValue();
	size_t		newsize = (pkey ? (strlen(pkey) + 1) : 1) + (pval ? (strlen(pval) + 1) : 1);
	char_type*	newbuff;
	if(reinterpret_cast<char_type*>(NULL) == (newbuff = reinterpret_cast<char_type*>(calloc(newsize, sizeof(char_type))))){
		ERR_CHMPRN("Could not allocate memory.");
		CHM_Free(pComPkt);
		CHM_Free(pbody);
		return false;
	}

	// set input buffer(key terminated by LF)
	size_t	setpos = 0;
	if(pkey){
		strcpy(&newbuff[setpos], pkey);
		newbuff[setpos + strlen(pkey)] = static_cast<char_type>('\n');		// \0 -> \n
		setpos += strlen(pkey) + 1;
	}else{
		newbuff[setpos++] = '\n';
	}
	if(pval){
		strcpy(&newbuff[setpos], pval);
	}else{
		newbuff[setpos++] = '\0';
	}
	CHM_Free(pbody);

	// reset input buffer
	CHM_Free(input_buff);
	CHM_Free(plastpkt);
	input_buff_size	= 0;
	streambuf_type::setg(input_buff, input_buff, reinterpret_cast<char_type*>(reinterpret_cast<off_type>(input_buff) + static_cast<off_type>(input_buff_size)));

	// set internal buffer
	plastpkt		= pComPkt;
	input_buff		= newbuff;
	input_buff_size	= newsize;

	streambuf_type::setg(input_buff, input_buff, reinterpret_cast<char_type*>(reinterpret_cast<off_type>(input_buff) + static_cast<off_type>(input_buff_size)));

	return true;
}

//
// This stream only receives Manipulator(std::endl).
// On server side:
//   If this gets Manipulator, sends output buffer as soon as possible.
//   But the object must start receiving at first, so if the compkt pointer
//   does not exist, fails to send(reply).
//   If the object has not got key value, sends with key value empty.
//
// On slave side:
//   If this gets Manipulator, depends on key value and default hash value.
//   If gets Manipulator when the object does not have default hash and key
//   value, this does not send and wait to get key value until getting next
//   Manipulator.
//   If gets when the object has default hash value, this sends with empty
//   key value.(not wait to get next Manipulator)
//   If gets second Manipulator, this sends key and value as soon as possible.
//
template<typename CharT, typename Traits>
int basic_chmstreambuf<CharT, Traits>::sync(void)
{
	if(!pchmcntrl){
		ERR_CHMPRN("This object did not initialized.");
		return -1;
	}
	if(reinterpret_cast<char_type*>(NULL) == streambuf_type::pptr()){
		MSG_CHMPRN("There is no current put pointer.");
		return -1;
	}

	// [NOTE]
	// If on slave chmpx and does not have default hash value and has not put key data.
	// Thus this sync means for putting key data, and waiting value data after this sync.
	//
	if(pchmcntrl->IsClientOnSlvType() && !is_init_hash && reinterpret_cast<char_type*>(NULL) == output_val_pos){
		// check key length
		if(streambuf_type::pbase() == streambuf_type::pptr()){
			ERR_CHMPRN("There is no key value.");
			return -1;
		}
		*(streambuf_type::pptr()) = static_cast<char_type>('\0');

		// check last character(LF)
		streambuf_type::pbump(-1);
		if(traits_type::eq(static_cast<char_type>('\n'), *(streambuf_type::pptr()))){
			*(streambuf_type::pptr()) = static_cast<char_type>('\0');

			// recheck key length
			if(streambuf_type::pbase() == streambuf_type::pptr()){
				ERR_CHMPRN("key value is empty.");
				return -1;
			}
		}
		streambuf_type::pbump(1);

		// key is OK.
		output_val_pos = streambuf_type::pptr();

		// do not send, wait for value.
		return 0;
	}

	// check last character(LF)
	if(streambuf_type::pbase() < streambuf_type::pptr()){
		*(streambuf_type::pptr()) = static_cast<char_type>('\0');

		streambuf_type::pbump(-1);
		if(traits_type::eq(static_cast<char_type>('\n'), *(streambuf_type::pptr()))){
			*(streambuf_type::pptr()) = static_cast<char_type>('\0');
		}
		streambuf_type::pbump(1);
	}

	if(!send_sync()){
		return -1;
	}
	return 0;
}

template<typename CharT, typename Traits>
typename basic_chmstreambuf<CharT, Traits>::int_type basic_chmstreambuf<CharT, Traits>::underflow(void)
{
	if(reinterpret_cast<char_type*>(NULL) == streambuf_type::eback() || streambuf_type::egptr() <= streambuf_type::gptr()){
		// reached end of input buffer or nothing read data, so it means EOF.
		// try to read new data.
		if(!receive_sync()){
			// reset input buffer
			CHM_Free(input_buff);
			input_buff_size	= 0;
			streambuf_type::setg(input_buff, input_buff, reinterpret_cast<char_type*>(reinterpret_cast<off_type>(input_buff) + static_cast<off_type>(input_buff_size)));

			return traits_type::eof();
		}
	}
	// Check EOF
	if(reinterpret_cast<char_type*>(NULL) == streambuf_type::eback() || reinterpret_cast<char_type*>(NULL) == streambuf_type::egptr()){
		return traits_type::eof();
	}

	return traits_type::to_int_type(streambuf_type::gptr()[0]);
}

template<typename CharT, typename Traits>
typename basic_chmstreambuf<CharT, Traits>::int_type basic_chmstreambuf<CharT, Traits>::overflow(int_type ch)
{
	if(!pchmcntrl){
		ERR_CHMPRN("This object did not initialized.");
		return traits_type::eof();
	}

	// for value position
	off_type	val_offset = (reinterpret_cast<char_type*>(NULL) == output_val_pos || output_val_pos < output_buff) ? static_cast<off_type>(-1) : (output_val_pos - output_buff);

	// expand buffer area
	size_t		newsize	= output_buff_size + basic_chmstreambuf<CharT, Traits>::pagesize;
	char_type*	newbuff;
	if(reinterpret_cast<char_type*>(NULL) == (newbuff = reinterpret_cast<char_type*>(realloc(output_buff, newsize)))){
		ERR_CHMPRN("Could not allocate memory.");
		return traits_type::eof();
	}

	// set newbuff
	streambuf_type::setp(newbuff, reinterpret_cast<char_type*>(reinterpret_cast<off_type>(newbuff) + static_cast<off_type>(newsize)));
	streambuf_type::pbump(output_buff_size);

	// add ch
	*(streambuf_type::pptr()) = ch;
	streambuf_type::pbump(1);

	// set internal data
	output_buff_size	= newsize;
	output_buff			= newbuff;
	output_val_pos		= (static_cast<off_type>(-1) == val_offset) ? reinterpret_cast<char_type*>(NULL) : (output_buff + val_offset);

	return traits_type::to_int_type(ch);
}

template<typename CharT, typename Traits>
typename basic_chmstreambuf<CharT, Traits>::int_type basic_chmstreambuf<CharT, Traits>::pbackfail(int_type ch)
{
	if(!pchmcntrl){
		ERR_CHMPRN("This object did not initialized.");
		return traits_type::eof();
	}

	if(streambuf_type::eback() == streambuf_type::gptr()){
		// now first position in reading buffer(data)
		return traits_type::eof();

	}else if(streambuf_type::eback() < streambuf_type::gptr()){
		if(!traits_type::eq(traits_type::to_char_type(ch), streambuf_type::gptr()[-1])){
			streambuf_type::gbump(-1);
			return traits_type::eof();
		}
		// decrement current
		streambuf_type::gbump(-1);

	}else{
		// why
		ERR_CHMPRN("Something error in pbackfail.");
		return traits_type::eof();
	}
	return traits_type::to_int_type(ch);
}

template<typename CharT, typename Traits>
typename basic_chmstreambuf<CharT, Traits>::pos_type basic_chmstreambuf<CharT, Traits>::seekoff(off_type offset, std::ios_base::seekdir bpostype, open_mode opmode)
{
	if(!(opmode & (std::ios_base::out | std::ios_base::in))){
		ERR_CHMPRN("Parameter is wrong");
		return pos_type(off_type(-1));
	}
	if(!pchmcntrl){
		ERR_CHMPRN("This object did not initialized.");
		return pos_type(off_type(-1));
	}

	// seekp()
	if(opmode & std::ios_base::out){
		// [NOTE]
		// Do not care for a case of that output_buff is NULL.
		// Following codes works good when it is NULL.
		//
		// make new current
		char_type*	newcur		= reinterpret_cast<char_type*>(NULL);
		off_type	newoffset	= offset;
		if(std::ios_base::beg == bpostype){
			newcur		= streambuf_type::pbase() + offset;
			newoffset	-= (streambuf_type::pptr() - streambuf_type::pbase());
		}else if(std::ios_base::end == bpostype){
			char_type*	endpos;
			if(reinterpret_cast<char_type*>(NULL) != output_val_pos){
				endpos	= &output_val_pos[strlen(output_val_pos)];
			}else{
				endpos	= &output_buff[strlen(output_buff)];
			}
			newcur		= endpos + offset;
			newoffset	= newcur - streambuf_type::pptr();
		}else if(std::ios_base::cur == bpostype){
			newcur		= streambuf_type::pptr() + offset;
		}

		// check key value
		if(pchmcntrl->IsClientOnSlvType() && reinterpret_cast<char_type*>(NULL) != output_val_pos){
			if(is_init_key){
				// do not erase key area
				if(newcur < output_val_pos){
					ERR_CHMPRN("Could not seek by underflow.");
					return pos_type(off_type(-1));
				}
			}else{
				if(newcur < output_val_pos){
					// This case is seek backward to over key value separator.
					output_val_pos = reinterpret_cast<char_type*>(NULL);
				}
			}
		}

		// check overflow
		if(streambuf_type::epptr() < newcur){
			// re-calc(from buffer start)
			newoffset = newcur - streambuf_type::pbase();

			// buffer overflow -> expand
			size_t		newsize	= ((newcur - streambuf_type::pbase() + 1) / basic_chmstreambuf<CharT, Traits>::pagesize + 1) * basic_chmstreambuf<CharT, Traits>::pagesize;
			char_type*	newbuff;
			if(reinterpret_cast<char_type*>(NULL) == (newbuff = reinterpret_cast<char_type*>(realloc(output_buff, newsize)))){
				ERR_CHMPRN("Could not allocate memory.");
				return pos_type(off_type(-1));
			}
			output_buff_size= newsize;
			output_buff		= newbuff;

			// reset buffer
			streambuf_type::setp(output_buff, reinterpret_cast<char_type*>(reinterpret_cast<off_type>(output_buff) + static_cast<off_type>(output_buff_size)));
		}
		// move current
		streambuf_type::pbump(newoffset);
		// terminate
		*(streambuf_type::pptr()) = static_cast<char_type>('\0');
	}

	// seekg()
	if(opmode & std::ios_base::in){
		if(reinterpret_cast<char_type*>(NULL) == input_buff){
			// Now have not loaded yet, so try to receiving.
			if(!receive_sync()){
				WAN_CHMPRN("There is no receiving data, and failed to receiving, thus could not seek.");
				return pos_type(off_type(-1));
			}
		}

		// make new current
		char_type*	newcur		= reinterpret_cast<char_type*>(NULL);
		off_type	newoffset	= offset;								// offset from current
		if(std::ios_base::end == bpostype){
			WAN_CHMPRN("Over end of receiving data, thus could not seek.");
			return pos_type(off_type(-1));
		}else if(std::ios_base::beg == bpostype){
			newcur		= streambuf_type::eback() + offset;
			newoffset	-= (streambuf_type::gptr() - streambuf_type::eback());
		}else if(std::ios_base::cur == bpostype){
			newcur		= streambuf_type::gptr() + offset;
		}

		// check over end of input buffer
		if(streambuf_type::egptr() < newcur){
			WAN_CHMPRN("Over end of receiving data, thus could not seek.");
			return pos_type(off_type(-1));
		}

		// move current
		streambuf_type::gbump(newoffset);
	}
	return pos_type(off_type(0));
}

template<typename CharT, typename Traits>
typename basic_chmstreambuf<CharT, Traits>::pos_type basic_chmstreambuf<CharT, Traits>::seekpos(pos_type abspos, open_mode opmode)
{
	return seekoff(off_type(abspos), std::ios_base::beg, opmode);
}

//---------------------------------------------------------
// Template basic_ichmstream
//---------------------------------------------------------
template<typename CharT, typename Traits>
class basic_ichmstream : public std::basic_istream<CharT, Traits>
{
	public:
		typedef CharT										char_type;
		typedef Traits										traits_type;
		typedef typename traits_type::int_type				int_type;
		typedef typename traits_type::pos_type				pos_type;
		typedef typename traits_type::off_type				off_type;
		typedef std::ios_base::openmode						open_mode;
		typedef basic_chmstreambuf<CharT, Traits>			chmstreambuf_type;
		typedef std::basic_istream<char_type, traits_type>	istream_type;
		typedef std::basic_ios<char_type, traits_type>		ios_type;

	private:
		chmstreambuf_type	chmstreambuf;

	protected:
		basic_ichmstream(open_mode opmode = std::ios_base::in) : istream_type(), chmstreambuf(opmode | std::ios_base::in) { ios_type::init(&chmstreambuf); }

	public:
		basic_ichmstream(ChmCntrl* pchmobj, open_mode opmode = std::ios_base::in) : istream_type(), chmstreambuf(pchmobj, opmode | std::ios_base::in) { ios_type::init(&chmstreambuf); }
		~basic_ichmstream() { }

		chmstreambuf_type* rdbuf() const { return const_cast<chmstreambuf_type*>(&chmstreambuf); }
		chmhash_t receivedhash(void) const { return chmstreambuf.receivedhash(); }
};


//---------------------------------------------------------
// Template basic_ochmstream
//---------------------------------------------------------
template <typename CharT, typename Traits>
class basic_ochmstream : public std::basic_ostream<CharT, Traits>
{
	public:
		typedef CharT										char_type;
		typedef Traits										traits_type;
		typedef typename traits_type::int_type				int_type;
		typedef typename traits_type::pos_type				pos_type;
		typedef typename traits_type::off_type				off_type;
		typedef std::ios_base::openmode						open_mode;
		typedef basic_chmstreambuf<CharT, Traits>			chmstreambuf_type;
		typedef std::basic_ostream<char_type, traits_type>	ostream_type;
		typedef std::basic_ios<char_type, traits_type>		ios_type;

	private:
		chmstreambuf_type	chmstreambuf;

	protected:
		basic_ochmstream(open_mode opmode = std::ios_base::out) : ostream_type(), chmstreambuf(opmode | std::ios_base::out) { ios_type::init(&chmstreambuf); }

	public:
		basic_ochmstream(ChmCntrl* pchmobj, open_mode opmode = std::ios_base::out) : ostream_type(), chmstreambuf(pchmobj, opmode | std::ios_base::out) { ios_type::init(&chmstreambuf); }
		basic_ochmstream(ChmCntrl* pchmobj, chmhash_t hash, open_mode opmode = std::ios_base::out) : ostream_type(), chmstreambuf(pchmobj, hash, opmode | std::ios_base::out) { ios_type::init(&chmstreambuf); }
		basic_ochmstream(ChmCntrl* pchmobj, const std::string& strkey, open_mode opmode = std::ios_base::out) : ostream_type(), chmstreambuf(pchmobj, strkey, opmode | std::ios_base::out) { ios_type::init(&chmstreambuf); }
		~basic_ochmstream() { }

		chmstreambuf_type* rdbuf() const { return const_cast<chmstreambuf_type*>(&chmstreambuf); }
		chmhash_t receivedhash(void) const { return chmstreambuf.receivedhash(); }
};


//---------------------------------------------------------
// Template basic_chmstream
//---------------------------------------------------------
template <typename CharT, typename Traits>
class basic_chmstream : public std::basic_iostream<CharT, Traits>
{
	public:
		typedef CharT										char_type;
		typedef Traits										traits_type;
		typedef typename traits_type::int_type				int_type;
		typedef typename traits_type::pos_type				pos_type;
		typedef typename traits_type::off_type				off_type;
		typedef std::ios_base::openmode						open_mode;
		typedef basic_chmstreambuf<CharT, Traits>			chmstreambuf_type;
		typedef std::basic_iostream<char_type, traits_type>	iostream_type;
		typedef std::basic_ios<char_type, traits_type>		ios_type;

	private:
		chmstreambuf_type	chmstreambuf;

	protected:
		basic_chmstream(open_mode opmode = std::ios_base::out | std::ios_base::in) : iostream_type(), chmstreambuf(opmode | std::ios_base::out | std::ios_base::in) { ios_type::init(&chmstreambuf); }

	public:
		basic_chmstream(ChmCntrl* pchmobj, open_mode opmode = std::ios_base::out | std::ios_base::in) : iostream_type(), chmstreambuf(pchmobj, opmode | std::ios_base::out | std::ios_base::in) { ios_type::init(&chmstreambuf); }
		basic_chmstream(ChmCntrl* pchmobj, chmhash_t hash, open_mode opmode = std::ios_base::out | std::ios_base::in) : iostream_type(), chmstreambuf(pchmobj, hash, opmode | std::ios_base::out | std::ios_base::in) { ios_type::init(&chmstreambuf); }
		basic_chmstream(ChmCntrl* pchmobj, const std::string& strkey, open_mode opmode = std::ios_base::out | std::ios_base::in) : iostream_type(), chmstreambuf(pchmobj, strkey, opmode | std::ios_base::out | std::ios_base::in) { ios_type::init(&chmstreambuf); }
		~basic_chmstream() { }

		chmstreambuf_type* rdbuf() const { return const_cast<chmstreambuf_type*>(&chmstreambuf); }
		chmhash_t receivedhash(void) const { return chmstreambuf.receivedhash(); }
};

//---------------------------------------------------------
// Typedefs
//---------------------------------------------------------
template<typename CharT, typename Traits = std::char_traits<CharT> >		class basic_chmstreambuf;
template<typename CharT, typename Traits = std::char_traits<CharT> >		class basic_ichmstream;
template<typename CharT, typename Traits = std::char_traits<CharT> >		class basic_ochmstream;
template<typename CharT, typename Traits = std::char_traits<CharT> >		class basic_chmstream;

typedef basic_chmstreambuf<char>		chmstreambuf;
typedef basic_ichmstream<char>			ichmstream;
typedef basic_ochmstream<char>			ochmstream;
typedef basic_chmstream<char>			chmstream;

#ifdef	_GLIBCXX_USE_WCHAR_T
typedef basic_chmstreambuf<wchar_t>		wchmstreambuf;
typedef basic_ichmstream<wchar_t>		wichmstream;
typedef basic_ochmstream<wchar_t>		wochmstream;
typedef basic_chmstream<wchar_t>		wchmstream;
#endif	// _GLIBCXX_USE_WCHAR_T

#endif	// CHMSTREAM_H

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
