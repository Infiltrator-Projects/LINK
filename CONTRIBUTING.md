# Contributing

## Ownership first

Before changing LINK, decide whether the behaviour belongs in Common, LINK or this manufacturer/product repository. Do not solve a shared problem by creating another private copy.

## Evidence first

Manufacturer-specific additions need traceable evidence. Preserve raw captures and uncertainty where interpretation is not justified. Do not infer one brand's behaviour from another brand.

## Safety first

New decoders do not automatically gain transmit authority. Preserve deny-by-default policy and add tests for permission boundaries when request capability changes.

## Verification

Run the repository's native tests and relevant CI. Keep exact dependency gitlinks intact. Physical-adapter or vehicle claims require corresponding physical evidence, not just simulator/replay success.

## Repository policy

main is the working branch. Published tags/releases are immutable. Update architecture, roadmap and validation documentation when ownership, support or evidence boundaries change.
