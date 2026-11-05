# Token baseline (S16)

This file compares the tokens of the worked examples G1 to G5 in
permit-lang, Python and Rust (PLAN 9, row A6). The counts in this file are
an offline estimate: ceil(bytes / 4) of each file (Q-S16-1 (a), the
cairn-lang route). They are not tokenizer counts. The API route (`make
tokens`: the Anthropic `count_tokens` endpoint with the newest Claude
model, Q-S0-10 (a), Q-S16-1 (a)) is not run, because no API Console key is
available. `make check` checks the file hashes and the estimate block in
this file, with no key and no network (Q-S16-3 (a)).

Status: the estimate is from 2026-11-05 (`build/tokcount --estimate`). The
API route is not run.

## The task

Each G is one worked example of `skills/permit-lang/SKILL.md` (S15). The 3
programs of a G do the same task and print the same lines (Q-S16-2 (a)):

| G | task | eval rows |
| :--- | :--- | :--- |
| G1 | a read-only scope with a step bound (kit mode) | `readOk`, `editMiss`, `readAgain` |
| G2 | a budget with costs and a split (kit mode) | `halves`, `rest`, `left` |
| G3 | an attenuated delegation to a sub-agent (kit mode) | `child`, `oneTwo`, `twoOne` |
| G4 | the propose and execute of one approved mutation (kit mode) | `exact`, `usedTwice`, `changed` |
| G5 | a deploy tool set behind the tool gate (the demo, guest mode) | `prod`, `dev`, `startSteps` |

G5 is the demo: `g5-deploy.lang` is `test/host/deploy.lang` with a 3-line
header and the rows `lawSteps` and `startSteps` (Q-S15-2 (a)). Thus the 5
programs are "G1 to G5 and the demo" of row A6.

## The files

- `examples/skill/g1-scope.lang` to `g5-deploy.lang`: the permit-lang
  programs (S15). The count is of the `.lang` file only.
- `test/baseline/g1-scope.py` to `g5-deploy.py`: the same tasks in Python,
  with the standard library only (Q-S16-3 (a)). G4 uses `hashlib` for the
  SHA-256 digest of the mutation.
- `test/baseline/rust/src/bin/g1.rs` to `g5.rs`: the same tasks in Rust, 1
  crate (`permit-baseline`) with 5 bins (Q-S16-3 (a)). Only G4 uses `sha2`
  0.10, because the Rust standard library has no SHA-256. `Cargo.lock`
  pins the versions.

Each Python and each Rust program prints the 3 eval lines of its G in the
SKILL.md order. The lines must be equal, byte for byte, to the output of
`langc --kit eval` (G1 to G4) or `langc eval` (G5). `make tokens` checks
this before it counts.

## The method

- Estimate: `build/tokcount --estimate` reads the full text of each file,
  comments included. The estimate of a file is ceil(bytes / 4), that is
  (bytes + 3) / 4 in integers (Q-S16-1 (a)). It needs no key and no
  network. An estimate is not a tokenizer count: it has no model and no
  message frame.
- Request (the API route): `POST /v1/messages/count_tokens` with 1 user
  message. The message is the full text of 1 file, comments included. The
  reply is `{"input_tokens": N}`.
- Model (the API route): the model with the newest `created_at` in the
  reply of `GET /v1/models` on the run date (Q-S16-1 (a)).
- Frame (the API route): N includes the tokens of the message frame. Thus
  `make tokens` also counts a fixed 1-byte message, `x`. The net count of
  a file is N minus the count of that message (Q-S16-1 (a)).
- Ratios: each ratio divides 2 estimates now (2 net counts on the API
  route), half up, with 2 decimals. One line gives each Python file and
  each Rust file over the permit-lang file of the same G. The last 2 lines
  give the sum of each language over the sum of the 5 permit-lang files.
  Each file is a full alternative, thus the table has no total row.
- Hashes: the SHA-256 of each counted file. `make check` runs
  `build/tokcount --check docs/TOKEN-BASELINE.md`. Each hash in the table
  must be the hash of the file now. `make check` also compares the lines
  between the markers with the output of `build/tokcount --estimate`.
  Thus a changed program fails the gate until a new estimate block
  replaces the old one.

## The run

The estimate: from the repo root, offline, with no key:

```
build/tokcount --estimate
```

The output is the block between the markers below, byte for byte.

The API route (not run; it needs an API Console key): from the repo root,
outside the sandbox, with `ANTHROPIC_API_KEY` set:

```
make tokens
```

The rule does these steps:

1. It builds the Rust crate (`RUST_BUILD`, by default `cargo build -j 2`).
2. It runs each program and compares its output with `langc eval`.
3. It gets the model list and selects the model (`build/tokcount --model`).
4. It sends 1 request for the frame message and 1 request for each of the
   15 files (`build/tokcount --request` makes each body). It keeps each
   reply in `build/tokens/`.
5. It writes the table with `build/tokcount --table build/tokens`.

Now the lines between the markers below come from `build/tokcount
--estimate`. On the API route, step 5 replaces them.

## The table

Model: none (an offline estimate). Date: 2026-11-05.

