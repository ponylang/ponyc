# Docker

Docker based Pony environments are available. The following tags are available:

- latest (most recent build of the `master` branch)
- release (most recent release)
- x.y.z (tagged release e.g. 0.33.1)

If you prefer to use the Alpine images, you can use the following tags:

- alpine (most recent build of the `master` branch)
- release-alpine (most recent release)
- x.y.z-alpine (tagged release e.g. 0.33.1)

The docker images also include common Pony tools like [ponyup](https://github.com/ponylang/ponyup), [corral](https://github.com/ponylang/corral), and [changelog-tool](https://github.com/ponylang/changelog-tool).

## Using Pony from Docker

You'll need to install Docker using [the instructions here](https://docs.docker.com/engine/installation/). Then you can pull a pony docker image using the following command (where TAG is the tag you want to use)

```bash
docker pull ponylang/ponyc:TAG
```

Then you'll be able to run `ponyc` to compile a Pony program in a given directory, running a command like this:

```bash
docker run -v /path/to/my-code:/src/main ponylang/ponyc
```

If you're unfamiliar with Docker, remember to ensure that whatever path you provide for `/path/to/my-code` is a full path name and not a relative path, and also note the lack of a closing slash, `/`, at the *end* of the path name.

Note that if your host doesn't match the docker container, you'll probably have to run the resulting program inside the docker container as well, using a command like this:

```bash
docker run -v /path/to/my-code:/src/main ponylang/ponyc ./main
```

### Docker for Windows

Pull an image as above:

```bash
docker pull ponylang/ponyc:TAG
```

Share a local drive (volume), such as `c:`, with Docker for Windows, so that they are available to your containers. (Refer to [shared drives](https://docs.docker.com/docker-for-windows/#shared-drives) in the Docker for Windows documentation for details.)

Then you'll be able to run `ponyc` to compile a Pony program in a given directory, running a command like this:

```bash
docker run -v c:/path/to/my-code:/src/main ponylang/ponyc
```

Note the inserted drive letter. Replace with your drive letter as appropriate.

To run a program, run a command like this:

```bash
docker run -v c:/path/to/my-code:/src/main ponylang/ponyc ./main
```

To compile and run in one step run a command like this:

```bash
docker run -v c:/path/to/my-code:/src/main ponylang/ponyc sh -c "ponyc && ./main"
```
