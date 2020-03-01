
#ifndef JENKINSHASH_H
#define JENKINSHASH_H
/*
-------------------------------------------------------------------------------
lookup3.c, by Bob Jenkins, May 2006, Public Domain.
-------------------------------------------------------------------------------
*/
#define SELF_TEST 1

#include <stdio.h>      /* defines printf for tests */
#include <time.h>       /* defines time_t for timings in the test */
#include <stdint.h>     /* defines uint32_t etc */
#include <sys/param.h>  /* attempt to define endianness */
#ifdef linux
# include <endian.h>    /* attempt to define endianness */
#endif

#define HASH_LITTLE_ENDIAN 1
#define hashsize(n) ((uint32_t)1<<(n))
#define hashmask(n) (hashsize(n)-1)
#define rot(x,k) (((x)<<(k)) | ((x)>>(32-(k))))

uint32_t hashlittle( const void *key, size_t length, uint32_t initval);

#endif