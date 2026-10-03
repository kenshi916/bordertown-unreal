# Validation scope

The published native C++ files are an unchanged snapshot of the current working project. The public snapshot has not been built as a complete playable game because third-party content is intentionally omitted.

Python files are syntax-checked before publication. Weapon results mentioned in the README refer to rendered checks in the licensed local project, not CI runs from a fresh public clone. Each weapon requires separate visual review of grip, ADS, materials and moving parts. Network play, final art quality and third-person hand choreography remain separate acceptance checks.

Generated files, local profiles, credential-bearing development settings and binary content are excluded. Publishing uses an explicit source-file allowlist, not a recursive project upload.
