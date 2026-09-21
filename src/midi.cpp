#define MINIMIDIO_IMPLEMENTATION
#include "../third_party/minimidio/minimidio.h"
#include <FL/Fl.H>

extern void trigger_midi_note(int channel, int note, int velocity);
extern void release_midi_note(int channel, int note);

void my_midi_callback(mm_device* dev, const mm_message* msg, void* userdata) {
    if (msg->type == MM_NOTE_ON && msg->data[1] > 0) {
        int note = msg->data[0];
        int vel = msg->data[1];
        int ch = msg->channel;
        Fl::awake([](void* data) {
            long val = (long)data;
            int c = (val >> 16) & 0xFF;
            int n = (val >> 8) & 0xFF;
            int v = val & 0xFF;
            trigger_midi_note(c, n, v);
        }, (void*)((long)((ch << 16) | (note << 8) | vel)));
    }
}

static mm_context midi_ctx;
static mm_device midi_in_hw;
static mm_device midi_in_virt;

void midi_init(const char* port_name) {
    mm_context_init(&midi_ctx, port_name ? port_name : "ksynth-repl");
    if (mm_in_count(&midi_ctx) > 0) {
        if (mm_in_open(&midi_ctx, &midi_in_hw, 0, my_midi_callback, NULL) == 0) {
            mm_in_start(&midi_in_hw);
            char name[128] = {0};
            mm_in_name(&midi_ctx, 0, name, sizeof(name));
            printf("Opened hardware MIDI input: %s\n", name);
        }
    } else {
        printf("No hardware MIDI inputs found.\n");
    }
    
    // Virtual port
    if (mm_in_open_virtual(&midi_ctx, &midi_in_virt, my_midi_callback, NULL) == 0) {
        mm_in_start(&midi_in_virt);
        printf("Opened virtual MIDI input port: ksynth-repl\n");
    }
}
