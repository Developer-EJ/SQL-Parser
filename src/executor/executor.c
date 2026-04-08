#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/interface.h"

/* =========================================================
 * TODO (김은재): 아래 stub을 실제 구현으로 교체한다.
 *
 * 데이터 파일 위치: data/{table_name}.dat
 * 파일 포맷 (src/executor/CLAUDE.md 참고):
 *   1,alice,30
 *   2,bob,25
 *
 * 구현 순서 제안:
 *   1. db_insert — append 모드로 CSV 한 줄 쓰기
 *   2. db_select — 파일 전체 읽기 + WHERE 필터
 *   3. SELECT * vs SELECT col1,col2 처리
 *   4. result_free — 모든 동적 메모리 해제
 * ========================================================= */

int executor_run(const ASTNode *node, const TableSchema *schema) {
    if (!node || !schema) return SQL_ERR;

    switch (node->type) {
        case STMT_INSERT:
            return db_insert(&node->insert, schema);
        case STMT_SELECT: {
            ResultSet *rs = db_select(&node->select, schema);
            /* 출력은 main.c 에서 처리하므로 여기서는 실행만 */
            result_free(rs);
            return SQL_OK;
        }
        default:
            fprintf(stderr, "executor: unknown statement type\n");
            return SQL_ERR;
    }
}

int db_insert(const InsertStmt *stmt, const TableSchema *schema) {
    /* TODO: 김은재 — 실제 구현 */
    (void)schema;

    char path[256];
    snprintf(path, sizeof(path), "data/%s.dat", stmt->table);

    FILE *fp = fopen(path, "a");
    if (!fp) {
        fprintf(stderr, "executor: cannot open '%s'\n", path);
        return SQL_ERR;
    }

    /* stub: 값을 쉼표로 이어서 한 줄 기록 */
    for (int i = 0; i < stmt->value_count; i++) {
        fprintf(fp, "%s", stmt->values[i]);
        if (i < stmt->value_count - 1) fprintf(fp, ",");
    }
    fprintf(fp, "\n");
    fclose(fp);
    return SQL_OK;
}

ResultSet *db_select(const SelectStmt *stmt, const TableSchema *schema) {
    /* TODO: 김은재 — 실제 구현 */
    (void)stmt;
    (void)schema;

    /* stub: 빈 ResultSet 반환 */
    ResultSet *rs = calloc(1, sizeof(ResultSet));
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
