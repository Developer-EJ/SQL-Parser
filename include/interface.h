#ifndef INTERFACE_H
#define INTERFACE_H

/* =========================================================
 * interface.h — 모듈 경계 계약
 *
 * 규칙:
 *   1. 선언(declaration)만 허용. 구현(definition) 금지.
 *   2. 수정 시 4명 전원 합의 필수.
 *   3. 메모리 소유권은 주석으로 명시한다.
 * ========================================================= */

/* =========================================================
 * 공통 에러 코드
 * ========================================================= */
#define SQL_OK   0
#define SQL_ERR -1

/* =========================================================
 * 모듈1 → 모듈2 : 토큰 목록
 * ========================================================= */
typedef enum {
    TOKEN_SELECT,
    TOKEN_INSERT,
    TOKEN_INTO,
    TOKEN_FROM,
    TOKEN_WHERE,
    TOKEN_VALUES,
    TOKEN_STAR,        /* * */
    TOKEN_COMMA,       /* , */
    TOKEN_LPAREN,      /* ( */
    TOKEN_RPAREN,      /* ) */
    TOKEN_EQ,          /* = */
    TOKEN_IDENT,       /* 테이블명, 컬럼명 */
    TOKEN_STRING,      /* 'alice' */
    TOKEN_INTEGER,     /* 42 */
    TOKEN_EOF
} TokenType;

typedef struct {
    TokenType type;
    char      value[256];  /* 토큰 원문 */
    int       line;        /* 에러 리포팅용 줄 번호 */
} Token;

typedef struct {
    Token *tokens;
    int    count;
} TokenList;

/* 모듈1 구현 (박민석) — 호출자가 free 책임 */
char      *input_read_file(const char *path);   /* 반환값: 호출자가 free() */
TokenList *lexer_tokenize(const char *sql);      /* 반환값: 호출자가 lexer_free() */
void       lexer_free(TokenList *list);

/* =========================================================
 * 모듈2 → 모듈3/4 : AST
 * ========================================================= */
typedef enum {
    STMT_SELECT,
    STMT_INSERT
} StmtType;

typedef struct {
    char col[64];   /* WHERE 컬럼명 */
    char val[256];  /* WHERE 비교값 */
} WhereClause;

typedef struct {
    int          select_all;    /* SELECT * 이면 1 */
    char       **columns;       /* SELECT col1, col2 → ["col1","col2"] */
    int          column_count;
    char         table[64];
    int          has_where;
    WhereClause  where;
} SelectStmt;

typedef struct {
    char   table[64];
    char **values;       /* VALUES 안의 값 목록 */
    int    value_count;
} InsertStmt;

typedef struct {
    StmtType type;
    union {
        SelectStmt select;
        InsertStmt insert;
    };
} ASTNode;

/* 모듈2 구현 (김주형) — 호출자가 free 책임 */
ASTNode *parser_parse(TokenList *tokens);  /* 반환값: 호출자가 parser_free() */
void     parser_free(ASTNode *node);

/* =========================================================
 * 모듈3 → 모듈4 : 스키마
 * ========================================================= */
typedef enum {
    COL_INT,
    COL_VARCHAR
} ColType;

typedef struct {
    char    name[64];
    ColType type;
    int     max_len;    /* VARCHAR 전용, INT 는 0 */
} ColDef;

typedef struct {
    char    table_name[64];
    ColDef *columns;
    int     column_count;
} TableSchema;

/* 모듈3 구현 (김민철) — 호출자가 free 책임 */
TableSchema *schema_load(const char *table_name);              /* schema/{name}.schema 읽기 */
int          schema_validate(const ASTNode *node,
                             const TableSchema *schema);       /* SQL_OK or SQL_ERR */
void         schema_free(TableSchema *schema);

/* =========================================================
 * 모듈4 : 실행 결과
 * ========================================================= */
typedef struct {
    char **values;
    int    count;
} Row;

typedef struct {
    char **col_names;
    int    col_count;
    Row   *rows;
    int    row_count;
} ResultSet;

/* 모듈4 구현 (김은재) — 호출자가 free 책임 */
int        executor_run(const ASTNode *node, const TableSchema *schema);
ResultSet *db_select(const SelectStmt *stmt, const TableSchema *schema); /* 반환값: 호출자가 result_free() */
int        db_insert(const InsertStmt *stmt, const TableSchema *schema);
void       result_free(ResultSet *rs);

#endif /* INTERFACE_H */
