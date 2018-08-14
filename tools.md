---
layout: contents
language: en-us
title: Tools
short_desc: Consistent Hashing Mq inProcess data eXchange
lang_opp_file: toolsja.html
lang_opp_word: To Japanese
prev_url: environments.html
prev_string: Environments
top_url: index.html
top_string: TOP
next_url: 
next_string: 
---

# Tools
CHMPX has the following tools for managing CHMPX processes and clusters.

## chmpxstatus
This tool is a tool for checking the status of all CHMPX server nodes and slave nodes including CHPX process itself.  
CHMPX stores the state of all the CHMPX nodes of the process in shared memory.  
This tool is a tool for getting state from this shared memory, without affecting the CHMPX process.  
This tool can display the status of the CHMPX process itself(default) or the status of all other CHMPX processes connected by this CHMPX process.  

### How to use
Please refer to man page.
Alternatively, if you start with **-h(help)** option specified, help will be displayed.

### Options
#### -h
Help on this tool is displayed.
#### -conf [filename]
Please specify the configuration file (.ini .yaml. Json) that was specified when starting CHMPX process.
#### -json [json string]
Please specify the configuration of the JSON string specified when starting CHMPX process.  
This option is an exclusive option to the **-conf** option.
#### -ctlport
Specify the control port specified when starting CHMPX process.
#### -self
Only the status (status) of the CHMPX process itself is displayed.  
If this option is not specified, the status of all CHMPX processes connected by the CHMPX process including itself is displayed.
#### -d [level]
Specify the level of debugging information of this tool. (ERR, WAN, INFO, DUMP)
#### -dfile [file]
Please specify the file path when outputting debugging information to a file.

## chmpxbench
This tool can be used for CHMPX process operation check and benchmark measurement.  
This tool can be started as a client program of CHMPX server node and slave node, and it can operate as a dummy client.  
Using this tool, you can check communication between CHMPX processes and measure benchmarks.  
When using this tool, you must use this tool on both sides to connect to both CHMPX server node and slave node.  

### How to use
Please refer to man page.
Alternatively, if you start with **-h(help)** option specified, help will be displayed.

### Options
#### -h
Help on this tool is displayed.
#### -conf [filename]
Please specify the configuration file (.ini .yaml. Json) that was specified when starting the connected CHMPX process.
#### -json [json string]
Please specify the configuration of the JSON string specified when starting the CHMPX process to be connected.  
This option is an exclusive option to the **-conf** option.
#### -s
Specify this tool when connecting to the CHMPX server node and start up as a server process.
#### -c
Specify this tool when connecting to the CHMPX slave node and start up as a client process.
#### -l
Specify the number of times to send and receive this tool.  
This tool will end automatically when transmission/reception is performed a specified number of times.
With this option, specify the number of transmission/reception.
If 0 is specified for this option, there will be no upper limit on the number of send/receive operations.
#### -proccnt
This tool can be started as multiple processes so that multiple processes can be simulated.  
This option specifies the number of processes to be activated.
#### -threadcnt
This tool can activate multiple threads to simulate multiple threads.  
This option specifies the number of threads to be activated.
The number of startup threads is one process unit, which is combined with the **-proccnt** option to determine the number of parallel processes.
#### -ta
Activate this process as a turnaround mode.  
For a server process with the **-s** option specified, it returns data to the client process after receiving the data.
In the case of a client process with the **-c** option specified, it waits for a reply from the server process after sending data.
This option allows you to simulate turnaround communication returned from the client process through the server process back to the client process.
#### -b
It is an option dedicated to the client process with the **-c** option specified.  
A client process that specifies this option broadcasts data to all CHMPX server nodes as broadcast.
#### -dl
It is an option dedicated to the client process with the **-c** option specified.  
Specify the data length to send.
The minimum size is 64 bytes.
#### -pr
The data transmitted and received by this tool is displayed.
#### -mergedump
Display communication data for automatic data merging provided by CHMPX.  
This option simulates and checks CHMPX's automatic data merge function.
#### -d [level]
Specify the level of debugging information of this tool. (ERR, WAN, INFO, DUMP)
#### -dfile [file]
Please specify the file path when outputting debugging information to a file.


