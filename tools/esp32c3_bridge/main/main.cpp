/*
 * main.cpp – SDS_110 ESP32-C3-MINI-1-N4: transparente Brücke UART0 <-> WLAN/TCP
 *
 *   STM32F746 USART3 (PD8 TX, PD9 RX) ── UART0 (GPIO20 RX, GPIO21 TX) ── TCP-Server :3333 ── PC-Monitor
 *
 * - Die Bytes gehen unverändert in beide Richtungen; Rahmen, Magic und CRC bleiben wie in
 *   doc/ICD_SDS_PC_Monitor.md. Der STM32 sucht die Kommandos im Bytestrom (CommandAssembler),
 *   der PC-Monitor die Nachrichten über das Magic.
 * - Ein PC zur Zeit. Ein weiterer wird angenommen, sobald der erste getrennt ist; einen PC, der ohne
 *   Abmelden verschwindet, erkennt Keepalive nach etwa 11 s.
 * - Ohne Client werden die Daten des STM32 verworfen (ICD 2: Senden nur bei verbundenem Host).
 * - TCP_NODELAY: kein Nagle, damit das Feedback (Id 8, bis 31/s) nicht verzögert wird.
 * - UDP-Ankündigung alle 2 s (Broadcast, Port CONFIG_BRIDGE_DISCOVERY_PORT), nötig im STA-Betrieb
 *   (Adresse per DHCP) und zugleich Statusanzeige, denn die Logs sind auf dem Board nicht zu sehen:
 *     "SDS110-BRIDGE <ip> <tcp-port> <mac> <version> pc=<0|1> up=<B> down=<B> drop=<B>"
 * - Firmware-Update per WLAN (OTA, Port CONFIG_BRIDGE_OTA_PORT, ota_upload.py): Die USB-Pins
 *   IO18/IO19 sind auf dem Board nicht beschaltet, UART0 hängt am STM32. Eine neue Firmware gilt
 *   erst als gut, wenn das WLAN läuft; sonst startet nach 60 s wieder die alte (Rollback).
 * - Logs nur auf USB-Serial/JTAG (sdkconfig.defaults), nie auf UART0.
 *
 * ESP-IDF ab 5.3.
 */
#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>

#include "driver/uart.h"
#include "esp_app_desc.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

namespace {

constexpr const char* TAG = "bridge";

constexpr uart_port_t UART_PORT   = UART_NUM_0;
constexpr int UART_RX_BUF         = 16384;  // ~160 ms bei 1 Mbaud: überbrückt kurze WLAN-Stockungen
constexpr int UART_TX_BUF         = 4096;
constexpr int CHUNK               = 1024;
constexpr int SEND_TIMEOUT_MS     = 500;    // länger blockiert: Client gilt als tot
constexpr int STATS_INTERVAL_MS   = 10000;

std::mutex        s_clientMutex;            // schützt s_client gegen send/close aus zwei Tasks
int               s_client = -1;
std::atomic<uint32_t> s_bytesToPc{0}, s_bytesToStm{0}, s_bytesDropped{0};
std::atomic<bool>     s_appConfirmed{false};

// ---------------------------------------------------------------- UART0 <-> STM32
void uartInit()
{
    uart_config_t cfg = {};
    cfg.baud_rate  = CONFIG_BRIDGE_UART_BAUD;
    cfg.data_bits  = UART_DATA_8_BITS;
    cfg.parity     = UART_PARITY_DISABLE;
    cfg.stop_bits  = UART_STOP_BITS_1;
    cfg.flow_ctrl  = UART_HW_FLOWCTRL_DISABLE;
    cfg.source_clk = UART_SCLK_DEFAULT;
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT, UART_RX_BUF, UART_TX_BUF, 0, nullptr, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT, CONFIG_BRIDGE_UART_TX_GPIO, CONFIG_BRIDGE_UART_RX_GPIO,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_set_rx_timeout(UART_PORT, 4));    // nach 4 Zeichen Pause Block weitergeben
}

void closeClientLocked()
{
    if (s_client >= 0) {
        shutdown(s_client, SHUT_RDWR);
        close(s_client);
        s_client = -1;
    }
}

