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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "metadata.h"

SgetMetadata *metadata_create(const char *url, const char *filename,
                              unsigned long total_size, int thread_count)
{
    SgetMetadata *meta;
    unsigned long chunk_size;
    int i;

    meta = (SgetMetadata *)calloc(1, sizeof(SgetMetadata));
    if (meta == NULL) return NULL;

    meta->version = SGET_META_VERSION;
    strncpy(meta->url, url, sizeof(meta->url) - 1);
    meta->url[sizeof(meta->url) - 1] = '\0';
    strncpy(meta->filename, filename, sizeof(meta->filename) - 1);
    meta->filename[sizeof(meta->filename) - 1] = '\0';
    meta->total_size = total_size;
    meta->thread_count = thread_count;
    meta->accept_ranges = 1;

    meta->chunks = (SgetChunk *)calloc(thread_count, sizeof(SgetChunk));
    if (meta->chunks == NULL) {
        free(meta);
        return NULL;
    }

    chunk_size = total_size / (unsigned long)thread_count;
    for (i = 0; i < thread_count; i++) {
        meta->chunks[i].index = i;
        meta->chunks[i].start = (unsigned long)i * chunk_size;
        if (i == thread_count - 1) {
            meta->chunks[i].end = total_size - 1;
        } else {
            meta->chunks[i].end = (unsigned long)(i + 1) * chunk_size - 1;
        }
        meta->chunks[i].downloaded = 0;
        meta->chunks[i].status = CHUNK_STATUS_PENDING;
    }

    return meta;
}

SgetMetadata *metadata_load(const char *meta_path)
{
    FILE *fp;
    char line[4096];
    SgetMetadata *meta;
    int chunk_idx = 0;

    fp = fopen(meta_path, "r");
    if (fp == NULL) return NULL;

    meta = (SgetMetadata *)calloc(1, sizeof(SgetMetadata));
    if (meta == NULL) {
        fclose(fp);
        return NULL;
    }

    /* First line: SGET_META v<version> */
    if (fgets(line, sizeof(line), fp) == NULL) {
        free(meta);
        fclose(fp);
        return NULL;
    }
    if (sscanf(line, "SGET_META v%d", &meta->version) != 1 ||
        meta->version != SGET_META_VERSION) {
        free(meta);
        fclose(fp);
        return NULL;
    }

    /* Read key=value lines and chunk lines */
    while (fgets(line, sizeof(line), fp) != NULL) {
        /* Remove trailing newline */
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }

        if (strncmp(line, "url=", 4) == 0) {
            strncpy(meta->url, line + 4, sizeof(meta->url) - 1);
            meta->url[sizeof(meta->url) - 1] = '\0';
        } else if (strncmp(line, "filename=", 9) == 0) {
            strncpy(meta->filename, line + 9, sizeof(meta->filename) - 1);
            meta->filename[sizeof(meta->filename) - 1] = '\0';
        } else if (strncmp(line, "total_size=", 11) == 0) {
            meta->total_size = strtoul(line + 11, NULL, 10);
        } else if (strncmp(line, "thread_count=", 13) == 0) {
            meta->thread_count = atoi(line + 13);
            if (meta->thread_count > 0 && meta->chunks == NULL) {
                meta->chunks = (SgetChunk *)calloc(meta->thread_count, sizeof(SgetChunk));
                if (meta->chunks == NULL) {
                    free(meta);
                    fclose(fp);
                    return NULL;
                }
            }
        } else if (strncmp(line, "accept_ranges=", 14) == 0) {
            meta->accept_ranges = atoi(line + 14);
        } else if (strncmp(line, "chunk:", 6) == 0) {
            if (meta->chunks != NULL && chunk_idx < meta->thread_count) {
                int idx, status;
                unsigned long start, end, downloaded;
                if (sscanf(line + 6, "%d:%lu:%lu:%lu:%d",
                           &idx, &start, &end, &downloaded, &status) == 5) {
                    meta->chunks[chunk_idx].index = idx;
                    meta->chunks[chunk_idx].start = start;
                    meta->chunks[chunk_idx].end = end;
                    meta->chunks[chunk_idx].downloaded = downloaded;
                    meta->chunks[chunk_idx].status = status;
                    chunk_idx++;
                }
            }
        }
    }

    fclose(fp);

    /* Validate we got all chunks */
    if (meta->chunks == NULL || chunk_idx != meta->thread_count) {
        metadata_free(meta);
        return NULL;
    }

    return meta;
}

int metadata_save(const SgetMetadata *meta, const char *meta_path)
{
    FILE *fp;
    char *tmp_path;
    int i;

    tmp_path = (char *)malloc(strlen(meta_path) + 5);
    if (tmp_path == NULL) return -1;
    snprintf(tmp_path, strlen(meta_path) + 5, "%s.tmp", meta_path);

    fp = fopen(tmp_path, "w");
    if (fp == NULL) {
        free(tmp_path);
        return -1;
    }

    fprintf(fp, "SGET_META v%d\n", meta->version);
    fprintf(fp, "url=%s\n", meta->url);
    fprintf(fp, "filename=%s\n", meta->filename);
    fprintf(fp, "total_size=%lu\n", meta->total_size);
    fprintf(fp, "thread_count=%d\n", meta->thread_count);
    fprintf(fp, "accept_ranges=%d\n", meta->accept_ranges);

    for (i = 0; i < meta->thread_count; i++) {
        fprintf(fp, "chunk:%d:%lu:%lu:%lu:%d\n",
                meta->chunks[i].index,
                meta->chunks[i].start,
                meta->chunks[i].end,
                meta->chunks[i].downloaded,
                meta->chunks[i].status);
    }

    fclose(fp);

    /* Atomic rename: on Windows, remove target first */
#if defined(WIN32) || defined(WIN64)
    remove(meta_path);
#endif
    if (rename(tmp_path, meta_path) != 0) {
        remove(tmp_path);
        free(tmp_path);
        return -1;
    }

    free(tmp_path);
    return 0;
}

int metadata_delete(const char *meta_path)
{
    return remove(meta_path);
}

void metadata_free(SgetMetadata *meta)
{
    if (meta != NULL) {
        if (meta->chunks != NULL) {
            free(meta->chunks);
        }
        free(meta);
    }
}

char *metadata_build_path(const char *filename)
{
    size_t len;
    char *path;

    len = strlen(filename) + strlen(SGET_META_SUFFIX) + 1;
    path = (char *)malloc(len);
    if (path == NULL) return NULL;
    snprintf(path, len, "%s%s", filename, SGET_META_SUFFIX);
    return path;
}
