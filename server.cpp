#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "helpers.h"

void usage(char *file)
{
	fprintf(stderr, "Usage: %s server_port\n", file);
	exit(0);
}

int main(int argc, char *argv[])
{
	int sockUDP, sockTCP, newsockTCP, portno;
	char buffer[BUFLEN];
	struct sockaddr_in TCP_addr, UDP_addr;
	int n, i, ret;
	socklen_t TCPlen = sizeof(struct sockaddr), UDPlen = sizeof(struct sockaddr);

	std::vector<topic> topics;
	std::vector<client> clients;
	clients.reserve(10000);

	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	fd_set read_fds; // multimea de citire folosita in select()
	fd_set tmp_fds;	 // multime folosita temporar
	int fdmax;		 // valoare maxima fd din multimea read_fds

	if (argc < 2)
	{
		usage(argv[0]);
	}

	sockUDP = socket(PF_INET, SOCK_DGRAM, 0);
	DIE(sockUDP < 0, "socket UDP failed");

	sockTCP = socket(PF_INET, SOCK_STREAM, 0);
	DIE(sockTCP < 0, "socket TCP failed");

	portno = atoi(argv[1]);
	DIE(portno == 0, "atoi");

	memset((char *)&TCP_addr, 0, sizeof(TCP_addr));
	TCP_addr.sin_family = PF_INET;
	TCP_addr.sin_port = htons(portno);
	TCP_addr.sin_addr.s_addr = INADDR_ANY;

	memset((char *)&UDP_addr, 0, sizeof(UDP_addr));
	UDP_addr.sin_family = PF_INET;
	UDP_addr.sin_port = htons(portno);
	UDP_addr.sin_addr.s_addr = INADDR_ANY;

	ret = bind(sockUDP, (struct sockaddr *)&UDP_addr, sizeof(struct sockaddr));
	DIE(ret < 0, "bind udp failed");

	ret = bind(sockTCP, (struct sockaddr *)&TCP_addr, sizeof(struct sockaddr));
	DIE(ret < 0, "bind tcp failed");

	ret = listen(sockTCP, MAX_CLIENTS);
	DIE(ret < 0, "listen failed");

	// se goleste multimea de descriptori de citire (read_fds) si multimea temporara (tmp_fds)
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	// se adauga noul file descriptor (socketul pe care se asculta conexiuni) in multimea read_fds
	FD_SET(sockUDP, &read_fds);
	FD_SET(sockTCP, &read_fds);
	FD_SET(0, &read_fds);
	fdmax = sockTCP;

	while (1)
	{
		tmp_fds = read_fds;

		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select failed");

		for (i = 0; i <= fdmax; i++)
		{
			if (FD_ISSET(i, &tmp_fds))
			{
				memset(buffer, 0, BUFLEN);

				if (i == sockTCP)
				{
					// a venit o cerere de conexiune pe socketul inactiv (cel cu listen),
					// pe care serverul o accepta
					newsockTCP = accept(sockTCP, (struct sockaddr *)&TCP_addr, &TCPlen);
					DIE(newsockTCP < 0, "accept subscriber failed");
					int optval = 1;

					// nigger's algorithm
					setsockopt(newsockTCP, IPPROTO_TCP, TCP_NODELAY, (char *)&optval, sizeof(int));

					// se adauga noul socket intors de accept() la multimea descriptorilor de citire
					FD_SET(newsockTCP, &read_fds);

					// client's id
					memset(buffer, 0, BUFLEN);
					n = recv(newsockTCP, buffer, 11, 0);
					DIE(n < 0, "recv ID failed");

					int pos = clientAlreadyExisting(clients, buffer);

					if (pos != -1)
					{
						client &currentTCPClient = clients[pos];

						// if client is already connected, we will close the connection
						// else we will reconnect it
						if (currentTCPClient.isConnected)
						{
							printf("Client %s already connected\n", currentTCPClient.id);
							FD_CLR(newsockTCP, &read_fds);
							close(newsockTCP);
							continue;
						}
						else
						{
							// update the fdmax if it is the case
							if (newsockTCP > fdmax)
							{
								fdmax = newsockTCP;
							}
							reconnect(currentTCPClient, newsockTCP);
							printClients(clients);

							printf("Client %s reconnected from %s:%d\n",
								   currentTCPClient.id, inet_ntoa(TCP_addr.sin_addr), ntohs(TCP_addr.sin_port));
							continue;
						}
					}
					else
					{
						// update the fdmax if it is the case
						if (newsockTCP > fdmax)
						{
							fdmax = newsockTCP;
						}

						// create a new client and connect it
						newClient(clients, buffer, newsockTCP);
						printSubscriptions(topics);

						printClients(clients);

						printf("New client %s connected from %s:%d\n",
							   buffer, inet_ntoa(TCP_addr.sin_addr), ntohs(TCP_addr.sin_port));
						continue;
					}
				}
				else if (i == sockUDP)
				{
					printf("sockudp\n");

					// s-au primit date pe unul din socketii de client,
					// asa ca serverul trebuie sa le receptioneze
					n = recvfrom(sockUDP, buffer, BUFLEN, 0, (struct sockaddr *)&UDP_addr, &UDPlen);
					DIE(n < 0, "recv udp failed");

					printf("S-a primit de la clientul de pe socketul %d mesajul: %s\n", i, buffer);

					convertAndSendTCPmessage(topics, buffer, UDP_addr);
					continue;
				}
				else if (i == 0)
				{
					fgets(buffer, BUFLEN, stdin);
					if (strcmp(buffer, "exit\n") == 0)
					{
						goto exitServer;
					}
					else
					{
						printf("wrong command kiddo\n");
						continue;
					}
				}
				else
				{
					printf("recv tcp\n");
					n = recv(i, buffer, sizeof(TCPcommand), 0);
					DIE(n < 0, "recv tcp failed");

					TCPcommand *command = (TCPcommand *)malloc(sizeof(TCPcommand));
					memcpy(command, buffer, sizeof(TCPcommand));

					// printf("Command type: %c\n", command->type);

					// subscribe
					if (command->type == '0')
					{
						for (client &j : clients)
						{
							if (j.socket == i)
							{
								subscribe(topics, command, j);
								continue;
							}
						}
					}
					// unsubscribe
					else if (command->type == '1')
					{
						unsubscribe(topics, i, command->topic);
						continue;
					}
					// exit
					else if (command->type == '2')
					{
						disconnect(clients, i);
						printClients(clients);

						// clear the socket and update the max sock
						FD_CLR(i, &read_fds);
						for (int j = fdmax; j > 2; j--)
						{
							if (FD_ISSET(j, &read_fds))
							{
								fdmax = j;
								break;
							}
						}
						continue;
					}
					free(command);
				}
			}
		}
	}

exitServer:

	// close all the sockets
	for (int i = 2; i <= fdmax; i++)
	{
		if (FD_ISSET(i, &read_fds))
		{
			FD_CLR(i, &read_fds);
			close(i);
		}
	}

	return 0;
}
