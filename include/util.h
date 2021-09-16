#pragma once

#include <stdint.h>
#include <sys/types.h>

#define mod(x, N) ((((x) < 0) ? (((x) % (N)) + (N)) : (x)) % (N))
#define min(x, y) ((x) < (y) ? (x) : (y))

#define unagi_ssizeof(foo)            (ssize_t)sizeof(foo)
#define unagi_countof(foo)            (unagi_ssizeof(foo) / unagi_ssizeof(foo[0]))

#define unagi_fatal(string, ...) _unagi_fatal(true,                     \
                                              __LINE__, __FUNCTION__,	\
                                              string, ## __VA_ARGS__)

#define unagi_fatal_no_exit(string, ...)                                \
  _unagi_fatal(false, __LINE__, __FUNCTION__, string, ## __VA_ARGS__)

void _unagi_fatal(bool, int, const char *, const char *, ...)   \
  __attribute__ ((format(printf, 4, 5)));

#define unagi_warn(string, ...) _unagi_warn(__LINE__,                   \
                                            __FUNCTION__,               \
                                            string, ## __VA_ARGS__)

void _unagi_warn(int, const char *, const char *, ...)
  __attribute__ ((format(printf, 3, 4)));

#define unagi_info(string, ...) _unagi_info(__LINE__,                   \
                                            __FUNCTION__,               \
                                            string, ## __VA_ARGS__)

void _unagi_info(int, const char *, const char *, ...)
  __attribute__ ((format(printf, 3, 4)));

#define unagi_debug(string, ...) _unagi_debug(__LINE__,                 \
                                              __FUNCTION__,             \
                                              string, ## __VA_ARGS__)

void _unagi_debug(int, const char *, const char *, ...)
  __attribute__ ((format(printf, 3, 4)));

#define unagi_util_free(mem_pp)			   \
  {						   \
    typeof(**(mem_pp)) **__ptr = (mem_pp);         \
    free(*__ptr);                                  \
    *__ptr = NULL;                                 \
  }

typedef struct _unagi_util_itree_t
{
  uint32_t key;
  int height;
  void *value;
  struct _unagi_util_itree_t *left;
  struct _unagi_util_itree_t *right;
  struct _unagi_util_itree_t *parent;
} unagi_util_itree_t;

unagi_util_itree_t *util_itree_new(void);
unagi_util_itree_t *util_itree_insert(unagi_util_itree_t *, uint32_t, void *);
void *util_itree_get(unagi_util_itree_t *, uint32_t);
unagi_util_itree_t *util_itree_remove(unagi_util_itree_t *, uint32_t);
uint32_t util_itree_size(unagi_util_itree_t *);
void unagi_util_itree_free(unagi_util_itree_t *);
