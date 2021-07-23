load("cirrus", "env", "fs", "hash")

def main():
  if env.get("CIRRUS_PR", "") != "":
    tasks = []
    pr_tasks = linux_pr_tasks()
    for t in pr_tasks:
      prt = linux_pr_task(t['name'],
        t['image'],
        t['triple_vendor'],
        t['triple_os']
      )

      prt.update(linux_pr_scripts().items())
      tasks.append(prt)

    return tasks

def linux_pr_tasks():
  return [
    { 'name': 'x86-64-unknown-linux-rocky8',
      'image': 'ponylang/ponyc-ci-x86-64-unknown-linux-rocky8-builder:20210707',
      'triple_vendor': 'unknown',
      'triple_os': 'linux-rocky8'
    },
    { 'name': 'x86-64-unknown-linux-centos8',
      'image': 'ponylang/ponyc-ci-x86-64-unknown-linux-centos8-builder:20210225',
      'triple_vendor': 'unknown',
      'triple_os': 'linux-centos8'
    }
  ]

def linux_pr_task(name, image, triple_vendor, triple_os):
  cache_key = hash.md5(str(fs.read('lib/CMakeLists.txt')) + image)

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
      'TRIPLE_VENDOR': triple_vendor,
      'TRIPLE_OS': triple_os
    },
     'libs_cache': {
      'folder': 'build/libs',
      'key': cache_key,
      'populate_script': 'make libs build_flags=-j8'
    },
  }

  return task

def linux_pr_scripts(config="release"):
  return {
    'configure_script': 'make configure config=' + config,
    'build_script': 'make build build_flags=-j8 config=' + config,
    'test_script': 'make test-ci config=' + config
  }

def linux_nightly_script():
  return {
    'nightly_script': 'bash .ci-scripts/x86-64-nightly.bash'
  }

def linux_release_script():
  return {
    'release_script': 'bash .ci-scripts/x86-64-release.bash'
  }
