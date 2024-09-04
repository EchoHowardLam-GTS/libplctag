#include <stdint.h>

struct buf_t {
    uint8_t *data;
    uint32_t len;
};


struct eip_header_t {
    uint16_t encap_command;
    uint16_t encap_length;
    uint32_t encap_session_handle;
    uint32_t encap_status;
    uint64_t encap_sender_context;
};



int decode_eip_header(struct buf_t *src, struct eip_header_t *header)
{


}


int encode_eip_header(struct eip_header_t *header, struct buf_t *dest)
{

}




#include <stdint.h>
#include <string.h>

// Helper functions to convert from little-endian to host byte order
uint16_t le16toh(const uint8_t *data)
{
    return (data[0] | (data[1] << 8));
}

uint32_t le32toh(const uint8_t *data)
{
    return (data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24));
}

uint64_t le64toh(const uint8_t *data)
{
    return (uint64_t)data[0] | ((uint64_t)data[1] << 8) | ((uint64_t)data[2] << 16) | ((uint64_t)data[3] << 24) |
           ((uint64_t)data[4] << 32) | ((uint64_t)data[5] << 40) | ((uint64_t)data[6] << 48) | ((uint64_t)data[7] << 56);
}

int decode_eip_header(struct buf_t *src, struct eip_header_t *header)
{
    if (src->len < sizeof(struct eip_header_t)) {
        // Not enough data to decode header
        return -1;
    }

    uint8_t *data = src->data;

    header->encap_command = le16toh(data);
    header->encap_length = le16toh(data + 2);
    header->encap_session_handle = le32toh(data + 4);
    header->encap_status = le32toh(data + 8);
    header->encap_sender_context = le64toh(data + 12);

    return 0;
}





#include <stdint.h>
#include <string.h>

// Helper functions to convert from host byte order to little-endian
void htole16(uint16_t value, uint8_t *data)
{
    data[0] = (uint8_t)(value);
    data[1] = (uint8_t)(value >> 8);
}

void htole32(uint32_t value, uint8_t *data)
{
    data[0] = (uint8_t)(value);
    data[1] = (uint8_t)(value >> 8);
    data[2] = (uint8_t)(value >> 16);
    data[3] = (uint8_t)(value >> 24);
}

void htole64(uint64_t value, uint8_t *data)
{
    data[0] = (uint8_t)(value);
    data[1] = (uint8_t)(value >> 8);
    data[2] = (uint8_t)(value >> 16);
    data[3] = (uint8_t)(value >> 24);
    data[4] = (uint8_t)(value >> 32);
    data[5] = (uint8_t)(value >> 40);
    data[6] = (uint8_t)(value >> 48);
    data[7] = (uint8_t)(value >> 56);
}

int encode_eip_header(struct eip_header_t *header, struct buf_t *dest)
{
    if (dest->len < sizeof(struct eip_header_t)) {
        // Not enough space in the buffer
        return -1;
    }

    uint8_t *data = dest->data;

    htole16(header->encap_command, data);
    htole16(header->encap_length, data + 2);
    htole32(header->encap_session_handle, data + 4);
    htole32(header->encap_status, data + 8);
    htole64(header->encap_sender_context, data + 12);

    return 0;
}










#include <stdint.h>
#include <string.h>

// Helper functions to convert from little-endian to host byte order
uint16_t le16toh(const uint8_t *data)
{
    return (data[0] | (data[1] << 8));
}

uint32_t le32toh(const uint8_t *data)
{
    return (data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24));
}

int decode_eip_header(struct buf_t *src, struct eip_header_t *header)
{
    if (src->len < sizeof(struct eip_header_t)) {
        // Not enough data to decode header
        return -1;
    }

    uint8_t *data = src->data;

    header->secs_per_tick = data[0];
    header->timeout_ticks = data[1];
    header->orig_to_targ_conn_id = le32toh(data + 2);
    header->targ_to_orig_conn_id = le32toh(data + 6);
    header->conn_serial_number = le16toh(data + 10);
    header->orig_vendor_id = le16toh(data + 12);
    header->orig_serial_number = le32toh(data + 14);
    header->conn_timeout_multiplier = data[18];
    memcpy(header->reserved, data + 19, 3);
    header->orig_to_targ_rpi = le32toh(data + 22);
    header->orig_to_targ_conn_params = le16toh(data + 26);
    header->targ_to_orig_rpi = le32toh(data + 28);
    header->targ_to_orig_conn_params = le16toh(data + 32);
    header->transport_class = data[34];
    header->target_epath_word_count = data[35];
    header->reserved_target_epath_padding = data[36];

    // Variable-length array, determine the size from the header
    size_t epath_bytes_size = src->len - 37;
    if (epath_bytes_size > 0) {
        header->target_epath_bytes = (uint8_t *)malloc(epath_bytes_size);
        if (!header->target_epath_bytes) {
            return -1; // Memory allocation failed
        }
        memcpy(header->target_epath_bytes, data + 37, epath_bytes_size);
    }

    return 0;
}




#include <stdint.h>
#include <string.h>

// Helper functions to convert from host byte order to little-endian
void htole16(uint16_t value, uint8_t *data)
{
    data[0] = (uint8_t)(value);
    data[1] = (uint8_t)(value >> 8);
}

void htole32(uint32_t value, uint8_t *data)
{
    data[0] = (uint8_t)(value);
    data[1] = (uint8_t)(value >> 8);
    data[2] = (uint8_t)(value >> 16);
    data[3] = (uint8_t)(value >> 24);
}

int encode_eip_header(struct eip_header_t *header, struct buf_t *dest)
{
    size_t target_epath_bytes_size = header->target_epath_bytes ? strlen((char*)header->target_epath_bytes) : 0;
    size_t total_size = sizeof(struct eip_header_t) - sizeof(header->target_epath_bytes) + target_epath_bytes_size;

    if (dest->len < total_size) {
        // Not enough space in the buffer
        return -1;
    }

    uint8_t *data = dest->data;

    data[0] = header->secs_per_tick;
    data[1] = header->timeout_ticks;
    htole32(header->orig_to_targ_conn_id, data + 2);
    htole32(header->targ_to_orig_conn_id, data + 6);
    htole16(header->conn_serial_number, data + 10);
    htole16(header->orig_vendor_id, data + 12);
    htole32(header->orig_serial_number, data + 14);
    data[18] = header->conn_timeout_multiplier;
    memcpy(data + 19, header->reserved, 3);
    htole32(header->orig_to_targ_rpi, data + 22);
    htole16(header->orig_to_targ_conn_params, data + 26);
    htole32(header->targ_to_orig_rpi, data + 28);
    htole16(header->targ_to_orig_conn_params, data + 32);
    data[34] = header->transport_class;
    data[35] = header->target_epath_word_count;
    data[36] = header->reserved_target_epath_padding;

    if (header->target_epath_bytes) {
        memcpy(data + 37, header->target_epath_bytes, target_epath_bytes_size);
    }

    return 0;
}
