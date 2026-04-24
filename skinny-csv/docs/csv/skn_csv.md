# skn_csv.h

Header-only CSV parser with automatic type inference and configurable separator.

---

## Usage

```c
#define SKN_CSV_IMPLEMENTATION
#include "skn_csv.h"

CsvDocument *doc = csv_load("data.csv", ',');
DatArray col = csv_get(doc, "age");
for (int i = 0; i < col.size; i++)
    printf("%d\n", col.i[i]);
csv_free(doc);
```

Define `SKN_CSV_IMPLEMENTATION` in **exactly one** translation unit.

---

## Data model

```mermaid
classDiagram
    class CsvType {
        <<enumeration>>
        CSV_INT    = 0
        CSV_FLOAT  = 1
        CSV_DOUBLE = 2
        CSV_STRING = 3
    }

    class CsvSchema {
        const char* name
        CsvType type
    }

    class CsvColData {
        <<union>>
        int*    i
        float*  f
        double* d
        char**  s
    }

    class CsvDocument {
        char**      names
        CsvType*    types
        CsvColData* data
        int col_count
        int row_count
    }

    class DatArray {
        <<view — do not free>>
        CsvType type
        int size
        int*    i
        float*  f
        double* d
        char**  s
    }

    CsvDocument "1" --> "col_count" CsvColData : data[]
    CsvDocument "1" --> "col_count" CsvType    : types[]
    CsvColData  "1" --> "1"         CsvType    : selects union member
    DatArray        ..>             CsvDocument : points into (not owned)
```

---

## Call graph

Colors: **blue** = public API · **grey** = internal · **green** = data types.
Each node shows: function name / parameter list / return type.

```mermaid
flowchart TD
    classDef pub      fill:#4a90d9,stroke:#2c5f8a,color:#fff
    classDef internal fill:#888888,stroke:#555555,color:#fff
    classDef dtype    fill:#5cad5c,stroke:#3a7a3a,color:#fff

    %% ── Public API ──────────────────────────────────────────────────
    subgraph PUB["Public API"]
        csv_load["csv_load
        (filename: char*, sep: char)
        → CsvDocument*"]:::pub

        csv_load_schema["csv_load_schema
        (filename: char*, schema: CsvSchema*, schema_count: int, sep: char)
        → CsvDocument*"]:::pub

        csv_parse_string["csv_parse_string
        (text: char*, sep: char)
        → CsvDocument*"]:::pub

        csv_parse_string_schema["csv_parse_string_schema
        (text: char*, schema: CsvSchema*, schema_count: int, sep: char)
        → CsvDocument*"]:::pub

        csv_get["csv_get
        (doc: CsvDocument*, name: char*)
        → DatArray"]:::pub

        csv_get_schema["csv_get_schema
        (doc: CsvDocument*, out: CsvSchema*)
        → void"]:::pub

        csv_print_schema["csv_print_schema
        (doc: CsvDocument*)
        → void"]:::pub

        csv_free["csv_free
        (doc: CsvDocument*)
        → void"]:::pub
    end

    %% ── Parsing pipeline ────────────────────────────────────────────
    subgraph PIPELINE["Parsing pipeline"]
        parse_do["csv__parse_do
        (text: char*, schema: CsvSchema*, schema_count: int, sep: char)
        → CsvDocument*"]:::internal

        parse_cells["csv__parse_cells
        (text: char*, out_names: char***, out_cols: int*,
        out_cells: char***, out_rows: int*, sep: char)
        → int (1 ok / 0 fail)"]:::internal

        build["csv__build
        (names: char**, col_count: int, cells: char**, row_count: int,
        schema: CsvSchema*, schema_count: int)
        → CsvDocument*"]:::internal
    end

    %% ── Field tokeniser ─────────────────────────────────────────────
    subgraph FIELD["Field tokeniser"]
        read_field["csv__read_field
        (p: char**, eol: int*, sep: char)
        → char*"]:::internal

        buf_push["csv__buf_push
        (buf: char**, len: size_t*, cap: size_t*, c: char)
        → int (1 ok / 0 fail)"]:::internal
    end

    %% ── Type system ─────────────────────────────────────────────────
    subgraph TYPES["Type inference & conversion"]
        infer_cell["csv__infer_cell
        (s: char*)
        → CsvType"]:::internal

        infer_col["csv__infer_column
        (cells: char**, row_count: int, col: int, col_count: int)
        → CsvType"]:::internal

        convert_col["csv__convert_column
        (cells: char**, row_count: int, col: int, col_count: int,
        type: CsvType, out: CsvColData*)
        → int (1 ok / 0 fail)"]:::internal
    end

    %% ── Utilities ───────────────────────────────────────────────────
    subgraph UTIL["Utilities"]
        read_file["csv__read_file
        (filename: char*)
        → char*"]:::internal

        strndup["csv__strndup
        (s: char*, n: size_t)
        → char*"]:::internal

        type_name["csv__type_name
        (t: CsvType)
        → const char*"]:::internal
    end

    %% ── Edges ───────────────────────────────────────────────────────
    %% file-loading public → pipeline
    csv_load              --> read_file
    csv_load              --> parse_do
    csv_load_schema       --> read_file
    csv_load_schema       --> parse_do

    %% string-parsing public → pipeline
    csv_parse_string            --> parse_do
    csv_parse_string_schema     --> parse_do

    %% public utilities
    csv_print_schema --> type_name

    %% pipeline internals
    parse_do    --> parse_cells
    parse_do    --> build
    parse_cells --> read_field
    read_field  --> buf_push

    %% build → type system + utilities
    build        --> infer_col
    build        --> convert_col
    build        --> strndup

    %% type system internals
    infer_col    --> infer_cell
    convert_col  --> strndup
```

