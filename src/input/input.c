#include <stdio.h>
#include <stdlib.h>
#include "../../include/interface.h"

/*
 * input_read_file
 * 파일 전체를 읽어 동적 할당된 문자열로 반환한다.
 * 반환값: 호출자가 free() 책임. 실패 시 NULL.
 */
char *input_read_file(const char *path) {
    if (!path) return NULL;

    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "input: cannot open file '%s'\n", path);
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    rewind(fp);

    char *buf = malloc(size + 1);
    if (!buf) {
        fclose(fp);
        return NULL;
    }

    size_t read = fread(buf, 1, size, fp);
    buf[read] = '\0';
    fclose(fp);
    return buf;
}
