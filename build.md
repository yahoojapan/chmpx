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
# Build

This chapter consists of three parts:

* how to set up **CHMPX** for local development
* how to build **CHMPX** from the source code
* how to install **CHMPX**

## 1. Install prerequisites

**CHMPX** primarily depends on **fullock**, **k2hash**. Each dependent library and the header files are required to build **CHMPX**. We provide two ways to install them. You can select your favorite one.

* Use [GitHub](https://github.com/)  
  Install the source code of dependent libraries and the header files. You will **build** them and install them.
* Use [packagecloud.io](https://packagecloud.io/)  
  Install packages of dependent libraries and the header files. You just install them. Libraries are already built.

### 1.1. Install each dependent library and the header files from GitHub

Read the following documents for details:  
* [fullock](https://fullock.antpick.ax/build.html)
* [k2hash](https://k2hash.antpick.ax/build.html)  
  An application, which implements the SSL/TLS protocols, will use one SSL/TLS library. You will select the same SSL/TLS library when you build [K2HASH](https://k2hash.antpick.ax/build.html). See [K2HASH](https://k2hash.antpick.ax/build.html) what SSL/TLS libraries [K2HASH](https://k2hash.antpick.ax/build.html) supports.

**Note**: You need consider **CHMPX**, I describe how to build it below, also requires one SSL/TLS library. This means the [K2HASH](https://k2hash.antpick.ax/build.html) build option affects the **CHMPX** build option. Table1 shows possible configure options.

Table1. possible configure option:

| K2HASH configure options | CHMPX configure options | SSL/TLS library |
|:--|:--|:--|
| ./configure --prefix=/usr --with-openssl | ./configure --prefix=/usr --with-openssl | [OpenSSL](https://www.openssl.org/) |
| ./configure --prefix=/usr --with-gcrypt | ./configure --prefix=/usr --with-gnutls | [GnuTLS](https://gnutls.org/) |
| ./configure --prefix=/usr --with-nettle | ./configure --prefix=/usr --with-gnutls | [GnuTLS](https://gnutls.org/) |
| ./configure --prefix=/usr --with-nss | ./configure --prefix=/usr --with-nss | [Mozilla NSS](https://developer.mozilla.org/en-US/docs/Mozilla/Projects/NSS) |

### 1.2. Install each dependent library and the header files from packagecloud.io

This section instructs how to install each dependent library and the header files from [packagecloud.io](https://packagecloud.io/). 

**Note**: Skip reading this section if you have installed each dependent library and the header files from [GitHub](https://github.com/) in the previous section.

**Note**: As I descripbed in the previous section, [K2HASH](https://k2hash.antpick.ax/build.html) build option affects the **CHMPX** build option. See the Table1 in the previous section.

For DebianStretch or Ubuntu(Bionic Beaver) users, follow the steps below:
```bash
$ sudo apt-get update -y
$ sudo apt-get install curl -y
$ curl -s https://packagecloud.io/install/repositories/antpickax/stable/script.deb.sh \
    | sudo bash
$ sudo apt-get install autoconf autotools-dev gcc g++ make gdb libtool pkg-config \
    libyaml-dev libfullock-dev k2hash-dev chmpx-dev gnutls-dev -y
$ sudo apt-get install git -y
```

For Fedora28 or CentOS7.x(6.x) users, follow the steps below:
```bash
$ sudo yum makecache
$ sudo yum install curl -y
$ curl -s https://packagecloud.io/install/repositories/antpickax/stable/script.rpm.sh \
    | sudo bash
$ sudo yum install autoconf automake gcc gcc-c++ gdb make libtool pkgconfig \
    libyaml-devel libfullock-devel k2hash-devel chmpx-devel nss-devel -y
$ sudo yum install git -y
```

## 2. Clone the source code from GitHub

Download the **CHMPX**'s source code from [GitHub](https://github.com/).
```bash
$ git clone https://github.com/yahoojapan/chmpx.git
```

## 3. Build and install

Just follow the steps below to build **CHMPX** and install it. We use [GNU Automake](https://www.gnu.org/software/automake/) to build **CHMPX**.

For DebianStretch or Ubuntu(Bionic Beaver) users, follow the steps below:
```bash
$ cd chmpx
$ sh autogen.sh
$ ./configure --prefix=/usr --with-gnutls
$ make
$ sudo make install
```

For Fedora28 or CentOS7.x(6.x) users, follow the steps below:
```bash
$ cd chmpx
$ sh autogen.sh
$ ./configure --prefix=/usr --with-nss
$ make
$ sudo make install
```

After successfully installing **CHMPX**, you will see the CHMPX help text:
```bash
$ chmpx -h
[Usage]
chmpx [-conf <file> | -json <json>] [-ctlport <port>] [-d [slient|err|wan|msg|dump]] [-dfile <debug file path>]
chmpx [ -h | -v ]

[option]
  -conf <path>         specify the configration file(.ini .yaml .json) path
  -json <json string>  specify the configration json string
  -ctlport <port>      specify the self contrl port(*)
  -d <param>           specify the debugging output mode:
                        silent - no output
                        err    - output error level
                        wan    - output warning level
                        msg    - output debug(message) level
                        dump   - output communication debug level
  -dfile <path>        specify the file path which is put debug output
  -h(help)             display this usage.
  -v(version)          display version.

[environments]
  CHMDBGMODE           debugging mode like "-d" option.
  CHMDBGFILE           the file path for debugging output like "-dfile" option.
  CHMCONFFILE          configuration file path like "-conf" option
  CHMJSONCONF          configuration json string like "-json" option

(*) if ctlport option is specified, chmpx searches same ctlport in configuration
    file and ignores "CTLPORT" directive in "GLOBAL" section. and chmpx will
    start in the mode indicated by the server entry that has beed detected.
```
