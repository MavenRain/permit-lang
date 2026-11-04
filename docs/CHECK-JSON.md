# The `langc check --json` line

S13 (A2 of `docs/PLAN.md` section 9, rulings Q-S13-1 to Q-S13-9) adds a
JSON form of `langc check`. It is a port of accrue-lang S12 A2 with the
permit facts. An agent reads the line and does not parse the text
diagnostic.

## Usage

```
build/langc check --json PROG
build/langc --kit check --json PROG
```

`--json` comes right after `check`. `check PROG --json` is `USAGE`.

| Exit | stdout | stderr |
| --- | --- | --- |
| 0 | the JSON line, with an empty diagnostic list | nothing |
| 1 | the JSON line, with one diagnostic: a failure of the lexer, the parser or the check | nothing |
| 2 | nothing | the text diagnostic `OOM`, `IO` or `USAGE` |

## The line

The line is one JSON object and a newline. `langc` writes the text of the
object, then `json_parse` reads it and `jcs_write` writes it in its JCS
form (RFC 8785), as `langc report` does (Q-S13-4, `docs/REPORT-JSON.md`).
The two writers share `src/jsonout.c`. Thus the keys of each object are in
JCS order:

```
{"diagnostics":[DIAG],"progress":{"laws_discharged":D,"laws_total":L,"parsed":P,"typed":T}}
```

- `diagnostics` is empty when `langc check` accepts the program. Else it
  holds one DIAG: the first failure.
- DIAG is `{"code":C,"fix":FIX,"message":M,"span":SPAN}`. There is no
  `expected` key and no `found` key. These words are only in the message
  text (the `TYPE_MISMATCH` and `PARSE_EXPECT` rows).
- `code` is the code of the failure, as `langc check` writes it.

## The span

`span` is `{"col":C,"end_col":EC,"end_line":EL,"file":F,"line":L}`.
`file` is the path as the command line gives it. Lines and columns start
at 1. A column counts bytes. The end column is exclusive.

- A lexer or a parser failure has the span of its token.
- A check failure has the span of the failing decl, from its first byte
  to the end of its last token.
- When the failure has no source position (line 0), `span` is `null`.

Two rules of the check spans (U1, H7):

- The decls that `check_generated` checks (the generated cofamily text)
  have the spans of the generated text.
- `check_label` sets no new span. It keeps the span of the decl at
  `decl_index`.

## The message

`message` is `DEF: MSG`. DEF is the name of the failing decl, or `-` when
there is no decl. It is the same text that `langc check` writes after
`langc: CODE: ` on stderr. A `LEX_` or `PARSE_` message keeps its
`FILE:LINE:COL:` prefix. The `LEX_CHAR` row:

```
"message":"-: test/json/LEX_CHAR.lang:1:18: a letter follows a Nat literal"
```

## The fix

`fix` is `null` or `{"span":SPAN,"text":T}`: replace the bytes of SPAN by
T. A fix edits one span. A fix is not `null` only at these sites
(Q-S13-5):

- `LEX_CHAR` (`src/front/lexer.c`, 2 sites): delete the byte.
- `REFUSE_CELL`: replace `List` by `FreeMon`.
- `REFUSE_SIG`: put the slot type in place of the type of the decl. The
  fix is `null` when the decl has no type.
- `REFUSE_STATE`: delete the second state decl.

For each other code, `fix` is `null` and the message gives the reason.

The `REFUSE_CELL` row is the one-List form `def t : Type 0 := List Size`.
Thus its JSON fix clears the code: a program with 2 `List` names needs 2
edits, and a JSON fix edits one span.

## Strings

`out_bytes` (`src/jsonout.c`) writes each string of the text. Each byte
where no valid UTF-8 sequence starts becomes one U+FFFD, and the next
sequence starts at the next byte. Such a byte is a byte 0x80 to 0xc1 or
0xf5 to 0xff, the start of a sequence that the end of the text cuts, a
lead byte with a bad continuation byte, an overlong form (0xe0 or 0xf0),
a surrogate (0xed) or a code point above U+10FFFF (0xf4). `json_parse`
refuses invalid UTF-8, so this map is necessary. `jcs_write` then gives
the JCS string form: `\"`, `\\`, `\b`, `\t`, `\n`, `\f`, `\r`, `\u00xx`
for each other byte below 0x20, and each other character as UTF-8.

