# 島輝 DaoHui: the Rime data the installer ships

The public installer ships the stock Weasel 0.17.4 data (`output/data`, unchanged) plus
the DaoHui input-method data in **`output/data_daohui/`**, which the installer copies into
the same shared data dir (`$INSTDIR\data`). Nothing in `data_daohui` comes from the
developer's private files or from the Taiwan MOE dictionaries.

Build (one command, from the input-method repository `D:\Document\輸入法`):

```
C:\Users\User\anaconda3\python.exe tools\package_daohui_data.py            build + privacy check
C:\Users\User\anaconda3\python.exe tools\package_daohui_data.py --check    privacy check only
C:\Users\User\anaconda3\python.exe tools\package_daohui_data.py --smoke [--no-gram]
```

- Output: `D:\src\daohui\output\data_daohui` (43 files, 26.8 MB, about 6.4 MB as 7z/LZMA).
  Scratch: `D:\.rime-harness\pkg` (the script refuses any path under `D:\Document`).
- `FILES.txt` in the output lists every file with its relative path (`lua\…`, `opencc\…`)
  for the NSIS data section. `FILES.txt` itself does not need to be installed.
- The generators `tools/build_lexicon.py` and `tools/build_lang_data.py` got a `--public`
  mode and `--out <dir>`; with no flags they write exactly what they wrote before
  (verified: byte-identical to the live `rime/` files).
- `--check` fails on forbidden files (`*.userdb`, `user.yaml`, `installation.yaml`,
  `emails.txt`, `shortcuts.txt`, `*.custom.yaml`, `*.gram`), on stubs with entries, on any
  dictionary entry that only the MOE dictionaries have, on any private e-mail address, and
  on personal words in the generated data. The private strings are read on the fly from
  `lexicon/personal_words.txt`, `lexicon/english_words.txt`, `UserDictionary.txt`,
  `tests/eval/private/asus_words.tsv`, `rime/shortcuts.txt`, `rime/emails.txt`, e-mail
  addresses found in `rime/*.userdb*` and the newest learning backup, `git config
  user.email`, and an optional never-committed file (`D:\.rime-harness\pkg\extra_private.txt`).
  Words that also occur in the public sources (McBopomofo, essay, terra_pinyin, easy_en,
  SymSpell) are not counted, since an ordinary word proves nothing. The ocd2 dicts are
  verified by converting their text source again and comparing bytes, and the text is
  scanned. A test with planted leaks (personal dict, an e-mail address in a Lua file, a
  `user.yaml`, a swapped ocd2, the MOE dict) failed on every one of them.
- `--smoke` installs the result virtually (fork data + `data_daohui` as the shared dir,
  an empty user dir, the LM hard-linked from a scratch copy), deploys, also deploys
  `weasel.yaml` as DaoHuiDeployer does, types 4 sentences and checks the deployed configs.
  It then repeats with a user's own `default/weasel/asus_zhuyin.custom.yaml` on top.
  Both pass, with and without the LM.

## What the installer must do

1. Copy everything listed in `FILES.txt` into `$INSTDIR\data`, keeping the subfolders.
   The current `install.nsi` data section has no `lua` folder. Add
   `SetOutPath $INSTDIR\data\lua` + `File "data\lua\*.lua"`, and remove the folder on
   uninstall/upgrade (`Delete "$R1\data\lua\*.*"`, `RMDir "$R1\data\lua"`, and the same
   for `$INSTDIR`). opencc needs `*.json` + `*.ocd2`, which the section already copies.
2. `default.yaml` and `weasel.yaml` in `data_daohui` **replace** the stock ones. They are the
   stock files with DaoHui's defaults appended as a root `__patch`; see below.

## Product defaults and how they are set

| Setting | Where it lived (developer) | In the package |
|---|---|---|
| `asus_lang/raw_keys: true` (raw keys until a tone key) | `rime/asus_zhuyin.custom.yaml` | baked into `asus_zhuyin.schema.yaml` (both `raw_keys: false` → `true`; the script fails if the schema text changes) |
| `schema_list: [asus_zhuyin]`, `menu/page_size: 9`, Shift/Ctrl taps = noop, Caps Lock = commit_text | `rime/default.custom.yaml` | appended to stock `default.yaml` as `__patch: [<patch>, default.custom:/patch?]` |
| ASUS look (colour schemes `asus_light`/`asus_dark`, vertical list, fonts, layout), `app_options` (terminals type English directly, code editors no autocorrect) | `rime/weasel.custom.yaml` | appended to stock `weasel.yaml` as `__patch: [<patch>, weasel.custom:/patch?]` |

