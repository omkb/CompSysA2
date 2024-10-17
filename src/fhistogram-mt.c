// Setting _DEFAULT_SOURCE is necessary to activate visibility of
// certain header file contents on GNU/Linux systems.
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fts.h>

#include "job_queue.h"

pthread_mutex_t stdout_mutex = PTHREAD_MUTEX_INITIALIZER;

// err.h contains various nonstandard BSD extensions, but they are
// very handy.
#include <err.h>

#include "histogram.h"

int global_histogram[8] = {0};

void *worker_thread(void *arg)
{
  struct job_queue *queue = (struct job_queue *)arg;
  int local_histogram[8] = {0};
  char *filename;

  while (job_queue_pop(queue, (void **)&filename) == 0)
  {
    FILE *file = fopen(filename, "rb");
    if (!file)
    {
      fprintf(stderr, "Error opening file %s\n", filename);
      free(filename);
      continue;
    }

    int i = 0;
    unsigned char c;

    while (fread(&c, 1, 1, file) == 1)
    {
      i++;
      update_histogram(local_histogram, c);

      if ((i % 100000) == 0)
      {
        merge_histogram(local_histogram, global_histogram);
        print_histogram(global_histogram);
      }
    }

    fclose(file);

    merge_histogram(local_histogram, global_histogram);
    print_histogram(global_histogram);

    free(filename);
  }
  return NULL;
}

int main(int argc, char *const *argv)
{
  if (argc < 2)
  {
    err(1, "usage: paths...");
    exit(1);
  }

  int num_threads = 1;
  char *const *paths = &argv[1];

  if (argc > 3 && strcmp(argv[1], "-n") == 0)
  {
    num_threads = atoi(argv[2]);
    if (num_threads < 1)
    {
      err(1, "invalid thread count: %s", argv[2]);
    }
    paths = &argv[3];
  }
  else
  {
    paths = &argv[1];
  }

  // Initialise the job queue and some worker threads here.
  struct job_queue jq;
  if (job_queue_init(&jq, 64) != 0)
  {
    err(1, "Failed to initialize job queue");
  }

  pthread_t *threads = calloc(num_threads, sizeof(pthread_t));
  for (int i = 0; i < num_threads; i++)
  {
    if (pthread_create(&threads[i], NULL, worker_thread, &jq) != 0)
    {
      err(1, "Failed to create thread");
    }
  }

  // FTS_LOGICAL = follow symbolic links
  // FTS_NOCHDIR = do not change the working directory of the process
  int fts_options = FTS_LOGICAL | FTS_NOCHDIR;

  FTS *ftsp;
  if ((ftsp = fts_open(paths, fts_options, NULL)) == NULL)
  {
    err(1, "fts_open() failed");
    return -1;
  }

  FTSENT *p;
  while ((p = fts_read(ftsp)) != NULL)
  {
    char *file_path = NULL;
    switch (p->fts_info)
    {
    case FTS_D:
      break;
    case FTS_F:
      file_path = strdup(p->fts_path);
      if (file_path == NULL)
      {
        err(1, "Failed to allocate memory for file path");
      }
      if (job_queue_push(&jq, file_path) != 0)
      {
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

  for (int i = 0; i < num_threads; i++)
    if (pthread_join(threads[i], NULL) != 0)
    {
      err(1, "pthread_join() failed");
    }
  free(threads);

  move_lines(9);

  return 0;
}
