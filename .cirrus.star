load("cirrus", "env", "fs", "hash")

def main():
  if env.get("CIRRUS_PR", "") != "":
    #tasks = []
    #prt = {}
    #for t in linux_pr_tasks():
    t = linux_pr_tasks()
    prt = linux_pr_task(t['name'],
        t['image'],
        t['triple_vendor'],
        t['triple_os']
      )
      #tasks.append(prt)

    return [prt]

def linux_pr_tasks():
  #return [
  return  { 'name': 'x86-64-unknown-linux-rocky8',
      'image': 'ponylang/ponyc-ci-x86-64-unknown-linux-rocky8-builder:20210707',
      'triple_vendor': 'unknown',
      'triple_os': 'linux-rocky8'
    }

  #  { 'name': 'x86-64-unknown-linux-centos8',
  #    'image': 'ponylang/#ponyc-ci-x86-64-unknown-linux-centos8-builder:20210225',
  #    'triple_vendor: 'unknown',
  #    'triple_os': 'linux-centos8'
  #  }
  #]

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
    'configure_script': 'make configure arch=x86-64',
    'build_script': 'make build arch=x86-64 build_flags=-j8',
    'test_script': 'make test-ci arch=x86-64'
  }

  return task
