#include "scope.h"
#include <string.h>

#define MAX_OUT_LEN 65536

void draw_line(unsigned char *grid, int x1, int y1, int x2, int y2, int w, int h) {
    int dx = abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
    int dy = -abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
    int err = dx + dy, e2;

    while (1) {
        if (x1 >= 0 && x1 < w && y1 >= 0 && y1 < h) {
            grid[y1 * w + x1] = 1;
        }
        if (x1 == x2 && y1 == y2) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x1 += sx; }
        if (e2 <= dx) { err += dx; y1 += sy; }
    }
}

void print_scope(hazel_ctx_t* ctx, double *data, int len, int width, int height) {
    if (len < 2) return;

    double min_y = DBL_MAX;
    double max_y = -DBL_MAX;
    for (int i = 0; i < len; i++) {
        if (data[i] < min_y) min_y = data[i];
        if (data[i] > max_y) max_y = data[i];
    }

    if (max_y == min_y) { max_y += 0.1; min_y -= 0.1; }

    int canvas_w = (width / 2) * 2;
    int canvas_h = (height / 4) * 4;
    unsigned char *grid = (unsigned char*)calloc(canvas_w * canvas_h, sizeof(unsigned char));
    if (!grid) return;

    int prev_y = -1;
    for (int x = 0; x < canvas_w; x++) {
        double data_pos = (double)x / (canvas_w - 1) * (len - 1);
        int idx_low = (int)floor(data_pos);
        int idx_high = (int)ceil(data_pos);
        double fraction = data_pos - idx_low;
        
        double val = data[idx_low] * (1.0 - fraction) + 
                     data[idx_high >= len ? len-1 : idx_high] * fraction;

        int y = (int)((max_y - val) / (max_y - min_y) * (canvas_h - 1));

        if (prev_y != -1) {
            draw_line(grid, x - 1, prev_y, x, y, canvas_w, canvas_h);
        } else {
            if (y >= 0 && y < canvas_h) grid[y * canvas_w + x] = 1;
        }
        prev_y = y;
    }

    double dur_ms = (double)len / 44100.0 * 1000.0;

    char* out = (char*)malloc(MAX_OUT_LEN);
    if (!out) { free(grid); return; }
    int pos = 0;

    pos += snprintf(out + pos, MAX_OUT_LEN - pos, "\n  MAX: %-10.4f", max_y);
    pos += snprintf(out + pos, MAX_OUT_LEN - pos, "  DUR: %0.4fms (@44100Hz) / %d samples", dur_ms, len);
    pos += snprintf(out + pos, MAX_OUT_LEN - pos, "\n  +");
    for(int i = 0; i < canvas_w / 2; i++) pos += snprintf(out + pos, MAX_OUT_LEN - pos, "-");
    pos += snprintf(out + pos, MAX_OUT_LEN - pos, "+\n");

    for (int y = 0; y < canvas_h; y += 4) {
        double row_val_top = max_y - ((double)y / (canvas_h - 1)) * (max_y - min_y);
        double row_val_bot = max_y - ((double)(y + 4) / (canvas_h - 1)) * (max_y - min_y);
        int has_zero = (row_val_top >= 0 && row_val_bot <= 0);

        if (has_zero) pos += snprintf(out + pos, MAX_OUT_LEN - pos, "0 |");
        else pos += snprintf(out + pos, MAX_OUT_LEN - pos, "  |");

        for (int x = 0; x < canvas_w; x += 2) {
            unsigned int byte_offset = 0;
            int bit_map[8] = {1, 2, 4, 8, 16, 32, 64, 128};
            int dots[8][2] = {{0,0}, {1,0}, {2,0}, {0,1}, {1,1}, {2,1}, {3,0}, {3,1}};

            for (int i = 0; i < 8; i++) {
                int dy = y + dots[i][0];
                int dx = x + dots[i][1];
                if (dy < canvas_h && dx < canvas_w && grid[dy * canvas_w + dx]) {
                    byte_offset |= bit_map[i];
                }
            }

            if (byte_offset == 0) {
                if (has_zero) pos += snprintf(out + pos, MAX_OUT_LEN - pos, "-");
                else pos += snprintf(out + pos, MAX_OUT_LEN - pos, " ");
            } else {
                unsigned int code = 0x2800 + byte_offset;
                pos += snprintf(out + pos, MAX_OUT_LEN - pos, "%c%c%c", 
                       (unsigned char)(0xE0 | (code >> 12)),
                       (unsigned char)(0x80 | ((code >> 6) & 0x3F)),
                       (unsigned char)(0x80 | (code & 0x3F)));
            }
        }

        if (has_zero) pos += snprintf(out + pos, MAX_OUT_LEN - pos, "| 0\n");
        else pos += snprintf(out + pos, MAX_OUT_LEN - pos, "|\n");
    }

    pos += snprintf(out + pos, MAX_OUT_LEN - pos, "  +");
    for(int i = 0; i < canvas_w / 2; i++) pos += snprintf(out + pos, MAX_OUT_LEN - pos, "-");
    pos += snprintf(out + pos, MAX_OUT_LEN - pos, "+\n  MIN: %-10.4f\n\n", min_y);

    hazel_append_output(ctx, out, 0);
    
    free(out);
    free(grid);
}
