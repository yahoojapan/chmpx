---
layout: contents
language: ja
title: Build
short_desc: Consistent Hashing Mq inProcess data eXchange
lang_opp_file: build.html
lang_opp_word: To English
prev_url: usageja.html
prev_string: Usage
top_url: indexja.html
top_string: TOP
next_url: developerja.html
next_string: Developer
---

# ビルド方法
CHMPXをビルドする方法を説明します。

## 1. 事前環境
- Debian / Ubuntu
```
$ sudo aptitude update
$ sudo aptitude install git autoconf autotools-dev gcc g++ make gdb dh-make fakeroot dpkg-dev devscripts libtool pkg-config libssl-dev libyaml-dev
```
- Fedora / CentOS
```
$ sudo yum install git autoconf automake gcc libstdc++-devel gcc-c++ make libtool openssl-devel libyaml-devel
```

## 2. ビルド、インストール：FULLOCK
```
$ git clone https://github.com/yahoojapan/fullock.git
$ cd fullock
$ ./autogen.sh
$ ./configure --prefix=/usr
$ make
$ sudo make install
```

## 3. ビルド、インストール：K2HASH
```
$ git clone https://github.com/yahoojapan/k2hash.git
$ cd k2hash
$ ./autogen.sh
$ ./configure --prefix=/usr
$ make
$ sudo make install
```

## 4. clone
```
$ git clone git@github.com:yahoojapan/chmpx.gif
```

## 5. ビルド、インストール：CHMPX
```
$ ./autogen.sh
$ ./configure --prefix=/usr
$ make
$ sudo make install
```
### SSL/TLSライブラリの変更
- OpenSSLを使う場合  
"--with-openssl"をconfigureコマンドのオプションとして指定してください。SSL/TLSライブラリの指定オプションが省略されている場合のデフォルトはOpenSSLになります。
- NSS(mozilla)を使う場合  
"--with-nss"をconfigureコマンドのオプションとして指定してください。
- GnuTLSを使う場合  
"--with-gnutls"をconfigureコマンドのオプションとして指定してください。
