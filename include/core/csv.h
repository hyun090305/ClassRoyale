#ifndef CORE_CSV_H
#define CORE_CSV_H

#include <stddef.h>

int csv_ensure_dir(const char *path);
int csv_append_row(const char *path, const char *fmt, ...);
int csv_read_last_lines(const char *path, int max_lines, char **out_buf, size_t *out_len);

#endif // CORE_CSV_H
