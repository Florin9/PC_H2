Dobos Claudiu-Florin 323CD

Am folosit implementarile din laboratorele 6, 7 si 8.

Server.cpp: Scris in C++, pentru implementarile existente de list si map.
    Am o lista de clienti, fiecare client are un ID, un sockfd pentru
a identifica socket-ul pe care se afla, un buffer ce contine toate
mesajele ce vor fi primite de client la urmatoare conectare (in cazul
in care avea vreun topic cu SF=1) si un hashmap intre numele topicului la 
care e abonat si valoarea parametrului SF.
    De asemenea, am o lista cu topicele, fiecare topic are un nume si o lista
de subscribers compusa din referinte la clientii abonati la acel topic.
    Initial deschid 2 socketi, unul pentru TCP, celalalt pentru UDP.
Ma foloses de multiplexare pentru a selecta socket-ul cu activitate, daca
cel activ este cel de TCP, inseamna ca a venit o cerere de conexiune pe 
socket-ul inactiv (cel cu listen) pe care o acceptam. Dupa ce acceptam
conexiunea, primesc de la client infromatiile sale (ID-ul sau) si verific
daca acesta s-a mai conectat pana acum sau nu si ii modific intrarea din 
lista respectiv il adaug in lista.
    Daca socket-ul activ este cel de UDP, atunci a venit o conexiune pe UDP
si receptionez datagrama de la clientul UDP. Daca topic-ul primit in diagrama
nu exista deja, atunci il creez si nu fac nimic cu mesajul, altfel trimit
mesajul mai departe catre toti subscriberii topicului cu ajutorul functiei
sendToALL.
    Daca socket-ul activ este 0, inseamna ca s-a introdus ceva la tastatura,
daca comanda este exit, inchid serverul.
    Daca nu am intrat in nici un caz anterior, inseamna ca am primit date de
la unul din clienti, trebuie sa le receptionez si sa le procesez. Daca conexiunea
s-a inchis marchez clientul ca fiind deconectat prin setarea scokfd la -1.
    La comanda subscribe, verifica daca exista deja acel topic, daca nu exista il
creeaza, daca exista adauga referinta la client la lista de subscriberi ai topicului
si modifica in hashmap-ul clientului valoare SF pentru acest topic.
    La comanda unsubscribe, daca nu exista acel topic afisez la consola faptul ca
topicul nu exista si numele topicului, altfel sterg referinta client din lista de
subscribers si sterg intrarea din hashmap pentru client.

void sendToAll(char *topic, char *value, int type, list<client *> list, int port, struct in_addr IP)

    Aceasta functie trimite tuturor clientilor din list un serverPacket compus din argumentele functiei.
Daca clientul este deconectat, se verifica valoare parametrului SF din hashmap pentru acest topic si se
salveaza in buffer sau nu se face nimic, in functie de acesta.

void sendAllStored(client *client)
    Aceasta functie este apelata la reconectarea clientului si ii trimite acestuia toate mesajele din
topicurile pentru care avea setat SF=1.

Client.c:
    Deschid un socket pentru a comunica cu serverul, ma conectez la server si ii trimit ID-ul meu,
dupa care verific pe care socket am activitate.
    Daca socket-ul activ este cel de TCP, inseamna ca am primit ceva de la server, receptionez
mesajul, daca recv intoarce 0 inseamna ca am pierdut conexiunea cu serverul si voi inchide
si eu clientul, altfel il parsez cu ajutorul void parse_response(serverPacket *packet).
    Daca socket-ul activ nu este cel TCP, inseamna ca am primit o comanda de la tastatura pe
care o voi transforma in mesaj cu ajutorul void compose_message(char *buffer, char *cmd,
char *topic, int sf) si il voi trimite mai departe serverului.
    Clientul afiseaza mesajul READY! dupa realizare conexiunii intre el si server, cand este
gata sa primeasca comenzi.
    Comenzile primite de client de la tastatura sunt de forma:
    
    subscribe topic SF(0/1)
    unsubscribe topic
    exit

void parse_response(serverPacket *packet)

    Este folosita la afisarea mesajului primit de la server, are 5 cazuri, in functie de 
tipul de date pe care il are de afisat: INT, SHORT_REAL, FLOAT, STRING sau altceva.
    Ma folosesc de regulile explicate in enuntul temei, la int si short_real tin cont de 
byte-ul de semn, transform datele din network-order in host-order.
    La float: afisez acelas numar de zecimale cat negativeValue, care este acel uint8_t
din continutul mesajului.
    Daca primesc un alt tip de date decat cele 4, afisez un mesaj de eroare si tipul
de date primit.

    Programul a fost compilat folosind gcc versiunea 7.4.0 (Ubuntu 7.4.0-1ubuntu1~18.04)
si a fost testat pe WSL Ubuntu 18.04. Toate functionalitatile merg normal. A fost
testat cu pana la 10 clienti TCP simultan, merge cu mai multi, in functie de capacitatea
memoriei RAM si a numarului de mesaje de trimis/stocat.
