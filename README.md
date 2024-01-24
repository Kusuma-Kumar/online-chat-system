Authors: Kusuma Kumar and Miriam Bamaca

Links/Recourses: 
https://www.ibm.com/docs/en/zos/3.1.0?topic=interface-c-socket-calls
https://www.ibm.com/docs/en/ztpf/1.1.0.15?topic=apis-fd-zeroinitialize-file-descriptor-set
https://stackoverflow.com/questions/10002868/what-value-of-backlog-should-i-use

Overview This project involves implementing a server and client for a basic online chat system. Each client represents a user with an optional associated name. Users can change their name and send messages that are visible to all connected clients.
Server Accepts a port number as a command-line argument. Handles each client in a separate thread. Prints messages to stdout for new connections, nickname changes, and disconnections. Supports an arbitrary number of simultaneous clients.
Client Takes hostname and port number of the server as command-line arguments. Prints received server messages with timestamps. Terminates on Ctrl-D (end-of-file) or if the server connection is terminated.
System Ensures that messages sent by a client are visible to all connected clients. Notifies remaining clients when a user disconnects, printing an informative message.
Files: chat-server.c: Contains server implementation. chat-client.c: Contains client implementation. Makefile: Default target builds chat-server and chat-client. README: Includes authors, known bugs, and consulted resources.

Example Convesation: Server (all text output by server process, pressed Ctrl-C after first client exited):
$ ./chat-server 8000 new connection from 127.0.0.1:46814 new connection from 127.0.0.1:46816 User unknown (127.0.0.1:46814) is now known as foo. User unknown (127.0.0.1:46816) is now known as bar. Lost connection from foo. ^C
Client 1 (the lines consisting of only "/nick foo", "hi", and "I’m just fine, thanks" were input at the terminal, followed by Ctrl-D):
$ ./chat-client localhost 8000 Connected /nick foo 22:31:37: User unknown (127.0.0.1:46814) is now known as foo. 22:31:41: User unknown (127.0.0.1:46816) is now known as bar. hi 22:33:40: foo: hi 22:33:42: bar: hello! 22:33:45: bar: how are you? I'm just fine, thanks 22:33:48: foo: I'm just fine, thanks Exiting. Client 2 (the lines consisting of only "/nick bar", "hello!", and "how are you?" were input at the terminal, exited when server terminated):
$ ./chat-client localhost 8000 Connected 22:31:37: User unknown (127.0.0.1:46814) is now known as foo. /nick bar 22:31:41: User unknown (127.0.0.1:46816) is now known as bar. 22:33:40: foo: hi hello! 22:33:42: bar: hello! how are you? 22:33:45: bar: how are you? 22:33:48: foo: I'm just fine, thanks 22:34:01: User foo (127.0.0.1:46814) has disconnected. Connection closed by remote host.