## chmpxconftest
This tool is a configuration file specified when starting CHMPX or a tool to check JSON string.  
This is a validation tool to confirm beforehand whether there is any mistake in the configuration file or JSON string.  

### How to use
Please refer to man page.
Alternatively, if you start with **-h(help)** option specified, help will be displayed.

### Options
#### -h
Help on this tool is displayed.
#### -conf [filename]
Please specify the configuration file (.ini .yaml. Json) to be inspected.
#### -json [json string]
Please specify the configuration of the JSON string to be inspected.  
This option is an exclusive option to the **-conf** option.
#### -print_default
The default value set by the CHMPX program for items not specified in the configuration is displayed.
#### -d [level]
Specify the level of debugging information of this tool. (ERR, WAN, INFO, DUMP)
#### -dfile [file]
Please specify the file path when outputting debugging information to a file.


## chmpxlinetool
An interactive command line tool that controls the active CHMPX process.  
This tool connects to the control port of the CHMPX process, can execute control commands, and manages the CHMPX process.  
This tool eliminates the need to manually enter control commands to the control port.  
Also, CHMPX processes on running RING can be automatically recognized and distinguished.  
Although this tool is based on an interactive command line, you can automatically execute that command file with multiple commands as command files.
This provides automatic execution of commands.  
The command of this tool has a command for checking the state of the CHMPX process in addition to the control command, and it is possible to check the consistency of the state of CHMPX server node and slave node on RING.  
In addition, this consistency check command is provided as a startup option and can be used as a monitoring tool.  

### How to use
For startup options, you can refer to the man page or by specifying the **-h(help)** option.  
For interactive commands, after launching this tool, you can reference it by specifying the "h (help)" command on the interactive prompt.

### How to execute
This tool provides three kinds of starting methods.
You can start it by one of the following methods.

#### Environment variable
Specify the configuration file or JSON string with the environment variable(CHMCONFFILE or CHMJSONCONF) and start it.  
This specification method is the same as when starting CHMPX.
This startup method can be used only on the host that is running the CHMPX process.

#### Configuration file or JSON string
Specify the configuration file or JSON string as the startup option.  
This specification method is the same as when starting the chmpx process.
This startup method can be used only on the host that is running the CHMPX process.

#### Host name and control port
Specify the host name(IP address) and control port on which the CHMPX process is running as startup options.  
With this activation method, the CHMPX process can be started on any host that allows access to the control port.
In this method, it is unknown whether the CHMPX process to be connected is a server node or a slave node.
For this reason, we start it by designating the node type (server/slave) as a temporary(initial value).
If the provisional(initial value) node type is not specified, the CHMPX process to be connected will operate as a slave node.  
This provisional(initial value) node type is a value only after startup, and after activation, it updates the information of CHMPX process to recognize the correct node type.


