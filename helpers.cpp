#include "helpers.h"

void printSubscriptions(std::vector<topic> &topics)
{
    for (auto i : topics)
    {
        printf("CONTENT %s\n", i.content);
        printf("SUBSCRIBERS ");

        for (auto j = i.subscribers.begin(); j != i.subscribers.end(); j++)
        {
            printf("%s - %d | ", (*j).cli->id, (*j).sf);
        }
        printf("\n");
    }
    printf("\n");
}

void printClients(std::vector<client> &clients)
{
    printf("CLIENTS are:\n");

    for (auto &i : clients)
    {
        printf("%s, who is %d connected\n", i.id, i.isConnected);
    }
    printf("\n");
}

int clientAlreadyExisting(std::vector<client> &clients, char *id)
{
    for (int i = 0; i < clients.size(); i++)
    {
        if (strncmp(clients[i].id, id, 11) == 0)
        {
            return i;
        }
    }
    return -1;
}

void newClient(std::vector<client> &clients, char *id, int sock)
{
    client newClient;
    strncpy(newClient.id, id, 11);
    newClient.socket = sock;
    newClient.isConnected = true;

    clients.push_back(newClient);
}

void reconnect(client &cli, int sock)
{
    cli.isConnected = true;
    cli.socket = sock;

    while (!cli.contentNotSent.empty())
    {
        printf("am de recuperat mesaje\n");
        TCPmessage *msg = &cli.contentNotSent.front();
        send(sock, msg->size, 10, 0);
        send(sock, msg, atoi(msg->size), 0);
        cli.contentNotSent.pop();
    }
}

void disconnect(std::vector<client> &clients, int sock)
{
    for (auto &i : clients)
    {
        if (i.socket == sock)
        {
            i.isConnected = false;
            close(sock);
            printf("Client was disconected\n");
            return;
        }
    }
    printf("Client to disconnect not found!\n");
}

int getTopicByContent(std::vector<topic> topics, char *topicContent)
{
    for (auto i = 0; i < topics.size(); i++)
    {
        if (strcmp(topics[i].content, topicContent) == 0)
        {
            return i;
        }
    }
    return -1;
}

void convertAndSendTCPmessage(std::vector<topic> &topics, char *buffer, struct sockaddr_in udp_addr)
{
    UDPmessage *udpMessage = new UDPmessage();
    memcpy(udpMessage, buffer, sizeof(UDPmessage));

    TCPmessage *tcpMessage = new TCPmessage();
    strcpy(tcpMessage->topic, udpMessage->topic);
    printf("TOPIC: %s\n", tcpMessage->topic);

    strcpy(tcpMessage->ip, inet_ntoa(udp_addr.sin_addr));
    tcpMessage->port = ntohs(udp_addr.sin_port);

    printf("TYPE: %d\n", udpMessage->type);

    switch (udpMessage->type)
    {
    case 0:
    {
        strcpy(tcpMessage->type, "INT");
        int sign = udpMessage->payload[0];

        long recvNo = ntohl(*(uint32_t *)(&udpMessage->payload[1]));

        // positive no.
        if (sign == 1)
        {
            recvNo *= -1;
        }
        else if (sign != 0)
        {
            DIE(1, "wrong data type");
        }
        printf("payload: %ld\n", recvNo);

        sprintf(tcpMessage->payload, "%ld", recvNo);
        break;
    }
    case 1:
    {
        strcpy(tcpMessage->type, "SHORT_REAL");
        double recvNo = ntohs(*(uint16_t *)(udpMessage->payload)) / 100.f;
        sprintf(tcpMessage->payload, "%.2f", recvNo);
        printf("payload: %.2f\n", recvNo);
        break;
    }
    case 2:
    {
        strcpy(tcpMessage->type, "FLOAT");
        double recvNo = (double)ntohl(*(uint32_t *)&udpMessage->payload[1]);

        int sign = udpMessage->payload[0];

        // positive no.
        if (sign == 1)
        {
            recvNo *= -1.f;
        }
        else if (sign != 0)
        {
            DIE(1, "wrong data type");
        }

        int power = udpMessage->payload[5];
        recvNo /= pow(10, power);
        printf("payload: %lf\n", recvNo);
        sprintf(tcpMessage->payload, "%lf", recvNo);

        break;
    }
    case 3:
    {
        strcpy(tcpMessage->type, "STRING");
        strcpy(tcpMessage->payload, udpMessage->payload);
        printf("payload: %s\n", tcpMessage->payload);
        break;
    }
    default:
        printf("wrong type of packet: %c ", udpMessage->type);
        return;
    }

    sprintf(tcpMessage->size, "%ld", sizeof(TCPmessage) - 1501 + strlen(tcpMessage->payload));

    // send tcp to all subscribers
    int pos = getTopicByContent(topics, tcpMessage->topic);

    if (pos == -1)
    {
        printf("Topic is yet to be registered\n");
        return;
    }
    struct topic top = topics[pos];

    for (auto j = top.subscribers.begin(); j != top.subscribers.end(); j++)
    {
        // if connected, just send the message
        if ((*j).cli->isConnected == true)
        {
            send((*j).cli->socket, tcpMessage->size, 10, 0);
            send((*j).cli->socket, tcpMessage, atoi(tcpMessage->size), 0);
        }
        // if not connected and wants to be kept up to date, save message for later
        else if ((*j).sf == 1)
        {
            printf("%s is not online, stack messages.\n", (*j).cli->id);
            (*j).cli->contentNotSent.push(*tcpMessage);
        }
    }
}

void unsubscribe(std::vector<topic> &topics, int sock, char *topicContent)
{
    int pos = getTopicByContent(topics, topicContent);

    if (pos != -1)
    {
        topic &top = topics[pos];
        for (auto j = top.subscribers.begin(); j != top.subscribers.end(); j++)
        {
            if ((*j).cli->socket == sock)
            {
                topics[pos].subscribers.erase(j);
                printf("Client was unsubscribed\n");
                printSubscriptions(topics);

                return;
            }
        }
        printf("Subscriber not found!\n");
    }
    else
    {
        printf("Topic not found!\n");
    }
}

void subscribe(std::vector<topic> &topics, struct TCPcommand *proto, client &cli)
{
    subscriber newSubscriber;
    newSubscriber.cli = &cli;
    newSubscriber.sf = proto->sf - 48;

    printf("subscribe request received\n");

    int pos = getTopicByContent(topics, proto->topic);

    // if topic exists, just add the new subscriber
    if (pos != -1)
    {
        for (auto j = topics[pos].subscribers.begin(); j != topics[pos].subscribers.end(); j++)
        {
            if (strcmp(j->cli->id, cli.id) == 0)
            {
                printf("Subscriber already existing. Will update the sf preference.\n");
                j->sf = proto->sf - 48;
                printSubscriptions(topics);
                return;
            }
        }
        topics[pos].subscribers.push_back(newSubscriber);
    }
    else
    {
        // if not already in the list, create a new topic
        topic newTopic;
        newTopic.subscribers.push_back(newSubscriber);

        strcpy(newTopic.content, proto->topic);
        topics.push_back(newTopic);
    }
    printSubscriptions(topics);
}