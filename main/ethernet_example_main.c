/* Ethernet Basic Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "ethernet_init.h"
#include "sdkconfig.h"
#include "ping/ping_sock.h"
#include <arpa/inet.h>

#define NODE_ID 2
#define TOT_PING_TGT_NUM 1

static const char *TAG = "eth_example";


void on_ping_success(esp_ping_handle_t hdl, void *args) {
    uint8_t ttl;
    uint16_t seqno;
    uint32_t elapsed_time, recv_len;
    ip_addr_t target_addr;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TTL, &ttl, sizeof(ttl));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    esp_ping_get_profile(hdl, ESP_PING_PROF_SIZE, &recv_len, sizeof(recv_len));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed_time, sizeof(elapsed_time));
    printf("%lu bytes from %s icmp_seq=%d ttl=%d time=%lu ms\n",
           recv_len, inet_ntoa(target_addr.u_addr.ip4), seqno, ttl, elapsed_time);
}


void on_ping_timeout(esp_ping_handle_t hdl, void *args) {
    uint16_t seqno;
    ip_addr_t target_addr;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    printf("From %s icmp_seq=%d timeout\n", inet_ntoa(target_addr.u_addr.ip4), seqno);

//    if(++ping_fail_cnt >= 2) {
//        ping_fail_cnt = 0;
//        esp_ping_stop(ping_session);
//        esp_ping_start(ping_session);
//    }
}
void on_ping_end(esp_ping_handle_t hdl, void *args) {
    uint32_t transmitted;
    uint32_t received;
    uint32_t total_time_ms;

    esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, &transmitted, sizeof(transmitted));
    esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &received, sizeof(received));
    esp_ping_get_profile(hdl, ESP_PING_PROF_DURATION, &total_time_ms, sizeof(total_time_ms));
    printf("%lu packets transmitted, %lu received, time %lums\n", transmitted, received, total_time_ms);
}



static void init_ping(void* session_arr) {
    esp_ping_handle_t *ping_session_arr = (esp_ping_handle_t *)session_arr;

    ip_addr_t ping_target = {
            .type = 0
    };
    esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
    ping_config.count = ESP_PING_COUNT_INFINITE;

    esp_ping_callbacks_t cbs;
    cbs.on_ping_success = on_ping_success;
    cbs.on_ping_timeout = on_ping_timeout;
    cbs.on_ping_end = on_ping_end;
    cbs.cb_args = NULL;

    for(uint8_t i = 0; i < TOT_PING_TGT_NUM; ++i) {
        esp_netif_set_ip4_addr(&ping_target, 192, 168, 1, i+2);
        ping_config.target_addr = ping_target;
        esp_ping_new_session(&ping_config, &cbs, &ping_session_arr[i]);
    }

}

/** Event handler for Ethernet events */
static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    uint8_t mac_addr[6] = {0};
    /* we can get the ethernet driver handle from event data */
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
        ESP_LOGI(TAG, "Ethernet Link Up");
        ESP_LOGI(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Ethernet Link Down");
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "Ethernet Started");
        break;
    case ETHERNET_EVENT_STOP:
        ESP_LOGI(TAG, "Ethernet Stopped");
        break;
    default:
        break;
    }
}

/** Event handler for IP_EVENT_ETH_GOT_IP */
static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;

    ESP_LOGI(TAG, "Ethernet Got IP Address");
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    ESP_LOGI(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
    ESP_LOGI(TAG, "~~~~~~~~~~~");

    if(NODE_ID == 1) {
        ESP_LOGI(TAG, "Starting ping sessions");
        esp_ping_handle_t *ping_session_arr = (esp_ping_handle_t *)arg;
        for(uint8_t i = 0; i < TOT_PING_TGT_NUM; ++i) {
            esp_ping_start(ping_session_arr[i]);
        }
    }
}

