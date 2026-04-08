#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../../include/interface.h"

/* =========================================================
 * lexer
 * TODO: 입력 문자열을 TokenList로 변환
 *
 * 구현 대상:
 *   1. 알파벳/키워드 처리
 *   2. 비교 연산자/기호 처리 (* , ( ) =)
 *   3. 문자열 리터럴 처리('...')
 *   4. 숫자 리터럴 처리
 *   5. 공백/주석 무시
 *   6. EOF 토큰 끝처리
 * ========================================================= */

/* keyword table */
// 적절한 tokentype 반환값이 나오면 대응하는 타입으로 매칭.
static struct {
    const char *word;
    TokenType type;
} keywords[] = {{"SELECT", TOKEN_SELECT}, {"INSERT", TOKEN_INSERT}, {"INTO", TOKEN_INTO},
                {"FROM", TOKEN_FROM},     {"WHERE", TOKEN_WHERE},   {"VALUES", TOKEN_VALUES},
                {NULL, TOKEN_EOF}};

// 입력된 문자열이 키워드인지 검사하는 함수
static TokenType keyword_lookup(const char *word) {
    char upper[64];
    int i;
    for (i = 0; word[i] && i < 63; i++)
        // 상단에 적힌 struct 랑 비교할 수 있게 대문자로 변환
        upper[i] = (char)toupper((unsigned char)word[i]);
    upper[i] = '\0';

    for (int k = 0; keywords[k].word; k++) {
        // compare two strs. if same, return keyword type
        if (strcmp(upper, keywords[k].word) == 0)
            return keywords[k].type;
    }
    return TOKEN_IDENT;
}

static int append_token(TokenList *list, TokenType type, const char *value, int line) {
    if (!list)
        return SQL_ERR;

    if (list->count == 0) {
        list->tokens = calloc(8, sizeof(Token));
    } else if (list->count % 8 == 0) {
        size_t new_cap = (size_t)list->count * 2;
        Token *next = realloc(list->tokens, new_cap * sizeof(Token));
        if (!next)
            return SQL_ERR;
        list->tokens = next;
    }

    if (!list->tokens)
        return SQL_ERR;

    Token *tok = &list->tokens[list->count++];
    tok->type = type;
    tok->line = line;
    if (value) {
        strncpy(tok->value, value, sizeof(tok->value) - 1);
        tok->value[sizeof(tok->value) - 1] = '\0';
    } else {
        tok->value[0] = '\0';
    }
    return SQL_OK;
}

// input.c에서 읽어서 작성한 buf 파일을 입력받아 한 글자씩 스캔합니다.
TokenList *lexer_tokenize(const char *sql) {
    if (!sql)
        return NULL;

    TokenList *list = malloc(sizeof(TokenList));
    if (!list)
        return NULL;
    list->tokens = NULL;
    list->count = 0;

    int line = 1;
    const char *p = sql;

    // 입력받은 문자열이 없어질때까지 반복
    while (*p) {
        //  공백인지 확인하기 위해 unsigned
        while (isspace((unsigned char)*p)) {
            if (*p == '\n')
                line++;
            p++;
        }
        if (!*p)
            break;

        if (isalpha((unsigned char)*p) || *p == '_') {
            const char *start = p;
            while (isalnum((unsigned char)*p) || *p == '_')
                p++;
            size_t len = (size_t)(p - start);

            char word[64];
            if (len >= sizeof(word))
                len = sizeof(word) - 1;
            memcpy(word, start, len);
            word[len] = '\0';

            TokenType type = keyword_lookup(word);
            if (append_token(list, type, word, line) == SQL_ERR)
                goto fail;
            continue;
        }

        if (isdigit((unsigned char)*p)) {
            const char *start = p;
            while (isdigit((unsigned char)*p))
                p++;
            size_t len = (size_t)(p - start);

            char num[256];
            if (len >= sizeof(num))
                len = sizeof(num) - 1;
            memcpy(num, start, len);
            num[len] = '\0';

            if (append_token(list, TOKEN_INTEGER, num, line) == SQL_ERR)
                goto fail;
            continue;
        }

        if (*p == '\'') {
            p++;
            const char *start = p;
            while (*p && *p != '\'') {
                p++;
            }
            if (!*p) {
                fprintf(stderr, "lexer: unterminated string literal at line %d\n", line);
                goto fail;
            }

            size_t len = (size_t)(p - start);
            char value[256];
            if (len >= sizeof(value))
                len = sizeof(value) - 1;
            memcpy(value, start, len);
            value[len] = '\0';

            if (append_token(list, TOKEN_STRING, value, line) == SQL_ERR)
                goto fail;
            p++;
            continue;
        }

        if (*p == '*') {
            if (append_token(list, TOKEN_STAR, "*", line) == SQL_ERR)
                goto fail;
            p++;
            continue;
        }
        if (*p == ',') {
            if (append_token(list, TOKEN_COMMA, ",", line) == SQL_ERR)
                goto fail;
            p++;
            continue;
        }
        if (*p == '(') {
            if (append_token(list, TOKEN_LPAREN, "(", line) == SQL_ERR)
                goto fail;
            p++;
            continue;
        }
        if (*p == ')') {
            if (append_token(list, TOKEN_RPAREN, ")", line) == SQL_ERR)
                goto fail;
            p++;
            continue;
        }
        if (*p == '=') {
            if (append_token(list, TOKEN_EQ, "=", line) == SQL_ERR)
                goto fail;
            p++;
            continue;
        }

        fprintf(stderr, "lexer: unknown character '%c' at line %d\n", *p, line);
        goto fail;
    }

    if (append_token(list, TOKEN_EOF, "", line) == SQL_ERR)
        goto fail;
    return list;

fail:
    lexer_free(list);
    return NULL;
}

void lexer_free(TokenList *list) {
    if (!list)
        return;
    free(list->tokens);
    free(list);
}
