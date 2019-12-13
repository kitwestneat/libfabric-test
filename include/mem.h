#ifndef MEM_H
#define MEM_H

struct buffer
{
  bool free;
  void *buf;
  struct buffer *next;
};
#endif