## Progress

`progress` is one object in the line. `langc` writes no progress lines
while it checks. The counts are for the program decls only: not
`domain/domain.lang`, not the kit prelude and not the kernel splices.

- `parsed`: the program decls that parse. A block counts each sort. The
  `bind` decls (`DECL_BIND`) and the state decl also count.
- `typed`: the program decls that check before the first failure, with
  the same rule.
- `laws_discharged`: the typed program defs whose type is a law: the type
  evaluates to a type that ends in a `Path`, the rule of `report.c`
  `out_laws` (U1, H6). A `Trunc1` law type, which `REFUSE_LAW` refuses,
  does not count.
- `laws_total`: `laws_discharged`, plus the program defs from the failing
  decl to the end whose type ends in `Path` or `PathP` by syntax
  (`law_syntax`).

After a `LEX_` or `PARSE_` failure, the 4 counts are 0.

The limits of `law_syntax` (H6): it does not see a `Path` type that a
program reaches only through a def, and it counts a type with a `Path`
head that has an error.

## Example

The row `test/json/REFUSE_SIG.lang` (with `--kit`):

```
{"diagnostics":[{"code":"REFUSE_SIG","fix":{"span":{"col":11,"end_col":14,"end_line":9,"file":"test/json/REFUSE_SIG.lang","line":9},"text":"Group -> (Member -> Choice) -> Member -> Choice"},"message":"act: the slot act must have the type Group -> (Member -> Choice) -> Member -> Choice, after Member, Choice and Group","span":{"col":1,"end_col":19,"end_line":9,"file":"test/json/REFUSE_SIG.lang","line":9}}],"progress":{"laws_discharged":0,"laws_total":0,"parsed":5,"typed":4}}
```

## The rows

`test/json/` has 28 rows. Each row `X.lang` has `X.json`, the bytes of
the line. A row `CODE.TAG.lang` is one more row for CODE (the code is the
file name up to the first dot).

- `ok` and `kit`: exit 0, an empty diagnostic list.
- One row for each `REFUSE_` code of `src/front/check.c` except
  `REFUSE_LAW`: 21 rows.
- `LEX_CHAR`, `PARSE_EXPECT`, `TYPE_MISMATCH`, `TYPE_MISMATCH.after` (a
  law def after the failure: `laws_total` 1) and `TYPE_MISMATCH.splice`
  (the permit kernel still splices the prelude).
- 11 rows run with `--kit` (`json_kit` of `test/gate.sh`): `kit`,
  `REFUSE_BUDGET_OVERSPEND`, `REFUSE_DELEGATE_WIDEN`,
  `REFUSE_EXECUTE_UNAPPROVED`, `REFUSE_HISTORY_WRITE`, `REFUSE_MAP`,
  `REFUSE_PARTIAL`, `REFUSE_REC`, `REFUSE_SIG`, `REFUSE_STATE` and
  `REFUSE_TIME`. In guest mode these rows give `REFUSE_KIT`.
- Each row other than `ok` and `kit` has `X.fix`: 26 fixes.
  `build/fronttool fix` applies it, and `langc check --json` of the result
  exits 0 or gives a first code other than the code of the row.

`REFUSE_LAW` has no row (Q-S13-9). The domain and the kernel splices have
no law def and no sealed family, and a program cannot declare one
(`REFUSE_DATA` in guest and in `--kit` mode). Thus `langc check` cannot
give `REFUSE_LAW`. The `law_guest` rows of `build/fronttool` test it.

Each new `.lang` row is canonical (`langc fmt`). The `LEX_CHAR` and
`PARSE_EXPECT` rows are in `test/fmt/refused.txt`.

`make check` prints these lines (`test/gate.sh`, the json block):

```
json rows: 28 passed
json refusal codes: 21 of 21
json fixes: 26 passed
```
