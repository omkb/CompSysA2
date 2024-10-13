// Setting _DEFAULT_SOURCE is necessary to activate visibility of
// certain header file contents on GNU/Linux systems.
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fts.h>

// err.h contains various nonstandard BSD extensions, but they are
// very handy.
#include <err.h>

#include <pthread.h>

#include "job_queue.h"

// to pass the arguments to the worker
struct worker_args {
  struct job_queue *jq;
  char const *needle;
};

int fauxgrep_file(char const *needle, char const *path) {
  FILE *f = fopen(path, "r");

  if (f == NULL) {
    warn("failed to open %s", path);
    return -1;
  }

  char *line = NULL;
  size_t linelen = 0;
  int lineno = 1;

  while (getline(&line, &linelen, f) != -1) {
    if (strstr(line, needle) != NULL) {
      printf("%s:%d: %s \n", path, lineno, line);
    }

    lineno++;
  }

  free(line);
  fclose(f);

  return 0;
}

void *worker(void *arg) {
  struct worker_args *args = arg;
  struct job_queue *jq = args->jq;
  char const *needle = args->needle;
  char *file_path;

  while (1) {
    if (job_queue_pop(jq, (void**)&file_path) == 0) {
      fauxgrep_file(needle, file_path);
      free(file_path);
    } else {
      break;
    }
  }
  return NULL;
}


int main(int argc, char * const *argv) {
  if (argc < 2) {
    err(1, "usage: [-n INT] STRING paths...");
    exit(1);
  }

  int num_threads = 1;
  char const *needle = argv[1];
  char * const *paths = &argv[2];


  if (argc > 3 && strcmp(argv[1], "-n") == 0) {
    // Since atoi() simply returns zero on syntax errors, we cannot
    // distinguish between the user entering a zero, or some
    // non-numeric garbage.  In fact, we cannot even tell whether the
    // given option is suffixed by garbage, i.e. '123foo' returns
    // '123'.  A more robust solution would use strtol(), but its
    // interface is more complicated, so here we are.
    num_threads = atoi(argv[2]);

    if (num_threads < 1) {
      err(1, "invalid thread count: %s", argv[2]);
    }

    needle = argv[3];
    paths = &argv[4];

  } else {
    needle = argv[1];
    paths = &argv[2];
  }

  
  // Create job queue
  struct job_queue jq;
  job_queue_init(&jq, 64);

  // Create worker arguments
  struct worker_args args;
  args.jq = &jq;
  args.needle = needle;

  // Start up the worker threads
  pthread_t *threads = calloc(num_threads, sizeof(pthread_t));
  for (int i = 0; i < num_threads; i++) {
    if (pthread_create(&threads[i], NULL, &worker, &args) != 0) {
      err(1, "pthread_create() failed");
    }
  }

  // FTS_LOGICAL = follow symbolic links
  // FTS_NOCHDIR = do not change the working directory of the process
  //
  // (These are not particularly important distinctions for our simple
  // uses.)
  int fts_options = FTS_LOGICAL | FTS_NOCHDIR;

  FTS *ftsp;
  if ((ftsp = fts_open(paths, fts_options, NULL)) == NULL) {
    err(1, "fts_open() failed");
    return -1;
  }

  FTSENT *p;
  while ((p = fts_read(ftsp)) != NULL) {
    char *file_path = NULL; 
    switch (p->fts_info) {
    case FTS_D:
      break;
    case FTS_F:
      // Duplicate the file path to push onto the job queue
      file_path = strdup(p->fts_path);
      if (file_path == NULL) {
        err(1, "Failed to allocate memory for file path");
      }
      if (job_queue_push(&jq, file_path) != 0) {
        free(file_path);
        err(1, "Failed to push to job queue");
        }
      break;
    default:
        break;
    }
}

  fts_close(ftsp);

  job_queue_destroy(&jq);

    for (int i = 0; i < num_threads; i++) {
      if (pthread_join(threads[i], NULL) != 0) {
      err(1, "pthread_join() failed");
    }
  }
  free(threads);

  return 0;
}
