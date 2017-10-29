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
    VERSION = v.read().strip()
    try:
        rev = '-' + cmd_output(['git', 'rev-parse', '--short', 'HEAD'])
        VERSION = VERSION + rev
    except:
        pass

# source and build directories
top = '.'
out = 'build'

# various configuration parameters
CONFIGS = [
    'release',
    'debug'
]

MSVC_VERSIONS = [ '15.4', '15.0', '14.0' ]

# keep these in sync with the list in .appveyor.yml
LLVM_VERSIONS = [
    '3.9.1',
    '3.8.1',
    '3.7.1'
]

WINDOWS_LIBS_TAG = "v1.5.0"
LIBRESSL_VERSION = "2.6.1"
PCRE2_VERSION = "10.30"

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

        base_env = ctx.env
        base_env.MSVC_VERSIONS = [ 'msvc ' + v for v in MSVC_VERSIONS ]
        base_env.MSVC_TARGETS = [ 'x64' ]
        ctx.load('msvc')

        base_env.CC_SRC_F = '/Tp' # force C++ for all files
        base_env.CXX_SRC_F = '/Tp'
        base_env.append_value('DEFINES', [
            '_CRT_SECURE_NO_WARNINGS', '_MBCS',
            'PLATFORM_TOOLS_VERSION=%d0' % base_env.MSVC_VERSION,
            'BUILD_COMPILER="msvc-%d-x64"' % base_env.MSVC_VERSION
        ])
        base_env.append_value('LIBPATH', [ base_env.LLVM_DIR ])

        for llvm_version in LLVM_VERSIONS:
            bld_env = base_env.derive()
            bld_env.PONYLIBS_DIR = 'ThirdParty'
            bld_env.LIBRESSL_DIR = os.path.join(bld_env.PONYLIBS_DIR, \
                'lib', 'libressl-' + LIBRESSL_VERSION)
            bld_env.PCRE2_DIR = os.path.join(bld_env.PONYLIBS_DIR, \
                'lib', 'pcre2-' + PCRE2_VERSION)
            bld_env.append_value('DEFINES', [
                'LLVM_VERSION="' + llvm_version + '"',
                'PONY_VERSION_STR="' + \
                    '%s [%s]\\ncompiled with: llvm %s -- msvc-%d-x64"' % \
                    (VERSION, ctx.options.config, \
                        llvm_version, base_env.MSVC_VERSION)
            ])

            libs_name = 'PonyWinLibs' + \
                '-LLVM-' + llvm_version + \
                '-LibreSSL-' + LIBRESSL_VERSION + \
                '-PCRE2-' + PCRE2_VERSION + '-' + \
                WINDOWS_LIBS_TAG

            # Debug configuration
            bldName = 'debug-llvm-' + llvm_version
            ctx.setenv(bldName, env = bld_env)
            ctx.env.PONYLIBS_NAME = libs_name + '-Debug'
            ctx.env.LLVM_DIR = os.path.join(ctx.env.PONYLIBS_DIR, \
                'lib', 'LLVM-' + llvm_version + '-Debug')

            ctx.env.append_value('DEFINES', [
                'DEBUG',
                'PONY_BUILD_CONFIG="debug"',
                '_SCL_SECURE_NO_WARNINGS'
            ])
            msvcDebugFlags = [
                '/EHsc', '/MP', '/GS', '/W3', '/Zc:wchar_t', '/Zi',
                '/Gm-', '/Od', '/Zc:inline', '/fp:precise', '/WX',
                '/Zc:forScope', '/Gd', '/MDd', '/FS', '/DEBUG'
            ]
            ctx.env.append_value('CFLAGS', msvcDebugFlags)
            ctx.env.append_value('CXXFLAGS', msvcDebugFlags)
            ctx.env.append_value('LINKFLAGS', [
                '/NXCOMPAT', '/SUBSYSTEM:CONSOLE', '/DEBUG', '/ignore:4099'
            ])
            ctx.env.PONYC_EXTRA_LIBS = [
                'kernel32', 'user32', 'gdi32', 'winspool', 'comdlg32',
                'advapi32', 'shell32', 'ole32', 'oleaut32', 'uuid',
                'odbc32', 'odbccp32', 'vcruntimed', 'ucrtd', 'Ws2_32',
                'dbghelp', 'Shlwapi'
            ]

            # Release configuration
            bldName = 'release-llvm-' + llvm_version
            ctx.setenv(bldName, env = bld_env)
            ctx.env.PONYLIBS_NAME = libs_name + '-Release'
            ctx.env.LLVM_DIR = os.path.join(ctx.env.PONYLIBS_DIR, \
                'lib', 'LLVM-' + llvm_version + '-Release')

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
            ctx.env.PONYC_EXTRA_LIBS = [
                'kernel32', 'user32', 'gdi32', 'winspool', 'comdlg32',
                'advapi32', 'shell32', 'ole32', 'oleaut32', 'uuid',
                'odbc32', 'odbccp32', 'vcruntime', 'ucrt', 'Ws2_32',
                'dbghelp', 'Shlwapi'
            ]


