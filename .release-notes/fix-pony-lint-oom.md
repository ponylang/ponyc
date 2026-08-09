## Fix pony-lint running out of memory on repos with many packages

Running `pony-lint` on a repository with many package directories — for example, a library with 35 example programs — used enough memory to crash the process. Memory grew with each package and was not released until the run finished, so a repo that needed about 1 GB for one package could need 18 GB for 37.

pony-lint now releases memory between packages. Peak usage stays near the cost of one compilation regardless of how many packages the repo contains.
