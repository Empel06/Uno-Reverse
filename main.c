#ifdef _WIN32
#define _WIN32_WINNT _WIN32_WINNT_WIN7
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

void OSInit(void) {
    WSADATA wsaData;
    int WSAError = WSAStartup(MAKEWORD(2, 0), &wsaData);
    if (WSAError != 0) {
        fprintf(stderr, "WSAStartup errno = %d\n", WSAError);
        exit(-1);
    }
}
#define perror(string) fprintf(stderr, string ": WSA errno = %d\n", WSAGetLastError())

void OSCleanup(void) {
    WSACleanup();
}

#else
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

void OSInit(void) {}
void OSCleanup(void) {}
#endif

int initialization();
int connection(int internet_socket);
void execution(int client_internet_socket);
void cleanup(int internet_socket, int client_internet_socket);
void log_message(const char *client_ip, int sendcount);
void http_get(const char *client_ip);

int main(int argc, char *argv[]) {
    while (1) {
        // Initialisatie
        OSInit();
        int internet_socket = initialization();
        printf("Init\n");

        // Verbinding
        printf("Starting internet socket\n");
        printf("Port 22 staat open:\n");
        int client_internet_socket = connection(internet_socket);

        // Uitvoering
        execution(client_internet_socket);

        // Opruimen
        cleanup(internet_socket, client_internet_socket);

        OSCleanup();
    }
    return 0;
}

int initialization() {
    // Step 1.1
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
        // Step 1.2
        internet_socket = socket(internet_address_result_iterator->ai_family, internet_address_result_iterator->ai_socktype, internet_address_result_iterator->ai_protocol);
        if (internet_socket == -1) {
            perror("socket error");
        } else {
            // Step 1.3
            int bind_return = bind(internet_socket, internet_address_result_iterator->ai_addr, internet_address_result_iterator->ai_addrlen);
            if (bind_return == -1) {
                perror("bind error");
                close(internet_socket);
            } else {
                // Step 1.4
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
    // Step 2.1
    struct sockaddr_storage client_internet_address;
    socklen_t client_internet_address_length = sizeof(client_internet_address);
    int client_socket = accept(internet_socket, (struct sockaddr *)&client_internet_address, &client_internet_address_length);
    if (client_socket == -1) {
        perror("accept error");
        close(internet_socket);
        exit(3);
    }

    // Finding out the IP address
    char client_ip[INET6_ADDRSTRLEN]; // This can accommodate both IPv4 and IPv6 addresses
    if (client_internet_address.ss_family == AF_INET) {
        struct sockaddr_in *s = (struct sockaddr_in *)&client_internet_address;
        inet_ntop(AF_INET, &(s->sin_addr), client_ip, sizeof(client_ip));
    } else if (client_internet_address.ss_family == AF_INET6) {
        struct sockaddr_in6 *s = (struct sockaddr_in6 *)&client_internet_address;
        inet_ntop(AF_INET6, &(s->sin6_addr), client_ip, sizeof(client_ip));
    } else {
        fprintf(stderr, "Unrecognized IP address family\n");
        close(client_socket);
        close(internet_socket);
        exit(4);
    }

    printf("Connected IP address: %s\n", client_ip);

    // Log the IP address
    FILE *logp = fopen("IPLOG.txt", "a");
    if (logp != NULL) {
        fprintf(logp, "Connected IP address: %s\n", client_ip);
        fclose(logp);
    }

    // Perform HTTP GET to get additional information about the client IP
    http_get(client_ip);

    return client_socket;
}

void* send_data(void* arg) {
    int client_internet_socket = *(int*)arg;
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
        int number_of_bytes_send = send(client_internet_socket, chartosend, strlen(chartosend), 0);
        if (number_of_bytes_send == -1) {
            perror("send error");
            break;
        } else {
            sendcount++;
            printf("sent: %s\n", chartosend);
            printf("messages sent: %d\n", sendcount);
        }
    }

    // Log IP address and number of sent messages
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    getsockname(client_internet_socket, (struct sockaddr *)&addr, &addr_len);
    char ip_address[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(addr.sin_addr), ip_address, INET_ADDRSTRLEN);
    log_message(ip_address, sendcount);

    return NULL;
}

void execution(int client_internet_socket) {
    pthread_t send_thread;
    pthread_create(&send_thread, NULL, send_data, &client_internet_socket);
    pthread_join(send_thread, NULL);
}

void log_message(const char *client_ip, int sendcount) {
    FILE *logp = fopen("IPLOG.txt", "a");
    if (logp != NULL) {
        // Geolocatie kan hier worden toegevoegd met behulp van een geolocatieservice-API
        fprintf(logp, "IP Address: %s, Messages Sent: %d\n", client_ip, sendcount);
        fclose(logp);
    } else {
        fprintf(stderr, "Failed to open IPLOG.txt for writing\n");
    }
}

void cleanup(int internet_socket, int client_internet_socket) {
    close(client_internet_socket);
    close(internet_socket);
}

void http_get(const char *client_ip) {
    int sockfd;
    struct sockaddr_in server_addr;
    char request[256];
    char response[1024];

    // Making a new connection
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        return;
    }

    // Set up server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(80);
    server_addr.sin_addr.s_addr = INADDR_ANY; //server zal luisteren op alle beschikbare netwerkinterfaces

    // Connect to the server
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("connect");
        close(sockfd);
        return;
    }

    // Prepare HTTP request
    snprintf(request, sizeof(request), "GET /json/%s HTTP/1.0\r\nHost: ip-api.com\r\n\r\n", client_ip);

    // Send the HTTP request
    if (send(sockfd, request, strlen(request), 0) == -1) {
        perror("send");
        close(sockfd);
        return;
    }

    // Receive the response and write it to the log file
    FILE *log_file = fopen("log.txt", "a");
    if (log_file == NULL) {
        perror("fopen");
        close(sockfd);
        return;
    }

    while (1) {
        ssize_t bytes_received = recv(sockfd, response, sizeof(response) - 1, 0);
        if (bytes_received == -1) {
            perror("recv");
            break;
        } else if (bytes_received == 0) {
            break;
        }

        response[bytes_received] = '\0';
        fprintf(log_file, "Response from GET request:\n%s\n", response);
    }

    fclose(log_file);
    close(sockfd);
}
