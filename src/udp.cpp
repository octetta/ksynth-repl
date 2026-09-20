#include "udp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <FL/Fl.H>

extern void my_eval_engine(const char* input, struct hazel_ctx_t* ctx, void* user_data);
extern void trigger_midi_note(int note, int velocity);

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
        int n = recvfrom(evt_socket, buffer, sizeof(buffer), 0, (struct sockaddr*)&client_addr, &addr_len);
        if (n > 0) {
            // Very simple Skred UDP event parser (type, channel, data1, data2)
            // type 0x90 = Note On.
            for (int i = 0; i < n; i += 4) {
                if (i + 3 < n) {
                    unsigned char status = buffer[i];
                    unsigned char data1 = buffer[i + 2]; // In Skred, 2 is Note
                    unsigned char data2 = buffer[i + 3]; // 3 is Vel
                    if ((status & 0xF0) == 0x90 && data2 > 0) {
                        Fl::awake([](void* data) {
                            long val = (long)data;
                            int note = (val >> 8) & 0xFF;
                            int vel = val & 0xFF;
                            trigger_midi_note(note, vel);
                        }, (void*)((long)((data1 << 8) | data2)));
                    }
                }
            }
        }
    }
    return NULL;
}

void udp_server_start(int cmd_port, int evt_port) {
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
