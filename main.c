#ifdef _WIN32
#define _WIN32_WINNT _WIN32_WINNT_WIN7
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

int total_bytes_sent = 0;

void OSInit(void) {
    WSADATA wsaData;
    int WSAError = WSAStartup(MAKEWORD(2, 0), &wsaData);
    if (WSAError != 0) {
        fprintf(stderr, "WSAStartup errno = %d\n", WSAError);
        exit(-1);
    }
}
#define perror(string) fprintf(stderr, string ": WSA errno = %d\n", WSAGetLastError())
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
void http_get(const char* ip_address);
void* send_lyrics(void* arg);
void* handle_client(void* arg);
void log_geolocation(const char* json_response);

int main(int argc, char* argv[]) {
    printf("Starting program\n");
    OSInit();
    int internet_socket = initialization();

    while (1) {
        int* client_internet_socket = malloc(sizeof(int));
        *client_internet_socket = connection(internet_socket);

        pthread_t client_thread;
        if (pthread_create(&client_thread, NULL, handle_client, client_internet_socket) != 0) {
            perror("pthread_create");
            close(*client_internet_socket);
            free(client_internet_socket);
        }
        pthread_detach(client_thread);
    }

    return 0;
}

int initialization() {
    //Step 1.1
    struct addrinfo internet_address_setup;
    struct addrinfo* internet_address_result;
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
    struct addrinfo* internet_address_result_iterator = internet_address_result;
    while (internet_address_result_iterator != NULL) {
        //Step 1.2
        internet_socket = socket(internet_address_result_iterator->ai_family, internet_address_result_iterator->ai_socktype, internet_address_result_iterator->ai_protocol);
        if (internet_socket == -1) {
            perror("socket");
        }
        else {
            //Step 1.3
            int bind_return = bind(internet_socket, internet_address_result_iterator->ai_addr, internet_address_result_iterator->ai_addrlen);
            if (bind_return == -1) {
                perror("bind");
                close(internet_socket);
            }
            else {
                //Step 1.4
                int listen_return = listen(internet_socket, SOMAXCONN);
                if (listen_return == -1) {
                    close(internet_socket);
                    perror("listen");
                }
                else {
                    break;
                }
            }
        }
        internet_address_result_iterator = internet_address_result_iterator->ai_next;
    }
    freeaddrinfo(internet_address_result);

    if (internet_socket == -1) {
        fprintf(stderr, "socket: no valid socket address found\n");
        exit(2);
    }

    return internet_socket;
}

int connection(int internet_socket) {
    printf("Starting server on port 22\nPort 22 is open:\n");
    struct sockaddr_storage client_internet_address;
    socklen_t client_internet_address_length = sizeof(client_internet_address);
    int client_socket = accept(internet_socket, (struct sockaddr*)&client_internet_address, &client_internet_address_length);
    if (client_socket == -1) {
        perror("accept");
        close(internet_socket);
        exit(3);
    }

    void* addr;
    if (client_internet_address.ss_family == AF_INET) {
        struct sockaddr_in* s = (struct sockaddr_in*)&client_internet_address;
        addr = &(s->sin_addr);
    }
    else {
        struct sockaddr_in6* s = (struct sockaddr_in6*)&client_internet_address;
        addr = &(s->sin6_addr);
    }

    char ip_address[INET6_ADDRSTRLEN];
    inet_ntop(client_internet_address.ss_family, addr, ip_address, sizeof(ip_address));

    // Save IP address
    FILE* log_file = fopen("IPLOG.txt", "a");
    if (log_file == NULL) {
        perror("fopen");
        close(client_socket);
        exit(4);
    }
    fprintf(log_file, "------------------------\n");
    fprintf(log_file, "Connection from %s\n", ip_address);
    fclose(log_file);

    return client_socket;
}

void log_geolocation(const char* json_response) {
    // Parse the JSON response to extract relevant information
    // In dit voorbeeld worden alleen de land- en stadsinformatie uit de JSON-reactie geëxtraheerd.
    // Je zou de parsing moeten aanpassen aan de structuur van de JSON-reactie van de IP-API.
    const char* country = "Country: ";
    const char* city = "City: ";
    char* country_start = strstr(json_response, country);
    char* city_start = strstr(json_response, city);

    if (country_start && city_start) {
        country_start += strlen(country);
        city_start += strlen(city);

        // Vind het einde van de land- en stadssnaar door naar het volgende komma- of accoladesymbool te zoeken
        char* country_end = strpbrk(country_start, ",}");
        char* city_end = strpbrk(city_start, ",}");

        if (country_end && city_end) {
            // Kopieer het land en de stad naar nieuwe buffers
            size_t country_len = country_end - country_start;
            size_t city_len = city_end - city_start;
            char country_buffer[country_len + 1];
            char city_buffer[city_len + 1];
            strncpy(country_buffer, country_start, country_len);
            strncpy(city_buffer, city_start, city_len);
            country_buffer[country_len] = '\0';
            city_buffer[city_len] = '\0';

            // Voeg de geolocatie-informatie toe aan de logs
            printf("Geolocation: %s, %s\n", country_buffer, city_buffer);
        }
    }
}

