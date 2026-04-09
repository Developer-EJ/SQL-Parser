#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/interface.h"

/* =========================================================
 * display_width
 *
 * UTF-8 문자열의 화면 표시 너비를 반환한다.
 * 한글 등 2바이트 폭 문자(CJK)는 2칸, ASCII는 1칸으로 계산한다.
 * strlen은 바이트 수를 반환하므로 한글이 포함되면 줄이 맞지 않는다.
 * ========================================================= */
static int display_width(const char *s) {
    int width = 0;
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        if (*p < 0x80) {
            /* ASCII: 1바이트, 1칸 */
            width += 1;
            p += 1;
        } else if ((*p & 0xE0) == 0xC0) {
            /* 2바이트 UTF-8 */
            width += 1;
            p += 2;
        } else if ((*p & 0xF0) == 0xE0) {
            /* 3바이트 UTF-8 (한글, CJK 등) — 화면에서 2칸 차지 */
            width += 2;
            p += 3;
        } else if ((*p & 0xF8) == 0xF0) {
            /* 4바이트 UTF-8 (이모지 등) — 화면에서 2칸 차지 */
            width += 2;
            p += 4;
        } else {
            p += 1;
        }
    }
    return width;
}

/* =========================================================
 * print_pretty_table
 *
 * ResultSet을 보기 좋은 표 형태로 출력한다.
 * 한글 등 멀티바이트 문자도 올바르게 정렬된다.
 *
 * 출력 예시:
 *   +----+-------+-----+
 *   | id | name  | age |
 *   +----+-------+-----+
 *   | 1  | alice | 30  |
 *   +----+-------+-----+
 *   (1 row)
 * ========================================================= */
static void print_pretty_table(ResultSet *rs) {
    if (!rs || rs->row_count == 0) {
        printf("(0 rows)\n");
        return;
    }

    /* 각 컬럼의 최대 표시 너비를 계산한다 */
    int *widths = calloc((size_t)rs->col_count, sizeof(int));
    if (!widths) return;

    for (int c = 0; c < rs->col_count; c++) {
        widths[c] = display_width(rs->col_names[c]);
        for (int r = 0; r < rs->row_count; r++) {
            int w = display_width(rs->rows[r].values[c]);
            if (w > widths[c]) widths[c] = w;
        }
    }

    /* 구분선 출력 (+----+-------+-----+) */
    #define PRINT_SEP() do { \
        for (int c = 0; c < rs->col_count; c++) { \
            printf("+"); \
            for (int w = 0; w < widths[c] + 2; w++) printf("-"); \
        } \
        printf("+\n"); \
    } while(0)

    /* 문자열을 표시 너비에 맞춰 패딩해서 출력한다.
     * %-*s 는 바이트 수 기준이라 한글이 있으면 줄이 맞지 않으므로
     * 표시 너비 차이만큼 공백을 직접 추가한다. */
    #define PRINT_CELL(str, col_width) do { \
        int dw = display_width(str); \
        printf("%s", str); \
        for (int _p = dw; _p < (col_width); _p++) printf(" "); \
    } while(0)

    PRINT_SEP();

    /* 헤더 출력 */
    for (int c = 0; c < rs->col_count; c++) {
        printf("| ");
        PRINT_CELL(rs->col_names[c], widths[c]);
        printf(" ");
    }
    printf("|\n");

    PRINT_SEP();

    /* 데이터 행 출력 */
    for (int r = 0; r < rs->row_count; r++) {
        for (int c = 0; c < rs->rows[r].count; c++) {
            printf("| ");
            PRINT_CELL(rs->rows[r].values[c], widths[c]);
            printf(" ");
        }
        printf("|\n");
    }

    PRINT_SEP();

    printf("(%d row%s)\n", rs->row_count, rs->row_count == 1 ? "" : "s");

    free(widths);
    #undef PRINT_SEP
    #undef PRINT_CELL
}

/* =========================================================
 * split_tokens
 *
 * 토큰 리스트에서 start 부터 다음 TOKEN_SEMICOLON 또는
 * TOKEN_EOF 까지를 잘라서 새 TokenList 를 만든다.
 * 잘라낸 끝에 TOKEN_EOF 를 붙여준다.
 *
 * 반환값: 새 TokenList* (호출자가 lexer_free()로 해제)
 * *next_start 에 다음 시작 인덱스를 저장한다.
 * ========================================================= */
