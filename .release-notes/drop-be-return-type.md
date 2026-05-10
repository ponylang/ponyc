## LSP: drop behaviour return type from hover and signature help

Hover popups and signature help for `be` behaviours no longer display a return type. Behaviour return types are always `None val` inserted by the compiler and cannot be written explicitly in source, so showing them was unnecessary and did not add information.
