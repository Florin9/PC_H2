//server
#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <list>
#include <algorithm>
#include <unordered_map>
#include <string>

#define TOPIC_SIZE 51
using namespace std;
typedef struct client {
    char ID[11];
    int sockfd;
    char *buffer;
    int inBuffer;
    unordered_map<string, int> stored;
} client;
typedef struct clientInfo {
    char ID[11];
} clientInfo;
typedef struct serverPacket {
    int length;
    struct in_addr IP;
    int port;
    char topic[51];
    int tip_date;
    char value[1500];
} serverPacket;
typedef struct fromTCP {
    int length;
    char topic[51];
    char cmd[15];
    int SF;
} fromTCP;
typedef struct fromUDP {
    char topic[50];
    uint8_t tip_date;
    char value[1500];
} fromUDP;
typedef struct topicStruct {
    char *topicName;
    std::list<client *> subscribers;
} topicStruct;

void sendToAll(char *topic, char *value, int type, list<client *> list, int port, struct in_addr IP);

void sendAllStored(client *client);

#define BUFLEN 1600
#define MAX_CLIENTS 128
#define DIE(assertion, call_description)    \
    do {                                    \
        if (assertion) {                    \
            fprintf(stderr, "(%s, %d): ",    \
                    __FILE__, __LINE__);    \
            perror(call_description);        \
            exit(EXIT_FAILURE);                \
        }                                    \
    } while(0)

void usage(char *file) {
    fprintf(stderr, "Usage: %s server_port\n", file);
    exit(0);
}

