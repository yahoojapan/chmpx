---
layout: contents
language: ja
title: Usage
short_desc: Consistent Hashing Mq inProcess data eXchange
lang_opp_file: usage.html
lang_opp_word: To English
prev_url: detailsja.html
prev_string: Details
top_url: indexja.html
top_string: TOP
next_url: buildja.html
next_string: Build
---

# 使い方
## 1. サンプルコンフィグレーション
CHMPXの利用するコンフィグレーションのサンプルを示します。

### 1.1 サーバーノード
- INI形式
[test_server.ini]({{ site.github.repository_url }}/blob/master/tests/test_server.ini)
- YAML形式
[test_server.yaml]({{ site.github.repository_url }}/blob/master/tests/test_server.yaml)
- JSON形式
[test_server.json]({{ site.github.repository_url }}/blob/master/tests/test_server.json)
- JSON文字列
[test_json_string.data]({{ site.github.repository_url }}/blob/master/tests/test_json_string.data) ファイルの中の "SERVER=" の以降の文字列

### 1.2 スレーブノード
- INI形式
[test_slave.ini]({{ site.github.repository_url }}/blob/master/tests/test_slave.ini)
- YAML形式
[test_slave.yaml]({{ site.github.repository_url }}/blob/master/tests/test_slave.yaml)
- JSON形式
[test_slave.json]({{ site.github.repository_url }}/blob/master/tests/test_slave.json)
- JSON文字列
[test_json_string.data]({{ site.github.repository_url }}/blob/master/tests/test_json_string.data) ファイルの中の "SLAVE=" の以降の文字列

## 2. 簡単な動作確認
CHMPXをビルドした後で、動作確認をしてみます。

### 2.1 利用環境構築

