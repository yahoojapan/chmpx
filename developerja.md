---
layout: contents
language: ja
title: Developer
short_desc: Consistent Hashing Mq inProcess data eXchange
lang_opp_file: developer.html
lang_opp_word: To English
prev_url: buildja.html
prev_string: Build
top_url: indexja.html
top_string: TOP
next_url: environmentsja.html
next_string: Environments
---

<!-- -----------------------------------------------------------　-->

# 開発者向け

#### [C言語インタフェース](#CAPI)
[デバッグ関連（C I/F）](#DEBUG)  
[DSOロード関連（C I/F）](#DSO)  
[Create/Destroy関連（C I/F）](#CD)  
[サーバノード送受信関連（C I/F）](#SERVERNODE)  
[スレーブノード送受信関連（C I/F）](#SLAVENODE)  
[サーバノード・スレーブノード返信関連（C I/F）](#SERVERSLAVENODE)  
[ユーティリティ関連（C I/F）](#UTILITY)  
[自動マージコールバック関数（C I/F）](#AUTOCALLBACK)  

#### [C++言語インタフェース](#CPPAPI)
[デバッグ関連(C/C++ I/F)](#CPPDEBUG)  
[ChmCntrlクラス](#CHMCNTRL)  
[chmstream（ichmstream、ochmstream）クラス](#CHMSTREAM)  

<!-- -----------------------------------------------------------　-->
*** 


## <a name="CAPI"> C言語インタフェース
CHMPXと接続するサーバー/クライアントプログラムは、CHMPXライブラリを利用する必要があります。    

開発時には以下のヘッダファイルをインクルードしてください。
```
#include <chmpx/chmpx.h>
```

リンク時には以下をオプションとして指定してください。  
```
-lchmpx
```

<!-- -----------------------------------------------------------　-->
*** 

### <a name="DEBUG"> デバッグ関連（C I/F）

#### 書式
- void chmpx_bump_debug_level(void)
- void chmpx_set_debug_level_silent(void)
- void chmpx_set_debug_level_error(void)
- void chmpx_set_debug_level_warning(void)
- void chmpx_set_debug_level_message(void)
- void chmpx_set_debug_level_dump(void)
- bool chmpx_set_debug_file(const char* filepath)
- bool chmpx_unset_debug_file(void)
- bool chmpx_load_debug_env(void)

#### 説明
- chmpx_bump_debug_level  
  メッセージ出力レベルを、非出力→エラー→ワーニング→インフォメーション→通信ダンプ→非出力・・・とBump upします。
- chmpx_set_debug_level_silent  
  メッセージ出力レベルを、出力しないようにします。
- chmpx_set_debug_level_error  
  メッセージ出力レベルを、エラーにします。
- chmpx_set_debug_level_warning  
  メッセージ出力レベルを、ワーニング以上にします。
- chmpx_set_debug_level_message  
  メッセージ出力レベルを、インフォメーション以上にします。
- chmpx_set_debug_level_dump  
  メッセージ出力レベルを、通信ダンプ以上にします。
- chmpx_set_debug_file  
  メッセージを出力するファイルを指定します。設定されていない場合には stderr に出力します。
- chmpx_unset_debug_file  
  メッセージを stderr に出力するように戻します。
- chmpx_load_debug_env  
  環境変数 CHMDBGMODE、CHMDBGFILE を読み込み、その値にしたがってメッセージ出力、出力先を設定します。

#### パラメータ
- filepath  
  メッセージ出力先のファイルパスを指定します。

#### 返り値
chmpx_set_debug_file、chmpx_unset_debug_file、chmpx_load_debug_env は成功した場合には、true を返します。失敗した場合には false を返します。

#### 注意
環境変数 CHMDBGMODE、CHMDBGFILE については、CHMPX 詳細部分を参照してください。

<!-- -----------------------------------------------------------　-->
*** 

### <a name="DSO"> DSOロード関連（C I/F）

#### 書式
- bool chmpx_load_hash_library(const char* libpath)
- bool chmpx_unload_hash_library(void)

#### 説明
- chmpx_load_hash_library  
  HASH関数のDSOモジュールをロードさせます。ファイルパスを指定してください。（K2HASH参照）
- chmpx_unload_hash_library  
  ロードしているHASH関数のDSOモジュールがある場合には、アンロードします。（K2HASH参照）

#### 返り値
成功した場合には、true を返します。失敗した場合には false を返します。

#### 注意
HASH関数のDSOは、K2HASHの機能になります。本関数群はK2HASHの関数をラップしたものになります。詳細は、K2HASHのDSOロード関連を参照してください。

#### サンプル
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

### <a name="CD"> Create/Destroy関連（C I/F）

#### 書式
- chmpx_h chmpx_create(const char* conffile, bool is_on_server, bool is_auto_rejoin)
- chmpx_h chmpx_create_ex(const char* conffile, bool is_on_server, bool is_auto_rejoin, chm_merge_get_cb getfp, chm_merge_set_cb setfp, chm_merge_lastts_cb lastupdatefp)
- bool chmpx_destroy(chmpx_h handle)
- bool chmpx_pkth_destroy(chmpx_pkt_h pckthandle)

#### 説明
- chmpx_create  
  CHMPXオブジェクトを生成し、ハンドルを受け取ります。
- chmpx_create_ex  
  chmpx_createと同じですが、引数に自動マージ用の関数ポインタを3つ受け取ります。
- chmpx_destroy  
  CHMPXオブジェクトハンドルを破棄します。
- chmpx_pkth_destroy  
  通信情報ハンドル（chmpx_pkt_h）の破棄をします。

#### パラメータ
- conffile  
  CHMPXを起動したとき（ロードさせている）コンフィグレーションと同じものを指定してください。
  コンフィグレーションファイル（.ini/.yaml/.json）もしくは、JSON文字列を指定します。
  NULLもしくは、空文字列を指定した場合には、環境変数 CHMCONFFILE もしくは CHMJSONCONF が参照され、適切に設定値が読み込まれます。
- is_on_server  
  サーバノードに接続する目的の場合 true を指定します。（スレーブノードの場合には false を指定します。）
- is_auto_rejoin  
  CHMPXプロセスが停止（終了）した場合に、再起動を待つ（REJOIN）か、再起動を待たないでエラーとするかを指定します。
- handle  
  CHMPXオブジェクトのハンドルを指定します。
- pckthandle  
  通信情報ハンドルを指定します。

#### 返り値
- chmpx_create  
  CHMPXオブジェクトへのハンドルを返します。
- chmpx_destroy  
  正常にCHMPXを破棄できた場合に true を返します。
- chmpx_pkth_destroy  
  正常に通信情報ハンドルを破棄できた場合に true を返します。

#### 注意
生成したCHMPXオブジェクトハンドル、受け取った通信情報ハンドルは必ず破棄してください。

#### サンプル

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

### <a name="SERVERNODE"> サーバノード送受信関連（C I/F）

#### 書式
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

#### 説明
- chmpx_svr_send  
  サーバ１台にデータを送信します。（注意参照）
- chmpx_svr_send_kvp / chmpx_svr_send_kvp_ex  
  サーバ１台もしくはルーティングモードが設定されている場合にはそのうち１台にデータを送信します。（注意参照）
- chmpx_svr_send_kv / chmpx_svr_send_kv_ex  
  サーバ１台もしくはルーティングモードが設定されている場合にはそのうち１台にデータを送信します。（注意参照）
- chmpx_svr_broadcast / chmpx_svr_broadcast_ex  
  サーバ全台にデータを送信します。（注意参照）
- chmpx_svr_replicate / chmpx_svr_replicate_ex  
  指定したHASHと同等（レプリケーション、マージ状態を考慮して）のサーバにデータを送信します。（注意参照）
- chmpx_svr_recieve  
  データを受信します。

#### パラメータ
- handle  
  CHMPXオブジェクトへのハンドルを指定します。
- pbody  
  送信データ（バイナリ）へのポインタを指定します。
- blength  
  送信データ長を指定します。
- hash  
  送信先のHASH値を指定します。
- is_routing  
  true を指定した場合には、CHMPXのクラスタが、HASHタイプであり、多重化された構成（レプリカ数が1以上）では複数のあて先に送付されることを意味します。
- without_self  
  true を指定した場合には、Routingモード、Broadcastモード時において、自分自身（chmpx）のサーバを除外して送信をします。
- pkvp  
  送信データ（KEY、VALUE構造体）へのポインタを指定します。
- pkey  
  送信データのキー（バイナリ）へのポインタを指定します。
- keylen  
  送信データのキー長を指定します。
- pval  
  送信データの値（バイナリ）へのポインタを指定します。
- vallen  
  送信データの値長を指定します。
- ppckthandle  
  受信データの通信情報ハンドルを取得するポインタを指定します。受け取ったデータは、chmpx_pkth_destroyで開放してください。
- ppbody  
  受信データを受け取るためのポインタを指定します。受け取ったデータは、CHM_Free() などで開放してください。
- plength  
  受信データ長を受け取るためのポインタを指定します。
- hash  
  ブロードキャスト時にHASH値を指定します。
- timeout_ms  
  受信タイムアウトを指定します。ミリ秒で指定します。
- no_giveup_rejoin  
  CHMPXが停止した場合に、再起動待ちの上限回数を無視（上限なし）するかどうかを指定します。

#### 返り値
成功した場合には、true を返します。失敗した場合には false を返します。

#### 注意
サーバノードから通常は送信を開始することはありません（非同期通信を行う場合には利用することはあります）。
通常、サーバノードでは受信処理を行い、その受信した際の「通信情報ハンドル」を使い、クライアントプロセスへ返信をすることになります。（後章参照）

#### サンプル

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

### <a name="SLAVENODE"> スレーブノード送受信関連（C I/F）

#### 書式

- msgid_t chmpx_open(chmpx_h handle, bool no_giveup_rejoin)
- bool chmpx_close(chmpx_h handle, msgid_t msgid)
- bool chmpx_msg_send(chmpx_h handle, msgid_t msgid, const unsigned char* pbody, size_t blength, chmhash_t hash, long* precievercnt, bool is_routing)
- bool chmpx_msg_send_kvp(chmpx_h handle, msgid_t msgid, const PCHMKVP pkvp, long* precievercnt, bool is_routing)
- bool chmpx_msg_send_kv(chmpx_h handle, msgid_t msgid, unsigned char* pkey, size_t keylen, unsigned char* pval, size_t vallen, long* precievercnt, bool is_routing)
- bool chmpx_msg_broadcast(chmpx_h handle, msgid_t msgid, const unsigned char* pbody, size_t blength, chmhash_t hash, long* precievercnt)
- bool chmpx_msg_replicate(chmpx_h handle, msgid_t msgid, const unsigned char* pbody, size_t blength, chmhash_t hash, long* preceivercnt)
- bool chmpx_msg_recieve(chmpx_h handle, msgid_t msgid, chmpx_pkt_h* ppckthandle, unsigned char** ppbody, size_t* plength, int timeout_ms)

#### 説明
- chmpx_open  
  スレーブノードとの通信のためのメッセージハンドルをオープンします。
- chmpx_close  
  メッセージハンドルをクローズします。
- chmpx_msg_send  
  データを送信します。
- chmpx_msg_send_kvp  
  データを送信します。
- chmpx_msg_send_kv  
  データを送信します。
- chmpx_msg_broadcast  
  データをブロードキャスト送信します。
- chmpx_msg_replicate  
  指定したHASHと同等（レプリケーション、マージ状態を考慮して）のサーバにデータを送信します。
- chmpx_msg_recieve  
  データを受信します。

#### パラメーター
- handle  
  CHMPXオブジェクトへのハンドルを指定します。
- no_giveup_rejoin  
  CHMPXが停止した場合に、再起動待ちの上限回数を無視（上限なし）するかどうかを指定します。
- msgid  
  メッセージハンドルを指定します。
- pbody  
  送信データ（バイナリ）へのポインタを指定します。
- blength  
  送信データ長を指定します。
- hash  
  送信データのHASH値を指定します。
- precievercnt  
  データが送信された際の、受け取り側のノード数を返します。
- is_routing  
  true を指定した場合には、CHMPXのクラスタが、HASHタイプであり、多重化された構成（レプリカ数が1以上）では複数のあて先に送付されることを意味します。
- pkvp  
  送信データ（KEY、VALUE構造体）へのポインタを指定します。
- pkey  
  送信データのキー（バイナリ）へのポインタを指定します。
- keylen  
  送信データのキー長を指定します。
- pval  
  送信データの値（バイナリ）へのポインタを指定します。
- vallen  
  送信データの値長を指定します。
- ppckthandle  
  受信データの通信情報ハンドルを取得するポインタを指定します（注意参照）。受け取ったデータは、chmpx_pkth_destroy で開放してください。
- ppbody  
  受信データを受け取るためのポインタを指定します。受け取ったデータは、CHM_Free() などで開放してください。
- plength  
  受信データ長を受け取るためのポインタを指定します。
- timeout_ms  
  受信タイムアウトを指定します。ミリ秒で指定します。

#### 返り値
成功した場合には、true を返します。失敗した場合には false を返します。

#### 注意
スレーブノードは、通常は受信シーケンスから開始することはありません（非同期通信を行う場合には利用することはあります）。
通常、スレーブノードでは送信処理を行い、その送信したメッセージハンドルに対して、返信を待つことになります。

#### サンプル
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

### <a name="SERVERSLAVENODE"> サーバノード・スレーブノード返信関連（C I/F）

#### 書式
- bool chmpx_msg_reply(chmpx_h handle, chmpx_pkt_h pckthandle, const unsigned char* pbody, size_t blength)
- bool chmpx_msg_reply_kvp(chmpx_h handle, chmpx_pkt_h pckthandle, const PCHMKVP pkvp)
- bool chmpx_msg_reply_kv(chmpx_h handle, chmpx_pkt_h pckthandle, unsigned char* pkey, size_t keylen, unsigned char* pval, size_t vallen)

#### 説明
- chmpx_msg_reply  
  バイナリデータを返信します。
- chmpx_msg_reply_kvp  
  バイナリ（KEY、VALUE構造体）データを返信します。
- chmpx_msg_reply_kv  
  キーおよび値を指定してデータを返信します。

#### パラメータ
- handle  
  CHMPXオブジェクトへのハンドルを指定します。
- pckthandle  
  受信時に受け取った通信情報ハンドルを指定します。
- pbody  
  返信データ（バイナリ）へのポインタを指定します。
- blength  
  返信データ長を指定します。
- pkvp  
  返信データ（KEY、VALUE構造体）へのポインタを指定します。
- pkey  
  返信データのキー（バイナリ）へのポインタを指定します。
- keylen  
  返信データのキー長を指定します。
- pval  
  返信データの値（バイナリ）へのポインタを指定します。
- vallen  
  返信データの値長を指定します。

#### 返り値
成功した場合には、true を返します。失敗した場合には false を返します。

#### 注意
スレーブノードは、通常は返信を行いません（非同期通信を行う場合には利用することがあります）。
よって、本関数群は主にサーバノードで利用されます。

#### サンプル

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

### <a name="UTILITY"> ユーティリティ関連（C I/F）

#### 書式
- bool is_chmpx_proc_exists(chmpx_h handle)
- void chmpx_print_version(FILE* stream)
- unsigned char* cvt_kvp_bin(const PCHMKVP pkvp, size_t* plength)
- bool cvt_bin_kvp(PCHMKVP pkvp, unsigned char* bydata, size_t length)
- chmhash_t make_chmbin_hash(const PCHMBIN pchmbin)
- chmhash_t make_kvp_hash(const PCHMKVP pkvp)

#### 説明
- is_chmpx_proc_exists  
  CHMPXプロセスが存在するか確認します。
- chmpx_print_version  
  CHMPXライブラリのバージョンを返します。
- cvt_kvp_bin  
  CHMKVP構造体のデータをバイナリ列のデータに変換します。
- cvt_bin_kvp  
  バイナリ列のデータをCHMKVP構造体に変換します。
- make_chmbin_hash  
  CHMBIN構造体のデータからHASH値を生成します。
- make_kvp_hash  
  CHMKVP構造体のデータからHASH値を生成します。

#### パラメータ
- handle  
  CHMPXオブジェクトへのハンドルを指定します。
- stream  
  ファイルポインタを指定します。（stdout / stderr の指定可能）
- pkvp  
  CHMKVP構造体へのポインタを指定します。
- plength  
  返されるバイナリ列のデータ長を保管するバッファを指定してください。
- bydata  
  変換するバイナリ列へのポインタを指定します。
- length  
  変換するバイナリ列のデータ長を指定します。
- pchmbin  
  CHMBIN構造体へのポインタを指定します。
- pkvp  
  CHMKVP構造体へのポインタを指定します。

#### 返り値
- is_chmpx_proc_exists  
  成功した場合には、true を返します。失敗した場合には false を返します。
- chmpx_print_version  
  返り値はありません。
- cvt_kvp_bin  
  領域確保されたバイナリ列へのポインタを返します。
- cvt_bin_kvp  
  成功した場合には、true を返します。失敗した場合には false を返します。
- make_chmbin_hash  
  HASH値を返します。
- make_kvp_hash  
  HASH値を返します。

#### 注意
特になし。

<!-- -----------------------------------------------------------　-->
*** 

### <a name="AUTOCALLBACK"> 自動マージコールバック関数（C I/F）
自動マージ（オートマージ）機能を利用する場合に、サーバーノード上のクライアントが設定することができるコールバック関数です。下記の関数型で定義された関数をchmpx_create_ex関数、InitializeOnServerメソッドに指定します。
これらの関数が指定されていない場合にはコールバックによる処理はされず、自動マージはchmpxで処理されたものとして動作をします。


#### 書式 
- typedef bool (*chm_merge_lastts_cb)(chmpx_h handle, struct timespec* pts)
- typedef bool (*chm_merge_get_cb)(chmpx_h handle, const PCHM_MERGE_GETPARAM pparam, chmhash_t* pnexthash, PCHMBIN* ppdatas, size_t* pdatacnt)
- typedef bool (*chm_merge_set_cb)(chmpx_h handle, size_t length, const unsigned char* pdata, const struct timespec* pts);

#### 説明
- chm_merge_lastts_cb  
  クラスタ全体のマージ開始前に、所持しているデータの最終更新時刻を取得するときに呼び出されます。timespec構造体で最終更新時刻を返してください。返された時刻以降に更新されたデータがマージ対象となります。全データをマージ対象とする場合には、*pts = {0, 0}を返してください。本コールバックが未設定の場合には、全データをマージ対象として動作します。
- chm_merge_get_cb  
  マージ処理中にマージ対象のデータを取得するために呼び出されます。PCHM_MERGE_GETPARAM にはマージ対象データの情報が入っています。この情報から所持するデータの探索を行い、対象データを ppdatas に設定して返してください。対象データが複数存在する場合には、ppdatas を PCHMBINデータの配列として返してください。 pdatacnt には ppdatas の個数を設定してください。本コールバックを未設定の場合には、対象データがなかったものとしてマージが実行されます。探索条件は、pparam で示され、この構造体には対象データの更新時刻の開始~終了、探索開始HASH値（このHASH値以降のデータ）、マージ対象のHASH値、マージ対象とするHASH値の最大値が指定されています。
- chm_merge_set_cb  
  マージを必要とするノードでchmpxから呼び出されます。他ノードから取得したマージ対象のデータ１つを引数に呼び出されます。このコールバックの処理は、受け取ったデータを自身のデータベースへの登録をすることになります。本コールバックを未設定の場合には、対象データがなかったものとしてマージが実行されます。

#### パラメータ
- handle  
  chmpx_hが渡されます。
- pts  
  chm_merge_lastts_cb の場合はマージ対象データの開始時刻を返します。
  chm_merge_set_cb の場合には、設定するデータの最終時刻（この時刻以前に更新されたことを意味し、この時刻に更新されたわけではない）を渡します。
- pparam  
  マージ対象データの探索条件が設定されています。
- pnexthash  
  データ探索を行った結果、次の探索条件として使用するHASH値を返してください。
- ppdatas  
  検出されたマージ対象データをこのポインタに設定します。対象データは複数でもよく、配列のポインタとして返してください。探索データが存在しない （完了）場合は、NULLを返してください。
- pdatacnt  
  ppdatasデータの数を返してください。探索データが存在しない（完了）場合は、0を返してください。
- length  
  pdataのデータ長を示します。
- pdata  
  マージデータのバイナリ列です。

#### 構造体
chm_merge_get_cbで引き渡されるパラメータCHM_MERGE_GETPARAMです。

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

以下は、メンバーの説明および定義です。
- starthash  
  データ探索を行う開始HASH値が指定されています。
- startts  
  対象データの更新時刻範囲の最小時刻が指定されています。
- endts  
  対象データの更新時刻範囲の最大時刻が指定されています。
- target_hash  
  データ探索を行う際のchmpx リング上の対象HASH値（10台のノードであれば、0～9までの値となります）が指定されています。
- target_max_hash  
  データ探索を行う際のchmpx リング上の最大ノード数が示されています。最大HASH値は、(target__max_hash - 1)となります。（データのHASH値が12であり、target_max_hash=9、target_hash=3の場合には、このHASH値12のデータは対象となります）
- target_hash_range  
  対象HASH値の範囲をしめします。target_hash から、この range個数分が対象となるHASH値の範囲となります。（target_hash=3、target_hash_range=2 であれば、対象HASH値は、3、4の2つとなります。）

#### 返り値
正常に処理が完了した場合には、true を返してください。処理に失敗した場合には false を返してください。（falseの場合にはマージ処理が中断します。）

#### 注意
特になし。

<!-- -----------------------------------------------------------　-->
<!-- -----------------------------------------------------------　-->
<!-- -----------------------------------------------------------　-->
<!-- -----------------------------------------------------------　-->
<!-- -----------------------------------------------------------　-->
***

## <a name="CPPAPI"> C++言語インタフェース
CHMPXと接続するサーバー/クライアントプログラムは、CHMPXライブラリを利用する必要があります。  
開発時には以下のヘッダファイルおよびその他適切なヘッダファイルをインクルードしてください。
```
#include <chmpx/chmcntrl.h>
#include <chmpx/chmpx.h>
```

リンク時には以下をオプションとして指定してください。  
```
-lchmpx
```

以下にC++言語用の関数の説明をします。

<!-- -----------------------------------------------------------　-->
*** 

### <a name="CPPDEBUG"> デバッグ関連(C/C++ I/F)

#### 書式
- ChmDbgMode SetChmDbgMode(ChmDbgMode mode);
- ChmDbgMode BumpupChmDbgMode(void);
- ChmDbgMode GetChmDbgMode(void);
- bool LoadChmDbgEnv(void);
- bool SetChmDbgFile(const char* filepath);
- bool UnsetChmDbgFile(void);

#### 説明
- SetChmDbgMode  
  メッセージ出力のレベルを指定します。CHMDBG_SILENT、CHMDBG_ERR、CHMDBG_WARN、CHMDBG_MSG、CHMDBG_DUMP のいずれかの値を指定します。
- BumpupChmDbgMode  
  メッセージ出力レベルを、非出力→エラー→ワーニング→インフォメーション→通信ダンプ→非出力・・・とBump upします。
- GetChmDbgMode  
  現在のメッセージ出力レベルを取得します。
- LoadChmDbgEnv  
  環境変数 CHMDBGMODE、CHMDBGFILE を読み込み、その値に従ってメッセージ出力、出力先を設定します。
- SetChmDbgFile  
  メッセージを出力するファイルを指定します。設定されていない場合には stderr に出力します。
- UnsetChmDbgFile  
  メッセージを stderr に出力するように戻します。

#### パラメータ
- mode  
  メッセージ出力のレベルを指定します。
- filepath  
  メッセージ出力先のファイルパスを指定します。

#### 返り値
- SetChmDbgMode  
  直前のメッセージ出力のレベルを返します。
- BumpupChmDbgMode  
  直前のメッセージ出力のレベルを返します。
- GetChmDbgMode  
  現在のメッセージ出力のレベルを返します。
- LoadChmDbgEnv  
  成功した場合にはtrueを返し、失敗した場合にはfalseを返します。
- SetChmDbgFile  
  成功した場合にはtrueを返し、失敗した場合にはfalseを返します。
- UnsetChmDbgFile  
  成功した場合にはtrueを返し、失敗した場合にはfalseを返します。

#### 注意
環境変数 CHMDBGMODE、CHMDBGFILE については、CHMPX 詳細部分を参照してください。

<!-- -----------------------------------------------------------　-->
*** 

### <a name="CHMCNTRL"> ChmCntrlクラス

#### 説明
CHMPXオブジェクトのクラスです。
このクラスを通じて、CHMPXとのデータ送受信を行います。
基本的なサーバノードのプログラミングは、本クラスのインスタンスを生成してデータの送受信を行います。
スレーブノードの場合は、本クラスのインスタンスを生成してメッセージハンドルをオープンし、データの送受信を行います。
操作が完了したら、メッセージハンドルのクローズを行い、本オブジェクトを破棄します。

#### メソッド
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

#### メソッド説明
- ChmCntrl::InitializeOnServer  
  CHMPXオブジェクトをサーバノード上のクライアントプロセス用に初期化します。CHMPXプロセスを起動したときと同じコンフィグレーションと同じものを指定してください。これは、コンフィグレーションファイル（.ini/.yaml/.json）もしくは、JSON文字列を指定します。 コンフィグレーションのパラメータが、NULLもしくは、空文字列を指定した場合には、環境変数 CHMCONFFILE もしくは CHMJSONCONF が参照され、適切に設定値が読み込まれます。
  C++インターフェースの場合には、ctlport引数（chmpx起動時の-ctlportと同じ仕様）があります。
- ChmCntrl::InitializeOnSlave  
  CHMPXオブジェクトをスレーブノード上のクライアントプロセス用に初期化します。CHMPXプロセスを起動したときと同じコンフィグレーションと同じものを指定してください。これは、コンフィグレーションファイル（.ini/.yaml/.json）もしくは、JSON文字列を指定します。 コンフィグレーションのパラメータが、NULLもしくは、空文字列を指定した場合には、環境変数 CHMCONFFILE もしくは CHMJSONCONF が参照され、適切に設定値が読み込まれます。C++インターフェースの場合には、ctlport引数（chmpx起動時の-ctlportと同じ仕様）があります。
- ChmCntrl::OnlyAttachInitialize  
  指定されたコンフィグレーション（ファイル指定、JSON文字列指定、環境変数指定のいずれか）および制御ポートに応じて、CHMPXオブジェクトを初期化します。初期化は、CHMSHM（chmpxの管理SHM）にアタッチすることで行われます。 初期化されたCHMPXオブジェクトは、chmpxの状態（ステータス）を取得するメソッド（DupAllChmInfo、DupSelfChmpxInfo）でのみ利用できます。
- ChmCntrl::Recieve  
  サーバノード上のクライアントプロセス用の受信メソッドです。 COMPKTポインタ、バイナリデータへのポインタ、バイナリデータ長を受け取ります。タイムアウトの指定が出来ます。またCHMPXプロセスが停止していた場合に再起動を待つかどうか指定できます。 受け取ったppComPktポインタ、ボディデータは、CHM_Free()  などで開放してください。
- ChmCntrl::Send  
  サーバノード上のクライアントプロセス用の送信メソッドです。 バイナリデータ列、および長さ、HASH値を指定します。
- ChmCntrl::Broadcast  
  サーバノード上のクライアントプロセス用のブロードキャスト送信メソッドです。 バイナリデータ列、および長さ、HASH値を指定します。
- ChmCntrl::Replicate  
  サーバノード上のクライアントプロセス用の指定したHASH値に応じた（レプリケーション設定およびマージ状態を考慮した）サーバ群への送信メソッドです。 バイナリデータ列、および長さ、HASH値を指定します。
- ChmCntrl::Open  
  スレーブノード上のクライアントプロセス用途であり、メッセージハンドルをオープンします。 CHMPXプロセスが停止していた場合に再起動を待つかどうか指定できます。
- ChmCntrl::Close  
  スレーブノード上のクライアントプロセス用途であり、メッセージハンドルをクローズします。 クローズするメッセージハンドルを指定します。
- ChmCntrl::Recieve  
  スレーブノード上のクライアントプロセス用の受信メソッドです。 COMPKTポインタ、バイナリデータへのポインタ、バイナリデータ長を受け取ります。タイムアウトの指定が出来ます。 受け取ったppComPktポインタ、ボディデータは、CHM_Free() などで開放してください。
- ChmCntrl::Send  
  スレーブノード上のクライアントプロセス用の送信メソッドです。 バイナリデータ列、および長さ、HASH値を指定します。 precievercnt を指定した場合には、送信先のCHMPX数を取得することもできます。 CHMPXのクラスタが、HASHタイプであり、多重化された構成（レプリカ数が1以上）のとき、複数のあて先に送付するか否かを指定できます。
- ChmCntrl::Broadcast  
  スレーブノード上のクライアントプロセス用のブロードキャスト送信メソッドです。 バイナリデータ列、および長さ、HASH値を指定します。 precievercnt を指定した場合には、送信先のCHMPX数を取得することもできます。
- ChmCntrl::Replicate  
  スレーブノード上のクライアントプロセス用の指定したHASH値に応じた（レプリケーション設定およびマージ状態を考慮した）サーバ群への送信メソッドです。 バイナリデータ列、および長さ、HASH値を指定します。
- ChmCntrl::Send  
  クライアントプロセスから返信する場合に利用するメソッドです。 COMPKTポインタ、バイナリデータへのポインタ、バイナリデータ長を指定して、データを返信します。
- ChmCntrl::Reply  
  クライアントプロセスから返信する場合に利用するメソッドです。（上記と同じものです） COMPKTポインタ、バイナリデータへのポインタ、バイナリデータ長を指定して、データを返信します。
- ChmCntrl::IsChmpxExit  
  CHMPXプロセスの存在確認用のメソッドです。
- ChmCntrl::DupAllChmInfo  
  OnlyAttachInitialize で初期化したCHMPXオブジェクトにて利用するメソッドです。このメソッドでは、アタッチしているCHMSHMの持つ全chmpxの状態を取得することができます。 取得データは、アロケートされたPCHMINFOEX構造体で返されます。このポインタは、ChmCntrl::FreeDupAllChmInfoクラスメソッドで開放してください。
- ChmCntrl::DupSelfChmpxInfo  
  OnlyAttachInitialize で初期化したCHMPXオブジェクトにて利用するメソッドです。このメソッドでは、アタッチしているCHMSHMの持つ自分自身のchmpxの状態を取得することができます。 取得データは、アロケートされたPCHMPX構造体で返されます。このポインタは、ChmCntrl::FreeDupSelfChmpxInfoクラスメソッドで開放してください。
- ChmCntrl::FreeDupAllChmInfo  
  ChmCntrl::DupAllChmInfo で取得したPCHMINFOEXデータポインタの領域を開放します。
- ChmCntrl::FreeDupSelfChmpxInfo  
  ChmCntrl::DupSelfChmpxInfo で取得したPCHMPXデータポインタの領域を開放します。

#### メソッド返り値
bool値の返り値のプロトタイプは、成功した場合には、true を返します。失敗した場合には false を返します。  
ChmCntrl::Openメソッドは、オープンしたメッセージハンドルを返します。

#### サンプル（サーバノード）

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

#### サンプル（スレーブノード）
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
#### ステータス（状態）取得
```
ChmCntrl    chmobj;
if(!chmobj.OnlyAttachInitialize(conffile.c_str(), ctlport)){
    return false;
}
if(!is_self){
    PCHMINFOEX    pInfo = pchmobj->DupAllChmInfo();
        ・
        ・
        ・
    ChmCntrl::FreeDupAllChmInfo(pInfo);
}else{
    PCHMPX    pInfo = pchmobj->DupSelfChmpxInfo();
        ・
        ・
        ・
    ChmCntrl::FreeDupSelfChmpxInfo(pInfo);
}
chmobj.Clean();
```

<!-- -----------------------------------------------------------　-->
*** 

### <a name="CHMSTREAM"> chmstream（ichmstream、ochmstream）クラス

#### 説明
CHMPXオブジェクトへの直接的なアクセス（送信、受信）を提供するiostream派生クラスです。
ChmCntrlクラスを指定して、ストリームクラスである chmstream、ichmstream、ochmstream を初期化し、iostreamとして利用できるクラスです。
通常のstd::stringstreamと同等であり、またseekposを利用できます。
詳細な説明は、std::stringstream などを参考にしてください。

#### 基本クラス
<table>
<tr><td>chmstream </td><td>std::basic_stream </td></tr>
<tr><td>ichmstream</td><td>std::basic_istream</td></tr>
<tr><td>ochmstream</td><td>std::basic_ostream</td></tr>
<table>

#### サンプル
```
ChmCntrl* pchmpx = new ChmCntrl();
 ・
 ・
 ・
  
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