static TokenList *split_tokens(const TokenList *all, int start, int *next_start) {
    int end = start;
    while (end < all->count &&
           all->tokens[end].type != TOKEN_SEMICOLON &&
           all->tokens[end].type != TOKEN_EOF) {
        end++;
    }

    int token_count = end - start;
    if (token_count == 0) {
        if (end < all->count && all->tokens[end].type == TOKEN_SEMICOLON)
            *next_start = end + 1;
        else
            *next_start = end;
        return NULL;
    }

    TokenList *sub = malloc(sizeof(TokenList));
    if (!sub) return NULL;

    sub->count = token_count + 1;
    sub->tokens = calloc((size_t)sub->count, sizeof(Token));
    if (!sub->tokens) {
        free(sub);
        return NULL;
    }

    for (int i = 0; i < token_count; i++) {
        sub->tokens[i] = all->tokens[start + i];
    }

    sub->tokens[token_count].type = TOKEN_EOF;
    sub->tokens[token_count].value[0] = '\0';
    sub->tokens[token_count].line = all->tokens[end > 0 ? end - 1 : 0].line;

    if (end < all->count && all->tokens[end].type == TOKEN_SEMICOLON)
        *next_start = end + 1;
    else
        *next_start = end;

    return sub;
}

/* =========================================================
 * run_statement
 *
 * 하나의 TokenList를 받아서 파싱 → 스키마 로딩 → 검증 → 실행.
 * SELECT 는 pretty table로 출력한다.
 *
 * 반환값: SQL_OK(0) 또는 SQL_ERR(-1)
 * ========================================================= */
static int run_statement(TokenList *tokens) {
    /* 1. 파싱 (모듈2) */
    ASTNode *ast = parser_parse(tokens);
    if (!ast) {
        fprintf(stderr, "Error: parsing failed\n");
        return SQL_ERR;
    }

    /* 2. 스키마 로딩 (모듈3) */
    const char *table = (ast->type == STMT_SELECT)
                      ? ast->select.table
                      : ast->insert.table;

    TableSchema *schema = schema_load(table);
    if (!schema) {
        fprintf(stderr, "Error: schema not found for table '%s'\n", table);
        parser_free(ast);
        return SQL_ERR;
    }

    /* 3. 스키마 검증 (모듈3) */
    if (schema_validate(ast, schema) != SQL_OK) {
        fprintf(stderr, "Error: schema validation failed\n");
        schema_free(schema);
        parser_free(ast);
        return SQL_ERR;
    }

    /* 4. 실행 (모듈4) */
    if (ast->type == STMT_SELECT) {
        /* SELECT: ResultSet을 받아서 pretty table로 출력 */
        ResultSet *rs = db_select(&ast->select, schema);
        print_pretty_table(rs);
        result_free(rs);
    } else {
        /* INSERT */
        if (db_insert(&ast->insert, schema) != SQL_OK) {
            fprintf(stderr, "Error: insert failed\n");
            schema_free(schema);
            parser_free(ast);
            return SQL_ERR;
        }
        printf("1 row inserted.\n");
    }

    schema_free(schema);
    parser_free(ast);
    return SQL_OK;
}

/* =========================================================
 * main
 *
 * SQL 파일을 읽어서 토크나이징한 뒤, ';' 기준으로
 * 토큰을 나눠서 각 SQL 문을 순서대로 실행한다.
 *
 * 사용법: ./sqlp <sql_file>
 *
 * 예시 파일:
 *   INSERT INTO users VALUES (1, 'alice', 30);
 *   INSERT INTO users VALUES (2, 'bob', 25);
 *   SELECT * FROM users;
 * ========================================================= */
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <sql_file>\n", argv[0]);
        return 1;
    }

    /* 1. 파일 전체 읽기 (모듈1) */
    char *sql = input_read_file(argv[1]);
    if (!sql) {
        fprintf(stderr, "Error: cannot open '%s'\n", argv[1]);
        return 1;
    }

    /* 2. 전체 SQL을 한 번에 토크나이징 (모듈1) */
    TokenList *all_tokens = lexer_tokenize(sql);
    free(sql);
    if (!all_tokens) {
        fprintf(stderr, "Error: tokenization failed\n");
        return 1;
    }

    /* 3. TOKEN_SEMICOLON 기준으로 나눠서 각각 실행 */
    int total = 0;
    int fail  = 0;
    int pos   = 0;

    while (pos < all_tokens->count) {
        if (all_tokens->tokens[pos].type == TOKEN_EOF) break;

        int next = 0;
        TokenList *sub = split_tokens(all_tokens, pos, &next);

        if (sub) {
            total++;
            if (run_statement(sub) != SQL_OK) fail++;
            lexer_free(sub);
        }

        pos = next;
    }

    lexer_free(all_tokens);

    /* 여러 SQL 문을 실행한 경우 결과 요약 출력 */
    if (total > 1) {
        printf("\n%d statement(s) executed", total);
        if (fail > 0) printf(", %d failed", fail);
        printf(".\n");
    }

    return fail > 0 ? 1 : 0;
}
