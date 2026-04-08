#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/interface.h"

/* =========================================================
 * TODO (김민철): 아래 stub을 실제 구현으로 교체한다.
 *
 * 스키마 파일 위치: schema/{table_name}.schema
 * 파일 포맷 (src/schema/CLAUDE.md 참고):
 *   table=users
 *   columns=3
 *   col0=id,INT,0
 *   col1=name,VARCHAR,64
 *   col2=age,INT,0
 *
 * 구현 순서 제안:
 *   1. 파일 열기 + 라인 파싱 (strtok 또는 sscanf)
 *   2. ColType 파싱 (INT / VARCHAR)
 *   3. schema_validate — INSERT 값 개수/타입 검사
 *   4. schema_validate — SELECT 컬럼명 존재 여부 검사
 * ========================================================= */

TableSchema *schema_load(const char *table_name) {
    /* TODO: 김민철 — 실제 구현 */
    char path[256];
    snprintf(path, sizeof(path), "schema/%s.schema", table_name);

    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "schema: cannot open '%s'\n", path);
        return NULL;
    }
    fclose(fp);

    /* stub: 빈 스키마 반환 */
    TableSchema *s = calloc(1, sizeof(TableSchema));
    if (!s) return NULL;
    strncpy(s->table_name, table_name, sizeof(s->table_name) - 1);
    return s;
}

int schema_validate(const ASTNode *node, const TableSchema *schema) {
    /* TODO: 김민철 — 실제 검증 구현 */
    if (!node || !schema) return SQL_ERR;

    /* stub: 항상 통과 */
    return SQL_OK;
}

void schema_free(TableSchema *schema) {
    if (!schema) return;
    free(schema->columns);
    free(schema);
}
