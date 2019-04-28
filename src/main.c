#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "zap.h"

static const char * const usage = "usage: unzap [working dir] <bundle>\n";

/* callback function for zap_unzap */
static void cb(ZapEnt *ent, const char *path) {
  if (!path) { /* before */
    fprintf(stderr, "extracting \"%s\" ... ", ent->name);
  } else { /* after */
    fprintf(stderr, "OK\n");
  }

  fflush(stderr);
}

int main(int argc, char *argv[]) {
  char *prefix = NULL;
  char *path;
  Bundle *b;

  if (argc < 2) {
    fputs(usage, stderr);
    return 1;
  }

  if (argc == 2) {
    prefix = NULL;
    path = argv[1];
  } else if (argc == 3) {
    prefix = argv[1];
    path = argv[2];
  }

  /* Load the bundle file */
  if ((b = zap_open(path)) == NULL) {
    fprintf(stderr, "failed to open bundle file: %s\n",
      errno > 0 ? strerror(errno) : "?");

    zap_free(b);
    return 2;
  }

  /* Extract */
  if (!zap_unzap(cb, prefix, b)) {
    fprintf(stderr, "ERROR: %s\n",
      errno > 0 ? strerror(errno) : "?");

    zap_free(b);
    return 2;
  }

  zap_free(b);

  return 0;
}
