#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "helpers.h"

void usage(char *file)
{
	fprintf(stderr, "Usage: %s server_address server_port\n", file);
	exit(0);
}

int main(int argc, char *argv[])
{
	int sockfd, n, ret;
	struct sockaddr_in serv_addr;
	fd_set read_fds, tmp_fds;
	char buffer[BUFLEN];

	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	if (argc < 3)
	{
		usage(argv[0]);
	}

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket");

	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	FD_SET(STDIN_FILENO, &read_fds);
	FD_SET(sockfd, &read_fds);

	int max_fd = STDIN_FILENO > sockfd ? STDIN_FILENO + 1 : sockfd + 1;

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[3]));

	ret = inet_aton(argv[2], &serv_addr.sin_addr);
	DIE(ret == 0, "inet_aton failed");

	// connect to server
	ret = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "connect failed");

	// send id to server
	ret = send(sockfd, argv[1], strlen(argv[1]) + 1, 0);
	DIE(ret < 0, "send id failed");

	int optval = 1;

	// Neagle's algorithm
	setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&optval, sizeof(int));

	while (1)
	{
		tmp_fds = read_fds;
		int rc = select(max_fd, &tmp_fds, NULL, NULL, NULL);
		DIE(rc < 0, "select failed");

		if (FD_ISSET(STDIN_FILENO, &tmp_fds))
		{
			// read from file descriptor
			// se citeste de la stdin
			memset(buffer, 0, sizeof(buffer));
			n = read(0, buffer, sizeof(buffer) - 1);
			DIE(n < 0, "read failed");

			TCPcommand *command = new TCPcommand();
			char *tok = strtok(buffer, " \n");

			if (strcmp(tok, "subscribe") == 0)
			{
				command->type = '0';

				tok = strtok(NULL, " \n");
				if (tok == NULL)
				{
					continue;
				}
				strcpy(command->topic, tok);

				tok = strtok(NULL, " \n");
				if (tok == NULL || (tok[0] != '0' && tok[0] != '1'))
				{
					continue;
				}
				command->sf = tok[0];
				printf("Subscribed to topic.\n");
			}
			else if (strcmp(tok, "unsubscribe") == 0)
			{
				command->type = '1';

				tok = strtok(NULL, " \n");
				if (tok == NULL)
				{
					continue;
				}
				strcpy(command->topic, tok);
				command->topic[strlen(command->topic)] = '\0';
				printf("Unsubscribed from topic.\n");
			}
			else if (strcmp(tok, "exit") == 0)
			{
				command->type = '2';
				n = send(sockfd, command, sizeof(TCPcommand), 0);
				DIE(n < 0, "send command failed");
				break;
			}
			else
			{
				continue;
			}

			// se trimite mesaj la server
			n = send(sockfd, command, sizeof(TCPcommand), 0);
			DIE(n < 0, "send command failed");
			continue;
		}

		if (FD_ISSET(sockfd, &tmp_fds))
		{
			// read from socket
			memset(buffer, 0, sizeof(buffer));
			int rc = recv(sockfd, buffer, 10, 0);
			DIE(rc < 0, "recv failed client");

			if (rc == 0)
			{
				break;
			}

			int sz = atoi(buffer);
			memset(buffer, 0, sizeof(buffer));
			int recvd = 0;

			while (recvd < sz)
			{
				rc = recv(sockfd, buffer, sz, 0);
				DIE(rc < 0, "recv failed client");
				recvd += rc;

				if (rc == 0)
				{
					break;
				}
			}

			TCPmessage *msg = new TCPmessage();
			memcpy(msg, buffer, recvd);

			printf("%s:%hu - %s - %s - %s\n", msg->ip, msg->port,
				   msg->topic, msg->type, msg->payload);
		}
	}

	close(sockfd);
	return 0;
}
