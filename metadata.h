/**
Open Source Initiative OSI - The MIT License (MIT):Licensing

The MIT License (MIT)
Copyright (c) <2012> <yunnysunny>

Permission is hereby granted, free of charge, to any person obtaining a copy of this software 
and associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial
portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, 
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
OR OTHER DEALINGS IN THE SOFTWARE.

@author yunnysunny<yunnysunny@gmail.com>

*/
#ifndef METADATA_H_
#define METADATA_H_
#include <stdint.h>
#include "win2linux.h"

#define SGET_META_VERSION 1
#define SGET_META_SUFFIX ".sget.meta"
#define SGET_MAX_THREAD_COUNT 64

#define CHUNK_STATUS_PENDING  0
#define CHUNK_STATUS_RUNNING  1
#define CHUNK_STATUS_DONE     2
#define CHUNK_STATUS_ERROR   -1

typedef struct SgetChunk {
    int index;
    uint64_t start;       /* byte range start */
    uint64_t end;         /* byte range end (inclusive) */
    uint64_t downloaded;  /* bytes downloaded so far in this chunk (offset from start) */
    int status;
} SgetChunk;

typedef struct SgetMetadata {
    int version;
    char url[2048];
    char filename[512];
    uint64_t total_size;
    int thread_count;
    int accept_ranges;        /* 1 if server supports Range */
    char validator[256];      /* strong ETag echoed via If-Range; empty means resume unverified */
    SgetChunk *chunks;
} SgetMetadata;

/* Create fresh metadata for a new download */
SgetMetadata *metadata_create(const char *url, const char *filename,
                              uint64_t total_size, int thread_count,
                              const char *validator);

/* Load metadata from file. Returns NULL if file doesn't exist or is invalid. */
SgetMetadata *metadata_load(const char *meta_path);

/* Save metadata to file (atomic: write temp then rename). Returns 0 on success. */
int metadata_save(const SgetMetadata *meta, const char *meta_path);

/* Delete metadata file. Returns 0 on success. */
int metadata_delete(const char *meta_path);

/* Free metadata struct */
void metadata_free(SgetMetadata *meta);

/* Build the meta file path from the download filename. Caller must free(). */
char *metadata_build_path(const char *filename);

#endif