Why not ship `*.custom.yaml` in the shared dir: librime looks a config file up in the user
dir first and uses the shared file only if the user has none, with no merging. The settings
app writes the user's `asus_zhuyin.custom.yaml`, and Weasel's own dialogs write
`default.custom.yaml` (schema list) and `weasel.custom.yaml` (colour scheme). Any of these
would silently hide a shared custom file, including the terminal list and the colour schemes
themselves. With the defaults in the base files, a user's custom file is applied on top. librime
skips its automatic `<id>.custom` patch when a root `__patch` exists, so the list ends with
`<id>.custom:/patch?` explicitly (the schema already does this). `--smoke` checks both cases.

**Settings app:** `DEFAULT_RAW_KEYS` in `app/daohui-settings/src-tauri/src/config.rs` is
`false`; it must become `true`, or the 功能開關 page shows the wrong state when the user has
no custom file.

## Inventory: every file the schema needs

Stock data, already in `output/data` and not shipped by this package (Weasel 0.17.4 = rime
data packages):

| File | Used for | Licence |
|---|---|---|
| `bopomofo_tw.schema.yaml`, `bopomofo.schema.yaml`, `zhuyin.yaml` | `__include` base of `asus_zhuyin`, speller rules | rime-bopomofo, LGPL-3.0 |
| `terra_pinyin.dict.yaml` | imported by `terra_pinyin.asus` (readings, phrases) | rime-terra-pinyin, LGPL-3.0; its header cites CC-CEDICT (CC BY-SA 3.0) |
| `essay.txt` | `use_preset_vocabulary`, and the scale of McBopomofo weights | rime-essay, LGPL-3.0 |
| `key_bindings.yaml`, `punctuation.yaml`, `symbols.yaml` | via `default.yaml` / punctuator | rime-prelude, LGPL-3.0 |
| `default.yaml`, `weasel.yaml` | replaced by the patched copies in the package | rime-prelude LGPL-3.0 / Weasel GPL-3.0 |
| `opencc/t2s.json`, `t2tw.json` + ocd2 | `simplifier@zh_hans`, `simplifier@zh_hant_tw` | OpenCC, Apache-2.0 |
| `stroke.dict.yaml` | named by bopomofo's reverse lookup, unused (`dependencies: []`, no reverse-lookup translator) | LGPL-3.0 |

The package (`data_daohui`):

