# Translation catalogues

LINK exposes 15 built-in selectable locales: `en-AU`, `en-US`, `en-GB`, `de-DE`, `fr-FR`, `es-419`, `it-IT`, `pl-PL`, `pt-BR`, `zh-CN`, `hi-IN`, `ar`, `ja-JP`, `ko-KR` and `id-ID`. Australian English (`en-AU`) is the canonical complete fallback and factory default.

The portable compiled catalogue in `src/core/i18n.c` is used at runtime; files in this directory are translator-facing metadata and future import/export surfaces only. Runtime code never parses JSON. English (United Kingdom) currently inherits the Australian wording where there is no regional difference, while Latin-American Spanish inherits the shared Spanish catalogue for common diagnostic terminology.

Reusable human-readable UI text belongs in the LINK semantic catalogue. Product-specific terminology stays in MBLINK or JAGLINK. Technical protocol identifiers, DTCs, CAN IDs, UDS service numbers, DIDs, ECU part numbers, VINs and measured numerical values remain untranslated. Arabic is treated as right-to-left by graphical shells.
