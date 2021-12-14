#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include "bitfield.h"

//set the bit to 1; inspired by: https://stackoverflow.com/questions/30007665/how-can-i-store-value-in-bit-in-c-language
void set_bit(uint8_t *buf, int pos) {
    int byte_pos = pos / 8;
    uint8_t mask = 1 << (7-(pos % 8));
    buf[byte_pos] |= mask;
}

//get the bit value; inspired by: https://stackoverflow.com/questions/30007665/how-can-i-store-value-in-bit-in-c-language
int get_bit(uint8_t *buf, int pos) {
    int byte_pos = pos / 8;
    uint8_t mask = 1 << (7-(pos % 8));
    if ((buf[byte_pos] & mask) == pow(2, 7-(pos % 8))) {
        return 1;
    } else {
        return 0;
    }
}

//find the first bit that is 1 that buf2 has that buf1 does not have and returns that position; returns -1 if buf1 has all bits that buf2 has
// buf_length is in term of bytes
// note: high bit in first byte corresponds to piece 0
int find_first_one_bit (uint8_t *buf1, uint8_t *buf2, int buf_length) {
    int byte_index = 0;

    while (byte_index < buf_length) {
        for (int i = 7; i>=0; i--) {
            uint8_t mask = 1 << i;
            if ((buf2[byte_index] & mask) == pow(2, i) && (buf1[byte_index] & mask) == 0) {
                return (byte_index * 8) + (7-i);
            }
        }
        byte_index++;
    }
    return -1;
}

//compare the spare bits to make sure they are not set
//returns 1 on if spare bits are not set, else returns -1
int compare_spare_bits (uint8_t *buf, int buf_length, int num_pieces) {
    int index_spare = (num_pieces % 8)-1;
    if (index_spare == -1) {
        return 1;
    }
    for (int i = index_spare; i>=0; i--) {

        uint8_t mask = 1 << i;
        if ((buf[buf_length-1] & mask) != 0) {
            return -1;
        }
    }
    return 1;
}

// compares the bitfields and returns *ret which holds all the bitfields we need
// returns the length of *ret
int compare_bitfields (uint8_t *buf1, uint8_t *buf2, int *ret, int buf_length) {
    int byte_index = 0;
    int ret_index = 0;

    while (byte_index < buf_length) {
        for (int i = 7; i>=0; i--) {
            uint8_t mask = 1 << i;
            if ((buf2[byte_index] & mask) == pow(2, i) && (buf1[byte_index] & mask) == 0) {
                ret[ret_index] = (byte_index *8) + (7-i);
                ret_index++;
            }
        }
        byte_index++;
    }
    return ret_index;
}

// initializes bitfield to all 0s
void initialize_bitfield(uint8_t *buf, int length) {
    for (int i = 0; i<length; i++) {
        buf[i] = 0;
    }
}

int firstzero(uint8_t *buf, int length) {
    int byte_index = 0;

    while (byte_index < length) {
        for (int i = 7; i>=0; i--) {
            uint8_t mask = 1 << i;
            if ((buf[byte_index] & mask) == 0) {
                return (byte_index * 8) + (7-i);
            }
        }
        byte_index++;
    }
    return -1;
}