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
#include <assert.h>

#include "decompress.h"
#include "zap.h"

#if defined(_WIN32) || defined(__CYGWIN__)
#define PATH_SEPARATOR ('\\')
#else
/* POSIX */
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

void ent_get(Bundle *b, int i, ZapEnt *ent, bool *empty, bool *compressed) {
  ptrdiff_t offset;

  offset = (ZAP_HEADER_SZ + ZAP_ENT_SZ * i);

  /* Copy into each struct field from the mmap file ptr */
  ent->meta.moffset = *(uint32_t*)(b->file + offset + 0);

  ent->meta.size.a = *(uint32_t*)(b->file + offset + 0x4);
  ent->meta.size.z = *(uint32_t*)(b->file + offset + 0x8);

  memcpy((void*)ent->meta.size.zblocks,
    (void*)(b->file + offset + 0xc),
    (sizeof(uint32_t) * 0x21));

  ent->meta.pad = *(uint32_t*)(b->file + offset + 0x90);
  ent->meta.unkx94 = *(uint32_t*)(b->file + offset + 0x94);

  memcpy((void*)ent->name, (void*)(b->file + offset + 0x98), 0x100);

  /* Helpers */
  *compressed = (ent->meta.size.a > ent->meta.size.z);
  *empty = (ZAP_ENT_Z_EMPTY == ent->meta.size.z);
}

/* Extracts ents
   `cb` is called before and after each ent extraction
   `prefix` changes the working directory
   `b` is the ZAP bundle file
   */
bool zap_unzap(void (*cb)(ZapEnt*, const char*), const char *prefix, Bundle *b) {
  ptrdiff_t offset;
  uint8_t *dbuf;
  size_t max_a;

  /* First set the working directory */
  errno = 0;
  if (prefix != NULL && chdir(prefix) == -1)
    return false;

  /* vars for decompress */
  dbuf = malloc(DECOMPRESS_MAX_ZBLOCK_A);
  max_a = DECOMPRESS_MAX_ZBLOCK_A;

  /* Start by setting the extract offset to after the header and ents table */
  offset = (ZAP_HEADER_SZ + ZAP_ENT_SZ * b->header.numents);

  /* For each ent in the bundle */
  for (int i = 0; i < b->header.numents; i++) {
    ZapEnt ent;
    bool empty, compressed;
    uint8_t *data;
    char path[255+1];
    FILE *fp = NULL;

    /* Each file in the bundle is prefixed with a copy of its ent meta, skip
      then data (the file) is at offset */
    offset += ZAP_ENT_META_SZ;
    data = (b->file + offset);

    /* Get `i` ent from the table */
    ent_get(b, i, &ent, &empty, &compressed);
    (*cb)(&ent, NULL); /* callback BEFORE */

    if (empty) {
      /* Skip empty ent */
      (*cb)(&ent, "<empty>"); /* callback AFTER */
      continue;
    }

    /* Normalize the extract path and make its dir */
    ent_name_os_path(&ent, path);
    if (mkdirs(path, S_IRWXU|S_IRWXG|S_IRWXO) < 0)
      return false;

    if ((fp = fopen(path, "wb")) == NULL)
      return false;

    /* Extract to fp */
    if (compressed) { /* Compressed */
      uint8_t *dbufp;
      int x;

      /* Keep track of maximum `a` size */
      if (ent.meta.size.a > max_a) { 
        max_a = ent.meta.size.a;
        dbuf = realloc(dbuf, max_a);
      }

      /* decompress loop
         each zblock must be kept in dbuf while calling decompress */
      for (x = 0, dbufp = dbuf; ent.meta.size.zblocks[x] > 0; x++) {
        size_t a;

        decompress(data, dbufp, NULL, &a);
        data += ent.meta.size.zblocks[x];
        dbufp += a;
      }

      fwrite(dbuf, sizeof(uint8_t), ent.meta.size.a, fp);
    } else { /* Not compressed, copy the data */ 
      fwrite(data, sizeof(uint8_t), ent.meta.size.z, fp);
    }
    fclose(fp); /* Close the extract file */

    /* Advance offset by what was extracted, and the pad */
    offset += (ent.meta.size.z + ent.meta.pad);

    (*cb)(&ent, path); /* callback AFTER */
  }

  free(dbuf);

  return true;
}

void zap_free(Bundle *b) {
  if (b) {
    /* Free the Bundle file ptr */
    munmap((void*)b->file, b->st_size);
    free(b);
  }
}

Bundle* zap_open(const char *path) {
  int fd;
  uint64_t magic;
  struct stat s;
  void *file;
  Bundle *ret;

  if ((fd = open(path, O_RDONLY)) < 0)
    return NULL;

  if (fstat(fd, &s) < 0) { /* fstat failed */
    close(fd);
    return NULL;
  }

  /* must be at least the size of the header */
  if (s.st_size < ZAP_HEADER_SZ) {
    close(fd);
    return NULL;
  }

  /* check the magic before reading the entire file */
  if (read(fd, &magic, sizeof(uint64_t)) < sizeof(uint64_t) ||
    (magic != ZAP_HEADER_MAGIC)) {
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
  ret->file = (uint8_t*)file; /* cast as byte* */
  ret->st_size = s.st_size;

  ret->header.magic = *(uint64_t*)(ret->file);
  ret->header.numents = *(uint32_t*)(ret->file + 0x8);
  ret->header.unkx0c = *(uint32_t*)(ret->file + 0x0c);

  return ret;
}
