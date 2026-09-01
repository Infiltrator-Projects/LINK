# J1979/J1979-DA public semantic provenance

LINK targets the current SAE standards revisions but does not redistribute the
licensed SAE J1979 Digital Annex.

Current targets:

- SAE J1979_202505
- SAE J1979DA_202607
- SAE J1978-1_202604
- SAE J1979-2_202604

The compiled semantic registry exposed by `link/j1979da.h` is therefore labelled
`J1979DA_201110+verified-public-updates`, not `J1979DA_202607`.

The public semantic baseline is independently represented from:

- publicly available, legally incorporated historical SAE J1979 material for
  first-generation Service 05 request/response shape and legacy TID scaling;
- public J1979DA_201110 tables for Mode 06 OBDMID/TID/UASID registry structure;
- later public J1979DA change-history material confirming assignments added
  after the 2011 public table;
- public OEM/diagnostic documentation used only where it independently
  corroborates a standard identifier; and
- the MIT-licensed `shinyorg/obd` Mode 06 implementation as an independent
  cross-check of the public UASID scaling table.

Rules:

1. A current-standard target revision is never used as a claim that its licensed
   semantic rows are compiled into LINK.
2. An identifier may be classified as STANDARD when public change history proves
   assignment, while its semantic definition remains NULL if the licensed row is
   unavailable.
3. A 2011-era hole that may have been assigned later is UNVERIFIED, not RESERVED.
4. Unknown semantics remain raw. LINK does not guess names, lengths, units,
   scaling, limits or bit meanings.
5. Importing a licensed current Digital Annex, if one is supplied under terms
   that permit derived implementation metadata, must use a reproducible
   generator and must not vendor or reproduce the SAE source document itself.

This separation is enforced by the J1979/J1979-DA regression tests.