# specifies build targets
def build(ctx):
    import os

    buildDir = ctx.bldnode.abspath()
    packagesDir = os.path.join(ctx.srcnode.abspath(), 'packages')

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
                libsFname = os.path.join(buildDir, '..', libsZip)
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
                if (ctx.options.config == 'debug'):
                    shutil.copy(os.path.join(pcre2Dir, 'pcre2-8d.lib'), buildDir)
                    shutil.copy(os.path.join(pcre2Dir, 'pcre2-8d.lib'), os.path.join(buildDir, 'pcre2-8.lib'))
                else:
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

    # gbenchmark
    ctx(
        features = 'cxx cxxstlib seq',
        target   = 'gbenchmark',
        source   = ctx.path.ant_glob('lib/gbenchmark/*.cc'),
        includes = [ 'lib/gbenchmark/include' ],
        defines  = [ 'HAVE_STD_REGEX' ]
    )
    
    # blake2
    ctx(
        features = 'c seq',
        target   = 'blake2',
        source   = ctx.path.ant_glob('lib/blake2/*.c'),
    )

    # libponyc
    ctx(
        features  = 'c cxx cxxstlib seq',
        target    = 'libponyc',
        source    = ctx.path.ant_glob('src/libponyc/**/*.c') + \
                    ctx.path.ant_glob('src/libponyc/**/*.cc'),
        includes  = [ 'src/common', 'lib/blake2' ] + llvmIncludes + sslIncludes,
        use       = [ 'blake2' ]
    )

    # libponyc.benchmarks
    ctx(
        features = 'cxx cxxprogram seq',
        target   = 'libponyc.benchmarks',
        source   = ctx.path.ant_glob('benchmark/libponyc/**/*.cc'),
        includes = [ 'lib/gbenchmark/include' ],
        use      = [ 'libponyc', 'gbenchmark' ],
        lib      = ctx.env.PONYC_EXTRA_LIBS
    )

    # libponyrt
    ctx(
        features = 'c cxx cxxstlib seq',
        target   = 'libponyrt',
        source   = ctx.path.ant_glob('src/libponyrt/**/*.c'),
        includes = [ 'src/common', 'src/libponyrt' ] + sslIncludes,
        defines  = [ 'PONY_NO_ASSERT' ]
    )

    # libponyrt.benchmarks
    ctx(
        features = 'cxx cxxprogram seq',
        target   = 'libponyrt.benchmarks',
        source   = ctx.path.ant_glob('benchmark/libponyrt/**/*.cc'),
        includes = [ 'lib/gbenchmark/include', 'src/common', 'src/libponyrt' ],
        use      = [ 'libponyrt', 'gbenchmark' ],
        lib      = ctx.env.PONYC_EXTRA_LIBS
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
    testcUses = [ 'gtest', 'libponyc', 'libponyrt' ]
    testcLibs = llvmLibs + ctx.env.PONYC_EXTRA_LIBS
    if os_is('win32'):
        testcUses = [ 'gtest', 'libponyc' ]
        testcLibs = [ '/WHOLEARCHIVE:libponyrt' ] + testcLibs

    ctx(
        features  = 'c cxx cxxprogram seq',
        target    = 'testc',
        source    = ctx.path.ant_glob('test/libponyc/**/*.cc'),
        includes  = [ 'src/common', 'src/libponyc', 'src/libponyrt',
                      'lib/gtest' ] + llvmIncludes,
        defines   = [ 'PONY_PACKAGES_DIR="' + packagesDir.replace('\\', '\\\\') + '"'],
        use       = testcUses,
        lib       = testcLibs,
        linkflags = [ '/INCREMENTAL:NO' ]
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


# this command builds and runs the test suites
def test(ctx):
    import os

    # stdlib tests
    stdlibTarget = 'stdlib'
    if os_is('win32'):
        stdlibTarget = 'stdlib.exe'

    ctx(
        features = 'seq',
        rule     = os.path.join(ctx.bldnode.abspath(), 'ponyc') + ' -d -s --checktree --verify ../../packages/stdlib',
        target   = stdlibTarget,
        source   = ctx.bldnode.ant_glob('ponyc*') + ctx.path.ant_glob('packages/**/*.pony'),
    )

    # grammar file
    ctx(
        features = 'seq',
        rule     = os.path.join(ctx.bldnode.abspath(), 'ponyc') + ' --antlr > pony.g.new',
        target   = 'pony.g.new',
    )

    def run_tests(ctx):
        buildDir = ctx.bldnode.abspath()
        sourceDir = ctx.srcnode.abspath()

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

        ponyg = os.path.join(sourceDir, 'pony.g')
        ponygNew = os.path.join(buildDir, 'pony.g.new')
        with open(ponyg, 'r') as pg:
            with open(ponygNew, 'r') as pgn:
                if pg.read() != pgn.read():
                    print('Grammar files differ')
                    sys.exit(1)

    ctx.add_post_fun(run_tests)


# subclass the build context for the test command,
# otherwise the context wouldn't be set up
from waflib.Build import BuildContext
class testContext(BuildContext):
    cmd = 'test'
    fun = 'test'
