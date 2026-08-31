## Fix incorrect viewpoint adaptation bounds for generic capability parameters

When accessing a field through a `trn` reference, and the field's type used a generic capability constraint (`#read`, `#alias`, or `#any`), the compiler computed incorrect capability bounds for the viewpoint-adapted type. Programs with unsound capability usage in these combinations could pass type checking without error.

Programs that relied on the incorrect bounds will now be correctly rejected by the compiler.
