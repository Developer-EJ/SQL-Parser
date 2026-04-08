#include <stdio.h>
#include <stdlib.h>
#include "../../include/interface.h"

/*
 * input_read_file
 * Read a whole file and return null-terminated SQL text.
 * Caller must free() the returned string.
 */
// main.c line 33. 파일을 읽어 들이는 과정에서 에러 체크, 성공적이면 buffer 반환
char *input_read_file(const char *path) {
    // 파일 경로가 없다면 NULL 반환
    if (!path) return NULL;

    // path 경로 아래에서 "rb" 모드(모드 문자열)를 받아 FILE * 형식 반환
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "input: cannot open file '%s'\n", path);
        return NULL;
    }

    // 파일 커서 이동(읽기/쓰기) 위치
    // cursor move start to end 
    /*
        SEEK_SET 시작점
        SEEK_CUR 현재 위치 기준
        SEEK_END 끝 위치 기준
    */
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return NULL;
    }
    // return switch current cursor location to byte
    long size = ftell(fp);
    if (size < 0) {
        fclose(fp);
        return NULL;
    }

    // empty in fp, return NULL;
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return NULL;
    }

    // 문자열인 만큼 '\0'을 계산하여야하기에 +1해서 공간 할당
    // 메모리 할당 실패 시, 파일 닫고 NULL 반환
    char *buf = malloc((size_t)size + 1); // inst
    if (!buf) {
        fclose(fp);
        return NULL;
    }


    size_t read = fread(buf, 1, (size_t)size, fp);
    if (ferror(fp) || read != (size_t)size) {
        free(buf);
        fclose(fp);
        return NULL;
    }

    buf[read] = '\0';
    fclose(fp);
    return buf;
}
