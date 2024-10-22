#include "frame_utils.h"


unsigned int funcI_BCC2(const unsigned char *arr, int len)
{
  unsigned int res = 0;

  for (int i = 0; i < len; i++) {
    res ^= arr[i];
  }

  return res;
}


void prepSU(unsigned char *buf, unsigned char addr, unsigned char ctrl)
{
  memset(buf, 0, SU_BUF_SIZE);
  buf[0] = SU_Flag;
  buf[1] = addr;
  buf[2] = ctrl;
  buf[3] = SU_BCC1(buf[1], buf[2]);
  buf[4] = SU_Flag;
}

