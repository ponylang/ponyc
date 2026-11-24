## Preserve `iftype` narrowing in inherited methods

Valid trait methods using `iftype` expressions or specialization guards could fail to compile after inheritance. The compiler now preserves narrowing when the receiving type passes one of its own type parameters to the trait, including when it passes the same parameter to several trait parameters.
