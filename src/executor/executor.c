#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/interface.h"


static void print_result(const ResultSet *rs) {
    if (!rs || rs->row_count == 0) {
        printf("(0 rows)\n");
        return;
    }
    for (int c = 0; c < rs->col_count; c++) {
        printf("%s", rs->col_names[c]);
        if (c < rs->col_count - 1) printf(" | ");
    }
    printf("\n");
    for (int r = 0; r < rs->row_count; r++) {
        for (int c = 0; c < rs->rows[r].count; c++) {
            printf("%s", rs->rows[r].values[c]);
            if (c < rs->rows[r].count - 1) printf(" | ");
        }
        printf("\n");
    }
    printf("(%d row%s)\n", rs->row_count, rs->row_count == 1 ? "" : "s");
}

int executor_run(const ASTNode *node, const TableSchema *schema) {
    if (!node || !schema) return SQL_ERR;

    switch (node->type) {
        case STMT_INSERT:
            return db_insert(&node->insert, schema);
        case STMT_SELECT: {
            ResultSet *rs = db_select(&node->select, schema);
            if (!rs) return SQL_ERR;
            print_result(rs);
            result_free(rs);
            return SQL_OK;
        }
        default:
            fprintf(stderr, "executor: unknown statement type\n");
            return SQL_ERR;
    }
}

int db_insert(const InsertStmt *stmt, const TableSchema *schema) {
    (void)schema;

    char path[256];
    snprintf(path, sizeof(path), "data/%s.dat", stmt->table);

    FILE *fp = fopen(path, "a");
    if (!fp) {
        fprintf(stderr, "executor: cannot open '%s'\n", path);
        return SQL_ERR;
    }

    for (int i = 0; i < stmt->value_count; i++) {
        fprintf(fp, "%s", stmt->values[i]);
        if (i < stmt->value_count - 1) fprintf(fp, ",");
    }
    fprintf(fp, "\n");
    fclose(fp);
    return SQL_OK;
}

ResultSet *db_select(const SelectStmt *stmt, const TableSchema *schema) {
    char path[256];
    snprintf(path, sizeof(path), "data/%s.dat", stmt->table);

    FILE *fp = fopen(path, "r");
    if (!fp) {
        /* 파일 없음 = 데이터 없음, 빈 ResultSet 반환 */
        ResultSet *rs = calloc(1, sizeof(ResultSet));
        if (!rs) return NULL;
        /* col_names 채우기 */
        rs->col_count = schema->column_count;
        rs->col_names = calloc(rs->col_count, sizeof(char *));
        if (!rs->col_names) { free(rs); return NULL; }
        for (int i = 0; i < rs->col_count; i++) {
            rs->col_names[i] = strdup(schema->columns[i].name);
        }
        rs->row_count = 0;
        rs->rows = NULL;
        return rs;
    }

    /* 파일 전체 읽어서 Row 목록 구성 */
    int capacity = 16;
    Row *rows = calloc(capacity, sizeof(Row));
    if (!rows) { fclose(fp); return NULL; }
    int row_count = 0;

    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        /* 줄 끝 개행 제거 */
        int len = (int)strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';
        if (len == 0) continue;

        /* WHERE 필터 */
        if (stmt->has_where) {
            /* 해당 컬럼 인덱스 찾기 */
            int col_idx = -1;
            for (int i = 0; i < schema->column_count; i++) {
                if (strcmp(schema->columns[i].name, stmt->where.col) == 0) {
                    col_idx = i;
                    break;
                }
            }
            if (col_idx >= 0) {
                /* 해당 인덱스의 값 추출 */
                char tmp[1024];
                strncpy(tmp, line, sizeof(tmp));
                char *tok = strtok(tmp, ",");
                for (int i = 0; i < col_idx && tok; i++)
                    tok = strtok(NULL, ",");
                if (!tok || strcmp(tok, stmt->where.val) != 0)
                    continue;  /* 조건 불일치 → 스킵 */
            }
        }

        /* 줄을 쉼표로 분리하여 Row 생성 */
        if (row_count == capacity) {
            capacity *= 2;
            Row *tmp = realloc(rows, capacity * sizeof(Row));
            if (!tmp) break;
            rows = tmp;
        }

        Row *row = &rows[row_count];
        row->count = schema->column_count;
        row->values = calloc(row->count, sizeof(char *));
        if (!row->values) break;

        char buf[1024];
        strncpy(buf, line, sizeof(buf));
        char *tok = strtok(buf, ",");
        for (int i = 0; i < row->count; i++) {
            row->values[i] = strdup(tok ? tok : "");
            tok = strtok(NULL, ",");
        }
        row_count++;
    }
    fclose(fp);

    /* col_names 결정: SELECT * vs SELECT col1, col2 */
    ResultSet *rs = calloc(1, sizeof(ResultSet));
    if (!rs) { /* 메모리 부족 시 rows 해제 */
        for (int i = 0; i < row_count; i++) {
            for (int j = 0; j < rows[i].count; j++) free(rows[i].values[j]);
            free(rows[i].values);
        }
        free(rows);
        return NULL;
    }

    if (stmt->select_all) {
        rs->col_count = schema->column_count;
        rs->col_names = calloc(rs->col_count, sizeof(char *));
        for (int i = 0; i < rs->col_count; i++)
            rs->col_names[i] = strdup(schema->columns[i].name);
        rs->rows = rows;
        rs->row_count = row_count;
    } else {
        /* 지정 컬럼만 추출 */
        rs->col_count = stmt->column_count;
        rs->col_names = calloc(rs->col_count, sizeof(char *));
        int *idx = calloc(rs->col_count, sizeof(int));

        for (int c = 0; c < stmt->column_count; c++) {
            rs->col_names[c] = strdup(stmt->columns[c]);
            idx[c] = -1;
            for (int s = 0; s < schema->column_count; s++) {
                if (strcmp(schema->columns[s].name, stmt->columns[c]) == 0) {
                    idx[c] = s;
                    break;
                }
            }
        }

        rs->rows = calloc(row_count, sizeof(Row));
        rs->row_count = row_count;
        for (int r = 0; r < row_count; r++) {
            rs->rows[r].count = rs->col_count;
            rs->rows[r].values = calloc(rs->col_count, sizeof(char *));
            for (int c = 0; c < rs->col_count; c++) {
                int si = idx[c];
                rs->rows[r].values[c] = strdup(
                    (si >= 0 && si < rows[r].count) ? rows[r].values[si] : "");
            }
            /* 원본 row 해제 */
            for (int j = 0; j < rows[r].count; j++) free(rows[r].values[j]);
            free(rows[r].values);
        }
        free(rows);
        free(idx);
    }

    return rs;
}

void result_free(ResultSet *rs) {
    if (!rs) return;

    for (int i = 0; i < rs->row_count; i++) {
        for (int j = 0; j < rs->rows[i].count; j++)
            free(rs->rows[i].values[j]);
        free(rs->rows[i].values);
    }
    free(rs->rows);

    for (int i = 0; i < rs->col_count; i++)
        free(rs->col_names[i]);
    free(rs->col_names);

    free(rs);
}
