# Generated UI fonts

`lv_font_ui_14/16/22/24.c` are generated, not hand-written. Do not edit them.

## Why they exist

The Montserrat fonts LVGL ships are compiled for ASCII only (`-r 0x20-0x7F`),
so German, French, Turkish and Japanese text rendered as empty boxes — LVGL
draws a placeholder for every glyph it lacks. These replace them for all
translated text.

## What they contain

Only the glyphs the UI actually uses, taken from every string in
`package/App/RomDownloader/lang.json` across all five languages:

- ASCII, plus the accented Latin characters that appear (`ÄÇÉàäçéêöüğİıŞş`)
  — from **Montserrat**
- the Japanese kana and kanji that appear (~153 characters) — from
  **Noto Sans JP**

Subsetting to the strings in use is what keeps this affordable: the full
Noto Sans JP is ~9.6 MB, while all four sizes here add ~118 KB to the binary.

They carry **no FontAwesome range**, so `LV_SYMBOL_*` glyphs are not available
in them. `ui_chrome.c`'s status label deliberately stays on
`lv_font_montserrat_16` for that reason — it renders only the Wi-Fi and
SD-card symbols and digits, never translated text.

## Licensing

Both source faces are SIL Open Font License 1.1, which permits redistribution
including in a bundled/derived form:

- Montserrat — Julieta Ulanovsky et al.
- Noto Sans JP — Google

## Regenerating

Needed whenever a translation introduces a character not already covered
(a new language, or new text in an existing one). Extract the character set
from `lang.json`, then for each size:

```sh
npx lv_font_conv \
  --font Montserrat.ttf -r 0x20-0x7F --symbols "<accented latin used>" \
  --font NotoSansJP-full.ttf --symbols "<japanese used>" \
  --size <14|16|22|24> --bpp 4 --no-compress --format lvgl \
  --lv-font-name lv_font_ui_<size> -o lv_font_ui_<size>.c
```

A missing glyph shows up as an empty box on the device, not as a build error,
so check any newly added text on hardware.
