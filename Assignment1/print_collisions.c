#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "frame.h"
#include "crc.h"
#include "checksum.h"

static void flip_bit(EthernetFrame *frame, int bit_pos) {
    int byte = bit_pos / 8;
    int bit = bit_pos % 8;
    frame->payload[byte] ^= (1 << bit);
}

static void setup_base_frame(EthernetFrame *frame) {
    const uint8_t src[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    const uint8_t dst[6] = {0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB};
    const char *text = "The brown fox jumps over the lazy dog today.";
    initializeFrame(frame);
    setSourceMAC(frame, src);
    setDestinationMAC(frame, dst);
    frame->type = 0x0800;
    copyPayload(frame, (const uint8_t *)text, (uint16_t)strlen(text));
}

void print_hex_diff(const EthernetFrame *f1, const EthernetFrame *f2) {
    printf("Original Frame (Payload 44B):\n  ");
    for (int i = 0; i < PAYLOAD_SIZE; i++) {
        printf("%02X ", f1->payload[i]);
        if ((i + 1) % 16 == 0) printf("\n  ");
    }
    printf("\nCorrupted Frame (Payload 44B):\n  ");
    for (int i = 0; i < PAYLOAD_SIZE; i++) {
        printf("%02X ", f2->payload[i]);
        if ((i + 1) % 16 == 0) printf("\n  ");
    }
    printf("\nDiffering Bytes:\n");
    for (int i = 0; i < PAYLOAD_SIZE; i++) {
        if (f1->payload[i] != f2->payload[i]) {
            printf("  Byte offset %2d: 0x%02X ('%c') -> 0x%02X ('%c') [XOR mask: 0x%02X]\n",
                   i, f1->payload[i], (f1->payload[i] >= 32 && f1->payload[i] <= 126) ? f1->payload[i] : '.',
                   f2->payload[i], (f2->payload[i] >= 32 && f2->payload[i] <= 126) ? f2->payload[i] : '.',
                   f1->payload[i] ^ f2->payload[i]);
        }
    }
}

int main(void) {
    srand(12345);
    int total_bits = PAYLOAD_SIZE * 8;
    int found1 = 0, found2 = 0, found3 = 0;

    for (long it = 0; it < 5000000 && !(found1 && found2 && found3); it++) {
        EthernetFrame f1, f2;
        setup_base_frame(&f1);
        f2 = f1;

        uint16_t chk1 = calculateFrameChecksum(&f1);
        uint32_t crc1 = calculateCRC((unsigned char *)&f1, SERIALIZED_SIZE, CRC8_POLY, 8);

        // inject corruption
        int mode = it % 3;
        if (mode == 0) {
            // single bit or double bit
            flip_bit(&f2, rand() % total_bits);
            if (rand() % 2) flip_bit(&f2, rand() % total_bits);
        } else if (mode == 1) {
            // burst error
            int burst_len = 12;
            int start = rand() % (total_bits - burst_len + 1);
            for (int b = 0; b < burst_len; b++) {
                if (b == 0 || b == burst_len - 1 || (rand() % 100 < 70))
                    flip_bit(&f2, start + b);
            }
        } else {
            // 2 to 4 bit flips
            int k = 2 + (rand() % 3);
            for (int b = 0; b < k; b++) flip_bit(&f2, rand() % total_bits);
        }

        uint16_t chk2 = calculateFrameChecksum(&f2);
        uint32_t crc2 = calculateCRC((unsigned char *)&f2, SERIALIZED_SIZE, CRC8_POLY, 8);

        int chk_det = (chk1 != chk2);
        int crc_det = (crc1 != crc2);

        if (!found1 && chk_det && crc_det) {
            found1 = 1;
            printf("============================================================\n");
            printf("CASE 1: Detected by BOTH Checksum and CRC-8\n");
            printf("============================================================\n");
            printf("Original Checksum : 0x%04X | Corrupted Checksum: 0x%04X (Detected: YES)\n", chk1, chk2);
            printf("Original CRC-8    : 0x%02X   | Corrupted CRC-8   : 0x%02X   (Detected: YES)\n", crc1, crc2);
            print_hex_diff(&f1, &f2);
            printf("\n");
        }

        if (!found2 && chk_det && !crc_det) {
            found2 = 1;
            printf("============================================================\n");
            printf("CASE 2: Detected by Checksum, MISSED by CRC-8 (CRC Collision)\n");
            printf("============================================================\n");
            printf("Original Checksum : 0x%04X | Corrupted Checksum: 0x%04X (Detected: YES)\n", chk1, chk2);
            printf("Original CRC-8    : 0x%02X   | Corrupted CRC-8   : 0x%02X   (COLLISION! Missed)\n", crc1, crc2);
            print_hex_diff(&f1, &f2);
            printf("\n");
        }

        if (!found3 && !chk_det && crc_det) {
            found3 = 1;
            printf("============================================================\n");
            printf("CASE 3: Detected by CRC-8, MISSED by Checksum (Compensating Sum)\n");
            printf("============================================================\n");
            printf("Original Checksum : 0x%04X | Corrupted Checksum: 0x%04X (COLLISION! Missed)\n", chk1, chk2);
            printf("Original CRC-8    : 0x%02X   | Corrupted CRC-8   : 0x%02X   (Detected: YES)\n", crc1, crc2);
            print_hex_diff(&f1, &f2);
            printf("\n");
        }
    }
    return 0;
}
