---
layout: contents
language: en-us
title: Usage
short_desc: Consistent Hashing Mq inProcess data eXchange
lang_opp_file: usageja.html
lang_opp_word: To Japanese
prev_url: details.html
prev_string: Details
top_url: index.html
top_string: TOP
next_url: build.html
next_string: Build
---

# Usage
After building CHMPX, you can check the operation by the following procedure.

## Sample Configuration
The following is the configuration used for CHMPX test. please refer.

### For server node.
- Configuration file formatted by INI
[test_server.ini]({{ site.github.repository_url }}/blob/master/tests/test_server.ini)
- Configuration file formatted by YAML
[test_server.yaml]({{ site.github.repository_url }}/blob/master/tests/test_server.yaml)
- Configuration file formatted by JSON
[test_server.json]({{ site.github.repository_url }}/blob/master/tests/test_server.json)
- Configuration by string of JSON
The character string after "SERVER=" in the [test_json_string.data]({{ site.github.repository_url }}/blob/master/tests/test_json_string.data) file

### For slave node
- Configuration file formatted by INI
[test_slave.ini]({{ site.github.repository_url }}/blob/master/tests/test_slave.ini)
- Configuration file formatted by YAML
[test_slave.yaml]({{ site.github.repository_url }}/blob/master/tests/test_slave.yaml)
- Configuration file formatted by JSON
[test_slave.json]({{ site.github.repository_url }}/blob/master/tests/test_slave.json)
- Configuration by string of JSON
The character string after "SLAVE=" in the [test_json_string.data]({{ site.github.repository_url }}/blob/master/tests/test_json_string.data) file

## Operation check

