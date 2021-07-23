load("cirrus", "env", "fs", "hash")

def main():
  if env.get("CIRRUS_PR", "") != "":
    t = linux_pr_task('test',
      'ponylang/ponyc-ci-x86-64-unknown-linux-rocky8-builder:20210707',
      '20210707',
      'unknown',
      'linuxrocky8'
    )
    return [t]

def linux_pr_task(name, image, cache_buster, triple_vendor, triple_os):
  cache_key = hash.md5(
    str(fs.read('lib/CMakeLists.txt'))
    + cache_buster
    + triple_vendor
    + triple_os
    )

  cpu = 8
  memory = 24

  task = {
    'name': name,
    'container': {
      'image': image,
      'cpu': cpu,
      'memory': memory
    },
    'env': {
      'CACHE_BUSTER': cache_buster,
      'TRIPLE_VENDOR': triple_vendor,
      'TRIPLE_OS': triple_os
    },
     'libs_cache': {
      'folder': 'build/libs',
      'key': cache_key,
      'populate_script': 'make libs build_flags=-j8'
    },
    'configure_script': 'make configure arch=x86-64',
    'build_script': 'make build arch=x86-64 build_flags=-j8',
    'test_script': 'make test-ci arch=x86-64'
  }

  return task
