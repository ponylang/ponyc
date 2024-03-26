## Fix incorrect markdown formatting for types from documentation generation

Previously, we were incorrectly creating field types in markdown documentation. The markdown for the type should have been on a single line but for long union types, it would end up crossing lines. That resulted in broken markdown that wouldn't display correctly.