### 1. Creating a usage environment
There are two ways to install **CHMPX** in your environment.  
One is to download and install the package of **CHMPX** from [packagecloud.io](https://packagecloud.io/).  
The other way is to build and install **CHMPX** from source code yourself.  
These methods are described below.  

#### Installing packages
The **CHMPX** publishes [packages](https://packagecloud.io/app/antpickax/stable/search?q=chmpx) in [packagecloud.io - AntPickax stable repository](https://packagecloud.io/antpickax/stable) so that anyone can use it.  
The package of the **CHMPX** is released in the form of Debian package, RPM package.  
Since the installation method differs depending on your OS, please check the following procedure and install it.  

##### For recent Debian-based Linux distributions users, follow the steps below:
```
$ sudo apt-get update -y
$ sudo apt-get install curl -y
$ curl -s https://packagecloud.io/install/repositories/antpickax/stable/script.deb.sh | sudo bash
$ sudo apt-get install chmpx
```
To install the developer package, please install the following package.
```
$ sudo apt-get install chmpx-dev
```

##### For users who use supported Fedora other than latest version, follow the steps below:
```
$ sudo dnf makecache
$ sudo dnf install curl -y
$ curl -s https://packagecloud.io/install/repositories/antpickax/stable/script.rpm.sh | sudo bash
$ sudo dnf install chmpx
```
To install the developer package, please install the following package.
```
$ sudo dnf install chmpx-devel
```

##### For other recent RPM-based Linux distributions users, follow the steps below:
```
$ sudo yum makecache
$ sudo yum install curl -y
$ curl -s https://packagecloud.io/install/repositories/antpickax/stable/script.rpm.sh | sudo bash
$ sudo yum install chmpx
```
To install the developer package, please install the following package.
```
$ sudo yum install chmpx-devel
```

##### Other OS
If you are not using the above OS, packages are not prepared and can not be installed directly.  
In this case, build from the [source code](https://github.com/yahoojapan/chmpx) described below and install it.

#### Build and install from source code
For details on how to build and install **CHMPX** from [source code](https://github.com/yahoojapan/chmpx), please see [Build](https://chmpx.antpick.ax/build.html).

### 2. Run CHMPX for server node
```
$ chmpx -conf test_server.ini
```

### 3. Run server program(chmpxbench server mode) which joins server node CHMPX
```
$ chmpxbench -s -conf test_server.ini -l 0 -proccnt 1 -threadcnt 1 -ta -dl 128 -pr -g err -dummykey TEST
```

### 4. Run CHMPX for slave node
```
$ chmpx -conf test_slave.ini
```

### 5. Run client program(chmpxbench client mode) which joins slave node CHMPX
```
$ chmpxbench -c -conf test_slave.ini -l 1 -proccnt 1 -threadcnt 1 -ta -dl 128 -pr -g err -dummykey TEST
```
_If there is no error with the above operation, there is no problem._

1. Exit all programs  
Send signal HUP to all programs (chmpx, chmpxbench), and it will exit automatically.

## CHMPX control command
CHMPX is started by specifying the control port which is for controlling the CHMPX process itself and CHMPX server nodes on RING.  
By connecting to this control port and sending control command, you can manage and confirm status for CHMPX process and CHMPX server nodes on RING.
Control commands can be roughly divided into commands that control the CHMPX process itself that receives the commands and commands that control the CHMPX server nodes (RING) on the RING.
You can use these control commands to check and change the status of CHMPX server nodes on RING and also to check and change the status of the CHMPX process itself.  

We recommend not using the control command directly via this control port, but using **chmpxlinetool** which is easy to use.

### How to use
There is an item of **CTLPORT** in the configuration(file or JSON string) for starting the CHMPX program.  
You must specify a control port for this **CTLPORT**.
You can connect to this control port and send control commands as a string to the CHMPX process.
The CHMPX process receives a control command and processes and responds according to that command.
Please note that we currently do not support SSL encryption for connection and communication with the control port. (It will be supported in the future)

### About access to control port
Access to the control port is possible only for the server node and the slave node specified in the configuration (file or JSON string).

### Control command (effective for CHMPX process only)
#### VERSION
This command returns the version of the CHMPX process.

#### SELFSTATUS
This command returns the state of only the connected CHMPX process.
The result returns the basic state of the CHMPX process, which is rougher than the **DUMP** control command.

#### ALLSTATUS
Returns the status of all CHMPX server nodes and slave nodes held by the connected CHMPX process.
This command returns the minimum necessary state of other CHMPX processes including self contained in the connected CHMPX process.
With this command you can get the state on the RING between CHMPX processes.

#### UPDATESTATUS
This command sends the state of the CHMPX process that received this command to the other CHMPX process connected by this CHMPX process and forcibly updates the state.
If one CHMPX process state is propagated to the other CHMPX process in the wrong state, this command can be forcibly set to the correct CHMPX process state.
It is a case not normally occurring, but it is a recovery command to match the state of CHMPX process.

#### DUMP
This command returns all the internal information of the connected CHMPX process.
You can check the detailed status of the current CHMPX process with this command.

### Control command (effective for all CHMPX server node processes on RING)
#### SERVICEIN
This command instructs the connected CHMPX server node to provide the service.
If the CHMPX process of the server node is started and participates in RING but is not providing the service(SERVICE OUT), it instructs to provide the service.
You can use this command to set the CHMPX process ready for service provision(SERVICE IN, ADD, PENDING).
When the **AUTOMERGE** setting is ON in the configuration, the CHMPX process performs initial operation so that it automatically enters the SERVICE IN state, so you do not need to use this command.

#### SERVICEOUT [hostname]:[control port number]
When a CHMPX server node process participates in RING and provides service(SERVICE IN), this command can instruct the CHMPX process not to provide service.
When the CHMPX process receives this command, it becomes the service discontinuance ready state (SERVICE IN, DELETE, PENDING).
You can specify a CHMPX process(host) other than the CHMPX process that sends this command for the specified host name and control port number.
This is to make it possible for a server node that is not running the CHMPX process(DOWN) to be served out from another CHMPX server node.
When the **AUTOMERGE** setting of the configuration is set to ON and the CHMPX process is stopped(terminated), the SERVICE OUT state automatically operates so that it does not need to use this command.

#### MERGE
When the state of the CHMPX server node is in the preparation state(PENDING) by SERVICEIN/SERVICEOUT commands, etc, this command starts data merging.
When a client process(server side process) connecting with a CHMPX server node process has data, data must be merged with other CHMPX server node processes when starting or stopping service provision.
This is a control command to start merging this data.
If this command is successfully accepted, the CHMPX process will be in the merge executing state(DOING).
The CHMPX server node process automatically shifts to the merge complete status(DONE) when data merging is completed.
When the **AUTOMERGE** setting of the configuration is set to ON, the CHMPX server node process in the PENDING state automatically shifts to the DOING and DONE state as soon as preparation is completed.

#### COMPMERGE
When all CHMPX server node processes are in the merge complete status(DONE) of data, it becomes the service service provision/stop state(SERVICE IN/SERVICE OUT) by receiving this command.
If at least one CHMPX server node process is not in the data merge complete state(DONE), this command will fail.
When the **AUTOMERGE** setting of the configuration is set to ON, when all the CHMPX server node processes have reached the merge complete state(DONE) of the data, they are automatically set to the service service provision/stop state(SERVICE IN/SERVICE OUT).

#### ABORTMERGE
This command cancels the process when the CHMPX server node process is in the service provision/halt preparation state(PENDING), merging of data(DOING), and data merge complete(DONE), and returns the original state.
This command is used to cancel the process in case the data merging process can not be completed or it takes time.

#### SUSPENDMERGE
CHMPX reads the setting of **AUTOMERGE** from the configuration at startup and decides whether to execute automatic merging processing.
With this command you can specify whether to ignore **AUTOMERGE** configuration(SUSPEND) or valid it(NOSUSPEND).
This command instructs all CHMPX server nodes on RING to ignore the **AUTOMERGE** setting(SUSPEND).
With this command, this setting status is reflected not only to all CHMPX server nodes on RING at that time but also to CHMPX server nodes that join RING later.
This command temporarily disables the setting of **AUTOMERGE** when many numerous CHMPX server nodes are input to RING at a time.
For example, by starting service provision by SERVICEIN/MERGE/COMPMERGE commands after submission.
When adding a large number of CHMPX server nodes at the same time by this method, **AUTOMERGE** is not automatically executed every addition, so that merge processing can be performed efficiently.

#### NOSUSPENDMERGE
This command instructs all CHMPX server nodes on RING to enable **AUTOMERGE** setting(NOSUSPEND).
This command cancels the state where **AUTOMERGE** setting is invalid(SUSPEND).
With this command, this setting state is reflected not only to all CHMPX server nodes on RING but also to CHMPX server nodes that join RING later.