void http_get(const char* ip_address) {
    int sockfd;
    struct addrinfo hints, *res, *p;
    char request[256];
    char response[1024];
    char* json_response = NULL; // Store the JSON response

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    // Resolve the IP address of ip-api.com
    int status = getaddrinfo("ip-api.com", "80", &hints, &res);
    if (status != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return;
    }

    // Loop through all the results and connect to the first we can
    for (p = res; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("socket");
            continue;
        }
                if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            perror("connect");
            close(sockfd);
            continue;
        }

        break; // if we get here, we must have connected successfully
    }

    if (p == NULL) {
        fprintf(stderr, "failed to connect\n");
        freeaddrinfo(res);
        return;
    }

    freeaddrinfo(res); // all done with this structure

    // Prepare HTTP request
    snprintf(request, sizeof(request), "GET /json/%s HTTP/1.0\r\nHost: ip-api.com\r\n\r\n", ip_address);

    // Send the HTTP request
    if (send(sockfd, request, strlen(request), 0) == -1) {
        perror("send");
        close(sockfd);
        return;
    }

    // Receive the HTTP response
    int total_bytes_received = 0;
    while (1) {
        ssize_t bytes_received = recv(sockfd, response, sizeof(response) - 1, 0);
        if (bytes_received == -1) {
            perror("recv");
            break;
        } else if (bytes_received == 0) {
            break;
        }

        response[bytes_received] = '\0';
        total_bytes_received += bytes_received;

        // Write the received response to the log file
        FILE* file = fopen("IPLOG.txt", "a");
        if (file == NULL) {
            perror("fopen");
            close(sockfd);
            return;
        }
        fprintf(file, "%s", response);
        fclose(file);

        // Log geolocation
        log_geolocation(response);
    }

    // Close the connection
    close(sockfd);
}

void* send_lyrics(void* arg) {
    int client_internet_socket = *(int*)arg;
    const char* lyrics = "Ha, jij bent vies maar ik doe gemener\n"
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

    printf("\nStarted Attack\n");
    while (1) {
        int bytes_sent = send(client_internet_socket, lyrics, strlen(lyrics), 0);
        if (bytes_sent == -1) {
            perror("send");
            break;
        }
        usleep(200000);  // Sleep for 200 milliseconds between sends
        total_bytes_sent += bytes_sent;
    }
    printf("\nFinished Attack\n");

    return NULL;
}

void execution(int client_internet_socket) {
    // Step 1: Receive initial data
    printf("\nExecution Start!\n");

    // Get IP address
    struct sockaddr_storage addr;
    socklen_t addr_size = sizeof(addr);
    char ip_address[INET6_ADDRSTRLEN];
    getpeername(client_internet_socket, (struct sockaddr*)&addr, &addr_size);
    inet_ntop(addr.ss_family, &((struct sockaddr_in*)&addr)->sin_addr, ip_address, sizeof(ip_address));

    // Perform HTTP GET request with client's IP address
    http_get(ip_address);

    char buffer[1000];

    // Create a new thread to send lyrics
    pthread_t send_thread;
    pthread_create(&send_thread, NULL, send_lyrics, &client_internet_socket);

    // Seeing what the client sends
    while (1) {
        int number_of_bytes_received = recv(client_internet_socket, buffer, sizeof(buffer) - 1, 0);
        if (number_of_bytes_received == -1) {
            perror("recv");
            break;
        }
        else if (number_of_bytes_received == 0) {
            // System crashed or he leaves
            printf("Client closed the connection.\n");
            break;
        }

        buffer[number_of_bytes_received] = '\0';
        printf("Received: %s\n", buffer);

        // Write the received message in the log file
        FILE* log_file = fopen("IPLOG.txt", "a");
        if (log_file == NULL) {
            perror("fopen");
            break;
        }
        fprintf(log_file, "Message from client: %s\n", buffer);
        fclose(log_file);
    }

    // Wait for the send thread to finish
    pthread_join(send_thread, NULL);

    // Log and print the total number of bytes delivered successfully
    FILE* log_file = fopen("IPLOG.txt", "a");
    if (log_file == NULL) {
        perror("fopen");
        close(client_internet_socket);
        exit(4);
    }
    fprintf(log_file, "Total bytes delivered: %d\n", total_bytes_sent);
    fprintf(log_file, "------------------------\n");
    fclose(log_file);
    printf("------------------------\n");
    printf("Total bytes delivered: %d\n", total_bytes_sent);
    printf("------------------------\n");

    // Close the client connection
    close(client_internet_socket);
}

void* handle_client(void* arg) {
    int client_internet_socket = *(int*)arg;
    free(arg); // Free the allocated memory for the client socket
    execution(client_internet_socket);
    close(client_internet_socket);
    return NULL;
}
