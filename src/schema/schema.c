#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../../include/interface.h"

/* =========================================================
 * 모듈3: 스키마 로딩 + 검증 (김민철)
 *
 * 이 파일이 하는 일:
 *   1. schema_load()    — schema/ 폴더에서 테이블 정의 파일을 읽어옴
 *   2. schema_validate()— 파서(모듈2)가 만든 AST가 스키마에 맞는지 검사
 *   3. schema_free()    — 동적 할당한 스키마 메모리 해제
 *
 * 스키마 파일 예시 (schema/users.schema):
 *   table=users
 *   columns=3
 *   col0=id,INT,0
 *   col1=name,VARCHAR,64
 *   col2=age,INT,0
 * ========================================================= */


/* ---------------------------------------------------------
 * [내부 헬퍼] is_integer_string
 *
 * 문자열이 정수값인지 판단한다.
 * - "42"  → 1 (정수 맞음)
 * - "-7"  → 1 (음수도 허용)
 * - "abc" → 0 (정수 아님)
 *
 * INSERT 검증 시 INT 컬럼에 들어오는 값을 체크할 때 사용.
 * --------------------------------------------------------- */
static int is_integer_string(const char *s) {
    if (!s || *s == '\0') return 0;
    int i = 0;
    if (s[0] == '-') i = 1;       /* 음수 부호 허용 */
    if (s[i] == '\0') return 0;   /* '-' 만 있는 경우 제외 */
    for (; s[i] != '\0'; i++) {
        if (!isdigit((unsigned char)s[i])) return 0;
    }
    return 1;
}

/* ---------------------------------------------------------
 * [내부 헬퍼] find_column
 *
 * 스키마에 해당 컬럼명이 있으면 1, 없으면 0 반환.
 * SELECT / WHERE 검증 시 컬럼명 존재 여부 확인에 사용.
 * --------------------------------------------------------- */
static int find_column(const TableSchema *schema, const char *col_name) {
    for (int i = 0; i < schema->column_count; i++) {
        if (strcmp(schema->columns[i].name, col_name) == 0) return 1;
    }
    return 0;
}


/* ---------------------------------------------------------
 * schema_load
 *
 * 역할: "schema/{table_name}.schema" 파일을 읽어서
 *       TableSchema 구조체를 동적 할당 후 반환한다.
 *
 * 반환값:
 *   - 성공: TableSchema* (호출자가 schema_free()로 해제)
 *   - 실패: NULL (파일 없음, 메모리 부족, 포맷 오류 등)
 *
 * 파싱 순서:
 *   1. "table=" 라인 → 테이블 이름 저장
 *   2. "columns=" 라인 → 컬럼 수 확인 후 ColDef 배열 할당
 *   3. "colN=" 라인 → 각 컬럼의 이름/타입/길이 저장
 * --------------------------------------------------------- */