| File | Source data | Licence of the source | Private data in the developer's copy? | In the package |
|---|---|---|---|---|
| `asus_zhuyin.schema.yaml` | repository | GPL-3.0 (project) | no | copy, `raw_keys: true` |
| `terra_pinyin.asus.dict.yaml` | repository; imports `terra_pinyin`, `.mcb`, `.tw`, `.personal` | project | no | copy |
| `terra_pinyin.mcb.dict.yaml` | McBopomofo `BPMFMappings.txt` + `phrase.occ` (commit e663357dfdfa), weights rescaled with `essay.txt` | MIT (BPMFMappings originally from libtabe `tsi.src`, BSD) + LGPL-3.0 | no | rebuilt; identical to the developer's file |
| `terra_pinyin.tw.dict.yaml` | Taiwan MOE dictionaries (國語辭典簡編本 / 重編本 / 成語典) | **CC BY-ND 3.0 TW: no derivatives may be distributed** | no, but not redistributable | **empty stub** |
| `terra_pinyin.personal.dict.yaml` | `lexicon/personal_words.txt` (ASUS / MS Bopomofo / old Weasel exports) | private | **yes** (the developer's own words) | **empty stub** |
| `grammar.yaml` | repository (names the LM `wanxiang-lts-zh-hant`) | project | no | copy |
| `default.yaml` | stock + `rime/default.custom.yaml` | LGPL-3.0 + project | no | patched copy |
| `weasel.yaml` | stock + `rime/weasel.custom.yaml` | GPL-3.0 + project | no (generic exe names; author `TimLin-1231`) | patched copy |
| `lua/*.lua` (18 files) | repository | project | comments only, see "Findings" | copy (identical) |
| `lua/asus_zhuyin_data.lua` | terra_pinyin syllables; `max_en_len` over the English list | LGPL-3.0 | in theory (`max_en_len`) | rebuilt; identical |
| `opencc/asus_en.*` | easy_en + `lexicon/english_words.txt` | LGPL-3.0 (+ private) | **yes** (3 words, rank 0) | rebuilt without them |
| `opencc/asus_en_prefix.*`, `asus_en_fix.*` | the same + SymSpell word frequencies | LGPL-3.0 + MIT | **yes** | rebuilt without them |
| `opencc/asus_en_follow.*` | SymSpell word pairs, display forms from the English list | MIT (+ LGPL forms) | in theory | rebuilt; identical |
| `opencc/asus_next.*` (next-word prediction) | McBopomofo `phrase.occ` + personal words | MIT + private | **yes** | rebuilt without personal words |
| `opencc/asus_seg.*` (word splitting for learning) | McBopomofo `phrase.occ` + personal words | MIT + private | **yes** | rebuilt without personal words |
| `opencc/asus_tw.*` (Taiwan spellings) | `essay.txt` + McBopomofo + terra_pinyin readings, **no MOE** | LGPL-3.0 + MIT | no | rebuilt; identical |
| `shortcuts.example.txt` | repository | project | no | copy (template; see note) |
| `DAOHUI_DATA_LICENSES.txt` | written by the script: sources + the McBopomofo and SymSpell MIT notices (required in copies) | – | – | new |

Not shipped:

| File | Why |
|---|---|
| `*.userdb` (and `*.userdb.restore-*`) | learned data: corrections, next words, English words, **e-mail addresses** |
| `user.yaml`, `installation.yaml` | machine state (`installation_id`, deploy time) |
| `asus_zhuyin.custom.yaml` | the developer's settings; its only value (`raw_keys`) is baked into the schema |
| `shortcuts.txt` | private by design (now identical to the example). `asus_shortcuts.lua` and `asus_lang.lua` read `shortcuts.txt` from the **user dir only**, so a shared copy would not be read anyway. The example ships as a template |
| `emails.txt` | private (absent now; addresses live in `asus_learn.userdb`), read from the user dir only |
| `terra_pinyin.tw.dict.yaml` (MOE) | CC BY-ND. The developer keeps his own in his user dir, which overrides the stub |
| `wanxiang-lts-zh-hant.gram` (410 MB) | downloaded by the settings app into the user dir (octagram looks in the user dir first) |
| `zh-hant-t-essay-bgw.gram` | the old LM, unused |
| `build/` | rebuilt by the deployer (deploy of the package on an empty user dir: about 4 s) |

MOE derivation, checked: MOE data reaches only `terra_pinyin.tw` (`build_lexicon.build_tw`).
`zhuyin.py` uses MOE readings elsewhere only for personal words that have no reading (the
stub) and for generating eval sets (not shipped). `asus_tw` comes from essay.txt and
McBopomofo. The measure-word readings in `asus_lang.lua` and the 的/得/地 rules in
`asus_grammar.lua` are hand-written facts, not copied entries. `--check`: of the 1,289
words that only the MOE dictionaries have, 0 are entries of the package. (The planted MOE
dict was caught: 1,030 hits.)

## Accuracy: full private data vs public package vs no LM

The cases, scoring and no-learning setup are those of `tools/eval_ime.py` (44 sets, 5,028
cases, sets hash `2b86031ae6c4`), run by a scratch driver, `D:\.rime-harness\pkg\eval_pkg.py`
(not in the repository), with these stagings:

- **full**: the developer's data. Stock Weasel data as the shared dir, a copy of `rime/` as the
  user dir, as `eval_ime` does. It reproduces the last `eval_history.jsonl` record of `asus`
  exactly, in all 44 sets.
- **public**: DaoHui's layout. Fork data + `data_daohui` as the shared dir, an empty user dir
  plus the LM.
- **no LM**: the same as public, without `wanxiang-lts-zh-hant.gram`.

For comparability all three use `raw_keys: false`, the schema default that `eval_ime`
measures. A fourth run with `raw_keys: true` (as shipped) gave the same committed text in
every sentence set. Only preview-based guard metrics differ, by design, because raw keys
show letters and digits inline until a tone key (`dev_toneless` ASCII, `dev_partial`
Digits). `autofix_zh` was slightly better (Wrong 19.1 → 17.0%, FalseFix 16.5 → 12.7%).
Held-out sets: totals only. No held-out case was looked at.

Sentence accuracy (whole sentence right after Enter):

| Set (n) | full | public | no LM |
|---|---:|---:|---:|
| dev (100) | 96.0% | 96.0% | 92.0% |
| **test, held-out (100)** | **90.0%** | **89.0%** | **80.0%** |
| dev_typo / test_typo | 75 / 76% | 75 / 76% | 66 / 62% |
| dev_typo_end / test_typo_end | 72 / 60% | 72 / 60% | 68 / 50% |
| dev_stray / test_stray | 84 / 79% | 84 / 79% | 80 / 71% |
| dev_omit / test_omit | 56 / 59% | 55 / 58% | 47 / 50% |
| dev_swap / test_swap | 78 / 77% | 78 / 77% | 74 / 69% |
| mixed / mixed_test (40 / 40) | 95.0 / 97.5% | 95.0 / 97.5% | 92.5 / 87.5% |
| mixed_fix / mixed_fix_test | 93.3 / 71.9% | 93.3 / 71.9% | 93.3 / 75.0% |
| particles / particles_test (PartAcc) | 97.6 / 100% | 100 / 100% | 97.6 / 90.0% |
| numbers / numbers_test | 97.5 / 80.0% | 97.5 / 80.0% | 95.0 / 72.5% |
| numbers_amb / numbers_amb_test | 76.7 / 73.3% | 76.7 / 73.3% | 40.0 / 46.7% |
| en_short_test / en_edge_test | 95.2 / 76.9% | 95.2 / 76.9% | 92.9 / 73.1% |
| **All dev sentence sets (14 sets, 888)** | **81.9%** | **81.8%** | **76.4%** |
| **All held-out sentence sets (13 sets, 850)** | **76.4%** | **76.1%** | **67.1%** |

- The English sets (english, english_ctx, autofix, autofix_more, english_next, en_case, all with
  held-out) are **identical** in all three. The personal English words (3) never mattered for them.
- moe_words (400 common MOE words, held-out; Top1): full 92.0%, public 92.2%, no LM 92.2%.
  Dropping the MOE supplement does not hurt there.
- words (the developer's own 1,030-word ASUS dictionary; Top1 / Top9): full 99.1 / 100%,
  public 81.3 / 28.4%, no LM 73.7 / 28.3%. This is expected and is not a product metric. Those
  words are his, and his user dir keeps his `terra_pinyin.personal.dict.yaml`. Top9 is low
  because a word that is not in the dictionary is composed as a sentence, which the expanded
  list does not offer as a row.
- Of the dev cases, only 8 come out differently between full and public (3 better, 3 worse, 2
  wrong either way). The public data costs about 0.1 to 0.3 points overall.
- **Without the LM** it deploys and types, but it is clearly worse: about −5.5 points on dev and
  −9 on held-out. The biggest losses are the digit-or-Zhuyin decisions (numbers_amb 76.7 → 40%),
  typo correction (test_typo 76 → 62%) and held-out mixed input (97.5 → 87.5%). The settings
  app should push the download on first run.

## Licences: what is uncertain

- **easy_en**: the repository (BlindingDark/rime-easy-en) is LGPL-3.0. The README credits
  ECDICT (MIT) for the word list but states no licence for the dictionary data itself. Our
  copy comes from the developer's old Weasel folder (`D:\Document\小狼豪輸入法\easy_en.dict.yaml`,
  v0.2), not from a pinned upstream release. Before release, fetch it from upstream with a
  hash, like `fetch_models.py` does for SymSpell.
- **SymSpell word frequencies**: shipped as MIT with SymSpell, but made from Google Books
  Ngram (CC BY 3.0) and SCOWL; `DAOHUI_DATA_LICENSES.txt` credits both. The word-pair file's
  source is not documented upstream.
- **McBopomofo**: MIT for the repository. `BPMFMappings.txt` comes from libtabe (BSD), and
  `phrase.occ` is corpus counts. Both MIT notices are included.
- **terra_pinyin.dict.yaml** (stock, not ours) cites CC-CEDICT (CC BY-SA 3.0) although the
  package is LGPL. That is upstream Rime's situation and is unchanged.
- **Names**: the schema is still called 「華碩風注音」 and the colour schemes 「華碩淺色／ASUS
  Light」 / 「華碩深色／ASUS Dark」. For a public product, consider dropping the ASUS trademark
  from user-visible names (a schema `name:` / `preset_color_schemes/*/name` change in the repo).

## Findings to fix in the repository (not data leaks of this package)

`--check` lists them as warnings, because they are verbatim text from git-tracked sources. They
are already in the repository, which is to become public:

- A few code comments in `rime/lua/` (asus_lang, asus_email, asus_learn) use examples taken
  from the developer's own address and word lists; reword them to neutral examples
  (`name@host.com`) before the repository is made public.

## Without the language model

Before the LM is downloaded, DaoHui deploys and types normally. librime-octagram logs
`failed to load grammar database` about 14 times per process start (42 ERROR lines). The
count was the same for 4 sentences and for 5,028 eval cases, so it does not grow per key.
Accuracy without the LM is shown above. A failed load is not cached, so engines created later
(new sessions, or after a redeploy) pick up the file once the settings app has downloaded it
into the user dir. Triggering a redeploy after the download is the safe choice.
