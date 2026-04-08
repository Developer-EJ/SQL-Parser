#include <stdio.h>
#include <stdlib.h>
#include "../include/interface.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <sql_file>\n", argv[0]);
        return 1;
    }

    /* 1. 파일 읽기 (모듈1) */
    char *sql = input_read_file(argv[1]);
    if (!sql) {
        fprintf(stderr, "Error: cannot open '%s'\n", argv[1]);
        return 1;
    }

    /* 2. 토크나이징 (모듈1) */
    TokenList *tokens = lexer_tokenize(sql);
    free(sql);
    if (!tokens) {
        fprintf(stderr, "Error: tokenization failed\n");
        return 1;
    }

    /* 3. 파싱 (모듈2) */
    ASTNode *ast = parser_parse(tokens);
    lexer_free(tokens);
    if (!ast) {
        fprintf(stderr, "Error: parsing failed\n");
        return 1;
    }

    /* 4. 스키마 로딩 (모듈3) */
    const char *table = (ast->type == STMT_SELECT)
                      ? ast->select.table
                      : ast->insert.table;

    TableSchema *schema = schema_load(table);
    if (!schema) {
        fprintf(stderr, "Error: schema not found for table '%s'\n", table);
        parser_free(ast);
        return 1;
    }

    /* 5. 스키마 검증 (모듈3) */
    if (schema_validate(ast, schema) != SQL_OK) {
        fprintf(stderr, "Error: schema validation failed\n");
        schema_free(schema);
        parser_free(ast);
        return 1;
    }

    /* 6. 실행 (모듈4) */
    if (executor_run(ast, schema) != SQL_OK) {
        fprintf(stderr, "Error: execution failed\n");
        schema_free(schema);
        parser_free(ast);
        return 1;
    }

    schema_free(schema);
    parser_free(ast);
    return 0;
}
