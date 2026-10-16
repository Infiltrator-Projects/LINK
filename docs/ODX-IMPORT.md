# ODX / PDX import

Link keeps ODX parsing out of the runtime library.  The optional
`scripts/import-odx.py` development tool uses Mercedes-Benz's open-source
`odxtools` package to normalise ISO 22901 ODX/PDX descriptions into a small,
manufacturer-neutral JSON document.

This keeps three boundaries explicit:

- Link's C runtime has no Python or odxtools dependency.
- OEM diagnostic descriptions remain data; importing them does not silently
  promote a request into automatic live polling.
- ECU addresses, service names and coded request prefixes can be reviewed,
  diffed and consumed by manufacturer layers without copying the odxtools
  parser into Link.

Install the optional importer dependency and run:

```sh
python3 -m pip install odxtools
python3 scripts/import-odx.py vehicle.pdx -o vehicle.link-odx.json
```

The output records each ECU variant, its CAN receive/send IDs when available,
diagnostic services, request/response parameter metadata, and the contiguous
coded request prefix that can be established from ODX coded-constant
parameters.  For a UDS ReadDataByIdentifier service this may expose a prefix
such as `22F190`, but Link treats that only as source description data until
the consuming manufacturer layer applies its own verification policy.

The importer calls the public odxtools API; no odxtools source code is vendored
into Link.  odxtools is published by Mercedes-Benz under the MIT licence.
