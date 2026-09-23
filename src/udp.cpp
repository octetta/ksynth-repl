#include "udp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdint.h>

#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#include <FL/Fl.H>

extern void my_eval_engine(const char* input, struct hazel_ctx_t* ctx, void* user_data);
extern void trigger_midi_note(int channel, int note, int velocity);
extern void release_midi_note(int channel, int note);

static int cmd_socket = -1;
static int evt_socket = -1;

static void* udp_cmd_thread(void* arg) {
    char buffer[4096];
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    while (1) {
        int n = recvfrom(cmd_socket, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*)&client_addr, &addr_len);
        if (n > 0) {
            buffer[n] = '\0';
            char* cmd_copy = strdup(buffer);
            Fl::awake([](void* data) {
                char* cmd = (char*)data;
                my_eval_engine(cmd, NULL, NULL);
                free(cmd);
            }, cmd_copy);
        }
    }
    return NULL;
}

static void* udp_evt_thread(void* arg) {
    unsigned char buffer[4096];
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    while (1) {
        int n = recvfrom(evt_socket, (char*)buffer, sizeof(buffer), 0, (struct sockaddr*)&client_addr, &addr_len);
        if (n > 0) {
            for (int i = 0; i < n; i += 4) {
                if (i + 3 < n) {
                    unsigned char status = buffer[i];
                    unsigned char data1 = buffer[i + 2];
                    unsigned char data2 = buffer[i + 3];
                    unsigned char channel = buffer[i + 1];
                    if ((status & 0xF0) == 0x90 && data2 > 0) {
                        Fl::awake([](void* data) {
                            intptr_t val = (intptr_t)data;
                            int c = (val >> 16) & 0xFF;
                            int n = (val >> 8) & 0xFF;
                            int v = val & 0xFF;
                            trigger_midi_note(c, n, v);
                        }, (void*)((intptr_t)((channel << 16) | (data1 << 8) | data2)));
                    } else if ((status & 0xF0) == 0x80 || ((status & 0xF0) == 0x90 && data2 == 0)) {
                        Fl::awake([](void* data) {
                            intptr_t val = (intptr_t)data;
                            int c = (val >> 16) & 0xFF;
                            int n = val & 0xFFFF;
                            release_midi_note(c, n);
                        }, (void*)((intptr_t)((channel << 16) | data1)));
                    }
                }
            }
        }
    }
    return NULL;
}

void udp_server_start(int cmd_port, int evt_port) {
#if defined(_WIN32) || defined(_WIN64)
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) return;
#endif

    struct sockaddr_in cmd_addr, evt_addr;

    cmd_socket = socket(AF_INET, SOCK_DGRAM, 0);
    memset(&cmd_addr, 0, sizeof(cmd_addr));
    cmd_addr.sin_family = AF_INET;
    cmd_addr.sin_addr.s_addr = INADDR_ANY;
    cmd_addr.sin_port = htons(cmd_port);
    bind(cmd_socket, (struct sockaddr*)&cmd_addr, sizeof(cmd_addr));

    evt_socket = socket(AF_INET, SOCK_DGRAM, 0);
    memset(&evt_addr, 0, sizeof(evt_addr));
    evt_addr.sin_family = AF_INET;
    evt_addr.sin_addr.s_addr = INADDR_ANY;
    evt_addr.sin_port = htons(evt_port);
    bind(evt_socket, (struct sockaddr*)&evt_addr, sizeof(evt_addr));

    pthread_t t1, t2;
    pthread_create(&t1, NULL, udp_cmd_thread, NULL);
    pthread_create(&t2, NULL, udp_evt_thread, NULL);
}
