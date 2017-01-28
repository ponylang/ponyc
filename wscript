#! /usr/bin/env python
# encoding: utf-8

# This script is used to configure and build the Pony compiler.
# Target definitions are in the build() function.

import sys, subprocess

# check if the operating system's description contains a string
def os_is(name):
    return (sys.platform.lower().rfind(name) > -1)

# runs a shell command and captures its standard output
def cmd_output(cmd):
    if (sys.version_info > (3,0)):
        return str(subprocess.check_output(cmd), 'utf-8').strip('\n').strip('\'')
    else:
        return str(subprocess.check_output(cmd)).strip('\n').strip('\'')

# build ponyc version string
APPNAME = 'ponyc'
VERSION = '?'
with open('VERSION') as v:
    VERSION = v.read().strip() + '-' + cmd_output(['git', 'rev-parse', '--short', 'HEAD'])

# source and build directories
top = '.'
out = 'build'

# various configuration parameters
CONFIGS = [
    'debug',
    'release'
]

MSVC_VERSION = '14'

# keep these in sync with the list in .appveyor.yml
LLVM_VERSIONS = [
    '3.9.1',
    '3.9.0',
    '3.8.1',
    '3.7.1'
]

WINDOWS_LIBS_TAG = "v1.2.0"
LIBRESSL_VERSION = "2.5.0"
PCRE2_VERSION = "10.21"

# Adds an option for specifying debug or release mode.
def options(ctx):
    ctx.add_option('--config', action='store', default=CONFIGS[0], help='debug or release build')
    ctx.add_option('--llvm', action='store', default=LLVM_VERSIONS[0], help='llvm version')

# This replaces the default versions of these context classes with subclasses
# that set their "variant", i.e. build directory, to the debug or release config.
def init(ctx):
    from waflib.Build import BuildContext, CleanContext, InstallContext, UninstallContext
    for c in (BuildContext, CleanContext, InstallContext, UninstallContext, testContext):
        name = c.__name__.replace('Context','').lower()
        class tmp(c):
            cmd = name
            variant = ctx.options.config.lower() + '-llvm-' + ctx.options.llvm

# sets up one-time configuration for the build variants
def configure(ctx):
    ctx.env.append_value('DEFINES', [
        'PONY_VERSION="' + VERSION + '"',
        'PONY_USE_BIGINT',
    ])

    if os_is('win32'):
        import os
        ctx.env.PONYC_EXTRA_LIBS = [
            'kernel32', 'user32', 'gdi32', 'winspool', 'comdlg32',
            'advapi32', 'shell32', 'ole32', 'oleaut32', 'uuid',
            'odbc32', 'odbccp32', 'vcruntime', 'ucrt'
        ]

        ctx.env.MSVC_VERSIONS = ['msvc ' + MSVC_VERSION + '.0']
        ctx.env.MSVC_TARGETS = ['x64']
        ctx.load('msvc')
        ctx.env.CC_SRC_F = '/Tp' # force C++ for all files
        ctx.env.CXX_SRC_F = '/Tp'
        ctx.env.append_value('DEFINES', [
            '_CRT_SECURE_NO_WARNINGS', '_MBCS',
            'PLATFORM_TOOLS_VERSION=' + MSVC_VERSION + '0',
            'BUILD_COMPILER="msvc-' + MSVC_VERSION + '-x64"'
        ])
        ctx.env.append_value('LIBPATH', [ ctx.env.LLVM_DIR ])

        base_env = ctx.env.derive()

        for llvm_version in LLVM_VERSIONS:
            bld_env = base_env.derive()
            bld_env.PONYLIBS_NAME = 'PonyWindowsLibs' + \
                '-LLVM-' + llvm_version + \
                '-LibreSSL-' + LIBRESSL_VERSION + \
                '-PCRE2-' + PCRE2_VERSION + '-' + \
                WINDOWS_LIBS_TAG
            bld_env.PONYLIBS_DIR = bld_env.PONYLIBS_NAME
            bld_env.LIBRESSL_DIR = os.path.join(bld_env.PONYLIBS_DIR, \
                'lib', 'libressl-' + LIBRESSL_VERSION)
            bld_env.PCRE2_DIR = os.path.join(bld_env.PONYLIBS_DIR, \
                'lib', 'pcre2-' + PCRE2_VERSION)
            bld_env.LLVM_DIR = os.path.join(bld_env.PONYLIBS_DIR, \
                'lib', 'LLVM-' + llvm_version)
            bld_env.append_value('DEFINES', [
                'LLVM_VERSION="' + llvm_version + '"'
            ])

            bldName = 'debug-llvm-' + llvm_version
            ctx.setenv(bldName, env = bld_env)
            ctx.env.append_value('DEFINES', [
                'DEBUG',
                'PONY_BUILD_CONFIG="debug"'
            ])
            msvcDebugFlags = [
                '/EHsc', '/MP', '/GS', '/W3', '/Zc:wchar_t', '/Zi',
                '/Gm-', '/Od', '/Zc:inline', '/fp:precise', '/WX',
                '/Zc:forScope', '/Gd', '/MD', '/FS', '/DEBUG'
            ]
            ctx.env.append_value('CFLAGS', msvcDebugFlags)
            ctx.env.append_value('CXXFLAGS', msvcDebugFlags)
            ctx.env.append_value('LINKFLAGS', [
                '/NXCOMPAT', '/SUBSYSTEM:CONSOLE', '/DEBUG'
            ])

            bldName = 'release-llvm-' + llvm_version
            ctx.setenv(bldName, env = bld_env)
            ctx.env.append_value('DEFINES', [
                'NDEBUG',
                'PONY_BUILD_CONFIG="release"'
            ])
            msvcReleaseFlags = [
                '/EHsc', '/MP', '/GS', '/TC', '/W3', '/Gy', '/Zc:wchar_t',
                '/Gm-', '/O2', '/Zc:inline', '/fp:precise', '/GF', '/WX',
                '/Zc:forScope', '/Gd', '/Oi', '/MD', '/FS'
            ]
            ctx.env.append_value('CFLAGS', msvcReleaseFlags)
            ctx.env.append_value('CXXFLAGS', msvcReleaseFlags)
            ctx.env.append_value('LINKFLAGS', [
                '/NXCOMPAT', '/SUBSYSTEM:CONSOLE'
            ])


