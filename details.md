---
layout: contents
language: en-us
title: Details
short_desc: Consistent Hashing Mq inProcess data eXchange
lang_opp_file: detailsja.html
lang_opp_word: To Japanese
prev_url: feature.html
prev_string: Feature
top_url: index.html
top_string: TOP
next_url: usage.html
next_string: Usage
---

# Details
## CHMPX Options
The options of the CHMPX program are summarized below.

- -h(help)  
display help for the options of CHMPX program
- -v(version)  
display version of CHMPX program
- -conf <path>  
Specify the configuration file(formatted by INI, YAML, JSON). This option is exclusive with the -json option.
- -json <json string>  
Specify the configuration by JSON string. This option is exclusive with the -json option.
- -ctlport <port>  
Specify the control port number to clarify which setting value of the configuration the CHMPX to be activated uses.  
When starting multiple CHMPX programs with the same host name in the configuration, there are cases where the CHMPX program is ambiguous as to which setting value the confidence reads.  
In order to resolve such ambiguity of the configuration, it is necessary to specify the control port number.  
(The control port number is a setting value that should be unique within the same host in the cluster.)
- -cuk <cuk string>  
Specify CUK(Custom Unique Key) to clarify which setting value of the configuration is used by the starting CHMPX.  
When multiple CHMPX programs are started with the same host name(IP address) or control port in the configuration, there is a case where the CHMPX program is ambiguous as to which setting value it reads.  
When specifying such a configuration, CUK may be specified to clarify the settings.  
CUK is a setting that must be unique within a cluster.
- -d <param>  
Specify the level of output message. The level value is silent, err, wan, info or dump.
- -dfile <path>  
Specify the file path which the output message puts.

_You can see [configuration files in directory]({{ site.github.repository_url }}/tree/master/tests) for sample(the file formatted by INI/YAML/JSON, and string of JSON)._  
_If you do not specify both -conf and -json option, CHMPX checks CHMCONFFILE or CHMJSONCONF environments._  
_If there is not any option and environment for configuration, you can not run CHMPX program with error._

## Start CHMPX program
To start the CHMPX program, do as follows.
```
chmpx [-conf <file> | -json <json>] [-ctlport <port>] [-cuk <cuk>] [-d [silent|err|wan|msg|dump]] [-dfile <debug file path>]
```

The following is an example.
```
chmpx -conf server.ini -ctlport 8021 -d err -dfile /tmp/chmpx.log
```

## Configuration
The configuration that the CHMPX program and CHMPX library loads are described in.  
When the configuration is specified as a file, the setting is reloaded when the configuration file itself is updated.

### GLOBAL Section(\[GLOBAL\] is used for INI file)
This section specifies common configuration and default setting values of CHMPX configuration.

