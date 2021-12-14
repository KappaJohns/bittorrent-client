#ifndef BITFIELD_H
#define BITFIELD_H

void set_bit(uint8_t *buf, int pos);
int get_bit(uint8_t *buf, int pos);
int find_first_one_bit (uint8_t *buf1, uint8_t *buf2, int buf_length);
void initialize_bitfield(uint8_t *buf, int length);
int compare_spare_bits (uint8_t *buf, int buf_length, int num_pieces);
int compare_bitfields (uint8_t *buf1, uint8_t *buf2, int *ret, int buf_length);

#endif