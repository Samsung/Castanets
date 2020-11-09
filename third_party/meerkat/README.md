# service-discovery-manager
device discovery and process offloading agent service daemon
it provide multicast discovery and monitoring server resource status.
Please follow steps below

## BUILD
	$ cd Build
	$ touch .env
	$ make config
        -------------------------------------------
        |        Build MODULE [CONFIG]            |
        -------------------------------------------
        cd ./; ./.config

        <Select Target Platform>
	        (1) : PC/Server
	        (2) : (Tizen)Mobile
	        (3) : (Tizen)TV
	        (4) : (Tizen)Wearable
	        (5) : (Android)Mobile
	1   <=== Assuming you want PC/Server, so you press '1' here as a selection !!
	<Target Type : PC/Server (x86-x64)>

	$ make clean
	$ make

## GBS build (for Tizen)
        $ ./Build/build_tizen_standard_armv7l.sh (TM1/TW2)
        $ ./Build/build_tizen_tv_product_kantm2.sh (TV kantM2)

## Windows build
  open service_discovery_manager/Project/VC++/service-agent/service-agent.sln with visual studio
	set client_runner and server_runner project dependency with network_manager
	build client_runner and server_runner

## RUN
	$ cd Build/BIN/X64

### On Terminal for Server
	$ ./server_runner <=== Run with the configuration from server.ini in the same folder. You can edit the file as you want

	OR

	$ ./server_runner [multicast_addr] [multicast_port] [service_port] [monitor_port] [daemon = optional, false as default]
	ex) ./server_runner 224.1.1.2 9190 9191 9192 daemon <== run as daemon
	    ./server_runner 224.1.1.2 9190 9191 9192 <= run not as daemon

		CMMN >> Create Message Queue--sds-0000
	        CMMN >> [OSAL] Socket Initialize
	        CMMN >> Network Initialize success
	        CMMN >> Start Thread [Annonymous] Loop
	        CMMN >> start server with [9190] port
	        CMMN >> Create Message Queue--sms-0000
	        CMMN >> start tcp eco server with [9192] port
	        CMMN >> already called WSAStartup
	        CMMN >> Create Message Queue--srs-0000
	        CMMN >> already called WSAStartup
	        CMMN >> Start service server with [9191] port
        If you want to quit press 'q'

### On Terminal for client
	$ ./client_runner <=== Run with the configuration from client.ini in the same folder. You can edit the file as you want

	OR

	$ ./client_runner [multicast_addr] [multicast_port] [daemon=optional, false as default]
	ex) ./client_runner 224.1.1.2 9190 daemon <== run as daemon
	    ./client_runner 224.1.1.2 9190 <= run not as daemon

		CMMN >> Create Message Queue--sdc-0000
	        CMMN >> [OSAL] Socket Initialize
	        CMMN >> Network Initialize success
	        CMMN >> Start Thread [Annonymous] Loop
	        CMMN >> Create Message Queue--src-0000
	        CMMN >> already called WSAStartup
        Menu -- (Q) : quit program, (C) : continue <=== You need to press 'C' repeatedly at this moment whenever you see this msg.



## RESULT

**This result is part of the logs got from the sequence below :**

    1. Run server, first
    2. Run client

**Both were run in the same PC. So the result can be different on the environment or order of running server and client**

### On Server terminal
------------------------------------------------------------------------------------------------------------------------------
	        CMMN >> Receive- from:[10.113.112.155 - 45284] msg:[QUERY-SERVICE]
	        CMMN >> send unicast message : (QUERY-SERVICE)
	        CONN >> OnDiscoveryServerEvent : (1)-(45284)-(10.113.112.155)
	        CMMN >> Create Message Queue--sms-00006
	        CMMN >> AcceptEvent(SOCK:6)
	        CMMN >> Get Notify- form:sock[6] event[2]
	        CMMN >> Receive- from:[6-10.113.112.155] msg:[send tcp packet for getting latency]
        #36 36 36
	        CMMN >> Tcp Server Close Socket
	        CMMN >> Close Socket(SOCK:6)
	        CMMN >> Get Notify- form:sock[6] event[0]
	        CMMN >>  Unlinking the message queue header structure
	        CMMN >> Destroy Message queue -- sms-00006

------------------------------------------------------------------------------------------------------------------------------

### On Client terminal
------------------------------------------------------------------------------------------------------------------------------
	        CMMN >> Receive Response - [destination Address:10.113.112.155][discovery port:9190][payload:discovery://type:query-response,service-port:9191,monitor-port:9192]
	        CMMN >> Dump Packet [addr : 10.113.112.155] [monitor port : 9192] [service port : 9191]
	        CONN >> OnDiscoveryClientEvent : (9191)-(9192)-(10.113.112.155)
        ==Dump Service Provider Information!!==
        address : 10.113.112.155
        service port : 9191
        monitor port : 9192
        =======================================
	        CMMN >> Create Message Queue--mdc-00100
	        CMMN >> start tcp eco client - connect to (10.113.112.155)(9192)
	        CMMN >> already called WSAStartup
	        CMMN >> Socket Connect 10.113.112.155 9192
        #36 36 36
        Menu -- (Q) : quit program, (C) : continue
        	CMMN >> Receive- from:[socket:6] msg:[send tcp packet for getting latency] (sent : 2717269236 - recv : 2717269237)
	        CONN >> OnEcoClientEvent : (0)-(1) (mdc-00100)
	        CONN >> Found
	        CMMN >> Get Notify - event[0]
	        CMMN >> Close Socket(SOCK:6)


------------------------------------------------------------------------------------------------------------------------------

## PROCEDURE

	                   [Server]                                                     [Client]

	        Create Discovery SOCKET (UDP)
        	              |
	           BIND SOCKET (given port)
        	              |
	    JOIN MULTICAST Address (given address)
        	              |
	          Wait For MULTICAT Message                                      Create Discovery SOCKET
        	              |                                                             |
         	              |                                               Set TTL with default value (64)
         	              |                                                             |
         	              |                   <-----------------                Send Query Message
			      |                                            (send to given multicast address)
	            Receive Query Message                                                   |
	                      |                                                     Wait For Response
			      |                                       (listen from auto punched udp port by sendto)
	             Check Message Type                                                     |
        	              |                                                             |
	       Send Response with service port    ------------------>                       |
	          (message type: unicast)                                                   |
 	          (destination(IP/Port):                                                    |
	    get from query message's sock addr_in)                               receive response message
                                                                   (get service port(pre configured tcp listen port),
						                                        server IP)

