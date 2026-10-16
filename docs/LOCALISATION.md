# Link localisation

Link owns reusable automotive and shared application-shell language. Libraries supplies the allocation-free catalogue engine; Link supplies the automotive/UI catalogues and platform locale adapters; MBLink, JAGLink, BMWLink, AUDILink and FORDLink keep only manufacturer-specific terminology and content.

The initial built-in locales are `en-AU`, `en-US`, `de-DE`, `fr-FR`, `es-ES` and `it-IT`, with `en-AU` as the canonical complete fallback. Technical identifiers such as DTC codes, CAN IDs, UDS services, DIDs, baud rates and ECU part numbers are never translated.

Visible reusable text must be referenced by semantic keys through `link_i18n_tr()` or `link_i18n_format()`. Linux and Windows faces may initialise from the operating system's preferred locale. Apple product faces intentionally retain MBLink's established `en-AU` default until the user selects an explicit language, and persist that selection. Product-specific catalogues use the same Libraries engine rather than forking Link's tables.


## Apple language and measurement preferences

Apple product faces use Link's installed locale catalogue and persist the
explicitly selected language. The default is `en-AU`, matching MBLink's
established behaviour. Product applications apply that selected locale to
their own interface; Link does not own the complete Settings page.

Measurement units are independent from language. Link exposes the same two
choices established by MBLink: Metric and US customary. The MBLink conversions
for temperature, vehicle speed, pressure and fuel rate are shared; canonical
diagnostic values and CSV evidence remain unchanged.
