// Feeds raw pulse trains to all 125 kHz RFID protocol decoders, and drives their encoders/renderers
// from fuzzer-chosen payload bytes.
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <furi.h>
#include <toolbox/protocols/protocol_dict.h>
#include <lfrfid/protocols/lfrfid_protocols.h>

// Pulses to pull from a started encoder: several full frames of the longest protocol.
#define YIELD_COUNT 2048

static void drive_encoder(ProtocolDict* dict, ProtocolId id) {
    protocol_dict_encoder_start(dict, id);
    for(int i = 0; i < YIELD_COUNT; i++) {
        LevelDuration ld = protocol_dict_encoder_yield(dict, id);
        (void)level_duration_get_duration(ld);
    }
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if(size < 2) return 0;
    ProtocolDict* dict = protocol_dict_alloc(lfrfid_protocols, LFRFIDProtocolMax);
    FuriString* s = furi_string_alloc();
    if(data[0] & 0x80) {
        // Encoder mode: [flags|protocol][payload...]
        size_t idx = data[0] % LFRFIDProtocolMax;
        size_t dsz = protocol_dict_get_data_size(dict, idx);
        uint8_t buf[64] = {0};
        size_t n = size - 1 < dsz ? size - 1 : dsz;
        memcpy(buf, data + 1, n);
        protocol_dict_set_data(dict, idx, buf, dsz);
        protocol_dict_render_uid(dict, s, idx);
        protocol_dict_render_data(dict, s, idx);
        protocol_dict_render_brief_data(dict, s, idx);
        drive_encoder(dict, idx);
        uint8_t wd[64];
        protocol_dict_get_write_data(dict, idx, wd);
    } else {
        // Decoder mode: 2 bytes per pulse, duration in microseconds, levels alternate
        protocol_dict_decoders_start(dict);
        bool level = data[0] & 1;
        for(size_t i = 1; i + 1 < size; i += 2) {
            uint32_t duration = ((uint32_t)data[i] | ((uint32_t)data[i + 1] << 8)) & 0x7FF;
            ProtocolId id = protocol_dict_decoders_feed(dict, level, duration);
            if(id != PROTOCOL_NO) {
                protocol_dict_render_uid(dict, s, id);
                protocol_dict_render_data(dict, s, id);
                protocol_dict_render_brief_data(dict, s, id);
                drive_encoder(dict, id);
                protocol_dict_decoders_start(dict);
            }
            level = !level;
        }
    }
    furi_string_free(s);
    protocol_dict_free(dict);
    return 0;
}