TableSchema *schema_load(const char *table_name) {
    if (!table_name) return NULL;

    /* 파일 경로 조합: schema/users.schema 형태 */
    char path[256];
    snprintf(path, sizeof(path), "schema/%s.schema", table_name);

    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "schema: cannot open '%s'\n", path);
        return NULL;
    }

    /* TableSchema 구조체 초기화 (calloc → 0으로 초기화) */
    TableSchema *s = calloc(1, sizeof(TableSchema));
    if (!s) { fclose(fp); return NULL; }

    char line[512];
    int col_count = 0;

    /* 파일을 한 줄씩 읽으며 파싱 */
    while (fgets(line, sizeof(line), fp)) {
        /* 줄 끝 개행(\n, \r\n) 제거 */
        line[strcspn(line, "\r\n")] = '\0';

        if (strncmp(line, "table=", 6) == 0) {
            /* "table=users" → table_name 에 "users" 저장 */
            strncpy(s->table_name, line + 6, sizeof(s->table_name) - 1);

        } else if (strncmp(line, "columns=", 8) == 0) {
            /* "columns=3" → ColDef 배열을 3개 크기로 할당 */
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
            /* "col0=id,INT,0" 형태 파싱
             * - col 뒤의 숫자 → 배열 인덱스
             * - '=' 이후 → 이름, 타입, max_len을 쉼표로 분리 */
            char *eq = strchr(line, '=');
            if (!eq) continue;

            int idx = atoi(line + 3);   /* col0 → 0, col1 → 1 */
            if (idx < 0 || idx >= col_count || !s->columns) continue;

            char *val = eq + 1;         /* '=' 뒤 문자열 */
            char name[64]     = {0};
            char type_str[32] = {0};
            int  max_len      = 0;

            /* strtok으로 쉼표 기준 분리: "id", "INT", "0" */
            char *tok1 = strtok(val, ",");   /* 컬럼명 */
            char *tok2 = strtok(NULL, ",");  /* 타입 */
            char *tok3 = strtok(NULL, ",");  /* max_len */

            if (!tok1 || !tok2 || !tok3) continue;

            strncpy(name,     tok1, sizeof(name) - 1);
            strncpy(type_str, tok2, sizeof(type_str) - 1);
            max_len = atoi(tok3);

            strncpy(s->columns[idx].name, name, sizeof(s->columns[idx].name) - 1);
            s->columns[idx].max_len = max_len;

            /* 타입 문자열 → ColType enum 변환 */
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

    /* columns= 라인이 없었거나 컬럼이 0개면 비정상 파일 */
    if (s->column_count == 0 || !s->columns) {
        fprintf(stderr, "schema: missing column definitions in '%s'\n", path);
        schema_free(s);
        return NULL;
    }

    return s;
}


/* ---------------------------------------------------------
 * schema_validate
 *
 * 역할: 파서가 만든 AST(SQL 구문 트리)가 스키마 규칙에
 *       맞는지 검사한다.
 *
 * 반환값: SQL_OK(0) 또는 SQL_ERR(-1)
 *
 * INSERT 검증 항목:
 *   - VALUES 개수가 컬럼 수와 일치해야 함
 *   - INT 컬럼에는 숫자 값만 허용
 *
 * SELECT 검증 항목:
 *   - SELECT * 이면 무조건 통과
 *   - 지정한 컬럼명이 스키마에 존재해야 함
 *   - WHERE 절의 컬럼명도 스키마에 존재해야 함
 * --------------------------------------------------------- */
int schema_validate(const ASTNode *node, const TableSchema *schema) {
    if (!node || !schema) return SQL_ERR;

    if (node->type == STMT_INSERT) {
        const InsertStmt *ins = &node->insert;

        /* VALUES 개수 검사
         * 예: 컬럼이 3개인데 VALUES (1, 'alice') 처럼 2개만 넣으면 에러 */
        if (ins->value_count != schema->column_count) {
            fprintf(stderr, "schema: INSERT expects %d values, got %d\n",
                    schema->column_count, ins->value_count);
            return SQL_ERR;
        }

        /* 각 값의 타입 검사
         * INT 컬럼에 문자열이 들어오면 에러
         * VARCHAR 는 어떤 값이든 통과 */
        for (int i = 0; i < ins->value_count; i++) {
            const char *val      = ins->values[i];
            ColType     expected = schema->columns[i].type;

            if (expected == COL_INT && !is_integer_string(val)) {
                fprintf(stderr, "schema: column '%s' expects INT, got '%s'\n",
                        schema->columns[i].name, val);
                return SQL_ERR;
            }
        }

        return SQL_OK;

    } else if (node->type == STMT_SELECT) {
        const SelectStmt *sel = &node->select;

        /* SELECT * 은 항상 통과 */
        if (sel->select_all) return SQL_OK;

        /* SELECT col1, col2 — 각 컬럼명이 스키마에 있는지 확인 */
        for (int i = 0; i < sel->column_count; i++) {
            if (!find_column(schema, sel->columns[i])) {
                fprintf(stderr, "schema: unknown column '%s' in SELECT\n",
                        sel->columns[i]);
                return SQL_ERR;
            }
        }

        /* WHERE col = val — WHERE 절 컬럼명도 스키마에 있는지 확인 */
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


/* ---------------------------------------------------------
 * schema_free
 *
 * 역할: schema_load()가 할당한 메모리를 전부 해제한다.
 *   1. columns 배열 해제
 *   2. TableSchema 구조체 자체 해제
 *
 * NULL을 넘겨도 안전하게 처리.
 * --------------------------------------------------------- */
void schema_free(TableSchema *schema) {
    if (!schema) return;
    free(schema->columns);   /* ColDef 배열 해제 */
    free(schema);             /* TableSchema 구조체 해제 */
}
