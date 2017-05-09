Login Authentication Lab
-------------------------

To compile:
-On Client: type "`make`"
-On Server: type "`make`"
	
To run: 
- On Client: `./client <server_hostname> <port_number>`
- On Server: `./server <port_number>`
	
`<port_number>` obviously must be the same for both client and server. So if you type different `<port_number>`s, it will not work.

This code supports multiple simulataneous connections and times out the connections after 30 seconds of inactivity. The client is notified of the timeout, but does not interrupt cin.

You must put a file named "`account`" with tab-delimited usernames and passwords in the server directory. If you do not do this, the program will not work.