int main(int argc, char *argv[]) {
    std::list<topicStruct> topics;
    std::list<client> clients;
    int sockfd_TCP, sockfd_UDP, newsockfd, portno;
    char buffer[BUFLEN];
    struct sockaddr_in serv_addr, cli_addr;
    struct sockaddr_in from_station;
    int n, i, ret;
    socklen_t clilen;

    fd_set read_fds;    // multimea de citire folosita in select()
    fd_set tmp_fds;        // multime folosita temporar
    int fdmax;            // valoare maxima fd din multimea read_fds

    if (argc < 2) {
        usage(argv[0]);
    }

    // se goleste multimea de descriptori de citire (read_fds) si multimea temporara (tmp_fds)
    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);

    sockfd_TCP = socket(AF_INET, SOCK_STREAM, 0);
    sockfd_UDP = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(sockfd_TCP < 0, "socket");
    DIE(sockfd_UDP < 0, "socket UDP");
    portno = atoi(argv[1]);
    DIE(portno == 0, "atoi");

    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    ret = bind(sockfd_TCP, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr));
    DIE(ret < 0, "bind1");
    ret = bind(sockfd_UDP, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr));
    DIE(ret < 0, "bind2");

    ret = listen(sockfd_TCP, MAX_CLIENTS);
    DIE(ret < 0, "listen");

    // se adauga noul file descriptor (socketul pe care se asculta conexiuni) in multimea read_fds
    FD_SET(sockfd_TCP, &read_fds);
    FD_SET(sockfd_UDP, &read_fds);

    fdmax = sockfd_TCP > sockfd_UDP ? sockfd_TCP : sockfd_UDP;

    FD_SET(STDIN_FILENO, &read_fds);

    int exitReceived = 0;
    while (true) {
        tmp_fds = read_fds;

        ret = select(fdmax + 1, &tmp_fds, nullptr, nullptr, nullptr);
        DIE(ret < 0, "select");

        for (i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &tmp_fds)) {
                if (i == sockfd_TCP) {
                    // a venit o cerere de conexiune pe socketul inactiv (cel cu listen),
                    // pe care serverul o accepta
                    clilen = sizeof(cli_addr);
                    newsockfd = accept(sockfd_TCP, (struct sockaddr *) &cli_addr, &clilen);
                    DIE(newsockfd < 0, "accept");

                    // se adauga noul socket intors de accept() la multimea descriptorilor de citire
                    FD_SET(newsockfd, &read_fds);
                    if (newsockfd > fdmax) {
                        fdmax = newsockfd;
                    }

                    //primesc informatiile de la client
                    memset(buffer, 0, BUFLEN);
                    n = recv(newsockfd, buffer, sizeof(buffer), 0);
                    DIE(n < 0, "recv");
                    clientInfo clientInfo1;
                    memcpy(&clientInfo1, buffer, sizeof(clientInfo1));
                    client newClient;
                    memcpy(newClient.ID, clientInfo1.ID, sizeof(newClient.ID));
                    int recurringClient = 0;
                    for (auto it = clients.begin(); it != clients.end(); ++it) {
                        if (strcmp(it->ID, newClient.ID) == 0) {
                            it->sockfd = newsockfd;
                            recurringClient = 1;
                            sendAllStored(&*it);
                        }
                    }
                    if (recurringClient == 0) {
                        newClient.sockfd = newsockfd;
                        newClient.inBuffer = 0;
                        newClient.buffer = nullptr;
                        clients.push_back(newClient);
                    }
                    printf("New client %s connected from %s:%d.\n", newClient.ID, inet_ntoa(cli_addr.sin_addr),
                           ntohs(cli_addr.sin_port));

                } else if (i == sockfd_UDP) {
                    // a venit conexiune pe UDP,
                    unsigned int size = sizeof(struct sockaddr_in);
                    memset(buffer, 0, BUFLEN);
                    int read = recvfrom(sockfd_UDP, buffer, BUFLEN, 0, (struct sockaddr *) &from_station, &size);
                    if (read <= 0)
                        continue;
                    char *newTopic = (char *) calloc(50, 1);
                    fromUDP a;
                    memcpy(&a, buffer, sizeof(a));

                    memcpy(newTopic, a.topic, sizeof(a.topic));
                    char *value = (char *) malloc(1500);
                    memcpy(value, a.value, sizeof(a.value));
                    int type = 0;
                    memcpy(&type, &a.tip_date, sizeof(a.tip_date));
                    int ok = 0;
                    for (auto it = topics.begin(); it != topics.end(); ++it) {
                        if (strcmp(newTopic, (*it).topicName) == 0) {
                            sendToAll(newTopic, value, type, it->subscribers, from_station.sin_port,
                                      from_station.sin_addr);
                            ok = 1;
                            free(newTopic);
                            newTopic = nullptr;
                            break;
                        }
                    }
                    if (ok == 0) {
                        //create new topic
                        topicStruct tpS;
                        tpS.topicName = newTopic;
                        topics.push_back(tpS);
                    }


                } else if (i == 0) {
                    memset(buffer, 0, BUFLEN);
                    fgets(buffer, BUFLEN - 1, stdin);
                    if (strncmp(buffer, "exit", 4) == 0) {
                        exitReceived = 1;
                        break;
                    }

                } else {
                    // s-au primit date pe unul din socketii de client,
                    // asa ca serverul trebuie sa le receptioneze
                    memset(buffer, 0, BUFLEN);
                    n = recv(i, buffer, BUFLEN, 0);
                    DIE(n < 0, "recv");

                    if (n == 0) {
                        // conexiunea s-a inchis
                        close(i);
                        for (auto it = clients.begin(); it != clients.end(); ++it) {
                            if (it->sockfd == i) {
                                it->sockfd = -1;
                                //scriu la consola
                                printf("Client %s disconnected\n", it->ID);
                                break;
                            }
                        }

                        // se scoate din multimea de citire socketul inchis
                        FD_CLR(i, &read_fds);

                    } else {
                        //abonam/dezabonam
                        fromTCP command;
                        memcpy(&command, buffer, sizeof(command));
                        topicStruct *topic = nullptr;

                        for (auto it2 = topics.begin(); it2 != topics.end(); ++it2) {
                            if (strncmp((*it2).topicName, command.topic, strlen((*it2).topicName)) == 0) {
                                topic = &*it2;
                            }
                        }
                        if (strcmp(command.cmd, "subscribe") == 0) {
                            for (auto it = clients.begin(); it != clients.end(); ++it) {
                                if (it->sockfd == i) {
                                    if (topic == nullptr) {
                                        //create new topic
                                        char *newTopicName = (char *) malloc(TOPIC_SIZE);
                                        memcpy(newTopicName, command.topic, sizeof(command.topic));
                                        topicStruct tpS;
                                        tpS.topicName = newTopicName;
                                        tpS.subscribers.push_back(&*it);
                                        topics.push_back(tpS);
                                        it->stored[newTopicName] = command.SF;
                                    } else {
                                        // adaugam clientul la lista de subscriberi daca nu exista deja
                                        if (!(std::find(topic->subscribers.begin(), topic->subscribers.end(), &*it) !=
                                              topic->subscribers.end())) {
                                            topic->subscribers.push_back(&*it);
                                        }
                                        it->stored[topic->topicName] = command.SF;
                                    }
                                    break;
                                }
                            }
                        } else if (strcmp(command.cmd, "unsubscribe") == 0) {
                            topic = nullptr;
                            for (auto it2 = topics.begin(); it2 != topics.end(); ++it2) {
                                if (strncmp((*it2).topicName, command.topic, strlen((*it2).topicName)) == 0) {
                                    topic = &*it2;
                                }
                            }
                            if (topic == nullptr) {
                                printf("Nu s-a gasit topicul %s\n", command.topic);
                                continue;
                            }
                            for (auto it = clients.begin(); it != clients.end(); ++it) {
                                if (it->sockfd == i) {
                                    topic->subscribers.remove(&*it);
                                    it->stored.erase(topic->topicName);
                                    break;
                                }
                            }

                        } else {
                            printf("Couldn't find command = %s\n", command.cmd);
                        }

                    }
                }
            }
        }
        if (exitReceived) {
            break;
        }
    }


    close(sockfd_TCP);
    close(sockfd_UDP);

    return 0;
}

