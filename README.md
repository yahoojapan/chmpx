CHMPX
--------
[![Build Status](https://travis-ci.org/yahoojapan/chmpx.svg?branch=master)](https://travis-ci.org/yahoojapan/chmpx)
[![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](https://raw.githubusercontent.com/yahoojapan/chmpx/master/COPYING)
[![GitHub forks](https://img.shields.io/github/forks/yahoojapan/chmpx.svg)](https://github.com/yahoojapan/chmpx/network)
[![GitHub stars](https://img.shields.io/github/stars/yahoojapan/chmpx.svg)](https://github.com/yahoojapan/chmpx/stargazers)
[![GitHub issues](https://img.shields.io/github/issues/yahoojapan/chmpx.svg)](https://github.com/yahoojapan/chmpx/issues)
[![debian packages](https://img.shields.io/badge/deb-packagecloud.io-844fec.svg)](https://packagecloud.io/antpickax/stable)
[![RPM packages](https://img.shields.io/badge/rpm-packagecloud.io-844fec.svg)](https://packagecloud.io/antpickax/stable)

CHMPX - Consistent Hashing Mq inProcess data eXchange

### Overview
CHMPX is inprocess data exchange by MQ with consistent hashing system, and libraries for clients by Yahoo! JAPAN.  
CHMPX is made for the purpose of the construction of original messaging system and the offer of the client library.  
CHMPX transfers messages between the client and the server/slave.  
CHMPX based servers are dispersed by consistent hashing and are automatically laid out.  
As a result, it provides a high performance, a high scalability.  

![CHMPX](https://chmpx.antpick.ax/images/top_chmpx.png)

### Feature
  - Build up cluster with unique name by some servers.
  - Layouts servers in cluster by consistent hashing.
  - Supports interprocess communication across the servers.
  - Supports synchronous/asynchronous communication.
  - Supports communicating messages in the target specified(HASH).
  - Supports plugin Hashing function for target messaging.(k2hash)
  - Supports communicating messages in the random.
  - Supports SSL communication.
  - The message communication possible bypass.
  - No message lost during communication failure.
  - Broadcast a message communication possible.
  - Provision of high-level library for clients.
  - Supports multi-thread/process for client programs.
  - Supports synchronous communication
  - Supports asynchronous communication
  - Supports broadcast messages
  - Supports data merging automatically
  - Supports scaling automatically

### Documents
  - [Document top page](https://chmpx.antpick.ax/)
  - [Github wiki page](https://github.com/yahoojapan/chmpx/wiki)
  - [About AntPickax](https://antpick.ax/)

### Packages
  - [RPM packages(packagecloud.io)](https://packagecloud.io/antpickax/stable)
  - [Debian packages(packagecloud.io)](https://packagecloud.io/antpickax/stable)

### License
This software is released under the MIT License, see the license file.

### AntPickax
k2hash is one of [AntPickax](https://antpick.ax/) products.

Copyright(C) 2014 Yahoo Japan corporation.
