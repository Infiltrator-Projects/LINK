# LINK language packs

LINK-family applications support discoverable, Amiga-style, data-only language packs.
A translator can add a language without recompiling MBLINK, JAGLINK or LINK: place a
UTF-8 `.lang` file in a scanned `Languages` directory and restart the application.

## Format

```ini
locale=sv-SE
name=Svenska
direction=ltr
version=1
nav.vehicle=Fordon
nav.faults=Fel
Vehicle=Fordon
```

Required metadata is `locale`, `name`, `direction` (`ltr` or `rtl`) and `version=1`.
Every other `key=value` line is a translation. Keys can be semantic keys such as
`nav.vehicle` or exact human-readable English UI literals. Diagnostic data such as
VINs, DTCs, CAN identifiers, PIDs, numbers and measurements must not be translated.

Missing strings always fall back to compiled Australian English (`en-AU`). A pack for
an existing locale overrides the shipped catalogue. A pack for a new locale appears
in the language picker automatically.

## Search order

Later directories override earlier ones.

Linux applications scan:

1. `/usr/share/<product>/Languages`
2. `Languages` beside the executable
3. `~/.local/share/<product>/Languages`

Windows Discover applications scan:

1. `Languages` beside the executable
2. `%LOCALAPPDATA%\\<PRODUCT>\\Languages`

Apple applications use the same `.lang` format from their Application Support
`Languages` directory. Import is handled by the Apple UI because iOS does not allow
arbitrary writes into the signed application bundle.

Language packs are plain data and are never loaded as executable code.
