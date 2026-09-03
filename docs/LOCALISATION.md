# LINK localisation

LINK owns reusable automotive and shared application-shell language. Infiltratr Common supplies the allocation-free catalogue engine; LINK supplies the automotive/UI catalogues and platform locale adapters; MBLINK, JAGLINK, BMWLINK, AUDILINK and FORDLINK keep only manufacturer-specific terminology and content.

The initial built-in locales are `en-AU`, `en-US`, `de-DE`, `fr-FR`, `es-ES` and `it-IT`, with `en-AU` as the canonical complete fallback. Technical identifiers such as DTC codes, CAN IDs, UDS services, DIDs, baud rates and ECU part numbers are never translated.

Visible reusable text must be referenced by semantic keys through `link_i18n_tr()` or `link_i18n_format()`. Linux, Windows Discover and Apple faces should initialise the catalogue from the operating system's preferred locale and may expose an explicit override. Product-specific catalogues use the same Infiltratr Common engine rather than forking LINK's tables.


## Apple language and measurement preferences

Apple product faces use LINK's installed locale catalogue and persist the
explicitly selected language. The default is `en-AU`, matching MBLINK's
established behaviour. Product applications apply that selected locale to
their own interface; LINK does not own the complete Settings page.

Measurement units are independent from language. LINK exposes the same two
choices established by MBLINK: Metric and US customary. The MBLINK conversions
for temperature, vehicle speed, pressure and fuel rate are shared; canonical
diagnostic values and CSV evidence remain unchanged.
