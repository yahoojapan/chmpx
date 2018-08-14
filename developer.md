---
layout: contents
language: en-us
title: Developer
short_desc: Consistent Hashing Mq inProcess data eXchange
lang_opp_file: developerja.html
lang_opp_word: To Japanese
prev_url: build.html
prev_string: Build
top_url: index.html
top_string: TOP
next_url: environments.html
next_string: Environments
---

<!-- -----------------------------------------------------------　-->

# For developer

#### [C API](#CAPI)
[Debug family(C I/F)](#DEBUG)  
[DSO load family(C I/F)](#DSO)  
[Create/Destroy family(C I/F)](#CD)  
[Server node send/receive family(C I/F)](#SERVERNODE)  
[Slave node send/receive family(C I/F)](#SLAVENODE)  
[Server node / slave node reply family(C I/F)](#SERVERSLAVENODE)  
[Utility family(C I/F)](#UTILITY)  
[Automatic merge callback function(C I/F)](#AUTOCALLBACK)  

#### [C++ API](#CPPAPI)
[Debug family(C/C++ I/F)](#CPPDEBUG)  
[ChmCntrl Class](#CHMCNTRL)  
[chmstream(ichmstream、ochmstream) Class](#CHMSTREAM)  

<!-- -----------------------------------------------------------　-->
*** 


## <a name="CAPI"> C API
The server / client program connected to CHMPX needs to use the CHMPX library.
<br />
Include the following header file for development.
```
#include <chmpx/chmpx.h>
```

For the link, specify the following as an option.
```
-lchmpx
```

<!-- -----------------------------------------------------------　-->
*** 

### <a name="DEBUG"> Debug family(C I/F)

#### Format
- void chmpx_bump_debug_level(void)
- void chmpx_set_debug_level_silent(void)
- void chmpx_set_debug_level_error(void)
- void chmpx_set_debug_level_warning(void)
- void chmpx_set_debug_level_message(void)
- void chmpx_set_debug_level_dump(void)
- bool chmpx_set_debug_file(const char* filepath)
- bool chmpx_unset_debug_file(void)
- bool chmpx_load_debug_env(void)

#### Description
- chmpx_bump_debug_level  
  Bump up message output level as non-output -> error -> warning -> information -> communication dump -> non-output.
- chmpx_set_debug_level_silent  
  Set the message output level to non-output.
- chmpx_set_debug_level_error  
  Set the message output level to an error.
- chmpx_set_debug_level_warning  
  Set the message output level to more than warning.
- chmpx_set_debug_level_message  
  Set the message output level to more than information.
- chmpx_set_debug_level_dump  
  Set the message output level to more communication dump.
- chmpx_set_debug_file  
  Specify the file to output the message. If it is not set, it is output to stderr.
- chmpx_unset_debug_file  
  Set to output messages to stderr.
- chmpx_load_debug_env  
  Read the environment variables (CHMDBGMODE, CHMDBGFILE) and set the message output and output destination according to the value.

#### Parameters
- filepath  
  Specify the file path of the message output destination.

#### Return value
chmpx_set_debug_file, chmpx_unset_debug_file, chmpx_load_debug_env will return true on success. If it fails, it returns false.

#### Note
For the environment variables (CHMDBGMODE, CHMDBGFILE), refer to CHMPX details.

<!-- -----------------------------------------------------------　-->
*** 

### <a name="DSO"> DSO load family(C I/F)

#### Format
- bool chmpx_load_hash_library(const char* libpath)
- bool chmpx_unload_hash_library(void)

#### Description
- chmpx_load_hash_library  
  Load the DSO module of the HASH function. Please specify the file path. (Refer to K2HASH)
- chmpx_unload_hash_library  
  If there is a DSO module of the loading HASH function, unload it. (Refer to K2HASH)

#### Return value
Returns true if it succeeds. If it fails, it returns false.

#### Note
DSO of the HASH function becomes the function of K2HASH. This function group is the one wrapping the function of K2HASH. For details, refer to K2HASH DSO load related.

#### Sample
```
if(!chmpx_load_hash_library("/usr/lib64/myhashfunc.so")){
    return false;
}
    //...
    //...
    //...
chmpx_unload_hash_library();
```

<!-- -----------------------------------------------------------　-->
*** 

### <a name="CD"> Create/Destroy family(C I/F)

#### Format
- chmpx_h chmpx_create(const char* conffile, bool is_on_server, bool is_auto_rejoin)
- chmpx_h chmpx_create_ex(const char* conffile, bool is_on_server, bool is_auto_rejoin, chm_merge_get_cb getfp, chm_merge_set_cb setfp, chm_merge_lastts_cb lastupdatefp)
- bool chmpx_destroy(chmpx_h handle)
- bool chmpx_pkth_destroy(chmpx_pkt_h pckthandle)

#### Description
- chmpx_create  
  It generates a CHMPX object and receives a handle.
- chmpx_create_ex  
  It is the same as chmpx_create, but it accepts three function pointers for automatic merging as arguments.
- chmpx_destroy  
  Discard the CHMPX object handle.
- chmpx_pkth_destroy  
  Discard the communication information handle (chmpx_pkt_h).

#### Parameters
- conffile  
  Specify the same as the configuration when CHMPX is started (loaded).
  Specify the configuration file (.ini /. Yaml /. Json) or JSON string.
  If NULL or an empty string is specified, the environment variable (CHMCONFFILE or CHMJSONCONF) is referenced and the set value is loaded appropriately.
- is_on_server  
  Specify true for the purpose of connecting to the server node. (In case of slave node, specify false.)
- is_auto_rejoin  
  Specify whether to wait for reboot (REJOIN) or wait for reboot to cause an error if the CHMPX process is stopped (terminated).
- handle  
  Specifies the handle of the CHMPX object.
- pckthandle  
  Specify the communication information handle.

#### Return value
- chmpx_create  
  Returns a handle to a CHMPX object.
- chmpx_destroy  
  Returns true if CHMPX can be successfully destroyed.
- chmpx_pkth_destroy  
  Returns true if the communication information handle was successfully destroyed.

#### Note
Be sure to discard the generated CHMPX object handle and received communication information handle.

#### Sample

```
chmpx_h    handle;
if(CHM_INVALID_HANDLE == (handle = chmpx_create("myconffile", false, true))){
    return false;
}
    //...
    //...
    //...
if(!chmpx_destroy(handle)){
    return false;
}
```

<!-- -----------------------------------------------------------　-->
*** 

### <a name="SERVERNODE"> Server node send/receive family(C I/F)

#### Format
- bool chmpx_svr_send(chmpx_h handle, const unsigned char* pbody, size_t blength, chmhash_t hash, bool is_routing)
- bool chmpx_svr_send_kvp(chmpx_h handle, const PCHMKVP pkvp, bool is_routing)
- bool chmpx_svr_send_kvp_ex(chmpx_h handle, const PCHMKVP pkvp, bool is_routing, bool without_self)
- bool chmpx_svr_send_kv(chmpx_h handle, unsigned char* pkey, size_t keylen, unsigned char* pval, size_t vallen, bool is_routing)
- bool chmpx_svr_send_kv_ex(chmpx_h handle, unsigned char* pkey, size_t keylen, unsigned char* pval, size_t vallen, bool is_routing, bool without_self)
- bool chmpx_svr_broadcast(chmpx_h handle, const unsigned char* pbody, size_t blength, chmhash_t hash)
- bool chmpx_svr_broadcast_ex(chmpx_h handle, const unsigned char* pbody, size_t blength, chmhash_t hash, bool without_self)
- bool chmpx_svr_replicate(chmpx_h handle, const unsigned char* pbody, size_t blength, chmhash_t hash)
- bool chmpx_svr_replicate_ex(chmpx_h handle, const unsigned char* pbody, size_t blength, chmhash_t hash, bool without_self)
- bool chmpx_svr_recieve(chmpx_h handle, chmpx_pkt_h* ppckthandle, unsigned char** ppbody, size_t* plength, int timeout_ms, bool no_giveup_rejoin)

#### Description
- chmpx_svr_send  
  Send data to one server. (Refer to the note)
- chmpx_svr_send_kvp / chmpx_svr_send_kvp_ex  
  When one server or routing mode is set, data is sent to one of them.(Refer to the note)
- chmpx_svr_send_kv / chmpx_svr_send_kv_ex  
  When one server or routing mode is set, data is sent to one of them.(Refer to the note)
- chmpx_svr_broadcast / chmpx_svr_broadcast_ex  
  It sends data to all servers.(Refer to the note)
- chmpx_svr_replicate / chmpx_svr_replicate_ex  
  It sends data to the server equivalent to the specified HASH (taking into consideration the replication and merge status).(Refer to the note)
- chmpx_svr_recieve  
  Receive data.

#### Parameters
- handle  
  Specifies a handle to a CHMPX object.
- pbody  
  Specify a pointer to transmit data (binary).
- blength  
  Specify the transmission data length.
- hash  
  Specify the destination HASH value.
- is_routing  
  When true is specified, it means that the cluster of CHMPX is of type HASH and will be sent to multiple destinations in multiplexed configuration (number of replicas is 1 or more).
- without_self  
  When true is specified, in the Routing mode or Broadcast mode, send itself by excluding its own server (chmpx).
- pkvp  
  Specify a pointer to the transmission data (KEY, VALUE structure).
- pkey  
  Specify a pointer to the key (binary) of the transmission data.
- keylen  
  Specify the key length of the transmission data.
- pval  
  Specify a pointer to the value (binary) of the transmission data.
- vallen  
  Specify the value length of the transmission data.
- ppckthandle  
  Specify a pointer to acquire communication information handle of received data. Release the received data with chmpx_pkth_destroy.
- ppbody  
  Specify a pointer to receive received data. Release the received data with CHM_Free () etc.
- plength  
  Specify a pointer to receive the received data length.
- hash  
  Specify the HASH value when broadcasting.
- timeout_ms  
  Specify receive timeout. It is specified in milliseconds.
- no_giveup_rejoin  
  Specify whether to ignore the upper limit number of restart wait (no upper limit) when CHMPX stops.

#### Return value
Returns true if it succeeds. If it fails, it returns false.

#### Note
It usually does not start sending from the server node (it will be used for asynchronous communication).  
Normally, the server node performs reception processing and uses the communication information handle at the time of reception to reply to the client process. (See later chapter)

#### Sample

```
chmpx_h    handle;
if(CHM_INVALID_HANDLE == (handle = chmpx_create("myconffile", true, true))){
    return false;
}
chmpx_pkt_h    pckthandle;
unsigned char*    pbody;
size_t        length;
if(!chmpx_svr_recieve(handle, &pckthandle, &pbody, &length, 100, true)){
    chmpx_destroy(handle);
    return false;
}
    //...
    //...
    //...
    //...
chmpx_pkth_destroy(pckthandle);
if(!chmpx_destroy(handle)){
    return false;
}
```

<!-- -----------------------------------------------------------　-->
*** 

### <a name="SLAVENODE"> Slave node send/receive family(C I/F)

#### Format

- msgid_t chmpx_open(chmpx_h handle, bool no_giveup_rejoin)
- bool chmpx_close(chmpx_h handle, msgid_t msgid)
- bool chmpx_msg_send(chmpx_h handle, msgid_t msgid, const unsigned char* pbody, size_t blength, chmhash_t hash, long* precievercnt, bool is_routing)
- bool chmpx_msg_send_kvp(chmpx_h handle, msgid_t msgid, const PCHMKVP pkvp, long* precievercnt, bool is_routing)
- bool chmpx_msg_send_kv(chmpx_h handle, msgid_t msgid, unsigned char* pkey, size_t keylen, unsigned char* pval, size_t vallen, long* precievercnt, bool is_routing)
- bool chmpx_msg_broadcast(chmpx_h handle, msgid_t msgid, const unsigned char* pbody, size_t blength, chmhash_t hash, long* precievercnt)
- bool chmpx_msg_replicate(chmpx_h handle, msgid_t msgid, const unsigned char* pbody, size_t blength, chmhash_t hash, long* preceivercnt)
- bool chmpx_msg_recieve(chmpx_h handle, msgid_t msgid, chmpx_pkt_h* ppckthandle, unsigned char** ppbody, size_t* plength, int timeout_ms)

#### Description
- chmpx_open  
  Opens a message handle for communication with the slave node.
- chmpx_close  
  Close the message handle.
- chmpx_msg_send  
  Send the data.
- chmpx_msg_send_kvp  
  Send the data.
- chmpx_msg_send_kv  
  Send the data.
- chmpx_msg_broadcast  
  Broadcast data transmission.
- chmpx_msg_replicate  
  It sends data to the server equivalent to the specified HASH (taking into consideration the replication and merge status).
- chmpx_msg_recieve  
  Receive data.
   
#### Parameters
- handle  
  Specifies a handle to a CHMPX object.
- no_giveup_rejoin  
  Specify whether to ignore the upper limit number of restart wait (no upper limit) when CHMPX stops.
- msgid  
  Specify a message handle.  
- pbody  
  Specify a pointer to transmit data (binary).
- blength  
  Specify the transmission data length.
- hash  
  Specify HASH value of transmission data.
- precievercnt  
  Returns the number of receiving nodes when data is transmitted.
- is_routing  
  When true is specified, it means that the cluster of CHMPX is HASH type and will be sent to multiple destinations in multiplexed configuration (number of replicas is 1 or more).
- pkvp  
  Specify a pointer to the transmission data (KEY, VALUE structure).
- pkey  
  Specify a pointer to the key (binary) of the transmission data.
- keylen  
  Specify the key length of the transmission data.
- pval  
  Specify a pointer to the value (binary) of the transmission data.
- vallen  
  Specify the value length of the transmission data.
- ppckthandle  
  Specify a pointer to obtain communication information handle of received data (see note). Received data is released with chmpx_pkth_destroy.
- ppbody  
  Specify a pointer to receive received data. The received data is released with CHM_Free ().
- plength  
  Specify a pointer to receive the received data length.
- timeout_ms  
  Specify receive timeout. It is specified in milliseconds.

#### Return value
Returns true if it succeeds. If it fails, it returns false.

#### Note
The slave node will not normally start from the receive sequence (it will be used for asynchronous communications).
Normally, the slave node performs transmission processing and waits for a reply to the message handle sent.

#### Sample
```
chmpx_h    handle;
if(CHM_INVALID_HANDLE == (handle = chmpx_create("myconffile", false, true))){
    return false;
}
msgid_t    msgid;
if(CHM_INVALID_MSGID == (msgid = chmpx_open(handle, true))){
    chmpx_destroy(handle);
    return false;
}
unsigned char    body[]    = ....;
size_t        blength    = sizeof(body);
chmhash_t    hash    = 0x.....;
long        recievercnt;
if(!chmpx_msg_send(handle, msgid, body, blength, hash, &recievercnt, true)){
    chmpx_close(handle, msgid);
    chmpx_destroy(handle);
    return false;
}
chmpx_pkt_h    pckthandle;
unsigned char*    pbody;
size_t        length;
if(!chmpx_msg_recieve(handle, msgid, &pckthandle, &pbody, &length, 100)){
    chmpx_close(handle, msgid);
    chmpx_destroy(handle);
    return false;
}
    //...
    //...
    //...
    //...
chmpx_pkth_destroy(pckthandle);
if(!chmpx_close(handle, msgid)){
    chmpx_destroy(handle);
    return false;
}
if(!chmpx_destroy(handle)){
    return false;
}
```

<!-- -----------------------------------------------------------　-->
*** 

### <a name="SERVERSLAVENODE"> Server node / slave node reply family(C I/F)

#### Format
- bool chmpx_msg_reply(chmpx_h handle, chmpx_pkt_h pckthandle, const unsigned char* pbody, size_t blength)
- bool chmpx_msg_reply_kvp(chmpx_h handle, chmpx_pkt_h pckthandle, const PCHMKVP pkvp)
- bool chmpx_msg_reply_kv(chmpx_h handle, chmpx_pkt_h pckthandle, unsigned char* pkey, size_t keylen, unsigned char* pval, size_t vallen)

#### Description
- chmpx_msg_reply  
  It returns binary data.
- chmpx_msg_reply_kvp  
  It returns binary (KEY, VALUE structure) data.
- chmpx_msg_reply_kv  
  It returns data with key and value specified.

#### Parameters
- handle  
  Specifies a handle to a CHMPX object.
- pckthandle  
  Specify the communication information handle received at the time of reception.
- pbody  
  Specify a pointer to reply data (binary).
- blength  
  Specify the reply data length.
- pkvp  
  Specify a pointer to reply data (KEY, VALUE structure).
- pkey  
  Specify a pointer to the reply data key (binary).
- keylen  
  Specify the key length of reply data.
- pval  
  Specify a pointer to the value (binary) of the reply data.
- vallen  
  Specify the value length of reply data.

#### Return value
Returns true if it succeeds. If it fails, it returns false.

#### Note
The slave node will not normally reply (it may be used for asynchronous communication).
Therefore, this function group is mainly used in the server node.

#### Sample

```
chmpx_h    handle;
if(CHM_INVALID_HANDLE == (handle = chmpx_create("myconffile", true, true))){
    return false;
}
chmpx_pkt_h    pckthandle;
unsigned char*    pbody;
size_t        length;
if(!chmpx_svr_recieve(handle, &pckthandle, &pbody, &length, 100, true)){
    chmpx_destroy(handle);
    return false;
}
    //...
    //...
    //...
    //...
unsigned char    body[] = ....;
size_t        blength = sizeof(body);
if(!chmpx_msg_reply(handle, pckthandle, body, blength)){
    chmpx_pkth_destroy(pckthandle);
    chmpx_destroy(handle);
    return false;
}
if(!chmpx_pkth_destroy(pckthandle)){
    chmpx_destroy(handle);
    return false;
}
if(!chmpx_destroy(handle)){
    return false;
}
```

<!-- -----------------------------------------------------------　-->
*** 

### <a name="UTILITY"> Utility family(C I/F)

#### Format
- bool is_chmpx_proc_exists(chmpx_h handle)
- void chmpx_print_version(FILE* stream)
- unsigned char* cvt_kvp_bin(const PCHMKVP pkvp, size_t* plength)
- bool cvt_bin_kvp(PCHMKVP pkvp, unsigned char* bydata, size_t length)
- chmhash_t make_chmbin_hash(const PCHMBIN pchmbin)
- chmhash_t make_kvp_hash(const PCHMKVP pkvp)

#### Description
- is_chmpx_proc_exists  
  Make sure CHMPX process exists.
- chmpx_print_version  
  Returns the version of the CHMPX library.
- cvt_kvp_bin  
  Converts data of CHMKVP structure to binary column data.
- cvt_bin_kvp  
  Convert binary column data to CHMKVP structure.
- make_chmbin_hash  
  Generate HASH value from data of CHMBIN structure.
- make_kvp_hash  
  Generate HASH value from data of CHMKVP structure.

#### Parameters
- handle  
  Specifies a handle to a CHMPX object.
- stream  
  Specify the file pointer. (Stdout / stderr can be specified)
- pkvp  
  Specify a pointer to the CHMKVP structure.
- plength  
  Specify a buffer to store the data length of the binary string to be returned.
- bydata  
  Specify a pointer to the binary string to be converted.
- length  
  Specify the data length of the binary string to be converted.
- pchmbin  
  Specify a pointer to the CHMBIN structure.
- pkvp  
  Specify a pointer to the CHMKVP structure.

#### Return value
- is_chmpx_proc_exists  
  Returns true if it succeeds. If it fails, it returns false.
- chmpx_print_version  
  There is no return value.
- cvt_kvp_bin  
  Returns a pointer to the binary string allocated to the area.
- cvt_bin_kvp  
  Returns true if it succeeds. If it fails, it returns false.
- make_chmbin_hash  
  Returns the HASH value.
- make_kvp_hash  
  Returns the HASH value.

#### Note
nothing special.

<!-- -----------------------------------------------------------　-->
*** 

### <a name="AUTOCALLBACK"> Automatic merge callback function(C I/F)
This is a callback function that can be set by the client on the server node when using the auto merge function.
Specify the function defined by the following function type in the chmpx_create_ex function, InitializeOnServer method.
If these functions are not specified, processing by callback is not performed, and automatic merging works assuming that it is processed by chmpx.


#### Format
- typedef bool (*chm_merge_lastts_cb)(chmpx_h handle, struct timespec* pts)
- typedef bool (*chm_merge_get_cb)(chmpx_h handle, const PCHM_MERGE_GETPARAM pparam, chmhash_t* pnexthash, PCHMBIN* ppdatas, size_t* pdatacnt)
- typedef bool (*chm_merge_set_cb)(chmpx_h handle, size_t length, const unsigned char* pdata, const struct timespec* pts);

#### Description
- chm_merge_lastts_cb  
  It is called when acquiring the last update time of the data possessed before starting the merge of the whole cluster. Returns the last update time in the timespec structure. The data updated after the returned time will be merged. If all data is to be merged, * pts = {0, 0} is returned. If this callback is not set, all data will be merged.
- chm_merge_get_cb  
  It is called to acquire the data to be merged during merge processing. PCHM_MERGE_GETPARAM contains information on the data to be merged. It searches data possessed from this information, sets target data to ppdatas, and returns it. If there are multiple target data, please return ppdatas as an array of PCHMBIN data. For pdatacnt, set the number of ppdatas. If this callback is not set,merge is executed as if there was no target data. The search condition is indicated by pparam, and this structure contains the start and end of the update time of the target data, the search start HASH value (data after this HASH value), the HASH value to be merged, the HASH value to be merged The maximum value is specified.
- chm_merge_set_cb  
  Called from chmpx on the node that needs to merge. One data to be merged acquired from another node is called with an argument. Processing of this callback will register the received data in its own database. If this callback is not set, merge is executed as if there was no target data.

#### Parameters
- handle  
  chmpx_h is passed.
- pts  
  In the case of chm_merge_lastts_cb, it returns the start time of the data to be merged.
  In the case of chm_merge_set_cb, it passes the last time of data to be set (meaning that it was updated before this time and it was not updated at this time).
- pparam  
  The search condition of the data to be merged is set.
- pnexthash  
  As a result of data search, it returns the HASH value to be used as the next search condition.
- ppdatas  
  Set the detected merge target data in this pointer. More than one target data can be returned, and it is returned as an array pointer. If search data does not exist (complete), it returns NULL.
- pdatacnt  
  Returns the number of ppdatas data. If search data does not exist (complete), 0 is returned.
- length  
  Indicates the data length of pdata.
- pdata  
  It is a binary string of merge data.

#### Structure
It is the parameter CHM_MERGE_GETPARAM passed by chm_merge_get_cb.

```
typedef struct chm_merge_getparam{
    chmhash_t           starthash;
    struct timespec     startts;                // start time for update datas
    struct timespec     endts;                  // end time for update datas
    chmhash_t           target_hash;            // target hash value(0 ... target_max_hash)
    chmhash_t           target_max_hash;        // max target hash value
    long                target_hash_range;      // range for hash value
}CHM_MERGE_GETPARAM, *PCHM_MERGE_GETPARAM;
```

The following is a description and definition of the member.
- starthash  
  A start HASH value for data search is specified.
- startts  
  The minimum time in the update time range of the target data is specified.
- endts  
  The maximum time in the update time range of the target data is specified.
- target_hash  
  The target HASH value on the chmpx ring (when it is 10 nodes, it will be 0 to 9) is specified for data search.
- target_max_hash  
  The maximum number of nodes on the chmpx ring when data search is performed is shown. The maximum HASH value is (target__max_hash - 1). (If the HASH value of the data is 12 and target_max_hash = 9, target_hash = 3, this HASH value 12 data is eligible)
- target_hash_range  
  It shows the target HASH value range. From target_hash, this range number is the range of the target HASH value. (If target_hash = 3, target_hash_range = 2, the target HASH value is 2, 3 and 4.)

#### Return value
If processing is completed normally, please return true. If processing fails, return false. (If false, merge processing will be interrupted.)

#### Note
nothing special.

<!-- -----------------------------------------------------------　-->
<!-- -----------------------------------------------------------　-->
<!-- -----------------------------------------------------------　-->
<!-- -----------------------------------------------------------　-->
<!-- -----------------------------------------------------------　-->
***

## <a name="CPPAPI"> C++ API
The server / client program connected to CHMPX needs to use the CHMPX library. 
<br />
Include the following header file and other appropriate header file at development time.
```
#include <chmpx/chmcntrl.h>
#include <chmpx/chmpx.h>
```
<br />
When linking, specify the following as an option.  
```
-lchmpx
```
<br />
The functions for the C ++ language are explained below.

<!-- -----------------------------------------------------------　-->
*** 

### <a name="CPPDEBUG"> Debug family(C/C++ I/F)

#### Format
- ChmDbgMode SetChmDbgMode(ChmDbgMode mode);
- ChmDbgMode BumpupChmDbgMode(void);
- ChmDbgMode GetChmDbgMode(void);
- bool LoadChmDbgEnv(void);
- bool SetChmDbgFile(const char* filepath);
- bool UnsetChmDbgFile(void);

#### Description
- SetChmDbgMode  
  Specify the level of message output. Specify the value of CHMDBG_SILENT, CHMDBG_ERR, CHMDBG_WARN, CHMDBG_MSG, CHMDBG_DUMP.
- BumpupChmDbgMode  
  Bump up message output level as non-output -> error -> warning -> information -> communication dump -> non-output.
- GetChmDbgMode  
  Get the current message output level.
- LoadChmDbgEnv  
  Read the environment variables (CHMDBGMODE, CHMDBGFILE) and set the message output and output destination according to the value.
- SetChmDbgFile  
  Specify the file to output the message. If it is not set, it is output to stderr.
- UnsetChmDbgFile  
  Set to output messages to stderr.


#### Parameters
- mode  
  Specify the level of message output.
- filepath  
  Specify the file path of the message output destination.

#### Return value
- SetChmDbgMode  
  Returns the level of the previous message output.
- BumpupChmDbgMode  
  Returns the level of the previous message output.
- GetChmDbgMode  
  Returns the current message output level.
- LoadChmDbgEnv  
  Returns true if it succeeds. If it fails, it returns false.
- SetChmDbgFile  
  Returns true if it succeeds. If it fails, it returns false.
- UnsetChmDbgFile  
  Returns true if it succeeds. If it fails, it returns false.

#### Note
For the environment variables (CHMDBGMODE, CHMDBGFILE), refer to CHMPX details.

<!-- -----------------------------------------------------------　-->
*** 

### <a name="CHMCNTRL"> ChmCntrl class

#### Description
Class of CHMPX object.  
Through this class, we will send and receive data with CHMPX.  
For basic server node programming, create instances of this class and send and receive data.  
In the case of a slave node, it creates an instance of this class, opens a message handle, and sends and receives data.  
When the operation is completed, close the message handle and destroy this object.  

#### Method
- bool InitializeOnServer(const char* cfgfile, bool is_auto_rejoin = false, chm_merge_get_cb getfp = NULL, chm_merge_set_cb setfp = NULL, chm_merge_lastts_cb lastupdatefp = NULL, short ctlport = CHM_INVALID_PORT)
- bool InitializeOnSlave(const char* cfgfile, bool is_auto_rejoin = false, short ctlport = CHM_INVALID_PORT)
- bool OnlyAttachInitialize(const char* cfgfile, short ctlport = CHM_INVALID_PORT)
<br /><br />
- bool Recieve(PCOMPKT* ppComPkt, unsigned char** ppbody = NULL, size_t* plength = NULL, int timeout_ms = ChmCntrl::EVENT_NOWAIT, bool no_giveup_rejoin = false)
- bool Send(const unsigned char* pbody, size_t blength, chmhash_t hash, bool without_self)
- bool Broadcast(const unsigned char* pbody, size_t blength, chmhash_t hash, bool without_self)
- bool Replicate(const unsigned char* pbody, size_t blength, chmhash_t hash, bool without_self = true)
<br /><br /> 
- msgid_t Open(bool no_giveup_rejoin = false)
- bool Close(msgid_t msgid)
- bool Recieve(msgid_t msgid, PCOMPKT* ppComPkt, unsigned char** ppbody, size_t* plength, int timeout_ms = 0)
- bool Send(msgid_t msgid, const unsigned char* pbody, size_t blength, chmhash_t hash, long* precievercnt = NULL, bool is_routing = true)
- bool Broadcast(msgid_t msgid, const unsigned char* pbody, size_t blength, chmhash_t hash, long* precievercnt = NULL)
- bool Replicate(msgid_t msgid, const unsigned char* pbody, size_t blength, chmhash_t hash, long* preceivercnt = NULL)
<br /><br /> 
- bool Send(PCOMPKT pComPkt, const unsigned char* pbody, size_t blength)
- bool Reply(PCOMPKT pComPkt, const unsigned char* pbody, size_t blength)
<br /><br /> 
- bool IsChmpxExit(void)
<br /><br /> 
- PCHMINFOEX DupAllChmInfo(void)
- PCHMPX DupSelfChmpxInfo(void)
- static void FreeDupAllChmInfo(PCHMINFOEX ptr)
- static void FreeDupSelfChmpxInfo(PCHMPX ptr)

#### Method Description
- ChmCntrl::InitializeOnServer  
  Initialize the CHMPX object for the client process on the server node. Specify the same configuration as when starting CHMPX process. This specifies the configuration file (.ini /. Yaml /. Json) or JSON string. If the configuration parameter is NULL or an empty string is specified, the environment variable (CHMCONFFILE or CHMJSONCONF) is referenced and the set value is loaded appropriately. In the case of C ++ interface, there is a ctlport argument (the same specification as -ctlport when starting chmpx).
- ChmCntrl::InitializeOnSlave  
Initialize the CHMPX object for the client process on the slave node. Specify the same configuration as when starting CHMPX process. This specifies the configuration file (.ini /. Yaml /. Json) or JSON string. If the configuration parameter is NULL or an empty string is specified, the environment variable (CHMCONFFILE or CHMJSONCONF) is referenced and the set value is loaded appropriately. In the case of C ++ interface, there is a ctlport argument (the same specification as -ctlport when starting chmpx).
- ChmCntrl::OnlyAttachInitialize  
Initializes the CHMPX object according to the specified configuration (file specification, JSON string specification, environment variable specification) and control port. 
Initialization is done by attaching to CHMSHM (management SHM of chmpx). The initialized CHMPX object can only be used with the method (DupAllChmInfo, DupSelfChmpxInfo) which obtains the status (status) of chmpx.
- ChmCntrl::Recieve  
Receive method for client process on server node. It receives a COMPKT pointer, a pointer to binary data, and a binary data length. You can specify a timeout. You can also specify whether to wait for a reboot if the CHMPX process has stopped. Release the received ppComPkt pointer and body data with CHM_Free () etc.
- ChmCntrl::Send  
  The sending method for the client process on the server node. Specify binary data string, length, HASH value.
- ChmCntrl::Broadcast  
  Broadcast send method for client process on server node. Specify binary data string, length, HASH value.
- ChmCntrl::Replicate  
  It is a sending method to servers (depending on the replication setting and merge status) according to the specified HASH value for the client process on the server node. Specify binary data string, length, HASH value.
- ChmCntrl::Open  
  It is a client process use on a slave node and opens a message handle. You can specify whether to wait for a reboot if the CHMPX process has stopped.
- ChmCntrl::Close  
  It is a client process usage on the slave node and closes the message handle. Specify the message handle to close.
- ChmCntrl::Recieve  
  Receive method for client process on slave node. It receives a COMPKT pointer, a pointer to binary data, and a binary data length. You can specify a timeout. Release the received ppComPkt pointer and body data with CHM_Free () etc.
- ChmCntrl::Send  
  The sending method for the client process on the slave node.
Specify binary data string, length, HASH value. If precievercnt is specified, it is also possible to obtain the number of CHMPX of the destination.
  When the cluster of CHMPX is HASH type and multiplexed configuration (number of replicas is 1 or more), you can specify whether or not to send to multiple destinations.
- ChmCntrl::Broadcast  
  Broadcast send method for client process on slave node. Specify binary data string, length, HASH value. If precievercnt is specified, it is also possible to obtain the number of CHMPX of the destination.
- ChmCntrl::Replicate  
  It is a sending method to servers (depending on the replication setting and merge status) according to the specified HASH value for the client process on the slave node. Specify binary data string, length, HASH value.
- ChmCntrl::Send  
  This method is used when replying from the client process. It specifies COMPKT pointer, pointer to binary data, binary data length and returns data.
- ChmCntrl::Reply  
  It is a method to use when replying from client process (same as above). It specifies COMPKT pointer, pointer to binary data, binary data length and returns data.
- ChmCntrl::IsChmpxExit  
  Method for checking the existence of CHMPX process.
- ChmCntrl::DupAllChmInfo  
  This method is used in CHMPX object initialized with OnlyAttachInitialize. With this method, you can get the status of all chmpx owned by the attached CHMSHM. Acquisition data is returned in the allocated PCHMINFOEX structure. Free up this pointer with the ChmCntrl::FreeDupAllChmInfo class method.
- ChmCntrl::DupSelfChmpxInfo  
  This method is used in CHMPX object initialized with OnlyAttachInitialize. With this method you can get the status of your own chmpx owned by the attached CHMSHM. Acquired data is returned in the allocated PCHMPX structure. This pointer is released with ChmCntrl::FreeDupSelfChmpxInfo class method.
- ChmCntrl::FreeDupAllChmInfo  
  Free up the area of PCHMINFOEX data pointer acquired with ChmCntrl::DupAllChmInfo.
- ChmCntrl::FreeDupSelfChmpxInfo  
  Free up the area of PCHMPX data pointer obtained by ChmCntrl::DupSelfChmpxInfo.

#### Method return value
A prototype whose return value is bool returns true if it succeeds. If it fails, it returns false.
The ChmCntrl::Open method returns an open message handle.

#### Sample (server node)

```
ChmCntrl*    pchmpx = new ChmCntrl();
if(!pchmpx->InitializeOnServer("myconffile", true)){
    CHM_Delete(pchmpx);
    return false;
}
PCOMPKT        pComPkt;
unsigned char*    pbody;
size_t        length;
if(!pchmpx->Recieve(&pComPkt, &pbody, &length, 0, true)){
    CHM_Delete(pchmpx);
    return false;
}
    //...
    //...
    //...
unsigned char    body[] = ....;
size_t        blength = sizeof(body);
if(!pchmpx->Reply(pComPkt, body, blength)){
    CHM_Free(pComPkt);
    CHM_Delete(pchmpx);
    return false;
}
CHM_Free(pComPkt);
CHM_Delete(pchmpx);
```

#### Sample (slave node)
```
ChmCntrl*    pchmpx = new ChmCntrl();
if(!pchmpx->InitializeOnSlave("myconffile", true)){
    CHM_Delete(pchmpx);
    return false;
}
msgid_t    msgid;
if(CHM_INVALID_MSGID == (msgid = pchmpx->Open(true))){
    CHM_Delete(pchmpx);
    return false;
}
unsigned char    body[] = ....;
size_t        blength = sizeof(body);
chmhash_t    hash = 0x....;
long        recievercnt;
if(!pchmpx->Send(msgid, body, blength, hash, &recievercnt, true)){
    pchmpx->Close(msgid);
    CHM_Delete(pchmpx);
    return false;
}
    //...
    //...
    //...
PCOMPKT        pComPkt;
unsigned char*    pbody;
size_t        length;
if(!pchmpx->Recieve(&pComPkt, &pbody, &length, 0){
    pchmpx->Close(msgid);
    CHM_Delete(pchmpx);
    return false;
}
CHM_Free(pComPkt);
pchmpx->Close(msgid);
CHM_Delete(pchmpx);
```
#### Get status
```
ChmCntrl    chmobj;
if(!chmobj.OnlyAttachInitialize(conffile.c_str(), ctlport)){
    return false;
}
if(!is_self){
    PCHMINFOEX    pInfo = pchmobj->DupAllChmInfo();
        ...
        ...
        ...
    ChmCntrl::FreeDupAllChmInfo(pInfo);
}else{
    PCHMPX    pInfo = pchmobj->DupSelfChmpxInfo();
        ...
        ...
        ...
    ChmCntrl::FreeDupSelfChmpxInfo(pInfo);
}
chmobj.Clean();
```

<!-- -----------------------------------------------------------　-->
*** 

### <a name="CHMSTREAM"> chmstream(ichmstream、ochmstream) class

#### Description
An iostream derived class that provides direct access (send, receive) to a CHMPX object.
It is a class that can be used as an iostream by specifying the ChmCntrl class, initializing the stream classes chmstream, ichmstream, ochmstream.
It is equivalent to normal std::stringstream and you can use seekpos.  
Please refer to std::stringstream for detailed explanation.

#### Base class
<table>
<tr><td>chmstream </td><td>std::basic_stream </td></tr>
<tr><td>ichmstream</td><td>std::basic_istream</td></tr>
<tr><td>ochmstream</td><td>std::basic_ostream</td></tr>
<table>

#### Sample
```
ChmCntrl* pchmpx = new ChmCntrl();
 ...
 ...
 ...
  
// server node stream test
{
 chmstream strm(&chmobj);
 string strKey;
 string strVal;
 strm >> strKey;
 strm >> strVal;
 cout << "Key: " << strKey << " Value: " << strVal << endl;
 strm.clear();
 strm << strKey << endl;
 strm << strVal << endl;
}
  
// slave node stream test
{
 chmstream strm(&chmobj);
 string strKey = "....";
 string strVal = "....";
 strm << strKey << endl;
 strm << strVal << endl;
 strm.clear();
 strm >> strKey;
 strm >> strVal;
 cout << "Key: " << strKey << " Value: " << strVal << endl;
}
```

<!-- -----------------------------------------------------------　-->
<!-- -----------------------------------------------------------　-->
<!-- -----------------------------------------------------------　-->
<!-- -----------------------------------------------------------　-->
