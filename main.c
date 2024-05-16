#ifdef _WIN32
    #define _WIN32_WINNT _WIN32_WINNT_WIN7
    #include <winsock2.h> // for all socket programming
    #include <ws2tcpip.h> // for getaddrinfo, inet_pton, inet_ntop
    #include <stdio.h> // for fprintf, perror
    #include <unistd.h> // for close
    #include <stdlib.h> // for exit
    #include <string.h> // for memset
    void OSInit(void) {
        WSADATA wsaData;
        int WSAError = WSAStartup(MAKEWORD(2, 0), &wsaData);
        if (WSAError != 0) {
            fprintf(stderr, "WSAStartup errno = %d\n", WSAError);
            exit(-1);
        }
    }
    void OSCleanup(void) {
        WSACleanup();
    }
    #define perror(string) fprintf(stderr, string ": WSA errno = %d\n", WSAGetLastError())
#else
    #include <sys/socket.h> // for sockaddr, socket, socket
    #include <sys/types.h> // for size_t
    #include <netdb.h> // for getaddrinfo
    #include <netinet/in.h> // for sockaddr_in
    #include <arpa/inet.h> // for htons, htonl, inet_pton, inet_ntop
    #include <errno.h> // for errno
    #include <stdio.h> // for fprintf, perror
    #include <unistd.h> // for close
    #include <stdlib.h> // for exit
    #include <string.h> // for memset
    void OSInit(void) {}
    void OSCleanup(void) {}
#endif

#define debug
//#define infinite

int initialization();
int connection(int internet_socket);
void execution(int internet_socket);
void cleanup(int internet_socket, int client_internet_socket);
char ip_lookup[30];

int main(int argc, char *argv[]) {
    while (1) {
        // Initialization
        OSInit();
        int internet_socket = initialization();
        printf("init\n");

        // Connection
        printf("starting internet socket\n");
        printf("poort 22 staat open:\n");
        int client_internet_socket = connection(internet_socket);

        // Execution
        execution(client_internet_socket);

        // Clean up
        cleanup(internet_socket, client_internet_socket);

        OSCleanup();
    }
    return 0;
}

int initialization() {
    struct addrinfo internet_address_setup;
    struct addrinfo *internet_address_result;
    memset(&internet_address_setup, 0, sizeof internet_address_setup);
    internet_address_setup.ai_family = AF_UNSPEC;
    internet_address_setup.ai_socktype = SOCK_STREAM;
    internet_address_setup.ai_flags = AI_PASSIVE;
    int getaddrinfo_return = getaddrinfo(NULL, "22", &internet_address_setup, &internet_address_result);
    if (getaddrinfo_return != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(getaddrinfo_return));
        exit(1);
    }

    int internet_socket = -1;
    struct addrinfo *internet_address_result_iterator = internet_address_result;
    while (internet_address_result_iterator != NULL) {
        internet_socket = socket(internet_address_result_iterator->ai_family, internet_address_result_iterator->ai_socktype, internet_address_result_iterator->ai_protocol);
        if (internet_socket == -1) {
            perror("socket error");
        } else {
            int bind_return = bind(internet_socket, internet_address_result_iterator->ai_addr, internet_address_result_iterator->ai_addrlen);
            if (bind_return == -1) {
                perror("bind error");
                close(internet_socket);
            } else {
                int listen_return = listen(internet_socket, 1);
                if (listen_return == -1) {
                    close(internet_socket);
                    perror("listen error");
                } else {
                    break;
                }
            }
        }
        internet_address_result_iterator = internet_address_result_iterator->ai_next;
    }

    freeaddrinfo(internet_address_result);

    if (internet_socket == -1) {
        fprintf(stderr, "socket: geen adres gevonden\n");
        exit(2);
    }

    return internet_socket;
}

