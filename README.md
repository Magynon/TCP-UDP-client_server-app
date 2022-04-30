# Tema-2-PCom

The homework was built upon the code that I had already written durring the TCP lab. Requests can come from 3 different places: TCP client, UDP client, server or ???.

- TCP client - it checks whether the client is new, reconnecting or already connected. If new, the server adds it to the clients vector, if it's reconnecting, it's updating the client's status as being connected and if it's already connected, it does nothing (closes the connection).

- UDP client - the server receives the message, which it parses and converts into a TCP message. The TCP message struct is the following:

```
struct TCPmessage
{
	char size[10];
	char ip[16];
	uint16_t port;
	char type[11];
	char topic[50];
	char payload[1500];
};
```

The last three strings, it needs to receive the message, whereas the first three components are there just to make sure the destination knows where the message comes from.

- after the message is parsed, the topics vector is traversed and when the topic is found (if not, it just throws the message away) it proceeds to send the message to all the subscribers in the subscriber list associated with it. I chose to have a topic vector, with each vector having its own list of subscribers, as opposed to every client having a list of topics, because I was intending to use pointers to access the subscribers' identities inside the subscriber struct. That has proven to be a bad idea, because it turns out mixing C pointers with C++ is bad and is a noobie mistake which took me 5h to find. Whenever a push back happens inside a vector, all the addresses of the elements change and so saving them just does nothing (points to garbage). Finally I still used pointers by compromising the size of the vector (making sure it won't realloc). Retrospectively, I would use the "client - list of topics" approach, because my approach would be quite inneficient time-wise, without being able to access objects by their addresses.

- server - the only command the server can receive from itself (stdin) is 'exit' which is done with -*shrugs*- a GOTO (YES YOU WILL SAY IT'S BAD, YOU KNOW WHAT I DON'T CARE =)) I know why some people consider it to be bad code practice, but this was a willing decision.)

- TCP command - the three commands the server can receive from a TCP client are *subscribe, unsubscribe and disconnect*. Disconnecting involves changing the client's status, whereas subscribe and unsubscribe add it to the topic's subscribers list. Depending on whether the client wants to receive the messages which were sent while it was not online, those messages are pushed to a stack belonging to the client. When reconnecting, the server will make sure to check whether the client's stack is empty or not and send all the pending messages if it is the case.

The subscriber parses the commands from stdin and sends them to the server (after connecting successfully). It also receives messages from the server, which it parses effortlessly using the TCPmessage struct and prints to stdout.

The only problems I met were memory wise because I am still getting used to C++ and I am using the C reasoning, which is bad. But overall I found it to be a quite enjoyable homework, being able to actually see the results of the code.