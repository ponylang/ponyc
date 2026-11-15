## NormalizeIRI and IRIEquivalent now reject illegal literal non-ASCII characters

IRIs (Internationalized Resource Identifiers, RFC 3987) extend URIs with non-ASCII characters. `NormalizeIRI` normalizes an IRI so that equivalent forms compare equal, and `IRIEquivalent` tests whether two IRI/URI forms refer to the same resource. Both now return `InvalidIRILiteral` when an input contains literal non-ASCII characters that RFC 3987 does not allow: `iprivate` codepoints outside the query component, bidi formatting characters, C1 controls, noncharacters, and invalid UTF-8 byte sequences.

Previously, `NormalizeIRI` accepted these inputs and produced a wrong result — a normalized form that preserved illegal literal bytes unchanged. `IRIEquivalent` compared this against a correctly percent-encoded form and returned `false` instead of indicating an error. `IRIEquivalent(x, x)` returned `true` for such inputs, hiding the problem entirely.

Both return types gain the `InvalidIRILiteral` variant. Exhaustive matches must add a case for it:

Before:

```pony
match \exhaustive\ NormalizeIRI(u)
| let n: URI val => use(n)
| let e: InvalidPercentEncoding val => handle(e)
end
```

After:

```pony
match \exhaustive\ NormalizeIRI(u)
| let n: URI val => use(n)
| let e: InvalidPercentEncoding val => handle(e)
| let e: InvalidIRILiteral val => handle(e)
end
```

The same change applies to `IRIEquivalent`. Callers who match the error types as `(InvalidPercentEncoding val | InvalidIRILiteral val)` in a single arm can handle both uniformly.
