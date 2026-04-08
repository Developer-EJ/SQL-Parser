#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/interface.h"

/* =========================================================
 * TODO (김주형): 아래 stub을 실제 구현으로 교체한다.
 *
 * 구현 순서 제안:
 *   1. 토큰 소비 헬퍼 (peek, consume, expect)
 *   2. parse_select() 구현
 *   3. parse_insert() 구현
 *   4. parser_free() 에서 내부 동적 할당 해제 확인
 * ========================================================= */

/* 헬퍼: 현재 위치 토큰 반환 */
static Token *peek(TokenList *tokens, int pos) {
    if (pos >= tokens->count) return &tokens->tokens[tokens->count - 1]; /* EOF */
    return &tokens->tokens[pos];
}

/* 헬퍼: 기대 토큰 소비. 맞으면 pos 증가 후 SQL_OK, 틀리면 SQL_ERR */
static int expect(TokenList *tokens, int *pos, TokenType type) {
    if (peek(tokens, *pos)->type != type) {
        fprintf(stderr, "parse error: unexpected token '%s' at line %d\n",
                peek(tokens, *pos)->value,
                peek(tokens, *pos)->line);
        return SQL_ERR;
    }
    (*pos)++;
    return SQL_OK;
}
(void)expect; /* TODO 구현 전 경고 억제 */

ASTNode *parser_parse(TokenList *tokens) {
    /* TODO: 김주형 — 실제 파서 구현 */
    if (!tokens || tokens->count == 0) return NULL;

    /* stub: 항상 NULL 반환 (테스트 실패 유도) */
    fprintf(stderr, "parser: not implemented yet\n");
    return NULL;
}

void parser_free(ASTNode *node) {
    if (!node) return;

    if (node->type == STMT_SELECT) {
        if (node->select.columns) {
            for (int i = 0; i < node->select.column_count; i++)
                free(node->select.columns[i]);
            free(node->select.columns);
        }
    } else if (node->type == STMT_INSERT) {
        if (node->insert.values) {
            for (int i = 0; i < node->insert.value_count; i++)
                free(node->insert.values[i]);
            free(node->insert.values);
        }
    }

    free(node);
}
