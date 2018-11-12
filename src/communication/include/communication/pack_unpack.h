#ifndef _PACK_UNPACK_H
#define _PACK_UNPACK_H

void unpack(unsigned char *buf, char *format, ...);
unsigned int pack(unsigned char *buf, char *format, ...);

#endif