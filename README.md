# SQL Parser

C 언어로 구현한 파일 기반 SQL 처리기입니다.
INSERT / SELECT 문을 파싱하고 파일에 데이터를 저장하거나 조회합니다.

## 빌드 및 실행

```bash
make                        # 빌드 → ./sqlp 생성
./sqlp samples/insert.sql   # 실행
make test                   # 전체 단위 테스트
make clean                  # 빌드 산출물 삭제
```

---

## 처리 흐름

```
CLI Input → Lexer → Parser → Schema Load & Validate → Executor
```

---

## 주요 기능 소개

### 1. CLI Input

```bash
./sqlp samples/insert.sql
```

사용자는 터미널에서 실행 파일을 실행하고 인자로 SQL 파일 경로를 전달합니다.

`main` 함수에서 인자로 받은 경로를 `input_read_file` 함수에 넘기면, 파일을 열어 내용 전체를 하나의 문자열로 읽어옵니다. 이 문자열이 이후 Lexer로 전달됩니다.

---

### 2. Lexer

Lexer는 input 단계에서 전달받은 긴 문자열을 **의미 있는 토큰으로 잘게 쪼개는** 역할을 합니다.

SQL 문자열을 앞에서부터 한 글자씩 읽으면서, 문자의 종류에 따라 어떤 토큰인지 판단합니다.

| 입력 문자 | 처리 방식 | 생성되는 토큰 |
|-----------|-----------|---------------|
| 공백 / 줄바꿈 | 건너뜀 (줄바꿈은 줄 번호 +1) | — |
| 영문자 / `_` 로 시작하는 단어 | 키워드 목록과 대조 | `TOKEN_SELECT`, `TOKEN_INSERT` 등, 아니면 `TOKEN_IDENT` |
| 숫자 | 연속된 숫자를 모아서 처리 | `TOKEN_INTEGER` |
| `'` 작은따옴표로 감싼 값 | 닫는 `'` 까지 수집 | `TOKEN_STRING` |
| `*` `,` `(` `)` `=` `;` | 문자 하나 그대로 처리 | `TOKEN_STAR`, `TOKEN_LPAREN` 등 고유 토큰 타입 |
| 그 외 알 수 없는 문자 | 에러 출력 후 즉시 중단 | — |

매칭된 토큰들은 `TokenList` 구조체에 배열로 저장됩니다. 배열 크기는 입력 크기에 따라 `realloc` 으로 동적으로 늘어납니다.

파일에 SQL 문장이 여러 개 있을 경우, `;` 기준으로 하나씩 잘라서 Parser에 순서대로 전달합니다.

---

### 3. Parser

Parser는 Lexer가 만든 토큰 배열을 받아서, **토큰들이 올바른 SQL 문법인지 확인하고 의미 있는 정보를 뽑아내는** 역할을 합니다.

가장 먼저 첫 번째 토큰을 확인합니다. `SELECT` 이면 SELECT 파싱 로직으로, `INSERT` 이면 INSERT 파싱 로직으로 분기합니다.

분기 이후에는 토큰을 앞에서부터 순서대로 하나씩 소비하면서, 해당 자리에 와야 할 토큰이 맞는지 확인합니다. 순서가 어긋나는 순간 에러를 출력하고 중단합니다.

파싱에 성공하면 **ASTNode** 구조체를 반환합니다. 실행에 필요한 정보만 구조화해서 담으며, 이후 Schema 검증과 Executor는 이 ASTNode만 보고 동작합니다.

