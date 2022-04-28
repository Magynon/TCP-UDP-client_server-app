#ifndef _HELPERS_H
#define _HELPERS_H 1

#include <netinet/tcp.h>
#include <bits/stdc++.h>
#include <arpa/inet.h>

struct TCPmessage
{
	char size[10];
	char ip[16];
	uint16_t port;
	char type[11];
	char topic[50];
	char payload[1500];
};

struct client
{
	char id[11];
	int socket;
	bool isConnected;
	std::queue<TCPmessage> contentNotSent;
};

struct subscriber
{
	int sf;
	client *cli;
};

struct TCPcommand
{
	char type;
	char topic[50];
	char sf;
};

struct UDPmessage
{
	char topic[50];
	uint8_t type;
	char payload[1501];
};

struct topic
{
	char content[50];
	std::list<subscriber> subscribers;
};

#define DIE(assertion, call_description)  \
	do                                    \
	{                                     \
		if (assertion)                    \
		{                                 \
			fprintf(stderr, "(%s, %d): ", \
					__FILE__, __LINE__);  \
			perror(call_description);     \
			exit(EXIT_FAILURE);           \
		}                                 \
	} while (0)

#define BUFLEN 256	   // dimensiunea maxima a calupului de date
#define MAX_CLIENTS 10 // numarul maxim de clienti in asteptare

void printClients(std::vector<client> &clients);

void printSubscriptions(std::vector<topic> &topics);

int clientAlreadyExisting(std::vector<client> &clients, char *id);

void newClient(std::vector<client> &clients, char *id, int sock);

void convertAndSendTCPmessage(std::vector<topic> &topics, char *buffer, struct sockaddr_in udp_addr);

int getTopicByContent(std::vector<topic> topics, char *topicContent);

void unsubscribe(std::vector<topic> &topics, int sock, char *topicContent);

void subscribe(std::vector<topic> &topics, struct TCPcommand *proto, client &cli);

void disconnect(std::vector<client> &clients, int sock);

void reconnect(client &cli, int sock);

#endif
