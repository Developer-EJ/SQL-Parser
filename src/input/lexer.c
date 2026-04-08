#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../../include/interface.h"

/* =========================================================
 * TODO (박민석): 아래 stub을 실제 구현으로 교체한다.
 *
 * 구현 순서 제안:
 *   1. 공백/줄바꿈 스킵 로직
 *   2. 키워드 vs 식별자 구분 (대소문자 무시)
 *   3. 문자열 리터럴 ('...')
 *   4. 정수 리터럴
 *   5. 단일 문자 기호 (* , ( ) =)
 *   6. 알 수 없는 문자 에러 처리
 * ========================================================= */

/* 키워드 테이블 */
static struct { const char *word; TokenType type; } keywords[] = {
    {"SELECT", TOKEN_SELECT},
    {"INSERT", TOKEN_INSERT},
    {"INTO",   TOKEN_INTO},
    {"FROM",   TOKEN_FROM},
    {"WHERE",  TOKEN_WHERE},
    {"VALUES", TOKEN_VALUES},
    {NULL, TOKEN_EOF}
};

static TokenType keyword_lookup(const char *word) {
    char upper[64];
    int i;
    for (i = 0; word[i] && i < 63; i++)
        upper[i] = (char)toupper((unsigned char)word[i]);
    upper[i] = '\0';

    for (int k = 0; keywords[k].word; k++)
        if (strcmp(upper, keywords[k].word) == 0)
            return keywords[k].type;
    return TOKEN_IDENT;
}

TokenList *lexer_tokenize(const char *sql) {
    /* TODO: 박민석 — 실제 토크나이저 구현 */
    (void)sql;
    (void)keyword_lookup;  /* 사용 전까지 경고 억제 */

    TokenList *list = calloc(1, sizeof(TokenList));
    if (!list) return NULL;

    /* stub: EOF 토큰 하나만 반환 */
    list->tokens = calloc(1, sizeof(Token));
    if (!list->tokens) { free(list); return NULL; }
    list->tokens[0].type = TOKEN_EOF;
    list->count = 1;
    return list;
}

void lexer_free(TokenList *list) {
    if (!list) return;
    free(list->tokens);
    free(list);
}