// STM32 -> PC
void uartToTcpTask(void*)
{
    static uint8_t buf[CHUNK];
    for (;;) {
        const int n = uart_read_bytes(UART_PORT, buf, sizeof(buf), pdMS_TO_TICKS(5));
        if (n <= 0) continue;
        std::lock_guard<std::mutex> lock(s_clientMutex);
        if (s_client < 0) { s_bytesDropped += n; continue; }
        int off = 0;
        while (off < n) {
            const int w = send(s_client, buf + off, n - off, 0);
            if (w <= 0) {                                  // Timeout oder Fehler: Client verwerfen
                ESP_LOGW(TAG, "send: errno %d, Client getrennt", errno);
                s_bytesDropped += n - off;
                closeClientLocked();
                break;
            }
            off += w;
        }
        s_bytesToPc += off;
    }
}

// PC -> STM32 (ein Client, Lesen ohne Sperre: close nur unter Sperre, recv kehrt dann zurück)
void tcpServerTask(void*)
{
    const int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    const int one = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    sockaddr_in addr = {};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(CONFIG_BRIDGE_TCP_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(listener, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 || listen(listener, 1) != 0) {
        ESP_LOGE(TAG, "bind/listen Port %d: errno %d", CONFIG_BRIDGE_TCP_PORT, errno);
        vTaskDelete(nullptr);
    }
    ESP_LOGI(TAG, "TCP-Server auf Port %d", CONFIG_BRIDGE_TCP_PORT);

    static uint8_t buf[CHUNK];
    for (;;) {
        sockaddr_in peer = {};
        socklen_t   plen = sizeof(peer);
        const int sock = accept(listener, reinterpret_cast<sockaddr*>(&peer), &plen);
        if (sock < 0) { vTaskDelay(pdMS_TO_TICKS(100)); continue; }

        setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
        setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof(one));
        const int idle = 5, intvl = 2, cnt = 3;           // toter PC nach ~11 s erkannt
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof(intvl));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &cnt, sizeof(cnt));
        timeval tv = {SEND_TIMEOUT_MS / 1000, (SEND_TIMEOUT_MS % 1000) * 1000};
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

        {
            std::lock_guard<std::mutex> lock(s_clientMutex);
            closeClientLocked();                           // Rest eines alten Clients (nur zur Sicherheit)
            s_client = sock;
        }
        char ip[16];
        inet_ntoa_r(peer.sin_addr, ip, sizeof(ip));
        ESP_LOGI(TAG, "PC verbunden: %s", ip);
        uart_flush_input(UART_PORT);                       // keine Altdaten an den neuen PC

        // Ein PC ohne FIN fällt über Keepalive bzw. SO_SNDTIMEO (uartToTcpTask) heraus.
        for (;;) {
            const int n = recv(sock, buf, sizeof(buf), 0);
            if (n <= 0) break;
            uart_write_bytes(UART_PORT, buf, n);
            s_bytesToStm += n;
        }
        {
            std::lock_guard<std::mutex> lock(s_clientMutex);
            if (s_client == sock) closeClientLocked();     // sonst schon von uartToTcpTask geschlossen
        }
        ESP_LOGI(TAG, "PC getrennt: %s", ip);
    }
}

// ---------------------------------------------------------------- Ankündigung per UDP
void discoveryTask(void*)
{
    if (CONFIG_BRIDGE_DISCOVERY_PORT == 0) vTaskDelete(nullptr);
    const int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    const int one = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &one, sizeof(one));
    sockaddr_in dst = {};
    dst.sin_family      = AF_INET;
    dst.sin_port        = htons(CONFIG_BRIDGE_DISCOVERY_PORT);
    dst.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