**CHMPX** をご利用の環境にインストールするには、2つの方法があります。  
ひとつは、[packagecloud.io](https://packagecloud.io/)から **CHMPX** のパッケージをダウンロードし、インストールする方法です。  
もうひとつは、ご自身で **CHMPX** をソースコードからビルドし、インストールする方法です。  
これらの方法について、以下に説明します。

#### パッケージを使ったインストール
**CHMPX** は、誰でも利用できるように[packagecloud.io - AntPickax stable repository](https://packagecloud.io/antpickax/stable/)で[パッケージ](https://packagecloud.io/app/antpickax/stable/search?q=chmpx)を公開しています。  
**CHMPX** のパッケージは、Debianパッケージ、RPMパッケージの形式で公開しています。  
お使いのOSによりインストール方法が異なりますので、以下の手順を確認してインストールしてください。  

##### DebianベースLinuxの利用者は、以下の手順に従ってください。
```
$ sudo apt-get update -y
$ sudo apt-get install curl -y
$ curl -s https://packagecloud.io/install/repositories/antpickax/stable/script.deb.sh | sudo bash
$ sudo apt-get install chmpx
```
開発者向けパッケージをインストールする場合は、以下のパッケージをインストールしてください。
```
$ sudo apt-get install chmpx-dev
```

##### RPMベースのLinuxの場合は、以下の手順に従ってください。
```
$ sudo dnf makecache
$ sudo dnf install curl -y
$ curl -s https://packagecloud.io/install/repositories/antpickax/stable/script.rpm.sh | sudo bash
$ sudo dnf install chmpx
```
開発者向けパッケージをインストールする場合は、以下のパッケージをインストールしてください。
```
$ sudo dnf install chmpx-devel
```

##### ALPINEベースのLinuxの場合は、以下の手順に従ってください。
```
# apk update
# apk add curl
# curl -s https://packagecloud.io/install/repositories/antpickax/stable/script.alpine.sh | sh
# apk add chmpx
```
開発者向けパッケージをインストールする場合は、以下のパッケージをインストールしてください。
```
# apk add chmpx-dev
```

##### 上記以外のOS
上述したOS以外をお使いの場合は、パッケージが準備されていないため、直接インストールすることはできません。  
この場合には、後述の[ソースコード](https://github.com/yahoojapan/chmpx)からビルドし、インストールするようにしてください。

#### ソースコードからビルド・インストール
**CHMPX** を[ソースコード](https://github.com/yahoojapan/chmpx)からビルドし、インストールする方法は、[ビルド](https://chmpx.antpick.ax/buildja.html)を参照してください。

### 2.2 CHMPXサーバーノードを起動
```
$ chmpx -conf test_server.ini
```

### 2.3 サーバープログラムを起動
```
$ chmpxbench -s -conf test_server.ini -l 0 -proccnt 1 -threadcnt 1 -ta -dl 128 -pr -g err -dummykey TEST
```

### 2.4 CHMPXスレーブノード起動
```
$ chmpx -conf test_slave.ini
```

### 2.5 クライアントプログラムを起動
```
$ chmpxbench -c -conf test_slave.ini -l 1 -proccnt 1 -threadcnt 1 -ta -dl 128 -pr -g err -dummykey TEST
```

以上の動作でエラーが出なければ問題ありません。

## 3. プログラムの終了
chmpx、chmpxbenchともにシグナルHUPを送ります。  
自動的に終了します。


## 4. CHMPX制御コマンド
CHMPXは、CHMPXプロセス自身およびRING上のCHMPXサーバーノード群を制御するためのポートを指定して起動されます。  
この制御ポートに対して、制御コマンドを送ることでCHMPXプロセス、RING上のCHMPXサーバーノード群の制御、状態管理、確認ができます。
制御コマンドは、そのコマンドを受け取るCHMPXプロセス自身を制御するコマンドと、RING上のCHMPXサーバーノード群（RING）を制御するコマンドに大別できます。
この制御コマンドを使い、RING上のCHMPXサーバーノード群の状態確認、変更ができます。
また、CHMPXプロセス自身の状態確認、変更もできます。  
この制御ポート経由の制御コマンドを直接利用せず、簡単に利用できる**chmpxlinetool**を利用することを推奨します。

### 4.1 使い方
CHMPXプログラム起動時に指定するコンフィグレーション（ファイル、JSON文字列）に、**CTLPORT**の項目があります。
この**CTLPORT**は、制御ポートを示しています。
この制御ポートに接続し、制御コマンドを文字列でCHMPXプロセスに渡すことができます。
CHMPXプロセスは制御コマンドを受け、そのコマンドに応じた処理、返答を行います。  
なお、現在制御ポートとの接続と通信は暗号化をサポートしていません。（今後サポートされます）

### 4.2 制御ポートのアクセス制限
制御ポートへのアクセスは、コンフィグレーション（ファイル、JSON文字列）で指定されているサーバーノード、スレーブノードのみが可能です。

### 4.3 制御コマンド（CHMPXプロセスのみへ実効）
#### VERSION
CHMPXプロセスのバージョンを返します。

#### SELFSTATUS
接続したCHMPXプロセスのみの状態を返します。
このコマンドはCHMPXプロセスの基本的な状態を返し、それはDUMPコマンドよりも荒い情報です。

#### ALLSTATUS
接続したCHMPXプロセスが持っている全CHMPXサーバーノード、スレーブノードの状態を返します。
このコマンドは接続したCHMPXプロセスが持つ自分自身を含む他CHMPXプロセスの必要最小限な状態を返し、CHMPXプロセス同士のRINGに関する状態を得ることができます。

#### UPDATESTATUS
接続したCHMPXプロセスの状態を、このCHMPXプロセスが接続している他CHMPXプロセスに送り、状態を強制的にアップデートします。
もし、特定のCHMPXプロセスの状態が間違った状態で他CHMPXプロセスに伝播している場合、正しいCHMPXプロセスの状態に合わせるために利用するコマンドです。  
通常発生しないケースでありますが、CHMPXプロセスの状態を一致させるための復旧コマンドです。

#### DUMP
接続したCHMPXプロセスのもつ内部情報をすべて返します。
本コマンドにより、その時点のCHMPXプロセスの詳細な状態を確認することができます。

### 制御コマンド（RING上の全CHMPXサーバーノードプロセスに実効）
#### SERVICEIN
接続したCHMPXサーバーノードに対して、サービスを提供するように指示します。
サーバーノードのCHMPXプロセスが、起動し、RINGに参加しているが、サービスを提供していない状態（SERVICE OUT状態）である場合、サービスを提供するように指示します。
このコマンドにより、CHMPXプロセスはサービス提供開始準備状態（SERVICE IN、かつADD、PENDING状態）になります。
コンフィグレーションにて、**AUTOMERGE**設定がONの場合には、CHMPXプロセスは自動的にSERVICE IN状態になるように初期動作しますので、本コマンドを利用する必要はありません。

#### SERVICEOUT [hostname]:[control port number]
任意のCHMPXサーバーノードに対して、サービスを提供しないように指示します。
サーバーノードのCHMPXプロセスが、RINGに参加し、サービスを提供している状態（SERVICE IN状態）である場合、サービスを提供しないように指示します。
このコマンドにより、CHMPXプロセスはサービス提供停止準備状態（SERVICE IN、かつ、DELETE、PENDING状態）になります。
指定するホスト名と制御ポート番号は、このコマンドを送付するCHMPXプロセス以外のCHMPXプロセス（ホスト）を指定することができます。
これは、CHMPXプロセスが起動していない（DOWN状態）サーバーノードを、他CHMPXサーバーノードからサービスアウトできるようにするためです。
コンフィグレーションの**AUTOMERGE**設定がONに設定されており、CHMPXプロセスが停止（終了）した場合、自動的にSERVICE OUT状態になるように動作しますので、本コマンドを利用する必要はありません。

#### MERGE
SERVICEIN、SERVICEOUTコマンドなどにより、CHMPXサーバーノードの状態が準備状態（PENDING状態）となっている場合、本コマンドによりデーターマージを開始させます。
CHMPXサーバーノードプロセスと接続するクライアントプロセス（サーバーサイドプロセス）がデータを持つ場合、サービス提供を開始・停止するときに必要となる/必要とされるデータを他CHMPXサーバーノードプロセスとマージしなくてはなりません。
このデータをマージ開始させるための制御コマンドです。
このコマンドが正常に受け付けられた場合、CHMPXプロセスはマージ実行中状態（DOING）となります。
CHMPXサーバーノードプロセスは、データのマージが完了すると自動的にマージ完了状態（DONE）に移行します。
コンフィグレーションの**AUTOMERGE**設定がONに設定されている場合には、PENDING状態のCHMPXサーバーノードプロセスは、準備が整い次第自動的にDOING、DONE状態に移行します。

#### COMPMERGE
すべてのCHMPXサーバーノードプロセスが、データのマージ完了状態（DONE）となっているとき、本コマンドを受け取ることによって、サービス提供・サービス停止状態（SERVICE IN/SERVICE OUT）となります。
もし、ひとつでもCHMPXサーバーノードプロセスが、データのマージ完了状態（DONE）となっていない場合には、本コマンドは失敗します。
コンフィグレーションの**AUTOMERGE**設定がONに設定されている場合には、すべてのCHMPXサーバーノードプロセスが、データのマージ完了状態（DONE）となった時点で、自動的にサービス提供・サービス停止状態（SERVICE IN/SERVICE OUT）となります。

#### ABORTMERGE
CHMPXサーバーノードプロセスが、サービス提供・停止準備状態（PENDING）やデータのマージ中（DOING）、データのマージ完了（DONE）の状態のときに、その処理をキャンセルし、元の状態に戻します。
データのマージ処理が完了できない場合や、時間のかかる場合などにおいて、その処理をキャンセルために使用するコマンドです。

#### SUSPENDMERGE
CHMPXは起動時にコンフィグレーションから**AUTOMERGE**の設定を読み取り、自動的なマージ処理を実行するか決定します。
この**AUTOMERGE**のコンフィグレーションを無視（SUSPEND）するか、有効と判断（NOSUSPEND）するかを制御コマンドで指定することができます。
本コマンドは、**AUTOMERGE**の設定を無視（SUSPEND）するように、RING上の全CHMPXサーバーノードに指示します。
このコマンドにより、RING上の全CHMPXサーバーノードだけではなく、後からRINGに参加するCHMPXサーバーノードに対しても、この設定状態が反映されます。
本コマンドは、多数のCHMPXサーバーノードを一度にRINGに投入する場合に、一時的に**AUTOMERGE**の設定を無効化し、投入後SERVICEIN/MERGE/COMPMERGEコマンドによってサービス提供を開始させるなどで利用します。
この手法により多数のCHMPXサーバーノードを一度に追加するときに、追加毎に**AUTOMERGE**が自動的に実行されないようにし、効率よくマージ処理を行えるようにします。

#### NOSUSPENDMERGE
本コマンドは、**AUTOMERGE**の設定を有効（NOSUSPEND）するように、RING上の全CHMPXサーバーノードに指示します。
**AUTOMERGE**の設定が無効（SUSPEND）となっている状態を解除します。
このコマンドにより、RING上の全CHMPXサーバーノードだけではなく、後からRINGに参加するCHMPXサーバーノードに対しても、この設定状態が反映されます。

## 5. systemdサービス
CHMPXをパッケージとしてインストールした場合、そのパッケージには **chmpx.service** というsystemdサービスが含まれています。  
ここでは、この **chmpx.service** を使ったCHMPXプロセスの起動方法を説明します。

### 5.1 コンフィグレーション
**chmpx.service** によりCHMPXを起動する場合、コンフィグレーションファイルとして、`/etc/antpickax/chmpx.ini`ファイルが使われます。  
このファイルを最初に準備してください。

### 5.2 chmpx.service 起動
CHMPXパッケージをインストールした直後は、**chmpx.service** は無効となっています。  
コンフィグレーションファイル（`/etc/antpickax/chmpx.ini`）の準備ができたら、まず **chmpx.service** を有効にし、次に開始します。
```
$ sudo systemctl enable chmpx.service
$ sudo systemctl start chmpx.service
```
以上で、CHMPXが起動します。

### 5.3 カスタマイズ
`/etc/antpickax/chmpx-service-helper.conf`ファイルを変更することで、**chmpx.service** の動作をカスタマイズすることができます。  
もしくは、`/etc/antpickax/override.conf`ファイルを準備して、同様にカスタマイズすることができます。  
両ファイルが、同じキーワードをカスタマイズした場合は、`/etc/antpickax/override.conf`ファイルが優先されます。

#### chmpx-service-helper.conf
`chmpx-service-helper.conf`ファイルでカスタマイズできるキーワードを以下に示します。

##### INI_CONF_FILE
CHMPXを起動するときに指定されるコンフィグレーションファイルのパスを指定します。  
デフォルトは、`/etc/antpickax/chmpx.ini`です。

##### PIDDIR
CHMPXプロセスおよび、**chmpx.service**に関連するプロセスのプロセスIDを保管するディレクトリを指定します。  
デフォルトは、`/var/run/antpickax`です。

##### SERVICE_PIDFILE
**chmpx.service**に関連するプロセスのプロセスIDを保管するファイル名を指定します。  
デフォルトは、`chmpx-service-helper.pid`です。

##### SUBPROCESS_PIDFILE
CHMPXプロセスのプロセスIDを保管するファイル名を指定します。  
デフォルトは、`chmpx.pid`です。

##### SUBPROCESS_USER
CHMPXプロセスを起動するユーザを指定します。  
デフォルトは、**chmpx.service**を起動したユーザです。

##### LOGDIR
CHMPXプロセスおよび、**chmpx.service**に関連するプロセスのログを格納するディレクトリを指定します。  
デフォルトでは、`journald`にログ管理を任せています。

##### SERVICE_LOGFILE
**chmpx.service**に関連するプロセスのログを保管するファイル名を指定します。  
デフォルトでは、`journald`にログ管理を任せています。

##### SUBPROCESS_LOGFILE
CHMPXプロセスのログを格納するファイル名を指定します。  
デフォルトでは、`journald`にログ管理を任せています。

##### WAIT_DEPENDPROC_PIDFILE
CHMPXプロセスを起動する前に、起動を待つプロセスのプロセスIDのファイルパスを指定します。  
デフォルトは、未指定であり、CHMPXプロセスは他のプロセスの起動を待たずに起動します。

##### WAIT_SEC_AFTER_DEPENDPROC_UP
CHMPXプロセスを起動する前に、起動を待つプロセスが起動した後、待機する時間を秒で指定します。  
デフォルトは、15秒です。ただし、`WAIT_DEPENDPROC_PIDFILE`は未指定なので、他のプロセス起動を待機することはありません。

##### WAIT_SEC_STARTUP
**chmpx.service**が起動された後、CHMPXプロセスを起動するまでの待機時間を秒で指定します。  
デフォルトは、10秒です。

##### WAIT_SEC_AFTER_SUBPROCESS_UP
CHMPXプロセスを起動した後、CHMPXプロセスの状態を確認するまでの待機時間を秒で指定します。  
デフォルトは、15秒です。

##### INTERVAL_SEC_FOR_LOOP
プロセスの再起動、停止確認などで待機するときの時間を秒で指定します。  
デフォルトは、10秒です。

##### TRYCOUNT_STOP_SUBPROC
CHMPXプロセス停止を試行する最大回数を指定します。この回数を超過する場合は、CHMPXプロセス停止を失敗したと判断します。  
デフォルトは、10回です。

##### SUBPROCESS_OPTIONS
CHMPXプロセス起動時に引き渡すオプションを指定します。  
デフォルトは、空です。

##### BEFORE_RUN_SUBPROCESS
CHMPXプロセス起動前に実行するコマンドを指定できます。
デフォルトは、空です。

#### override.conf
`override.conf`ファイルは、`chmpx-service-helper.conf`ファイルよりも優先されます。  
`override.conf`ファイルでカスタマイズできるキーワードは、`chmpx-service-helper.conf`ファイルと同じです。  
ただし、`override.conf`ファイルの書式は、`chmpx-service-helper.conf`ファイルと異なります。

##### 書式1
```
<customize configuration file path>:<keyword> = <value>
```
カスタマイズコンフィグレーションファイルのパスとキーワードを指定して、値を直接設定する書式です。  
例えば、以下のように指定します。  
```
/etc/antpickax/chmpx-service-helper.conf:CHMPX_INI_CONF_FILE = /etc/antpickax/custom.ini
```

##### 書式2
```
<customize configuration file path>:<keyword> = <customize configuration file path>:<keyword>
```
カスタマイズコンフィグレーションファイルのパスとキーワードを指定して、値を他のコンフィグレーションファイルで設定する書式です。  
例えば、以下のように指定します。  
```
/etc/antpickax/chmpx-service-helper.conf:CHMPX_INI_CONF_FILE = /etc/antpickax/other.conf:CHMPX_INI_CONF_FILE
```

### 5.4 chmpx.service 停止
```
$ sudo systemctl stop chmpx.service
```
以上で、CHMPXを停止できます。
