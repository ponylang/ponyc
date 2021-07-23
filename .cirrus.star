load("cirrus", "env", "fs", "hash")

def main():
  if env.get("CIRRUS_PR", "") != "":
    return create_pr_tasks()

  if env.get("CIRRUS_CRON", "") == "nightly":
    return create_nightly_tasks()

def create_pr_tasks():
  tasks = []

  pr_tasks = linux_tasks()
  # release tasks
  for t in pr_tasks:
    release = linux_pr_task(t['name'],
      t['image'],
      t['triple_vendor'],
      t['triple_os']
    )

    # finish release task creation
    release.update(linux_pr_scripts().items())
    tasks.append(release)



  return tasks

def create_nightly_tasks():
  tasks = []

  pr_tasks = linux_tasks()
  for t in pr_tasks:
    nightly = linux_pr_task(t['name'],
      t['image'],
      t['triple_vendor'],
      t['triple_os']
    )

    # finish nightly task creation
    nightly.update(linux_nightly_scripts().items())

    nightly['env']['CLOUDSMITH_API_KEY'] = 'ENCRYPTED[!2cb1e71c189cabf043ac3a9030b3c7708f9c4c983c86d07372ae58ad246a07c54e40810d038d31c3cf3ed8888350caca!'

    tasks.append(nightly)

  return tasks

def linux_tasks():
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

def linux_nightly_scripts():
  return {
    'nightly_script': 'bash .ci-scripts/x86-64-nightly.bash'
  }

def linux_release_scripts():
  return {
    'release_script': 'bash .ci-scripts/x86-64-release.bash'
  }