- FILEVERSION  
The version number for configuration
- DATE  
Updated time of the configuration file in RFC 2822 format. As default 0(unixtime).
- GROUP  
Specify the name of the cluster to which CHMPX belongs. All CHMPX nodes connected to a specific cluster must have the same cluster name.
- MODE  
Specify either server node(=SERVER) or slave node(=SLAVE).  
If the control port number (such as the -ctlport option) is specified when starting the CHMPX program and initializing the CHMPX library, this item can be omitted.  
If this item is omitted, the same server/slave node as the specified control port number is detected from the list and the type of the node is determined.
- DELIVERMODE  
Specify random(=RANDOM) or hash(=HASH) in the specification item of cluster type.
- CHMPXIDTYPE  
This item is specified in the global section and can be omitted.  
This is a value that specifies the SEED used to generate the CHMPXID.  
For the value, specify NAME (default), CUK, CTLENDPOINTS, or CUSTOM.  
For NAME, use the node's HOSTNAME and control port number. This is the same generation method as before.  
For CUK, use the value of CUK, and for CTLENDPOINTS, use the value of CTLENDPOINTS.  
CUSTOM uses the value of CUSTOM_ID_SEED.
- MAXCHMPX  
Specify the maximum number of server nodes and slave nodes. As default 1024.
- REPLICA  
In case of hash type, specify multiplexing. As default 0.
- MAXMQSERVER  
Specify the number of MQs used by the server node and the slave node. As default 1.
- MAXMQCLIENT  
Specify the maximum MQ number (number of clients * number of MQs used by each client) used by the server/client program. As default 1024.
- MQPERATTACH  
When the server/client program opens MQ, it specifies the number of MQs reserved beforehand. As default 1.
- MAXQPERSERVERMQ  
Specify the number of queues (the number of queues in each MQ) for each MQ for the server node and the slave node. As default 16.
- MAXQPERCLIENTMQ  
Specify the number of queues (the number of queues in each MQ) for each MQ for the server/client program. As default 1.
- MAXMQPERCLIENT  
Specify the maximum MQ number (MQ number / server and client program number) that the server/client program uses. As default 1.
- MAXHISTLOG  
Specify the maximum size of the communication log history within 32768. As default 8192.
- PORT  
Specify the port number for Socket as default.  
This value will be the value used for the unspecified entry of the port number in the SVRNODE/SLVNODE section.  
If this item is omitted, each node in the SVRNODE/SLVNODE section must always specify PORT.
- CTLPORT  
Specify the default control port number.  
This value will be the value used for the unspecified entry of the control port number in the SVRNODE/SLVNODE section.  
If this item is omitted, each node in the SVRNODE/SLVNODE section must always specify CTLPORT.
- SELFCTLPORT  
When starting the CHMPX program of multiple server nodes on the same host, specify the control port number to specify the entry to be started.  
This item can be omitted unless multiple server nodes start on the same host.  
In addition, if control port number (-ctlport option etc.) is specified when CHMPX program is started and when CHMPX library is initialized, this item can be omitted.
- SELFCUK  
This item is specified in the global section and can be omitted.  
When building a cluster with CUK, specify the value of your own CUK.  
Same use as SELFCTLPORT.
- RWTIMEOUT  
Specify the timeout value(us) per read/write of TCP/IP(Socket) communication. As default 200us.
- RETRYCNT  
Specify the number of retries at the time of read/write failure in TCP/IP(Socket) communication. As default 500 times.  
If RWTIMEOUT and RETRYCNT are not specified, the timeout as default 200us * 500 = 10ms.
- CONTIMEOUT  
Specify the timeout value(us) for TCP/IP(Socket) connection when starting CHMPX program. As default 500ms.
- MQRWTIMEOUT  
Specify the timeout value(us) per read/write in IPC(MQ) communication from server/client program to CHMPX(server node, slave node). As default 1000us(1ms).
- MQRETRYCNT  
Specify the number of retries at the time of read/write failure in IPC(MQ) communication from server/client program to CHMPX(server node, slave node). As default 2(this means 3 times in total).  
If MQRWTIMEOUT and MQRETRYCNT are not specified, The timeout as default 1000us * 3 = 3ms.
- MQACK  
In IPC communication from MQ communication server/client program to CHMPX program, specify whether ACK is returned or not. It is the only ACK in CHMPX, the default is NO.
- SOCKTHREADCNT  
Specify the number of waiting threads for TCP/IP(Socket) reception/processing. As default 0. Specify a number greater than or equal to 0.
- MQTHREADCNT  
Specify the number of waiting threads for reception and processing in MQ communication. As default 0. Specify a number greater than or equal to 0.
- MAXSOCKPOOL  
The CHMPX program can pool Socket between CHMPX programs in TCP/IP(Socket) communication. Specify the number of pools. As default 1. Specify a number of 1 or more.
- SOCKPOOLTIMEOUT  
If two or more Socket pools are specified and the surplus(standby) Socket is not used for more than a certain period of time, the session is closed. Specify this timeout time in seconds. As default 60sec.
- DOMERGE  
A flag that indicates whether data merging is required when the server node joins the cluster and starts services. As default OFF.  
If the cluster is RANDOM type, it can not select other than OFF.
- AUTOMERGE  
If the value of this item is ON and data merging is required at the server node, data merging will be started automatically. As default OFF. If the cluster is RANDOM type, this item is ignored.  
If this item is OFF, you can manually enter the service provision state by using commands such as SERVICEIN, MERGE, COMPMERGE(or ABORTMERGE) from the control port.
- MERGETIMEOUT  
Specify the timeout for data merge processing in seconds. As default no timeout. When 0 is set, it means no timeout.
- SSL  
Specify whether to perform SSL communication (on or off). This value will be the value used for nodes without SSL specification in the SVRNODE/SLVNODE section.
- SSL_MIN_VER  
Specify minimum SSL/TLS protocol version number by following values. If this value is not specified, using DEFAULT as default. This value can be specified only at GLOBAL section.  
"DEFAULT" - TLS v1.0 or higher(default)  
"SSLV3"   - SSL v3 or higher  
"TLSV1.0" - TLS v1.0 or higher  
"TLSV1.1" - TLS v1.1 or higher  
"TLSV1.2" - TLS v1.2 or higher  
"TLSV1.3" - TLS v1.3 or higher
- SSL_VERIFY_PEER  
When performing SSL communication, specify whether to check the client certificate of the connection source (on or off). This value will be the value used for the node without SSL_VERIFY_PEER specification in the SVRNODE/SLVNODE section.
- CAPATH  
For SSL communication, specify the file path or directory path of the CA certificate. This value will be the value used for the node without the CAPATH specification in the SVRNODE/SLVNODE section.
- NSSDB_DIR  
When CHMPX is linked NSS library, you can specify this for NSSDB directory path. If this is not specified, /usr/pki/nssdb is used as default(or using the directory path specified by NSS_DIR environment). This value can be specified only at GLOBAL section.
- SERVER_CERT  
For SSL communication, specify the file path of the server certificate. This value will be the value used for the node without the SERVER_CERT specification in the SVRNODE/SLVNODE section.
- SERVER_PRIKEY  
For SSL communication, specify the file path of the secret key of the server certificate. This value will be the value used for the node without SERVER_PRIKEY specified in the SVRNODE/SLVNODE section.
- SLAVE_CERT  
For SSL communication, specify the file path of the client certificate. This value will be the value used for the node without SLAVE_CERT specification in the SVRNODE/SLVNODE section.
- SLAVE_PRIKEY  
For SSL communication, specify the file path of the private key of the client certificate. This value will be the value used for the node with no SLAVE_PRIKEY specified in the SVRNODE/SLVNODE section.
- K2HFULLMAP  
Specify the setting value of K2HASH used inside CHMPX. As default on. (See [K2HASH](https://k2hash.antpick.ax/))
- K2HMASKBIT  
Specify the setting value of K2HASH used inside CHMPX. As default 8. (See [K2HASH](https://k2hash.antpick.ax/))
- K2HCMASKBIT  
Specify the setting value of K2HASH used inside CHMPX. As default 4. (See [K2HASH](https://k2hash.antpick.ax/))
- K2HMAXELE  
Specify the setting value of K2HASH used inside CHMPX. As default 8. (See [K2HASH](https://k2hash.antpick.ax/))

### SVRNODE Section(\[SVRNODE\] is used for INI file)
In this section, it is the setting of the server node of the cluster.  
Please specify the following items as an array in the SVRNODE section for each server node.  
Only the INI formatted file is special, multiple \[SVRNODE\] sections can be described in one configuration file.

- NAME  
Describe the host name of the server node. Specify the description with FQDN, IP address. Simple pseudo-regular expressions can also be used.  
For simple pseudo-regular expressions, you can specify multiple (separator ",") character strings in the \[\] range, and use "-" for individual character strings to specify the range.
 - server-\[x1,y1,z1\].yahoo.co.jp  
"," Multiple specifications with separator
 - server-\[1-5\].yahoo.co.jp  
Specify numerical range with "-" designation (Caution: reversing large and small is impossible)
 - server-\[a-z\].yahoo.co.jp  
Specify character range with "-" designation (Caution: specify in the range of a to z)
 - server-\[A-Z\].yahoo.co.jp  
Specify character range with "-" designation (Caution: specify in the range of A to Z)
 - server-\[x1,y1,z1\]\[1-5\].yahoo.co.jp  
Multiple designation with "[" and "]"
 - server-\[1-2,a-c,X-Z\].yahoo.co.jp  
Specify range within "," delimited by
- PORT  
Specify the port number for TCP/IP(Socket) communication.  
To omit this item, it is necessary to set the default value in the GLOBAL section.
- CTLPORT  
Specify the control port number.  
To omit this item, it is necessary to set the default value in the GLOBAL section.
- CUK  
When specifying CUK, set the value.
- ENDPOINTS  
When providing services through NAT, enumerate HOSTANME (IP address) and ports at the NAT entrance to access the ports from the outside.  
Up to four can be set.
- CTLENDPOINTS  
When providing services through NAT, enumerate HOSTANME (IP address) and ports at the NAT entrance to access the control port from the outside.  
Up to four can be set.
- FORWARD_PEERS  
When passing through NAT or Gateway, list HOSTANME (IP address) of Peer connected to other nodes.  
Up to four can be set.
- REVERSE_PEERS  
When passing through NAT or Gateway, list HOSTANME (IP address) of Peer connected from other nodes.  
Up to four can be set.
- CUSTOM_ID_SEED  
This value is an arbitrary character string.  
This item must be set to all nodes when CUSTOM is specified for CHMPXIDTYPE.  
Otherwise it must not be specified.
- SSL  
Specify whether to perform SSL communication (on / off) in TCP / IP (Socket) communication.  
If this item is omitted and the setting does not exist in the GLOBAL section, As default off.
- SSL_VERIFY_PEER  
When performing SSL communication in TCP / IP (Socket) communication, specify whether to check the connection source client certificate(on or off).  
If this item is omitted and the setting does not exist in the GLOBAL section, As default off.
- CAPATH  
For SSL communication, specify the file path or directory path of the CA certificate. If this item is omitted and the setting does not exist in the GLOBAL section, As default empty.
- SERVER_CERT  
For SSL communication, specify the file path of the server certificate. If this item is omitted and the setting does not exist in the GLOBAL section, As default empty.
- SERVER_PRIKEY  
For SSL communication, specify the file path of the secret key of the server certificate. If this item is omitted and the setting does not exist in the GLOBAL section, As default empty.
- SLAVE_CERT  
For SSL communication, specify the file path of the client certificate. If this item is omitted and the setting does not exist in the GLOBAL section, As default empty.
- SLAVE_PRIKEY  
For SSL communication, specify the file path of the private key of the client certificate. If this item is omitted and the setting does not exist in the GLOBAL section, As default empty.

### SLVNODE Section(\[SLVNODE\] is used for INI file)
In this section, it is the setting of the slave node of the cluster.  
Please specify the following items as an array in the SLVNODE section for each server node.  
Only the INI formatted file is special, multiple \[SLVNODE\] sections can be described in one configuration file.

- NAME  
Describe the server name of the slave node. Specify the description with FQDN, IP address, localhost, 127.0.0.1, :: 1, and so on. You can also use regular expressions.
- CTLPORT  
Specify the control port number.  
To omit this item, it is necessary to set the default value in the GLOBAL section.
- CUK  
When specifying CUK, set the value.
- ENDPOINTS  
When providing services through NAT, enumerate HOSTANME (IP address) and ports at the NAT entrance to access the ports from the outside.  
Up to four can be set.
- CTLENDPOINTS  
When providing services through NAT, enumerate HOSTANME (IP address) and ports at the NAT entrance to access the control port from the outside.  
Up to four can be set.
- FORWARD_PEERS  
When passing through NAT or Gateway, list HOSTANME (IP address) of Peer connected to other nodes.  
Up to four can be set.
- REVERSE_PEERS  
When passing through NAT or Gateway, list HOSTANME (IP address) of Peer connected from other nodes.  
Up to four can be set.
- CUSTOM_ID_SEED  
This value is an arbitrary character string.  
This item must be set to all nodes when CUSTOM is specified for CHMPXIDTYPE.  
Otherwise it must not be specified.
- CAPATH  
For SSL communication, specify the file path or directory path of the CA certificate. If this item is omitted and the setting does not exist in the GLOBAL section, As default empty.
- SLAVE_CERT  
For SSL communication, specify the file path of the server certificate. If this item is omitted and the setting does not exist in the GLOBAL section, As default empty.
- SLAVE_PRIKEY  
For SSL communication, specify the file path of the secret key of the server certificate. If this item is omitted and the setting does not exist in the GLOBAL section, As default empty.

### About other descriptive section(for INI file)
- INCLUDE Section  
If the INCLUDE keyword appears, the file is loaded into that line.

### Notes on communication
Communication between CHMPXs in one cluster can only communicate with servers described in SVRNODE, SLVNODE specified in the configuration.  
Communication from hosts that do not exist will be rejected.

### Notes on Posix MQ
Posix MQ is used for IPC communication between CHMPX program and server/client program.  
Depending on the system environment, the MQ number and size of the specified configuration may be small.  
The CHMPX program can not be started when the number of MQs and the size is small.  
In such a case, increase one or both of the following MQ related settings.
```
# echo 1024 > /proc/sys/fs/mqueue/queues_max
```
or
```
# echo 1024 > /proc/sys/fs/mqueue/msg_max
```
_(1024 number value is sample)_

If CHMPX fails to start due to lack of MQ resources, a message to that effect is output.  
This message can be output with the -d option specified.  
The message tells you to increase one of the above values that you need.  
You can use the chmconftest tool in advance to load the configuration and check the required MQ resources.

