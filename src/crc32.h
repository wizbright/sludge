#ifndef CRC32_H
#define CRC32_H

#include <stdlib.h>
#include <stdint.h>
#include <sys/param.h>
#include <sys/types.h>

//typedef unsigned int uint32_t;
//typedef unsigned char uint8_t;

extern uint32_t crc32(uint32_t crc, const void *buf, size_t size);

#endif //crc32.h