int connection(int internet_socket) {
    struct sockaddr_storage client_internet_address;
    socklen_t client_internet_address_length = sizeof client_internet_address;
    int client_socket = accept(internet_socket, (struct sockaddr *)&client_internet_address, &client_internet_address_length);
    if (client_socket == -1) {
        perror("accept error");
        close(internet_socket);
        exit(3);
    }

    // Extracting the IP address
    char client_ip[INET6_ADDRSTRLEN]; // This can accommodate both IPv4 and IPv6 addresses
    if (client_internet_address.ss_family == AF_INET) {
        struct sockaddr_in *s = (struct sockaddr_in *)&client_internet_address;
        inet_ntop(AF_INET, &s->sin_addr, client_ip, sizeof client_ip);
    } else if (client_internet_address.ss_family == AF_INET6) {
        struct sockaddr_in6 *s = (struct sockaddr_in6 *)&client_internet_address;
        inet_ntop(AF_INET6, &s->sin6_addr, client_ip, sizeof client_ip);
    } else {
        fprintf(stderr, "geen herkend IPadres\n");
        close(client_socket);
        close(internet_socket);
        exit(4);
    }

    printf("verbonden IPadres: %s\n", client_ip);
    #ifndef debug
    strcpy(ip_lookup, client_ip);
    #else
    strcpy(ip_lookup, "94.110.92.242");
    #endif

    char CLI_buffer[1000];
    snprintf(CLI_buffer, sizeof(CLI_buffer), "curl http://ip-api.com/json/%s?fields=1561", ip_lookup);
    FILE *fp;
    char IP_LOG_ITEM[2000];
    fp = popen(CLI_buffer, "r");
    if (fp == NULL) {
        printf("Error cli\n");
        return client_socket;
    }

    fgets(IP_LOG_ITEM, sizeof(IP_LOG_ITEM) - 1, fp);
    pclose(fp);

    system("clear");
    printf("%s\n", IP_LOG_ITEM);

    FILE *logp;
    logp = fopen("IPLOG.txt", "a");
    if (logp != NULL) {
        if (IP_LOG_ITEM[1] != '}') {
            for (int i = 0; IP_LOG_ITEM[i] != '\0'; i++) {
                if (IP_LOG_ITEM[i] == ',') {
                    fprintf(logp, "\n");
                } else if (IP_LOG_ITEM[i] == ':') {
                    fprintf(logp, ": ");
                } else if (IP_LOG_ITEM[i] != '{' && IP_LOG_ITEM[i] != '}') {
                    fprintf(logp, "%c", IP_LOG_ITEM[i]);
                }
            }
            fprintf(logp, "\n");
        } else {
            fprintf(logp, "LocalHost, No geoloc available\n");
        }
        fclose(logp);
    }

    return client_socket;
}

void execution(int internet_socket) {
    char chartosend[] = 
        "Ha, jij bent vies maar ik doe gemener\n"
        "In de club, kom je moeder tegen\n"
        "\n"
        "En ik wil snel weg want we moeten wegen\n"
        "En je klant is geholpen, je moet vroeger wezen\n"
        "Ik was alles kwijt, maar met floes herenigd\n"
        "Voor me zondes af en toe gebeden\n"
        "\n"
        "Ik ga uit eten voor een goede prijs\n"
        "Ik ben een uitgever, ze boeken mij\n"
        "\n"
        "Van alarm voorzien aan de achterkant\n"
        "Dus ze komen via voor, maar wat dacht je dan?\n"
        "Voor die goldies zet ik brakka's man\n"
        "Ik was op streets en dat nachtenlang\n"
        "\n"
        "Hier zijn de nachten lang, en de dagen kort\n"
        "Ik maak het af of ik maak het op\n"
        "\n"
        "Hoe ze likt aan mijn banaan, maakt me apetrots\n"
        "Word ik vandaag gezocht, dan ben ik morgen weg\n"
        "Ik haal je kluis, niet je zorgen weg\n"
        "Weet nog die dagen ik was onbekend\n"
        "\n"
        "Nu aan de top terwijl je onder bent\n"
        "Ik moest zo vaak racen van de police man\n";

    int sendcount = 0;
    #ifdef infinite
    while (1) {
    #else
    for (int i = 0; i < 1000; i++) {
    #endif
        int number_of_bytes_send = send(internet_socket, chartosend, strlen(chartosend), 0);
        if (number_of_bytes_send == -1) {
            perror("send error");
            break;
        } else {
            sendcount++;
            printf("sent: %s\n", chartosend);
            printf("messages sent: %d\n", sendcount);
        }
    }

    char logbuffer[1000];
    FILE *logp = fopen("IPLOG.txt", "a");
    if (logp != NULL) {
        snprintf(logbuffer, sizeof(logbuffer), "messages sent: %d\n", sendcount);
        fprintf(logp, "%s", logbuffer);
        fclose(logp);
    }
}
