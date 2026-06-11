# Design Decisions

This directory records **cross-cutting architectural decisions** — choices that
affect multiple containers or modules and are not obvious from reading a single
file.

Most design rationale lives in **code comments** (file headers, concept docs,
function contracts). An ADR is only needed when the decision spans files and
requires comparing alternatives.

## Index

| Decision | Status | Date |
|----------|--------|------|
| [Iterator Interface](iterator-interface.md) | Accepted | 2026-06-10 |
| [Tree Policy-Based Design](tree-policy-design.md) | Accepted | 2026-06-10 |
| [Heap Policy-Based Design](heap-policy-design.md) | Accepted | 2026-06-11 |
| [Hash Table Design](hash-table-design.md) | Accepted | 2026-06-11 |

## Format

```markdown
# ADR NNN: Title

## Status

Proposed | Accepted | Superseded by ADR-XXX | Deprecated

## Context

What is the problem? What constraints exist?

## Decision

What was decided? Be specific.

## Consequences

Positive and negative effects.

## Alternatives Considered

What other options were evaluated and why rejected?
```

## When to Write a Decision Record

Write a record when **all three** are true:
1. The decision affects multiple containers or modules
2. The decision is not obvious from reading the code
3. There were meaningful alternatives to evaluate

**Do not write a record for:**
- Single-file design choices (document in file header)
- Bug fixes or conformance corrections
- Refactoring that preserves semantics
- Adding a container that follows established patterns
- Future plans or roadmaps (use issues or notes)

## When to Delete a Record

A record can be deleted when its content has been absorbed into code comments
and it no longer adds value beyond what a reader gets from the code itself.
This is normal — decision records are working documents, not historical monuments.
