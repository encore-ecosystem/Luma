/* Checked CSS reads for both the legacy and current std packages. No shared
 * error state: success is returned through a caller-owned byte. */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void *encore_str_from_cstr(const char *);
extern encore_str encore_str_from_buffer(const void *, size_t);

static encore_str css_file_error(const char *message) {
    return (encore_str){encore_str_from_cstr(message)};
}

encore_str luma_css_read_file(encore_str path, uint8_t *success) {
    if (!success) return css_file_error("missing read status");
    *success = 0;
    if (!path.object || memchr(path.object->data, 0, path.object->len))
        return css_file_error("invalid stylesheet path");
    char *name = malloc(path.object->len + 1);
    if (!name) return css_file_error("cannot allocate stylesheet path");
    memcpy(name, path.object->data, path.object->len);
    name[path.object->len] = 0;
    FILE *file = fopen(name, "rb");
    free(name);
    if (!file) return css_file_error(strerror(errno));
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return css_file_error("cannot seek stylesheet");
    }
    long end = ftell(file);
    if (end < 0 || (uintmax_t)end >= SIZE_MAX || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return css_file_error("cannot determine stylesheet size");
    }
    size_t size = (size_t)end;
    char *bytes = malloc(size + 1);
    if (!bytes) {
        fclose(file);
        return css_file_error("cannot allocate stylesheet contents");
    }
    size_t count = fread(bytes, 1, size, file);
    int failed = ferror(file);
    int closed = fclose(file);
    if (count != size || failed || closed != 0) {
        free(bytes);
        return css_file_error("cannot read complete stylesheet");
    }
    encore_str result = encore_str_from_buffer(bytes, size);
    free(bytes);
    *success = 1;
    return result;
}