#if CONFIG_BRIDGE_WIFI_AP
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
#else
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
#endif
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_netif_ip_info_t info = {};
        if (esp_netif_get_ip_info(netif, &info) != ESP_OK || info.ip.addr == 0) continue;
        int pc;
        {
            std::lock_guard<std::mutex> lock(s_clientMutex);
            pc = s_client >= 0;
        }
        char msg[192];
        const int n = snprintf(msg, sizeof(msg),
                               "SDS110-BRIDGE " IPSTR " %d %02X%02X%02X%02X%02X%02X %s pc=%d up=%lu down=%lu drop=%lu",
                               IP2STR(&info.ip), CONFIG_BRIDGE_TCP_PORT, mac[0], mac[1], mac[2], mac[3], mac[4],
                               mac[5], esp_app_get_description()->version, pc,
                               static_cast<unsigned long>(s_bytesToPc.load()),
                               static_cast<unsigned long>(s_bytesToStm.load()),
                               static_cast<unsigned long>(s_bytesDropped.load()));
        sendto(sock, msg, n, 0, reinterpret_cast<sockaddr*>(&dst), sizeof(dst));
    }
}

// ---------------------------------------------------------------- Firmware-Update per WLAN
// Protokoll (ota_upload.py): PC sendet die Zeile "OTA <größe> <schlüssel>\n", dann <größe> Byte des
// App-Images (build/sds110_esp32c3_bridge.bin). Antwort "OK\n" und Neustart, sonst "ERR <grund>\n".
void otaReply(int sock, const char* text)
{
    send(sock, text, strlen(text), 0);
    ESP_LOGI(TAG, "OTA: %s", text);
}

void otaSession(int sock)
{
    char line[160] = {};
    int n = 0;
    while (n < static_cast<int>(sizeof(line)) - 1) {          // Kopfzeile byteweise bis '\n'
        if (recv(sock, line + n, 1, 0) != 1) return;
        if (line[n] == '\n') break;
        ++n;
    }
    line[n] = '\0';
    unsigned long size = 0;
    char key[64] = {};
    if (sscanf(line, "OTA %lu %63s", &size, key) != 2) { otaReply(sock, "ERR Kopfzeile\n"); return; }
    if (strcmp(key, CONFIG_BRIDGE_OTA_KEY) != 0) { otaReply(sock, "ERR Schluessel\n"); return; }

    const esp_partition_t* part = esp_ota_get_next_update_partition(nullptr);
    if (part == nullptr || size == 0 || size > part->size) { otaReply(sock, "ERR Groesse/Partition\n"); return; }
    esp_ota_handle_t ota = 0;
    if (esp_ota_begin(part, size, &ota) != ESP_OK) { otaReply(sock, "ERR esp_ota_begin\n"); return; }

    static uint8_t buf[CHUNK];
    unsigned long got = 0;
    while (got < size) {
        const int r = recv(sock, buf, sizeof(buf), 0);
        if (r <= 0) { esp_ota_abort(ota); ESP_LOGW(TAG, "OTA abgebrochen bei %lu B", got); return; }
        if (esp_ota_write(ota, buf, r) != ESP_OK) { esp_ota_abort(ota); otaReply(sock, "ERR esp_ota_write\n"); return; }
        got += r;
    }
    if (esp_ota_end(ota) != ESP_OK) { otaReply(sock, "ERR Image ungueltig\n"); return; }
    if (esp_ota_set_boot_partition(part) != ESP_OK) { otaReply(sock, "ERR Bootpartition\n"); return; }
    otaReply(sock, "OK\n");
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
}

void otaServerTask(void*)
{
    const int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    const int one = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    sockaddr_in addr = {};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(CONFIG_BRIDGE_OTA_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(listener, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 || listen(listener, 1) != 0) {
        ESP_LOGE(TAG, "OTA bind/listen Port %d: errno %d", CONFIG_BRIDGE_OTA_PORT, errno);
        vTaskDelete(nullptr);
    }
    for (;;) {
        const int sock = accept(listener, nullptr, nullptr);
        if (sock < 0) { vTaskDelay(pdMS_TO_TICKS(100)); continue; }
        timeval tv = {10, 0};                                  // hängender PC blockiert nicht ewig
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        otaSession(sock);
        close(sock);
    }
}

// Neue Firmware nach einem OTA erst bestätigen, wenn das WLAN läuft; sonst nach 60 s zurück zur alten
void confirmApp()
{
    if (!s_appConfirmed.exchange(true)) esp_ota_mark_app_valid_cancel_rollback();
}

void rollbackWatchTask(void*)
{
    esp_ota_img_states_t state;
    if (esp_ota_get_state_partition(esp_ota_get_running_partition(), &state) == ESP_OK &&
        state == ESP_OTA_IMG_PENDING_VERIFY) {
        vTaskDelay(pdMS_TO_TICKS(60000));
        if (!s_appConfirmed) {
            ESP_LOGE(TAG, "neue Firmware ohne WLAN – zurück zur vorigen");
            esp_ota_mark_app_invalid_rollback_and_reboot();
        }
    }
    vTaskDelete(nullptr);
}

// ---------------------------------------------------------------- WLAN
void onWifiEvent(void*, esp_event_base_t base, int32_t id, void* data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "WLAN getrennt, neuer Versuch");
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const auto* ev = static_cast<ip_event_got_ip_t*>(data);
        ESP_LOGI(TAG, "WLAN verbunden, Adresse " IPSTR ", Port %d", IP2STR(&ev->ip_info.ip), CONFIG_BRIDGE_TCP_PORT);
        confirmApp();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_AP_START) {
        confirmApp();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_AP_STACONNECTED) {
        ESP_LOGI(TAG, "Gerät im AP angemeldet");
    }
}