# specifies build targets
def build(ctx):
    import os

    buildDir = ctx.bldnode.abspath()

    llvmIncludes = []
    llvmLibs = []
    sslIncludes = []

    # download windows libraries needed for building
    if (os_is('win32')):
        libsName = ctx.env.PONYLIBS_NAME
        libsDir = os.path.join(buildDir, ctx.env.PONYLIBS_DIR)
        libresslDir = os.path.join(buildDir, ctx.env.LIBRESSL_DIR)
        pcre2Dir = os.path.join(buildDir, ctx.env.PCRE2_DIR)
        llvmDir = os.path.join(buildDir, ctx.env.LLVM_DIR)

        if (ctx.cmd == 'clean'): # make sure to delete the libs directory
            import shutil
            if os.path.exists(libsDir):
                shutil.rmtree(libsDir)
        else:
            llvmIncludes.append(os.path.join(llvmDir, 'include'))
            ctx.env.append_value('LIBPATH', os.path.join(llvmDir, 'lib'))

            sslIncludes.append(os.path.join(libresslDir, 'include'))

            if not (os.path.exists(libsDir) \
                and os.path.exists(libresslDir) \
                and os.path.exists(pcre2Dir) \
                and os.path.exists(llvmDir)):
                libsZip = libsName + '.zip'
                libsFname = os.path.join(buildDir, libsZip)
                if not os.path.isfile(libsFname):
                    libsUrl = 'https://github.com/kulibali/ponyc-windows-libs/releases/download/' \
                        + WINDOWS_LIBS_TAG + '/' + libsZip
                    print('downloading ' + libsUrl)
                    if (sys.version_info > (3,0)):
                        import urllib.request
                        urllib.request.urlretrieve(libsUrl, libsFname)
                    else:
                        import urllib2
                        buf = urllib2.urlopen(libsUrl)
                        with open(libsFname, 'wb') as output:
                            while True:
                                data = buf.read(65536)
                                if data:
                                    output.write(data)
                                else:
                                    break
                print('unzipping ' + libsZip)
                import zipfile
                if not (os.path.exists(libsDir)):
                    os.makedirs(libsDir)
                with zipfile.ZipFile(libsFname) as zf:
                    zf.extractall(libsDir)

                import shutil
                shutil.copy(os.path.join(pcre2Dir, 'pcre2-8.lib'), buildDir)
                shutil.copy(os.path.join(libresslDir, 'lib', 'crypto.lib'), buildDir)
                shutil.copy(os.path.join(libresslDir, 'lib', 'ssl.lib'), buildDir)
                shutil.copy(os.path.join(libresslDir, 'lib', 'tls.lib'), buildDir)

            llvmConfig = os.path.join(llvmDir, 'bin', 'llvm-config.exe')
            llvmLibFiles = cmd_output([llvmConfig, '--libs'])
            import re
            if ctx.options.llvm.startswith(('3.7', '3.8')):
                llvmLibs = [re.sub(r'-l([^\\\/)]+)', r'\1', x) for x in llvmLibFiles.split(' ')]
            else:
                llvmLibs = [re.sub(r'.*[\\\/]([^\\\/)]+)\.lib', r'\1', x) for x in llvmLibFiles.split(' ')]

    # build targets:

    # gtest
    ctx(
        features = 'cxx cxxstlib seq',
        target   = 'gtest',
        source   = ctx.path.ant_glob('lib/gtest/*.cc'),
    )

    # libponyc
    ctx(
        features  = 'c cxx cxxstlib seq',
        target    = 'libponyc',
        source    = ctx.path.ant_glob('src/libponyc/**/*.c') + \
                    ctx.path.ant_glob('src/libponyc/**/*.cc'),
        includes  = [ 'src/common' ] + llvmIncludes + sslIncludes
    )

    # libponyrt
    ctx(
        features = 'c cxx cxxstlib seq',
        target   = 'libponyrt',
        source   = ctx.path.ant_glob('src/libponyrt/**/*.c'),
        includes = [ 'src/common', 'src/libponyrt' ] + sslIncludes
    )

    # ponyc
    ctx(
        features  = 'c cxx cxxprogram seq',
        target    = 'ponyc',
        source    = ctx.path.ant_glob('src/ponyc/**/*.c'),
        includes  = [ 'src/common' ],
        use       = [ 'libponyc', 'libponyrt' ],
        lib       = llvmLibs + ctx.env.PONYC_EXTRA_LIBS
    )

    # testc
    ctx(
        features  = 'c cxx cxxprogram seq',
        target    = 'testc',
        source    = ctx.path.ant_glob('test/libponyc/**/*.cc'),
        includes  = [ 'src/common', 'src/libponyc', 'lib/gtest' ] + llvmIncludes,
        use       = [ 'gtest', 'libponyc', 'libponyrt' ],
        lib       = llvmLibs + ctx.env.PONYC_EXTRA_LIBS
    )

    # testrt
    ctx(
        features  = 'c cxx cxxprogram seq',
        target    = 'testrt',
        source    = ctx.path.ant_glob('test/libponyrt/**/*.cc'),
        includes  = [ 'src/common', 'src/libponyrt', 'lib/gtest' ],
        use       = [ 'gtest', 'libponyrt' ],
        lib       = ctx.env.PONYC_EXTRA_LIBS
    )

    # stdlib tests
    stdlibTarget = 'stdlib'
    if os_is('win32'):
        stdlibTarget = 'stdlib.exe'

    ctx(
        features = 'seq',
        rule     = os.path.join(ctx.bldnode.abspath(), 'ponyc') + ' -d -s ../../packages/stdlib',
        target   = stdlibTarget,
        source   = ctx.bldnode.ant_glob('ponyc*') + ctx.path.ant_glob('packages/**/*.pony'),
    )


# this command runs the test suites
def test(ctx):
    import os
    buildDir = ctx.bldnode.abspath()

    total = 0
    passed = 0

    total = total + 1
    testc = os.path.join(buildDir, 'testc')
    returncode = subprocess.call([ testc ])
    if returncode == 0:
        passed = passed + 1

    total = total + 1
    testrt = os.path.join(buildDir, 'testrt')
    print(testrt)
    returncode = subprocess.call([ testrt ])
    if returncode == 0:
        passed = passed + 1

    total = total + 1
    stdlib = os.path.join(buildDir, 'stdlib')
    print(stdlib)
    returncode = subprocess.call([ stdlib, '--sequential' ])
    if returncode == 0:
        passed = passed + 1

    print('')
    print('{0} test suites; {1} passed; {2} failed'.format(total, passed, total - passed))
    if passed < total:
        sys.exit(1)


# subclass the build context for the test command,
# otherwise the context wouldn't be set up
from waflib.Build import BuildContext
class testContext(BuildContext):
    cmd = 'test'
    fun = 'test'
