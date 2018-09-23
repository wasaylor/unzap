#include <sys/stat.h>
#include <sys/mman.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "decompress.h"
#include "zap.h"

#if defined(_WIN32) || defined(__CYGWIN__)
#define PATH_SEPARATOR ('\\')
#else
#define PATH_SEPARATOR ('/')
#endif

/* for a file path,
   create the directories that do not exist underneath it */
int mkdirs(const char *path, mode_t mode) {
  char dpath[255+1],
    *sep;

  /* reset errno
     */
  errno = 0;

  /* dup path and set the ptr
     */
  strncpy(dpath, path, 255);
  sep = &dpath[0];

  if ('\0' == *sep) /* empty path check */
    return 0; 

  /* start checking from the beginning
     data[/]a/b/c/file.txt */
  while ((sep = strchr(sep, PATH_SEPARATOR)) != NULL) {
    *sep = '\0'; /* break it off */

    if (mkdir(dpath, mode) < 0 && errno != EEXIST)
      return -1; /* unexpected errno */

    /* restore & advance */
    *sep++ = PATH_SEPARATOR; 
  }

  return 0;
}

int tolower(int c) {
  return (c >= 'A' && c <= 'Z') ? (c | 0x20) : (c);
}

/* Converts ent name into a lowercase file path */
void ent_name_os_path(ZapEnt *ent, char *out) {
  const char *name;
  char c;

  name = (const char*)ent->name;
  while ((c = *name++)) {
    if (c == '/' || c == '\\') {
      *out++ = PATH_SEPARATOR;
    } else {
      *out++ = tolower(c);
    }
  }

  *out = '\0';
}

/* Is the ent compressed? */
static inline bool ent_compress(ZapEnt *ent) {
  return (ent->meta.size.a > ent->meta.size.z);
}

/* Is the ent empty? */
static inline bool ent_empty(ZapEnt *ent) {
  return (ENT_SIZE_EMPTY == ent->meta.size.z);
}

/* Extracts ents
   `cb` is called before and after each ent extraction
   `prefix` changes the working directory
   `b` is the ZAP bundle file
   */
bool unzap(void (*cb)(ZapEnt*, const char*), const char *prefix, Bundle *b) {
  ptrdiff_t offset;
  uint8_t *file, *dbuf;
  size_t max_a;

  /* First set the working directory */
  errno = 0;
  if (prefix && chdir(prefix) == -1)
    return false;

  /* buffer vars for decompress */
  dbuf = malloc(DECOMPRESS_MAX_BLOCK_A);
  max_a = DECOMPRESS_MAX_BLOCK_A;

  /* start by setting offset to after the header and ents table */
  file = (uint8_t*)b->header;
  offset = (ptrdiff_t)(sizeof(ZapHeader) + (sizeof(ZapEnt) * b->header->numents));

  /* walk ents */
  for (int i = 0; i < b->header->numents; i++) {
    ZapEnt *ent;
    uint8_t *data;
    char path[255+1];
    FILE *fp = NULL;

    ent = &b->ents[i];
    (*cb)(ent, NULL); /* callback BEFORE */

    if (ent_empty(ent)) { /* Skip empty ent */
      offset += (ptrdiff_t)sizeof(ent->meta);

      (*cb)(ent, "<empty>"); /* callback AFTER */
      continue;
    }

    /* ent meta is repeated before z data ..
       .. set data past it */
    data = file + offset + (ptrdiff_t)sizeof(ent->meta);

    /* normalize the path
       The names in the bundle vary by case and path separator */
    ent_name_os_path(ent, path);

    /* make the neccessary directories for the file path and open */
    if (mkdirs(path, S_IRWXU|S_IRWXG|S_IRWXO) < 0)
      return false;
    if ((fp = fopen(path, "wb")) == NULL)
      return false;

    /* Write */
    if (ent_compress(ent)) { /* Compressed */
      int x;
      uint8_t *dbufp;

      /* Keep track of maximum `a` size */
      if (ent->meta.size.a > max_a) { 
        max_a = ent->meta.size.a;
        dbuf = realloc(dbuf, max_a);
      }

      /* decompress loop
         each zblock must be kept in dbuf while calling decompress ..
         .. because a zblock can reach into a previous one */
      for (x = 0, dbufp = dbuf; ent->meta.size.zblocks[x] > 0; x++) {
        size_t a;

        decompress(data, dbufp, NULL, &a);
        data += (ptrdiff_t)ent->meta.size.zblocks[x];
        dbufp += (ptrdiff_t)a;
      }

      fwrite(dbuf, sizeof(uint8_t), ent->meta.size.a, fp);
    } else {
      /* Not compressed */
      fwrite(data, sizeof(uint8_t), ent->meta.size.z, fp);
    }
    fclose(fp);

    /* Advance offset */
    offset += (ptrdiff_t)(sizeof(ent->meta) + ent->meta.size.z + ent->meta.pad);

    (*cb)(ent, path); /* callback AFTER */
  }

  free(dbuf);

  return true;
}

void zap_free(Bundle *b) {
  munmap((void*)b->header, b->st_size);
  free(b);
}

Bundle* zap_open_file(const char *path) {
  int fd;
  uint64_t magic;
  struct stat s;
  uint8_t *file;
  Bundle *ret;

  if ((fd = open(path, O_RDONLY)) < 0)
    return NULL;

  if (fstat(fd, &s) < 0) {
    close(fd);
    return NULL;
  }

  /* must be at least the size of the header */
  if (s.st_size < sizeof(ZapHeader)) {
    close(fd);
    return NULL;
  }

  /* check the magic before reading the entire file */
  if (read(fd, &magic, sizeof(uint64_t)) < sizeof(uint64_t) ||
    magic != ZAP_HEADER_MAGIC) {
  
    close(fd);
    return NULL;
  }

  /* Map the entire file into memory (read only) */
  file = (uint8_t*)mmap(
    NULL,
    s.st_size,
    PROT_READ,
    MAP_FILE|MAP_PRIVATE,
    fd,
    0);

  close(fd);

  if (MAP_FAILED == file)
    return NULL;

  ret = malloc(sizeof(Bundle));
  ret->st_size = s.st_size;
  ret->header = (ZapHeader*)file;
  ret->ents = (ZapEnt*)(file + (ptrdiff_t)sizeof(ZapHeader));

  return ret;
}