/** Event handler for IP_EVENT_ETH_LOST_IP **/
static void lost_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data) {
    ESP_LOGI(TAG, "Ethernet Lost IP Address");
    ESP_LOGI(TAG, "Stopping ping session");
    esp_ping_handle_t *ping_session_arr = (esp_ping_handle_t *)arg;
    for(uint8_t i = 0; i < TOT_PING_TGT_NUM; ++i) {
        esp_ping_stop(ping_session_arr[i]);
    }
}


void app_main(void)
{
    // Initialize Ethernet driver
    uint8_t eth_port_cnt = 0;
    esp_eth_handle_t *eth_handles;
    ESP_ERROR_CHECK(example_eth_init(&eth_handles, &eth_port_cnt));

    // Initialize TCP/IP network interface aka the esp-netif (should be called only once in application)
    ESP_ERROR_CHECK(esp_netif_init());
    // Create default event loop that running in background
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create instance(s) of esp-netif for Ethernet(s)
    esp_netif_t *eth_netif = NULL;
    if (eth_port_cnt == 1) {
        // Use ESP_NETIF_DEFAULT_ETH when just one Ethernet interface is used and you don't need to modify
        // default esp-netif configuration parameters.
        esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
        eth_netif = esp_netif_new(&cfg);
        // Attach Ethernet driver to TCP/IP stack
        ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handles[0])));
    } else {
        // Use ESP_NETIF_INHERENT_DEFAULT_ETH when multiple Ethernet interfaces are used and so you need to modify
        // esp-netif configuration parameters for each interface (name, priority, etc.).
        esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_ETH();
        esp_netif_config_t cfg_spi = {
            .base = &esp_netif_config,
            .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH
        };
        char if_key_str[10];
        char if_desc_str[10];
        char num_str[3];
        for (int i = 0; i < eth_port_cnt; i++) {
            itoa(i, num_str, 10);
            strcat(strcpy(if_key_str, "ETH_"), num_str);
            strcat(strcpy(if_desc_str, "eth"), num_str);
            esp_netif_config.if_key = if_key_str;
            esp_netif_config.if_desc = if_desc_str;
            esp_netif_config.route_prio -= i*5;
            eth_netif = esp_netif_new(&cfg_spi);

            // Attach Ethernet driver to TCP/IP stack
            ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handles[i])));
        }
    }


    // Stop dhcp client
    ESP_ERROR_CHECK(esp_netif_dhcpc_stop(eth_netif));
    // Set static ip address
    esp_ip4_addr_t addr, gw, netmask;
    switch (NODE_ID) {
        case 1:
            esp_netif_set_ip4_addr(&addr, 192, 168, 1, 1);
            break;
        case 2:
            esp_netif_set_ip4_addr(&addr, 192, 168, 1, 2);
            break;
        case 3:
            esp_netif_set_ip4_addr(&addr, 192, 168, 1, 3);
            break;
        case 4:
            esp_netif_set_ip4_addr(&addr, 192, 168, 1, 4);
            break;
        default:
            esp_netif_set_ip4_addr(&addr, 192, 168, 1, 1);
            break;
    }
    esp_netif_set_ip4_addr(&gw, 192, 168, 1, 1);
    esp_netif_set_ip4_addr(&netmask, 255, 255, 255, 0);
    esp_netif_ip_info_t ip_info = {
            .ip = addr,
            .gw = gw,
            .netmask = netmask
    };
    printf("Set static ip addr");
    ESP_ERROR_CHECK(esp_netif_set_ip_info(eth_netif, &ip_info));


    // Register user defined event handers
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    if(NODE_ID != 1) {
        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));
    } else {
        // Init ICMP
        esp_ping_handle_t ping_sessions[TOT_PING_TGT_NUM];
        init_ping(ping_sessions);

        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, ping_sessions));
        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_LOST_IP, &lost_ip_event_handler, ping_sessions));
    }

    // Start Ethernet driver state machine
    for (int i = 0; i < eth_port_cnt; i++) {
        ESP_ERROR_CHECK(esp_eth_start(eth_handles[i]));
    }

}