void sendAllStored(client *client) {
    int i = 0;
    while (i < client->inBuffer) {
        serverPacket pkt;
        memcpy(&pkt, client->buffer + i * sizeof(serverPacket), sizeof(serverPacket));
        send(client->sockfd, &pkt, sizeof(pkt), 0);
        i++;
    }
    client->inBuffer = 0;
    free(client->buffer);
    client->buffer = nullptr;
}

void sendToAll(char *topic, char *value, int type, list<client *> list, int port, struct in_addr IP) {
    serverPacket pkt;
    pkt.tip_date = type;
    memcpy(pkt.value, value, sizeof(pkt.value));
    memcpy(pkt.topic, topic, sizeof(pkt.topic));
    pkt.port = port;
    pkt.IP = IP;

    for (auto it = list.begin(); it != list.end(); ++it) {
        if ((*it)->sockfd == -1) {
            if ((*it)->stored.find(topic) != (*it)->stored.end()) {
                if ((*it)->stored.find(topic)->second == 1) {
                    //store message for *it
                    if ((*it)->buffer == nullptr) {
                        (*it)->buffer = (char *) malloc(sizeof(pkt));
                        memcpy((*it)->buffer, &pkt, sizeof(pkt));
                        (*it)->inBuffer = 1;
                    } else {
                        if (((*it)->inBuffer + 1) * sizeof(pkt) >= sizeof((*it)->buffer)) {
                            (*it)->buffer = (char *) realloc((*it)->buffer, 2 * (*it)->inBuffer * sizeof(pkt));
                        }
                        memcpy((*it)->buffer + (*it)->inBuffer * sizeof(pkt), &pkt, sizeof(pkt));
                        (*it)->inBuffer++;
                    }
                }
            }
        } else {
            int n = send((*it)->sockfd, &pkt, sizeof(pkt), 0);
            if (n < 0) {
                //store message for *it
                if ((*it)->stored.find(topic) != (*it)->stored.end()) {
                    if ((*it)->stored.find(topic)->second == 1) {
                        //store message for *it
                        if ((*it)->buffer == nullptr) {
                            (*it)->buffer = (char *) malloc(sizeof(pkt));
                            memcpy((*it)->buffer, &pkt, sizeof(pkt));
                            (*it)->inBuffer = 1;
                        } else {
                            if (((*it)->inBuffer + 1) * sizeof(pkt) >= sizeof((*it)->buffer)) {
                                (*it)->buffer = (char *) realloc((*it)->buffer, 2 * (*it)->inBuffer * sizeof(pkt));
                            }
                            memcpy((*it)->buffer + (*it)->inBuffer * sizeof(pkt), &pkt, sizeof(pkt));
                            (*it)->inBuffer++;
                        }
                    }
                }
            }
        }
    }

}
