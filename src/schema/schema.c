#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../../include/interface.h"

/* =========================================================
 * 모듈3: 스키마 로딩 + 검증 (김민철)
 * ========================================================= */

/* 문자열이 모두 숫자(또는 음수 부호)인지 검사 */
static int is_integer_string(const char *s) {
    if (!s || *s == '\0') return 0;
    int i = 0;
    if (s[0] == '-') i = 1;
    if (s[i] == '\0') return 0;
    for (; s[i] != '\0'; i++) {
        if (!isdigit((unsigned char)s[i])) return 0;
    }
    return 1;
}

/* 스키마에서 컬럼명이 존재하는지 확인 */
static int find_column(const TableSchema *schema, const char *col_name) {
    for (int i = 0; i < schema->column_count; i++) {
        if (strcmp(schema->columns[i].name, col_name) == 0) return 1;
    }
    return 0;
}

/* --------------------------------------------------------- */

TableSchema *schema_load(const char *table_name) {
    if (!table_name) return NULL;

    char path[256];
    snprintf(path, sizeof(path), "schema/%s.schema", table_name);

    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "schema: cannot open '%s'\n", path);
        return NULL;
    }

    TableSchema *s = calloc(1, sizeof(TableSchema));
    if (!s) { fclose(fp); return NULL; }

    char line[512];
    int col_count = 0;

    /* 1단계: table= 과 columns= 라인 파싱 */
    while (fgets(line, sizeof(line), fp)) {
        /* 줄 끝 개행 제거 */
        line[strcspn(line, "\r\n")] = '\0';

        if (strncmp(line, "table=", 6) == 0) {
            strncpy(s->table_name, line + 6, sizeof(s->table_name) - 1);
        } else if (strncmp(line, "columns=", 8) == 0) {
            col_count = atoi(line + 8);
            if (col_count <= 0) {
                fprintf(stderr, "schema: invalid column count in '%s'\n", path);
                free(s);
                fclose(fp);
                return NULL;
            }
            s->column_count = col_count;
            s->columns = calloc(col_count, sizeof(ColDef));
            if (!s->columns) {
                free(s);
                fclose(fp);
                return NULL;
            }
        } else if (strncmp(line, "col", 3) == 0) {
            /* col0=id,INT,0 형태 파싱 */
            char *eq = strchr(line, '=');
            if (!eq) continue;

            /* 컬럼 인덱스 추출 */
            int idx = atoi(line + 3);
            if (idx < 0 || idx >= col_count || !s->columns) continue;

            /* '=' 뒤의 값: name,TYPE,max_len */
            char *val = eq + 1;
            char name[64] = {0};
            char type_str[32] = {0};
            int max_len = 0;

            /* 쉼표로 분리 */
            char *tok1 = strtok(val, ",");
            char *tok2 = strtok(NULL, ",");
            char *tok3 = strtok(NULL, ",");

            if (!tok1 || !tok2 || !tok3) continue;

            strncpy(name, tok1, sizeof(name) - 1);
            strncpy(type_str, tok2, sizeof(type_str) - 1);
            max_len = atoi(tok3);

            strncpy(s->columns[idx].name, name, sizeof(s->columns[idx].name) - 1);
            s->columns[idx].max_len = max_len;

            if (strcmp(type_str, "INT") == 0) {
                s->columns[idx].type = COL_INT;
            } else if (strcmp(type_str, "VARCHAR") == 0) {
                s->columns[idx].type = COL_VARCHAR;
            } else {
                fprintf(stderr, "schema: unknown type '%s' for column '%s'\n",
                        type_str, name);
                schema_free(s);
                fclose(fp);
                return NULL;
            }
        }
    }

    fclose(fp);

    /* columns= 라인이 없었으면 실패 */
    if (s->column_count == 0 || !s->columns) {
        fprintf(stderr, "schema: missing column definitions in '%s'\n", path);
        schema_free(s);
        return NULL;
    }

    return s;
}

/* --------------------------------------------------------- */

int schema_validate(const ASTNode *node, const TableSchema *schema) {
    if (!node || !schema) return SQL_ERR;

    if (node->type == STMT_INSERT) {
        const InsertStmt *ins = &node->insert;

        /* INSERT 값 개수 == 컬럼 수 */
        if (ins->value_count != schema->column_count) {
            fprintf(stderr, "schema: INSERT expects %d values, got %d\n",
                    schema->column_count, ins->value_count);
            return SQL_ERR;
        }

        /* 각 값의 타입 검사 */
        for (int i = 0; i < ins->value_count; i++) {
            const char *val = ins->values[i];
            ColType expected = schema->columns[i].type;

            if (expected == COL_INT) {
                if (!is_integer_string(val)) {
                    fprintf(stderr, "schema: column '%s' expects INT, got '%s'\n",
                            schema->columns[i].name, val);
                    return SQL_ERR;
                }
            }
            /* VARCHAR 는 문자열이면 통과 */
        }

        return SQL_OK;

    } else if (node->type == STMT_SELECT) {
        const SelectStmt *sel = &node->select;

        /* SELECT * 은 항상 통과 */
        if (sel->select_all) return SQL_OK;

        /* 지정 컬럼명이 스키마에 존재하는지 */
        for (int i = 0; i < sel->column_count; i++) {
            if (!find_column(schema, sel->columns[i])) {
                fprintf(stderr, "schema: unknown column '%s' in SELECT\n",
                        sel->columns[i]);
                return SQL_ERR;
            }
        }

        /* WHERE 절 컬럼 검사 */
        if (sel->has_where) {
            if (!find_column(schema, sel->where.col)) {
                fprintf(stderr, "schema: unknown column '%s' in WHERE\n",
                        sel->where.col);
                return SQL_ERR;
            }
        }

        return SQL_OK;
    }

    fprintf(stderr, "schema: unknown statement type\n");
    return SQL_ERR;
}

/* --------------------------------------------------------- */

void schema_free(TableSchema *schema) {
    if (!schema) return;
    free(schema->columns);
    free(schema);
}
