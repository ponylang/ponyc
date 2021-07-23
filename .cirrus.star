load("cirrus", "env")
load("github.com/cirrus-modules/graphql", "cache_info")

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
  task = {
    'name': name,
    'container': {
      'image': image,
      'cpu': 2,
      'memory': 4
    },
    'env': {
      'CACHE_BUSTER': cache_buster,
      'TRIPLE_VENDOR': triple_vendor,
      'TRIPLE_OS': triple_os
    },
    'configure_script': 'make configure arch=x86-64',
    'build_script': 'make build arch=x86-64 build_flags=-j8',
    'test_script': 'make test-ci arch=x86-64'
  }

  return task