void wifiInit()
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, onWifiEvent, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, onWifiEvent, nullptr));

    wifi_config_t cfg = {};
#if CONFIG_BRIDGE_WIFI_AP
    esp_netif_create_default_wifi_ap();
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    const int n = snprintf(reinterpret_cast<char*>(cfg.ap.ssid), sizeof(cfg.ap.ssid), "%s-%02X%02X",
                           CONFIG_BRIDGE_AP_SSID, mac[4], mac[5]);
    cfg.ap.ssid_len = static_cast<uint8_t>(n);
    strlcpy(reinterpret_cast<char*>(cfg.ap.password), CONFIG_BRIDGE_AP_PASSWORD, sizeof(cfg.ap.password));
    cfg.ap.channel        = CONFIG_BRIDGE_AP_CHANNEL;
    cfg.ap.max_connection = 2;
    cfg.ap.authmode       = WIFI_AUTH_WPA2_PSK;
    if (strlen(CONFIG_BRIDGE_AP_PASSWORD) < 8) {
        ESP_LOGE(TAG, "AP-Passwort kürzer als 8 Zeichen – WPA2 nicht möglich, Brücke startet nicht");
        return;
    }
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &cfg));
    ESP_LOGI(TAG, "AP \"%s\", Adresse 192.168.4.1, Port %d", cfg.ap.ssid, CONFIG_BRIDGE_TCP_PORT);
#else
    esp_netif_create_default_wifi_sta();
    strlcpy(reinterpret_cast<char*>(cfg.sta.ssid), CONFIG_BRIDGE_STA_SSID, sizeof(cfg.sta.ssid));
    strlcpy(reinterpret_cast<char*>(cfg.sta.password), CONFIG_BRIDGE_STA_PASSWORD, sizeof(cfg.sta.password));
    cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &cfg));
#endif
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));      // kein Modem-Sleep: Latenz statt Strom
}

} // namespace

extern "C" void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    xTaskCreate(rollbackWatchTask, "rollback", 3072, nullptr, 2, nullptr);
    uartInit();
    wifiInit();

    xTaskCreate(uartToTcpTask, "uart2tcp", 4096, nullptr, 12, nullptr);
    xTaskCreate(tcpServerTask, "tcpsrv", 4096, nullptr, 11, nullptr);
    xTaskCreate(discoveryTask, "discovery", 3072, nullptr, 3, nullptr);
    if (CONFIG_BRIDGE_OTA_PORT != 0) xTaskCreate(otaServerTask, "ota", 4096, nullptr, 5, nullptr);

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(STATS_INTERVAL_MS));
        size_t pending = 0;
        uart_get_buffered_data_len(UART_PORT, &pending);
        ESP_LOGI(TAG, "zum PC %lu B, zum STM32 %lu B, verworfen %lu B, UART-Puffer %u B",
                 static_cast<unsigned long>(s_bytesToPc.load()), static_cast<unsigned long>(s_bytesToStm.load()),
                 static_cast<unsigned long>(s_bytesDropped.load()), static_cast<unsigned>(pending));
    }
}
