# Apple provider boundary

LINK owns the reusable Apple transport/controller and the Apple portable-core build glue consumed by branded product faces.

The LINK-owned Apple amalgamation entry points are:

- `platform/apple/LinkPortableCore.c`
- `platform/apple/LinkPortableObd2.c`
- `platform/apple/LinkPortableUds.c`

Product repositories should consume these LINK files rather than creating product-owned translation units that directly include generic LINK implementation `.c` files.

Mercedes me native-adapter support remains a LINK capability because it is shared infrastructure used by MBLINK, but it is an optional provider implementation. `LINK_ENABLE_MERCEDES_ME_NATIVE` defaults to `1`. A non-Mercedes product may define it as `0`; the public Mercedes adapter types remain available to the shared Apple controller while provider discovery/parser hooks resolve to inert inline stubs, allowing the Mercedes implementation translation units to be omitted from that product binary.

This preserves one shared Apple controller while preventing a Ford, BMW, Audi or Jaguar product from carrying Mercedes provider implementation merely because it consumes LINK.