---

## Local variables

For each function, the local variables it declares and their initial values where relevant.

### csv__strndup
| Variable | Type | Initial value |
|----------|------|---------------|
| `copy` | `char *` | `malloc(n + 1)` |

### csv__buf_push
| Variable | Type | Initial value |
|----------|------|---------------|
| `t` | `char *` | `realloc(*buf, *cap)` — temp for safe realloc |

### csv__read_field
| Variable | Type | Initial value |
|----------|------|---------------|
| `len` | `size_t` | `0` |
| `cap` | `size_t` | `32` |
| `buf` | `char *` | `malloc(cap)` |
| `s` | `const char *` | `*p` |

### csv__infer_cell
| Variable | Type | Initial value |
|----------|------|---------------|
| `p` | `const char *` | `s` — walking pointer |

### csv__infer_column
| Variable | Type | Initial value |
|----------|------|---------------|
| `t` | `CsvType` | `CSV_INT` — running most-general type |
| `ct` | `CsvType` | set per iteration by `csv__infer_cell` |

### csv__convert_column
| Variable | Type | Initial value |
|----------|------|---------------|
| `arr` | `int *` / `float *` / `double *` / `char **` | `malloc(row_count * sizeof(...))` — type depends on `CsvType` |

### csv__parse_cells
| Variable | Type | Initial value |
|----------|------|---------------|
| `p` | `const char *` | `text` (advanced past BOM if present) |
| `col_count` | `int` | `0` |
| `names_cap` | `size_t` | `8` |
| `names` | `char **` | `calloc(names_cap, sizeof(char *))` |
| `eol` | `int` | `0` — reused for each field/row read |
| `row_count` | `int` | `0` |
| `cells_cap` | `size_t` | `col_count * 16` |
| `cells` | `char **` | `calloc(cells_cap, sizeof(char *))` |
| `old_cap` | `size_t` | previous `cells_cap` before doubling — used for `memset` |
| `col` | `int` | `0` — column index within the current row |

### csv__build
| Variable | Type | Initial value |
|----------|------|---------------|
| `doc` | `CsvDocument *` | `malloc(sizeof(CsvDocument))` |
| `type` | `CsvType` | set per column — from schema or `csv__infer_column` |
| `found` | `int` | `0` — set to `1` if column name matches schema |

### csv__read_file
| Variable | Type | Initial value |
|----------|------|---------------|
| `f` | `FILE *` | `fopen(filename, "r")` |
| `size` | `long` | `ftell(f)` after `fseek(SEEK_END)` |
| `buf` | `char *` | `malloc(size + 1)` |
| `nread` | `size_t` | `fread(buf, 1, size, f)` |

### csv__parse_do
| Variable | Type | Initial value |
|----------|------|---------------|
| `names` | `char **` | output from `csv__parse_cells` |
| `col_count` | `int` | output from `csv__parse_cells` |
| `cells` | `char **` | output from `csv__parse_cells` |
| `row_count` | `int` | output from `csv__parse_cells` |
| `doc` | `CsvDocument *` | return value of `csv__build` |

### csv_load / csv_load_schema
| Variable | Type | Initial value |
|----------|------|---------------|
| `buf` | `char *` | return value of `csv__read_file` |
| `doc` | `CsvDocument *` | return value of `csv__parse_do` |

### csv_get
| Variable | Type | Initial value |
|----------|------|---------------|
| `arr` | `DatArray` | built locally, fields set inside the matching loop iteration |
