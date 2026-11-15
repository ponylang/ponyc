## Fix `Path.split` and `Path.dir` dropping the root from absolute paths

`Path.split("/a")` returned `(".", "a")` instead of `("/", "a")`, and `Path.dir("/a")` returned `"."` instead of `"/"`. The root separator was lost from any path whose last component sat directly under the root — `/a`, `/`, and the volume root of `//host/share` paths. `Path.split("//h/s")` returned `("/h", "s")` instead of `("//h/s", "")`.
