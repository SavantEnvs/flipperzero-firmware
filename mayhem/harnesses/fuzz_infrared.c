// Feeds raw level/duration pulses into every IR protocol decoder, and drives every encoder
// from fuzzer-chosen (protocol, address, command).
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <infrared.h>

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if(size < 1) return 0;
    if(data[0] & 0x80) {
        // Encoder mode: [flags|protocol][address:4][command:4][repeat]
        if(size < 10) return 0;
        InfraredMessage msg;
        msg.protocol = (InfraredProtocol)(data[0] % InfraredProtocolMAX);
        memcpy(&msg.address, data + 1, 4);
        memcpy(&msg.command, data + 5, 4);
        msg.repeat = data[9] & 1;
        InfraredEncoderHandler* enc = infrared_alloc_encoder();
        infrared_reset_encoder(enc, &msg);
        uint32_t duration = 0;
        bool level = false;
        InfraredStatus st;
        size_t guard = 0;
        do {
            st = infrared_encode(enc, &duration, &level);
        } while(st != InfraredStatusError && st != InfraredStatusDone && ++guard < 4096);
        infrared_free_encoder(enc);
        return 0;
    }
    // Decoder mode: 3 bytes per pulse = level bit + 16 bit duration (x16 us)
    InfraredDecoderHandler* dec = infrared_alloc_decoder();
    bool level = data[0] & 1;
    for(size_t i = 1; i + 1 < size; i += 2) {
        uint32_t duration = ((uint32_t)data[i] | ((uint32_t)data[i + 1] << 8)) * 4;
        const InfraredMessage* m = infrared_decode(dec, level, duration);
        if(m) {
            (void)infrared_get_protocol_name(m->protocol);
            (void)infrared_get_protocol_address_length(m->protocol);
            (void)infrared_get_protocol_command_length(m->protocol);
        }
        level = !level;
    }
    (void)infrared_check_decoder_ready(dec);
    infrared_reset_decoder(dec);
    infrared_free_decoder(dec);
    return 0;
}