```
┌─ ASTNode ───────────────────────────────────────────────────┐
│  type  StmtType                                             │
│                                                             │
│  ┌─ SelectStmt (type == STMT_SELECT) ──────────────────┐   │
│  │  table         char[64]      대상 테이블명           │   │
│  │  select_all    int           SELECT * 여부           │   │
│  │  columns       char**        컬럼 이름 목록          │   │
│  │  column_count  int           컬럼 수                 │   │
│  │  has_where     int           WHERE 절 유무           │   │
│  │  where ──→ ┌─ WhereClause ──────────────────────┐   │   │
│  │            │  col  char[64]   조건 컬럼명        │   │   │
│  │            │  val  char[256]  비교 값            │   │   │
│  │            └───────────────────────────────────┘   │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                             │
│  ┌─ InsertStmt (type == STMT_INSERT) ──────────────────┐   │
│  │  table         char[64]      대상 테이블명           │   │
│  │  columns       char**        컬럼 이름 목록 (선택)   │   │
│  │  column_count  int           컬럼 수                 │   │
│  │  values        char**        삽입할 값 목록          │   │
│  │  value_count   int           값 수                   │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

---

### 4. Schema Load & Validate

#### Schema Load

ASTNode에는 테이블 이름, 컬럼명, 값만 있습니다. **컬럼의 타입이나 개수** 같은 테이블 구조 정보는 SQL 문장 자체에 없기 때문에, 별도로 저장해둔 스키마 파일에서 읽어옵니다.

`schema/{테이블명}.schema` 파일을 읽어서 **TableSchema** 구조체로 만듭니다. 실제 데이터는 건드리지 않고, 테이블 구조 정보만 가져옵니다.

- 컬럼 개수
- 각 컬럼의 이름, 타입, 최대 길이

스키마 파일 형식:

```
table=users
columns=4
col0=student_no,INT,0
col1=name,VARCHAR,64
col2=gender,VARCHAR,10
col3=is_cs_major,BOOLEAN,0
```

#### Validate

Parser가 해석한 SQL 내용이 실제 테이블 구조와 맞는지 확인합니다. 예를 들어 존재하지 않는 컬럼을 조회하거나, INT 컬럼에 문자열을 넣으려 하면 이 단계에서 걸러냅니다.

**INSERT 검증**
- 값의 개수가 컬럼 수와 일치하는지
- 각 값의 타입이 컬럼 타입과 맞는지 (INT / VARCHAR 최대 길이 / BOOLEAN)

**SELECT 검증**
- `SELECT *` 는 무조건 통과
- 컬럼을 직접 지정했다면 해당 컬럼명이 테이블에 존재하는지
- WHERE 절이 있다면 WHERE에 쓴 컬럼명이 테이블에 존재하는지

검증을 통과하면 ASTNode와 TableSchema가 함께 Executor로 넘어가고, 실패하면 에러 메시지를 출력하고 중단합니다.

---

### 5. Executor

Executor는 ASTNode와 TableSchema를 받아서 **실제로 파일에 데이터를 쓰거나 읽어오는** 마지막 단계입니다.

**INSERT일 때**

ASTNode에 저장된 테이블명과 동일한 이름의 `.dat` 파일을 열어 값들을 한 줄로 씁니다. 컬럼을 지정한 경우에는 스키마 컬럼 순서에 맞게 값을 재배열해서 저장하고, 지정하지 않은 컬럼은 빈 문자열로 채웁니다.

**SELECT일 때**

`data/{테이블명}.dat` 파일을 한 줄씩 읽으면서 WHERE 조건에 맞는 행만 걸러냅니다. 걸러낸 행들로 ResultSet을 조립한 뒤 아래와 같이 출력합니다.

```
+------------+-------+--------+-------------+
| student_no | name  | gender | is_cs_major |
+------------+-------+--------+-------------+
| 1          | alice | female | T           |
+------------+-------+--------+-------------+
(1 rows)
```

출력이 끝나면 ResultSet에 할당된 메모리를 전부 해제하고 종료합니다.

---

## 빌드 상세

```bash
gcc -std=c99 -Wall -Wextra -Iinclude -o sqlp \
    src/main.c \
    src/input/input.c \
    src/input/lexer.c \
    src/parser/parser.c \
    src/schema/schema.c \
    src/executor/executor.c
```

6개의 .c 파일을 한 번에 컴파일하고 하나의 실행 파일로 링크합니다.

| 옵션 | 설명 |
|------|------|
| `-std=c99` | C99 표준으로 컴파일 |
| `-Wall -Wextra` | 경고 전체 출력 |
| `-Iinclude` | `include/interface.h` 탐색 경로 지정 |
| `-o sqlp` | 출력 파일명 지정 |

---

## 파일 구조

```
include/interface.h      ← 모듈 간 인터페이스 계약
src/main.c               ← 진입점
src/input/               ← CLI Input + Lexer
src/parser/              ← Parser
src/schema/              ← Schema Load & Validate
src/executor/            ← Executor
schema/                  ← 테이블 스키마 정의 파일
data/                    ← 테이블 데이터 파일 (자동 생성)
samples/                 ← 테스트용 SQL 파일
tests/                   ← 단위 테스트
```

---

## 팀 구성

| 모듈 | 담당자 |
|------|--------|
| Input + Lexer | 박민석 |
| Parser | 김주형 |
| Schema + Validate | 김민철 |
| Executor | 김은재 |
