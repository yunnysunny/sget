#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "download.h"
#include "metadata.h"

/* ParseURL is currently an internal entry point; exercise it without exposing a new API. */
extern bool ParseURL(const char *url, char *host, char *port, char *path, char *filename);

static int check_url(const char *url, const char *expected_host,
    const char *expected_port, const char *expected_path, const char *expected_name)
{
    char host[HOST_STR_LEN] = {0};
    char port[HTTP_PORT_LEN] = {0};
    char path[URL_STR_LEN] = {0};
    char name[URL_FILENAME_LEN] = {0};
    if (!ParseURL(url, host, port, path, name) ||
        strcmp(host, expected_host) != 0 || strcmp(port, expected_port) != 0 ||
        strcmp(path, expected_path) != 0 || strcmp(name, expected_name) != 0) {
        fprintf(stderr, "Unexpected URL parse result: %s\n", url);
        return 0;
    }
    return 1;
}

static int check_invalid_url(const char *url)
{
    char host[HOST_STR_LEN] = {0};
    char port[HTTP_PORT_LEN] = {0};
    char path[URL_STR_LEN] = {0};
    char name[URL_FILENAME_LEN] = {0};
    if (ParseURL(url, host, port, path, name)) {
        fprintf(stderr, "Accepted invalid URL: %s\n", url);
        return 0;
    }
    return 1;
}

static int check_validator_roundtrip(const char *meta_path)
{
    SgetMetadata *meta;
    SgetMetadata *loaded;
    int ok = 0;

    meta = metadata_create("http://example.com/v", "v", 4096, 2, "\"abc123\"");
    if (meta == NULL || strcmp(meta->validator, "\"abc123\"") != 0) {
        fprintf(stderr, "Failed to store validator on create\n");
        metadata_free(meta);
        return 0;
    }
    if (metadata_save(meta, meta_path) != 0) {
        fprintf(stderr, "Failed to save metadata with validator\n");
        metadata_free(meta);
        return 0;
    }
    metadata_free(meta);

    loaded = metadata_load(meta_path);
    if (loaded == NULL || strcmp(loaded->validator, "\"abc123\"") != 0) {
        fprintf(stderr, "Validator did not survive a metadata round-trip\n");
        metadata_free(loaded);
        return 0;
    }
    metadata_free(loaded);

    /* An empty validator must still load: cross-process resume without an ETag is legal. */
    meta = metadata_create("http://example.com/v", "v", 4096, 2, NULL);
    if (meta == NULL || meta->validator[0] != '\0' ||
        metadata_save(meta, meta_path) != 0) {
        fprintf(stderr, "Failed to create metadata without validator\n");
        metadata_free(meta);
        return 0;
    }
    metadata_free(meta);

    loaded = metadata_load(meta_path);
    if (loaded == NULL || loaded->validator[0] != '\0') {
        fprintf(stderr, "Empty validator must be accepted on load\n");
        metadata_free(loaded);
        return 0;
    }
    metadata_free(loaded);
    ok = 1;
    metadata_delete(meta_path);
    return ok;
}

int main(void)
{
    SgetMetadata *meta;
    SgetMetadata *loaded;
    const char *meta_path = "sget-core-test.sget.meta";
    uint64_t large_size = (UINT64_C(1) << 33) + 31;
    int i;

    if (!check_url("http://example.com", "example.com", "80", "/", "index.html") ||
        !check_url("http://example.com?x=1#frag", "example.com", "80", "/?x=1", "index.html") ||
        !check_url("http://example.com:8080/a.tar?v=2#frag", "example.com", "8080",
            "/a.tar?v=2", "a.tar") ||
        /* Schemes are case-insensitive per RFC 3986. */
        !check_url("HTTP://example.com/a.tar", "example.com", "80", "/a.tar", "a.tar") ||
        !check_invalid_url("https://example.com/file") ||
        !check_invalid_url("http://") ||
        !check_invalid_url("http://example.com:0/file") ||
        !check_invalid_url("http://example.com:65536/file") ||
        !check_invalid_url("http://example.com/file\r\nInjected: value")) return 1;

    if (!check_validator_roundtrip(meta_path)) return 1;

    meta = metadata_create("http://example.com/a", "a", 3, 8, NULL);
    if (meta == NULL || meta->thread_count != 3) {
        fprintf(stderr, "Small-file chunk count is wrong\n");
        metadata_free(meta);
        return 1;
    }
    for (i = 0; i < meta->thread_count; i++) {
        if (meta->chunks[i].start != (uint64_t)i || meta->chunks[i].end != (uint64_t)i) {
            fprintf(stderr, "Small-file chunk range is wrong\n");
            metadata_free(meta);
            return 1;
        }
    }
    metadata_free(meta);

    meta = metadata_create("http://example.com/large", "large", large_size, 4, NULL);
    if (meta == NULL || meta->chunks[3].end != large_size - 1 ||
        metadata_save(meta, meta_path) != 0) {
        fprintf(stderr, "Failed to create or save large-file metadata\n");
        metadata_free(meta);
        return 1;
    }
    loaded = metadata_load(meta_path);
    if (loaded == NULL || loaded->total_size != large_size ||
        loaded->chunks[3].end != large_size - 1) {
        fprintf(stderr, "64-bit metadata round-trip failed\n");
        metadata_free(meta);
        metadata_free(loaded);
        return 1;
    }
    metadata_free(loaded);
    metadata_free(meta);
    if (metadata_delete(meta_path) != 0) return 1;
    puts("core tests passed");
    return 0;
}
