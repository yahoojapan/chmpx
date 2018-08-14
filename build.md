---
layout: contents
language: en-us
title: Build
short_desc: Consistent Hashing Mq inProcess data eXchange
lang_opp_file: buildja.html
lang_opp_word: To Japanese
prev_url: usage.html
prev_string: Usage
top_url: index.html
top_string: TOP
next_url: developer.html
next_string: Developer
---

# Building
The build method for CHMPX is explained below.

## 1. Install prerequisites before compiling
- Debian / Ubuntu
```
$ sudo aptitude update
$ sudo aptitude install git autoconf autotools-dev gcc g++ make gdb dh-make fakeroot dpkg-dev devscripts libtool pkg-config libssl-dev libyaml-dev
```
- Fedora / CentOS
```
$ sudo yum install git-core gcc-c++ make libtool openssl-devel libyaml-devel
```

## 2. Building and installing FULLOCK
```
$ git clone https://github.com/yahoojapan/fullock.git
$ cd fullock
$ ./autogen.sh
$ ./configure --prefix=/usr
$ make
$ sudo make install
```

## 3. Building and installing K2HASH
```
$ git clone https://github.com/yahoojapan/k2hash.git
$ cd k2hash
$ ./autogen.sh
$ ./configure --prefix=/usr
$ make
$ sudo make install
```

## 4. Clone source codes from Github
```
$ git clone git@github.com:yahoojapan/chmpx.gif
```

## 5. Building and installing CHMPX
```
$ ./autogen.sh
$ ./configure --prefix=/usr
$ make
$ sudo make install
```
### Switch SSL/TLS library
- Linking OpenSSL  
Specify "--with-openssl" for configure command option, OpenSSL is default if you do not specify any option for SSL/TLS library.
- Linking NSS(mozilla)  
Specify "--with-nss" for configure command option.
- Linking GnuTLS  
Specify "--with-gnutls" for configure command option.
