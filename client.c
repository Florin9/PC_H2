#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>

int power(int x, unsigned int y) {
    if (y == 0)
        return 1;
    else if (y % 2 == 0)
        return power(x, y / 2) * power(x, y / 2);
    else
        return x * power(x, y / 2) * power(x, y / 2);
}

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
typedef struct clientInfo {
    char ID[11];
} clientInfo;
#define BUFLEN 1600
#define DIE(assertion, call_description)    \
    do {                                    \
        if (assertion) {                    \
            fprintf(stderr, "(%s, %d): ",    \
                    __FILE__, __LINE__);    \
            perror(call_description);        \
            exit(EXIT_FAILURE);                \
        }                                    \
    } while(0)

void compose_message(char *buffer, char *cmd, char *topic, int sf);

void parse_response(serverPacket *packet);

void usage(char *file) {
    fprintf(stderr, "Usage: %s <ID_Client> <IP_Server> <Port_Server>\n", file);
    exit(0);
}


int main(int argc, char *argv[]) {
    int sockfd, n, ret, ret2;
    struct sockaddr_in serv_addr;
    char buffer[BUFLEN];
    fd_set read_fds;    // multimea de citire folosita in select()
    fd_set tmp_fds;        // multime folosita temporar
    int fdmax;
    if (argc < 4) {
        usage(argv[0]);
    }
    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(sockfd < 0, "socket");
    int flag = 1;
    int result = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
    DIE(result < 0, "Nagle");
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[3]));
    ret = inet_aton(argv[2], &serv_addr.sin_addr);
    DIE(ret == 0, "inet_aton");
    FD_SET(sockfd, &read_fds);
    FD_SET(STDIN_FILENO, &read_fds);
    ret = connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
    DIE(ret < 0, "connect");
    fdmax = sockfd;
    //send client info
    char *ID = argv[1];
    clientInfo clientInfo1;
    memcpy(clientInfo1.ID, ID, strlen(ID) + 1);
    printf("READY!\n");
    n = send(sockfd, &clientInfo1, strlen(clientInfo1.ID), 0);
    DIE(n < 0, "send");

    while (1) {
        tmp_fds = read_fds;
        ret2 = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
        DIE(ret2 < 0, "select");
        for (int i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &tmp_fds)) {
                if (i == sockfd) {
                    memset(buffer, 0, BUFLEN);
                    n = recv(sockfd, buffer, sizeof(serverPacket), 0);
                    if (n == 0) {
                        close(sockfd);
                        close(STDIN_FILENO);
                        return 0;
                    }

                    parse_response((serverPacket *) buffer);
                } else {
                    memset(buffer, 0, BUFLEN);
                    fgets(buffer, BUFLEN - 1, stdin);
                    // se trimite mesaj la server
                    if (strncmp(buffer, "exit", 4) == 0) {
                        break;
                    }
                    char *cmd;
                    char *topic;
                    char *arg2;
                    int SF = 0;
                    cmd = strtok(buffer, " ");
                    topic = strtok(NULL, " ");
                    char *newcmd = malloc(15);
                    memcpy(newcmd, cmd, strlen(cmd) + 1);
                    char *newtopic = malloc(50);
                    memcpy(newtopic, topic, strlen(topic) + 1);
                    if (strcmp(cmd, "subscribe") == 0) {
                        arg2 = strtok(NULL, " ");
                        SF = atoi(arg2);
                    }
                    if ((strcmp(cmd, "subscribe") == 0) || (strcmp(cmd, "unsubscribe") == 0)) {
                        compose_message(buffer, newcmd, newtopic, SF);
                        n = send(sockfd, buffer, sizeof(fromTCP), 0);
                        DIE(n < 0, "send");
                        strcat(newcmd, "d");
                        printf("%s %s\n", newcmd, newtopic);
                    }else {
                        printf("Comanda introdusa nu este valida! Comanda primita=%s\n",cmd);
                    }

                }
            }
        }


    }

    close(sockfd);

    return 0;
}

void parse_response(serverPacket *packet) {
    char *IP = inet_ntoa(packet->IP);
    int port = ntohs(packet->port);
    int number = 0;
    double newNumber = 0;
    double floatNumber = 0;
    uint8_t negativeValue = 0;
    uint32_t floatAux = 0;
    switch (packet->tip_date) {
        case 0:
            memcpy(&number, packet->value + 1, sizeof(int));
            number = ntohl(number);
            if (packet->value[0] == 1) {
                number *= -1;
            }
            printf("%s:%d - %s - INT - %d\n", IP, port, packet->topic, number);
            break;
        case 1:
            memcpy(&number, packet->value, sizeof(uint16_t));
            number = ntohs(number);
            newNumber = (float) number / 100;

            printf("%s:%d - %s - SHORT_REAL - %.2f\n", IP, port, packet->topic, newNumber);
            break;
        case 2:
            memcpy(&floatAux, packet->value + 1, sizeof(uint32_t));
            floatAux = ntohl(floatAux);
            floatNumber = (double) floatAux;
            memcpy(&negativeValue, packet->value + 1 + sizeof(uint32_t), sizeof(uint8_t));
            floatNumber = floatNumber / (power(10, negativeValue));
            if (packet->value[0] == 1) {
                floatNumber *= -1;
            }
            printf("%s:%d - %s - FLOAT - %.*f\n", IP, port, packet->topic, negativeValue, floatNumber);
            break;
        case 3:
            printf("%s:%d - %s - STRING - %s\n", IP, port, packet->topic, packet->value);
            break;
        default:
            printf("Valoare necorespunzatoare tip_date = %d\n", packet->tip_date);
    }

}

void compose_message(char *buffer, char *cmd, char *topic, int sf) {
    fromTCP p1;
    memcpy(p1.topic, topic, 50);
    memcpy(p1.cmd, cmd, 15);
    p1.SF = sf;

    memcpy(buffer, &p1, sizeof(p1));
}