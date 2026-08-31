## Fix incorrect `#read->trn` viewpoint adaptation bounds

The compiler computed wrong upper and lower bounds when adapting `trn` through the `#read` generic capability. The upper bound was `box` instead of `trn`, and the lower bound was `trn` instead of `box`. This affected type checking of `this->trn` fields in classes and actors whose receiver capability is generic, and any other code path where the compiler needs the bounds of `#read->trn`.
