## Fix LSP hover showing on declaration keywords

Hovering over a declaration keyword (`class`, `actor`, `trait`, `interface`, `primitive`, `type`, `struct`, `fun`, `be`, `new`, `let`, `var`, or `embed`) incorrectly showed a hover popup for the entity, method, or field being declared. Hover information now only appears when hovering over the declaration name itself.
