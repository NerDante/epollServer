#ifndef __UTIL_H__
#define __UTIL_H__

int raw_dump(const unsigned char* buff, int len);
uint16_t get_be16value(uint8_t *buf);
uint32_t get_be32value(uint8_t *buf);
void pack16be(uint8_t *buf, uint16_t host_value);
void pack32be(uint8_t *buf, uint32_t host_value);
int setnonblocking(int fd);

#endif // include guard