### Options
#### -h
Help on this tool is displayed.
#### -conf [filename]
Please specify the configuration file(.ini .yaml. Json) that was specified when starting the connected CHMPX process.
#### -json [json string]
Please specify the configuration of the JSON string specified when starting the CHMPX process to be connected.
This option is an exclusive option to the **-conf** option.
#### -host [hostname]
Specify the host name of the operating CHMPX process to be connected.  
This option is exclusive with the **-conf** and **-json** options.
#### -ctlport
Specify the control port specified when starting CHMPX process.  
This option is mandatory if you specify the **-host** option.
#### -server
This option specifies the temporary(initial value) node type of the CHMPX process to be connected as a server node when the **-host** option is specified.
#### -save
This option specifies the temporary(initial value) node type of the CHMPX process to be connected as a slave node when the **-host** option is specified.  
If the **-host** option is specified and the **-server**/**-slave** option is not specified, the temporary(initial value) node type of the CHMPX process to be connected will operate as a slave node.
#### -threadcnt [count]
In this command processing, each CHMPX control command is sent internally to each CHMPX process.  
In the process of communicating with all nodes, it is possible to specify the number of threads in order to enable parallel processing.  
To do parallel processing, specify the number of threads with this option.  
If this option is not specified, parallel processing is not performed.
#### -check [second]
Execute the check command of this tool at startup.  
The check command will continue to run at the second interval indicated by the parameters passed to this option.
To stop, enter **Ctrl-C**.
#### -status [second]
Execute the status command of this tool at startup.  
The status command will continue to run at the interval indicated by the parameters passed to this option.
To stop, enter **Ctrl-C**.
#### -run [file path]
Specify a command file(one line for one command) describing multiple commands and execute it immediately after startup.
#### -his [count]
Specifies the maximum number of history of commands to be entered interactively.  
The default can hold as many as 500 commands as history.
#### -nocolor
Do not use color(escape sequence) to display the processing result of the check/status command of this tool.
#### -lap
When executing each command of this tool, specify execution time(lap time) to be displayed.
#### -d [level]
Specify the level of debugging information of this tool.(SILENT, ERR, WAN, INFO, DUMP)
#### -d chmpx
Enable debugging information from libchmpx.so used internally by this tool.  
The same debug information level as the level specified by the **-d** option is applied.
#### -dfile [file]
Please specify the file path when outputting debugging information to a file.

### Interactive commands
This command can be input interactively after starting this tool.  
These commands can be written as a command file in a file and specified with the **-run** option at startup.

#### help(h)
Display help for interactive commands.
#### quit(q)/exit
Exit this tool.
#### update(u)
In this tool, CHMPX(specified by configuration or host name) specified at startup is registered as the initial CHMPX node group.  
By executing this command, all CHMPX nodes are listed from the initial CHMPX node group and registered.
Registered CHMPX nodes are used for other interactive commands as targets.
Within this tool, a DUMP control command is called on the initial CHMPX node to acquire all nodes.
The list of CHMPX nodes registered after this is called a dynamic CHMPX node list.
#### nodes [nodyna | noupdate] [server | slave]
The list of CHMPX nodes registered in this tool is displayed.  
When nodyna parameter is specified, a list of CHMPX nodes (specified by configuration or host name) specified at startup is displayed.  
If you do not specify the nodyna parameter, use the dynamic CHMPX node list.  
If the noupdate parameter is specified, the dynamic CHMPX node list is not updated.  
If you do not specify the noupdate parameter, the dynamic CHMPX node list is updated immediately before this command is executed.  
When the server(or slave) parameter is specified, only the server node(or slave node) is displayed.  
If you do not specify the server/slave parameter, a list of all CHMPX nodes will be displayed.
#### status [self | all] [host(:port)]
Information on the target CHMPX node is displayed.  
This information is the result of CHMPX control command(**SELFSTATUS** or **ALLSTATUS**).  
When the self parameter is specified, the result of the **SELFSTATUS** control command of the target CHMPX node is displayed.  
When the all parameter is specified, the result of the **ALLSTATUS** control command is displayed for the target CHMPX node.  
When this tool is started with the **-host** option specified, the CHMPX process specified by that host and control port becomes the target CHMPX node.  
When this tool is activated by configuration, it is necessary to specify the target CHMPX node with the parameter(host: port) of this command.  
The specified **host:port** parameter must be a CHMPX node registered in this tool.
#### check [noupdate] [all | host(:port)]
Check the integrity of the state of all CHMPX nodes in the dynamic CHMPX node list.  
The information to be checked is the status of the CHMPX node, the number of communication sockets(IN/OUT), and the HASH value of the server node.  
The check result is displayed with coloring(when **-nocolor** is not specified), and if there is a problem with consistency, ERR is displayed.  
If the **host:port** parameter is not specified, check results of all CHMPX nodes are displayed in a simplified manner.  
When the **host:port** parameter is specified, the detailed check result of only that CHMPX node is displayed.  
The specified **host:port** parameter must be a CHMPX node registered in this tool.
#### statusupdate [noupdate] [all | host(:port)]
It forcibly reflects the state of the CHMPX node to other CHMPX nodes for all CHMPX nodes or one of the dynamic CHMPX node lists.  
This command corresponds to the **UPDATESTATUS** control command.  
If all and **host:port** parameters are omitted, all CHMPX nodes are targeted.
#### servicein [noupdate] [host(:port)]
It instructs to service the target CHMPX node (**SERVICEIN** control command).  
When **host:port** is specified, the target CHMPX node becomes the specified CHMPX node.  
When this tool is started with the host option, **host:port** can be omitted.  
If omitted, the CHMPX node specified at startup is targeted.
#### serviceout [noupdate] [host(:port)]
It instructs to halt the service of the target CHMPX node (**SERVICEOUT** control command).  
When **host:port** is specified, the target CHMPX node becomes the specified CHMPX node.  
When this tool is started with the host option, **host:port** can be omitted. If omitted, the CHMPX node specified at startup is targeted.
#### merge [noupdate] [start | abort | complete]
When the **start** parameter is specified, it instructs all CHMPX server nodes of the dynamic CHMPX node list to start execution of AUTOMERGE (**MERGE** control command).  
When the **abort** parameter is specified, instruct interrupts (**ABORTMERGE** control command) of the processing being merged to all CHMPX server nodes in the dynamic CHMPX node list.  
When the **complete** parameter is specified, instruct the completion of merge processing (**COMPMERGE** control command) to all CHMPX server nodes in the dynamic CHMPX node list.
#### suspend [noupdate]
Instruct all CHMPX server nodes in the dynamic CHMPX node list to ignore **AUTOMERGE** setting (**SUSPEND** control command).
#### nosuspend [noupdate]
Instructs all CHMPX server nodes in the dynamic CHMPX node list to enable **AUTOMERGE** setting (**NOSUSPEND** control command).
#### dump [noupdate] [host(:port)]
Displays the result of the **DUMP** control command of the target CHMPX node.  
If this tool is started with the **-host** option specified and the **host:port** parameter is omitted, the CHMPX process specified at startup becomes the target CHMPX node.  
When this tool is activated by configuration, it is necessary to specify the target CHMPX node with the parameter(**host:port**) of this command.  
The specified **host:port** parameter must be a CHMPX node registered in this tool.
#### version [nodyna | noupdate]
Display the version of the target CHMPX node group (**VERSION** control command).  
When nodyna parameter is specified, the CHMPX node(specified by configuration or host name) specified at startup becomes the target CHMPX node.  
If you do not specify the nodyna parameter, the dynamic CHMPX node list becomes the target CHMPX node.  
If the noupdate parameter is specified, the dynamic CHMPX node list is not updated.  
If you do not specify the noupdate parameter, the dynamic CHMPX node list is updated immediately before this command is executed.
#### loop [second] [loop limit count]
It instructs to continuously execute the command of this tool.  
The second parameter specifies the interval of execution.  
Specify the execution count with the loop limit count parameter.  
If this parameter is not specified, it is executed continuously with no limit.  
After entering this command, the input prompt changes to **CLT LOOP ....>** command prompt.  
The commands entered at this command prompt are executed in order.  
**CLT LOOP ....>** To complete command entry to the command prompt, enter **"."**.
**CLT LOOP ....>** As soon as the input to the command prompt is completed, continuous command execution starts.  
To stop continuous command execution, enter **Ctrl-C**.
#### loopcmd [command ...]
This command is not available for interactive input.  
This command can be used only in the command file specified for the **-run** option.  
In the command file, following this loop command, this command is described in succession.  
Until this command is discontinuous, it is recognized as command range of continuous execution.  
All commands to be continuously executed by the loop command displayed in the command history are displayed as a prefix of this command.
#### dbglevel [slt | err | wan | msg | dmp]
Dynamically change the debug information level at the time of execution of this tool.
#### dchmpx [on | off]
Switch debug output of libchmpx at the time of executing this tool.
#### history(his)
Display the command history.  
From the displayed command history, you can re-execute the command using **"!"**.
#### save [file path]
You can output the command history to a file.  
The outputted file can be specified with the **-run** option at startup. You can also specify it with **load** command.
#### load [file path]
Read the command file and execute it.
#### shell
Start the shell.  
Please exit the shell to return to this tool.
#### echo [string]
Display an arbitrary character string.
#### sleep [second]
Stop execution for the specified number of seconds.

