# Translation catalogues

LINK ships six built-in locales: `en-AU`, `en-US`, `de-DE`, `fr-FR`, `es-ES` and `it-IT`. The portable compiled catalogue in `src/core/i18n.c` is used at runtime; files in this directory are translator-facing metadata and future import/export surfaces only. Runtime code never parses JSON.

`en-AU` is the canonical complete fallback. New reusable UI text must be introduced as a semantic key in the LINK catalogue and translated where practical. Product-specific terminology stays in MBLINK or JAGLINK. Technical protocol identifiers, DTCs, CAN IDs, UDS service numbers, DIDs and ECU part numbers remain untranslated.