<!-- tokcount begin -->
| file | bytes | estimate | sha256 |
| :--- | ---: | ---: | :--- |
| examples/skill/g1-scope.lang | 1181 | 296 | b1884664932d23892880d794b8fa5b07a33ad50d4c8ffdc307caebebd9a1b7a3 |
| test/baseline/g1-scope.py | 1802 | 451 | 2071d76f666d66c70a6f84c7ec408e82f1b97b595e0bd0869914eb6de922f848 |
| test/baseline/rust/src/bin/g1.rs | 2441 | 611 | ac89db817c50a9c22266543c74eabc54de82e30c1e3db4ce64fb370eabedd08a |
| examples/skill/g2-budget.lang | 749 | 188 | 7b10c3e05332308dcf8a05e6c40f314bd22c5880e5bbb349b42a1a03ea9dd05f |
| test/baseline/g2-budget.py | 1024 | 256 | 3192861f1831a277a178c8b334ed9d7bdccd9836cdbf86b66183f2e50a2fb726 |
| test/baseline/rust/src/bin/g2.rs | 1260 | 315 | 76d2641d3bfe698b46f25f878f7c4e2a4371619e2021ee109b034f4a914603cb |
| examples/skill/g3-delegate.lang | 1179 | 295 | bac96645d591d4dbc2d01ab3586806a6bc7640745b84a3de1550745d907bc3d0 |
| test/baseline/g3-delegate.py | 2426 | 607 | f32e7b35cea85424742f7a35617482b15e695824836ab13fedfa0fb3d7d8a06f |
| test/baseline/rust/src/bin/g3.rs | 3352 | 838 | b03fcd00f4a82a9e91532b171e1fbabd30bf4a571ead430475e0425e8e61cbbf |
| examples/skill/g4-approve.lang | 1366 | 342 | 3e64f16908fba67836da6923d4d7dbd4014872fef034c71aee13dfe0a3b79714 |
| test/baseline/g4-approve.py | 2481 | 621 | fe0bea092aeb998a4fb52410f582f29393c34dd27bce73a139fb2eae62348882 |
| test/baseline/rust/src/bin/g4.rs | 3822 | 956 | 19a4ba549eefe1c70ed7c3693e66689c5a2d64ccec9df45fffd1bcf812b3d7ae |
| examples/skill/g5-deploy.lang | 1212 | 303 | c9b2715457e45957c3ea88f4793b98b67e0cad10755b0076aab1aa51e8fc22d4 |
| test/baseline/g5-deploy.py | 1565 | 392 | 8c80ec75fc781d59124e6f06250224c27675f32ca82cc32e1b4960e2c61c4baa |
| test/baseline/rust/src/bin/g5.rs | 2283 | 571 | 143cde2ab95fe7ce1c2f589968a3b8c4f06459dea0b4d9def52db889482d31c5 |

estimate: ceil(bytes / 4) of each file; this is not a tokenizer count.

ratio of the estimates (test/baseline/g1-scope.py over examples/skill/g1-scope.lang): 1.52
ratio of the estimates (test/baseline/rust/src/bin/g1.rs over examples/skill/g1-scope.lang): 2.06
ratio of the estimates (test/baseline/g2-budget.py over examples/skill/g2-budget.lang): 1.36
ratio of the estimates (test/baseline/rust/src/bin/g2.rs over examples/skill/g2-budget.lang): 1.68
ratio of the estimates (test/baseline/g3-delegate.py over examples/skill/g3-delegate.lang): 2.06
ratio of the estimates (test/baseline/rust/src/bin/g3.rs over examples/skill/g3-delegate.lang): 2.84
ratio of the estimates (test/baseline/g4-approve.py over examples/skill/g4-approve.lang): 1.82
ratio of the estimates (test/baseline/rust/src/bin/g4.rs over examples/skill/g4-approve.lang): 2.80
ratio of the estimates (test/baseline/g5-deploy.py over examples/skill/g5-deploy.lang): 1.29
ratio of the estimates (test/baseline/rust/src/bin/g5.rs over examples/skill/g5-deploy.lang): 1.88
ratio of the estimates (the 5 Python files over the 5 permit-lang files): 1.63
ratio of the estimates (the 5 Rust files over the 5 permit-lang files): 2.31
<!-- tokcount end -->

Result: for the 5 G together, the estimate of the Python files is 1.63
times the estimate of the permit-lang files, and the estimate of the Rust
files is 2.31 times the estimate of the permit-lang files.

## Notes

- Versions: the estimate uses no tool version; it reads only the bytes of
  the files. On the API route, the `python3`, `rustc` and `cargo`
  versions come from `build/tokens/versions.txt`.
- A `make tokens` run that replaces the estimate block must also remove
  the estimate compare of `make check` (the 2 Makefile lines after
  `build/tokcount --check`).
- Cargo dependencies apply to the full crate. Thus `sha2` builds with each
  bin, but only `g4.rs` uses it. The Rust build gives 0 warnings.
- Run `make tokens` outside the sandbox. In the sandbox, the crates.io
  fetch cannot read the CA file and the Rust build stops at `sccache
  rustc -vV`.
