#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../include/interface.h"

#define PASS(msg) printf("[PASS] %s\n", msg)

static ASTNode *parse_str(const char *sql) {
    TokenList *tokens = lexer_tokenize(sql);
    assert(tokens != NULL);
    ASTNode *ast = parser_parse(tokens);
    lexer_free(tokens);
    return ast;
}

static int test_select_star(void) {
    ASTNode *ast = parse_str("SELECT * FROM users");
    assert(ast != NULL);
    assert(ast->type == STMT_SELECT);
    assert(ast->select.select_all == 1);
    assert(strcmp(ast->select.table, "users") == 0);
    parser_free(ast);
    PASS("parse: SELECT * FROM users");
    return 0;
}

static int test_select_columns(void) {
    ASTNode *ast = parse_str("SELECT id, name FROM users");
    assert(ast != NULL);
    assert(ast->type == STMT_SELECT);
    assert(ast->select.select_all == 0);
    assert(ast->select.column_count == 2);
    assert(strcmp(ast->select.columns[0], "id") == 0);
    assert(strcmp(ast->select.columns[1], "name") == 0);
    parser_free(ast);
    PASS("parse: SELECT id, name FROM users");
    return 0;
}

static int test_select_where(void) {
    ASTNode *ast = parse_str("SELECT * FROM users WHERE id = 1");
    assert(ast != NULL);
    assert(ast->select.has_where == 1);
    assert(strcmp(ast->select.where.col, "id") == 0);
    assert(strcmp(ast->select.where.val, "1") == 0);
    parser_free(ast);
    PASS("parse: SELECT * FROM users WHERE id = 1");
    return 0;
}

static int test_insert(void) {
    ASTNode *ast = parse_str("INSERT INTO users VALUES (1, 'alice', 30)");
    assert(ast != NULL);
    assert(ast->type == STMT_INSERT);
    assert(strcmp(ast->insert.table, "users") == 0);
    assert(ast->insert.value_count == 3);
    assert(strcmp(ast->insert.values[0], "1") == 0);
    assert(strcmp(ast->insert.values[1], "alice") == 0);
    assert(strcmp(ast->insert.values[2], "30") == 0);
    parser_free(ast);
    PASS("parse: INSERT INTO users VALUES (1, 'alice', 30)");
    return 0;
}

static int test_invalid_returns_null(void) {
    ASTNode *ast = parse_str("GARBAGE SQL !!!");
    assert(ast == NULL);
    PASS("parse: invalid SQL returns NULL");
    return 0;
}

int main(void) {
    int fail = 0;
    printf("=== test_parser ===\n");
    fail += test_select_star();
    fail += test_select_columns();
    fail += test_select_where();
    fail += test_insert();
    fail += test_invalid_returns_null();
    printf("=== %s ===\n", fail == 0 ? "ALL PASS" : "FAILED");
    return fail;
}
