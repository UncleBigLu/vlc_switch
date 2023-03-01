#include "mytcp.h"
#include "sdkconfig.h"
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <netdb.h>            // struct addrinfo
#include <arpa/inet.h>
#include "esp_netif.h"
#include "esp_log.h"

static const char *TAG = "mytcp";

static int connect_to_server(void *dest_addr) {
    // dns lookup
    struct addrinfo hints;
    struct addrinfo *servinfo;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if(getaddrinfo((char*)dest_addr, SERVER_PORT, &hints, &servinfo) != 0)
    {
        ESP_LOGE(TAG, "Get addr info failed");
        return -1;
    }
    // servinfo now points to a linkf 1 or more struct addrinfos
    struct addrinfo *p;
    int sockfd = -1;
    bool is_sock_created = false;
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if((sockfd = socket(p->ai_family, p->ai_socktype,
                            p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }
        is_sock_created = true;
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            perror("client: connect");
            continue;
        }
        break;
    }
    freeaddrinfo(servinfo);
    if(p == NULL) {
        ESP_LOGE(TAG, "client: failed to connect");
        if(is_sock_created)
            close(sockfd);
        return -1;
    }
    return sockfd;
}

void vtcp_client_task(void *pvParameters) {
    tcp_client_param_t *param = (tcp_client_param_t*)pvParameters;
    int sock;
    while(1) {
        if((sock = connect_to_server(param->dest_addr)) < 0) {
            ESP_LOGE(TAG, "sleep 1s before retry connect...");
            vTaskDelay(1000/portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "Successfully connected");

        uint8_t buf[ADC_READ_LEN] = {0};
        memset(buf, 0xcc, ADC_READ_LEN);
        uint32_t actual_read_num = 0;

        while (1) {
            // Block here until adc data avialable
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

            ESP_ERROR_CHECK(adc_continuous_read(param->adc_handle, buf, ADC_READ_LEN, &actual_read_num, 0));
            int actual_send = send(sock, buf, actual_read_num, 0);
            while (actual_send < actual_read_num) {
                if (actual_send < 0) {
                    ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                    break;
                }
                actual_send = send(sock, (buf+actual_send), actual_read_num - actual_send, 0);
            }
        }
        if (sock != -1) {
            ESP_LOGE(TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
    }
    vTaskDelete(NULL);
}

void tcp_server() {
    int keepAlive = 1;
    int keepIdle = 5;
    int keepInterval = 5;
    int keepCount = 5;

    struct sockaddr_in bind_addr;
    bind_addr.sin_addr.s_addr = inet_addr("192.168.1.1");
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(23333);

    while (1) {
        int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            break;
        }
        ESP_LOGI(TAG, "socket created.\n");
        const int opt = 1;
        if(setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int)) < 0) {
            ESP_LOGE(TAG, "setsockopt failed, errno: %d\n", errno);
            close(listen_sock);
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "set socket reuse address.\n");

        if(bind(listen_sock,(struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
            ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
            close(listen_sock);
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "bind success.\n");
        if (listen(listen_sock, 1) < 0 ) {
            ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
            close(listen_sock);
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            continue;
        }
        fd_set tmp_fds;
        fd_set read_fds;
        int fdmax;
        FD_ZERO(&read_fds);
        FD_SET(listen_sock, &read_fds);
        fdmax = listen_sock;
        int newsock, recv_bytes;
        char recvbuf[1024];
        char addr_str[128];
        while (1) {
            tmp_fds = read_fds;
//            ESP_LOGI(TAG, "Socket listening");
            struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
            socklen_t addr_len = sizeof(source_addr);

            if(select(fdmax+1, &tmp_fds, NULL, NULL, NULL) == -1) {
                ESP_LOGE(TAG, "select error\n");
                break;
            }

            for(int i = 0; i <= fdmax; ++i) {
                if(FD_ISSET(i, &tmp_fds)) {
                    if(i == listen_sock) {
                        newsock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
                        if(newsock < 0) {
                            ESP_LOGE(TAG, "accept error\n");
                            break;
                        } else {

                            if (source_addr.ss_family == PF_INET) {
                                inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
                            }
                            ESP_LOGI(TAG, "Socket accepted ip address: %s", addr_str);
                            FD_SET(newsock, &read_fds);
                            if(newsock > fdmax)
                                fdmax = newsock;
                            // Set tcp keepalive option
                            setsockopt(newsock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
                            setsockopt(newsock, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
                            setsockopt(newsock, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
                            setsockopt(newsock, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));
                        }
                    } else {
                        if((recv_bytes = recv(i, recvbuf, sizeof recvbuf, 0)) <= 0) {
                            if(recv_bytes == 0) {
                                ESP_LOGW(TAG, "connection %d closed.\n", i);
                            } else {
                                ESP_LOGE(TAG, "Recv error.\n");
                            }
                            close(i);
                            FD_CLR(i, &read_fds);
                        } else {
                            // Got data;
                            recvbuf[recv_bytes] = '\0';
                            puts(recvbuf);
                        }
                    }
                }
            }

        }
        close(listen_sock);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }

}