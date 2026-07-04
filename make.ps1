Param(
    [Parameter(Position=0, HelpMessage="Enter the action to take, e.g. libs, cleanlibs, configure, build, clean, distclean, test, install.")]
    [string]
    $Command = 'build',

    [Parameter(HelpMessage="The build configuration (Release, Debug, RelWithDebInfo, MinSizeRel).")]
    [string]
    $Config = "Release",

    [Parameter(HelpMessage="The CMake generator, e.g. `"Visual Studio 17 2022`"")]
    [string]
    $Generator = "default",

    [Parameter(HelpMessage="The architecture to use for compiling, e.g. `"X64`", `"Arm64`"")]
    [string]
    $Arch = [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture,

    [Parameter(HelpMessage="The location to install to")]
    [string]
    $Prefix = "default",

    [Parameter(HelpMessage="The version to use when packaging")]
    [string]
    $Version = "default",

    [Parameter(HelpMessage="Whether or not to turn on LTO")]
    [string]
    $Lto = "no",

    [Parameter(HelpMessage="Whether to build the LLVM command-line tools (true/false). They are large and unused by a normal build; set false to omit them.")]
    [ValidateSet("true", "false")]
    [string]
    $LlvmTools = "true",

    [Parameter(HelpMessage="Whether or not to run tests in LLDB debugger")]
    [string]
    $Uselldb = "no",

    [Parameter(HelpMessage="Tests to run")]
    [string]
    $TestsToRun = 'libponyrt.tests,libponyc.tests,libponyc.run.tests.debug,libponyc.run.tests.release,stdlib-debug,stdlib-release,grammar',

    [Parameter(HelpMessage="Runtime use options to enable at configure time, comma-separated. On Windows only 'systematic_testing' is supported.")]
    [string]
    $Use = ""
)

# Function to extract process exit code from LLDB output
function Get-ProcessExitCodeFromLLDB {
    param (
        [string[]]$LLDBOutput
    )

    $processExitMatch = $LLDBOutput | Select-String -Pattern 'Process \d+ exited with status = (\d+)'
    if ($processExitMatch.Matches.Count -gt 0) {
        return [int]$processExitMatch.Matches[0].Groups[1].Value
    } else {
        # If we can't find the pattern, return LLDB's exit code
        return $LastExitCode
    }
}

$srcDir = Split-Path $script:MyInvocation.MyCommand.Path

switch ($Version)
{
    "default" { $Version = (Get-Content $srcDir\VERSION) + "-" + (git rev-parse --short --verify HEAD) }
    "nightly" { $Version = "nightly" + (Get-Date).ToString("yyyyMMdd") }
    "date" { $Version = (Get-Date).ToString("yyyyMMdd") }
}

# Sanitize config to conform to CMake build configs.
switch ($Config.ToLower())
{
    "release" { $Config = "Release"; break; }
    "debug" { $Config = "Debug"; break; }
    "relwithdebinfo" { $Config = "RelWithDebInfo"; break; }
    "minsizerel" { $Config = "MinSizeRel"; break; }
    default { throw "'$Config' is not a valid config; use Release, Debug, RelWithDebInfo, or MinSizeRel)." }
}
$config_lower = $Config.ToLower()

# `-Use` flags only take effect at configure time (they set CMake cache
# variables), so reject them on any other command instead of silently ignoring
# them, matching the Makefile's "You can only specify use= for 'make configure'".
if (($Use.Trim().Length -gt 0) -and ($Command.ToLower() -ne "configure"))
{
    throw "-Use can only be specified for the 'configure' command."
}

if ($null -eq (Get-Command "cmake.exe" -ErrorAction SilentlyContinue)) {
	Write-Output "Warning, unable to find cmake.exe in your PATH, trying to discover one in Visual Studio installation."
	Push-Location
	$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
	if (Test-Path $vswhere -PathType Leaf ) {
    	$cmakePath = $(Get-Item $( Invoke-Expression '& "$vswhere" -prerelease -latest -requires Microsoft.VisualStudio.Component.VC.CMake.Project -find Common7/IDE/**/cmake.exe ' )).Directory.FullName
	    if ($null -ne $cmakePath) {
			$env:Path = "$env:Path;$cmakePath"
			Write-Output "Success, CMake added to current PATH from $cmakePath"
		} else {
			Write-Output "Your latest Visual Studio installation does not include CMake package."
		}
	} else {
		Write-Output "No Visual Studio 2017+ was found in the system."
	}
	Pop-Location
}

if ($Generator -eq "default")
{
    $Generator = cmake --help | Where-Object { $_ -match '\*\s+(.*\S)\s+(\[arch\])?\s+=' } | Foreach-Object { $Matches[1].Trim() } | Select-Object -First 1
}

if ($Generator -match 'Visual Studio')
{
    $buildDir = Join-Path -Path $srcDir -ChildPath "build\build"
}
else
{
    $buildDir = Join-Path -Path $srcDir -ChildPath "build\build_$config_lower"
}

# Some CI services build inside the temp directory, which MSVC doesn't like.
$tempPath = [IO.Path]::GetFullPath($env:TEMP)
$buildPath = [IO.Path]::GetFullPath((Join-Path -Path $srcDir -ChildPath "build"))
if ($buildPath.StartsWith($tempPath, [StringComparison]::OrdinalIgnoreCase))
{
    $newTempPath = Join-Path -Path $srcDir -ChildPath "tempdir"
    if (!(Test-Path $newTempPath -PathType Leaf))
    {
        New-Item $newTempPath -ItemType Directory
    }
    $env:TMP = $newTempPath
    $env:TEMP = $newTempPath
}

$libsDir = Join-Path -Path $srcDir -ChildPath "build\libs"

# Output directory. A `-Use` build appends a suffix to the output directory
# (e.g. build\debug-systematic_testing); see PONY_OUTPUT_SUFFIX in
# CMakeLists.txt. Discover the real, suffixed directory from the configured
# CMake install script -- the same approach the Unix Makefile uses -- so that
# build, test, install, and run all locate a use-flag build's binaries instead
# of looking in the un-suffixed default. Fall back to that default when nothing
# has been configured yet.
$outDir = Join-Path -Path $srcDir -ChildPath "build\$config_lower"
$cmakeInstall = Join-Path -Path $buildDir -ChildPath "cmake_install.cmake"
if (Test-Path $cmakeInstall)
{
    $srcFwd = $srcDir -replace '\\', '/'
    $needle = [regex]::Escape("$srcFwd/build/$config_lower") + '[^/"]*' + '/libponyrt\.tests'
    $found = Select-String -Path $cmakeInstall -Pattern $needle -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($null -ne $found)
    {
        $outDir = ($found.Matches[0].Value -replace '/libponyrt\.tests.*$', '') -replace '/', '\'
    }
}

Write-Output "Source directory: $srcDir"
Write-Output "Build directory:  $buildDir"
Write-Output "Libs directory:   $libsDir"
Write-Output "Output directory: $outDir"
Write-Output "Temp directory:   $env:TEMP"

if ($Prefix -eq "default")
{
    $Prefix = Join-Path -Path $srcDir -ChildPath "build\install"
}
elseif (![System.IO.Path]::IsPathRooted($Prefix))
{
    $Prefix = Join-Path -Path $srcDir -ChildPath $Prefix
}

Write-Output "make.ps1 $Command -Config $Config -Generator `"$Generator`" -Prefix `"$Prefix`" -Version `"$Version`""

$libsOptionalCommands = @("libs", "distclean")
if (($libsOptionalCommands -notcontains $Command.ToLower()) -and !(Test-Path -Path $libsDir))
{
    throw "Libs directory '$libsDir' does not exist; you may need to run 'make.ps1 libs' first."
}

$Thost = "x64"
if ($Arch -ieq "arm64")
{
    # if this is lowercase arm64, then things go boom.
    $Thost = "ARM64"
}

switch ($Command.ToLower())
{
    "dummy" { break }
    "libs"
    {
        if (!(Test-Path -Path $libsDir))
        {
            New-Item -ItemType "directory" -Path $libsDir | Out-Null
        }

        $libsBuildDir = Join-Path -Path $srcDir -ChildPath "build\build_libs"
        if (!(Test-Path -Path $libsBuildDir))
        {
            New-Item -ItemType "directory" -Path $libsBuildDir | Out-Null
        }

        $libsSrcDir = Join-Path -Path $srcDir -ChildPath "lib"
        Write-Output "Configuring libraries..."
        if ($Arch.Length -gt 0)
        {
            & cmake.exe -B "$libsBuildDir" -S "$libsSrcDir" -G "$Generator" -A $Arch -Thost="$Thost" -DCMAKE_INSTALL_PREFIX="$libsDir" -DCMAKE_BUILD_TYPE=Release -DLLVM_TARGETS_TO_BUILD="X86;ARM;AArch64;WebAssembly;RISCV" -DPONY_LLVM_TOOLS="$LlvmTools"
            $err = $LastExitCode
        }
        else
        {
            & cmake.exe -B "$libsBuildDir" -S "$libsSrcDir" -G "$Generator" -Thost="$Thost" -DCMAKE_INSTALL_PREFIX="$libsDir" -DCMAKE_BUILD_TYPE=Release -DLLVM_TARGETS_TO_BUILD="X86;ARM;AArch64;WebAssembly;RISCV" -DPONY_LLVM_TOOLS="$LlvmTools"
            $err = $LastExitCode
        }
        if ($err -ne 0) { throw "Error: exit code $err" }

        # Write-Output "Building libraries..."
        # Capped at 4 (not NUMBER_OF_PROCESSORS like the `build` command) because
        # LLVM translation units are memory-hungry; a higher count risks
        # exhausting a CI runner's RAM.
        Write-Output "cmake.exe --build `"$libsBuildDir`" --target install --config Release --parallel 4"
        & cmake.exe --build "$libsBuildDir" --target install --config Release --parallel 4
        $err = $LastExitCode
        if ($err -ne 0) { throw "Error: exit code $err" }
        break
    }
    "cleanlibs"
    {
        if (Test-Path -Path $libsDir)
        {
            Write-Output "Removing $libsDir..."
            Remove-Item -Path $libsDir -Recurse -Force
        }
        break
    }
    "configure"
    {
        $lto_flag = ""
        if ($Lto -eq "yes")
        {
            $lto_flag = "-DPONY_USE_LTO=true"
        }

        $PonyCpu = switch ($Arch.ToLower()) {
            "x64"   { "x86-64" }
            "arm64" { "generic" }
            default { "" }
        }

        # Translate -Use flags into the -DPONY_USE_* cmake defines that the Unix
        # Makefile sets for `use=...`. Only the use options that are meaningful on
        # Windows are accepted; anything else is rejected rather than silently
        # ignored, mirroring the Makefile's USE_CHECK behavior.
        $useDefines = @()
        foreach ($useItem in ($Use -split ',' | ForEach-Object { $_.Trim() } | Where-Object { $_.Length -gt 0 }))
        {
            switch ($useItem)
            {
                "systematic_testing" { $useDefines += "-DPONY_USE_SYSTEMATIC_TESTING=true" }
                default { throw "Unknown use option '$useItem'. Supported on Windows: systematic_testing." }
            }
        }

        if ($Arch.Length -gt 0)
        {
            Write-Output "cmake.exe -B `"$buildDir`" -S `"$srcDir`" -G `"$Generator`" -A $Arch -Thost="$Thost" -DCMAKE_INSTALL_PREFIX=`"$Prefix`" -DCMAKE_BUILD_TYPE=`"$Config`" -DPONYC_VERSION=`"$Version`" -DPONY_CPU=`"$PonyCpu`" $useDefines"
            & cmake.exe -B "$buildDir" -S "$srcDir" -G "$Generator" -A $Arch -Thost="$Thost" -DCMAKE_INSTALL_PREFIX="$Prefix" -DCMAKE_BUILD_TYPE="$Config" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DPONYC_VERSION="$Version" -DPONY_CPU="$PonyCpu" $lto_flag $useDefines --no-warn-unused-cli
        }
        else
        {
            Write-Output "cmake.exe -B `"$buildDir`" -S `"$srcDir`" -G `"$Generator`" -Thost="$Thost" -DCMAKE_INSTALL_PREFIX=`"$Prefix`" -DCMAKE_BUILD_TYPE=`"$Config`" -DPONYC_VERSION=`"$Version`" -DPONY_CPU=`"$PonyCpu`" $useDefines"
            & cmake.exe -B "$buildDir" -S "$srcDir" -G "$Generator" -Thost="$Thost" -DCMAKE_INSTALL_PREFIX="$Prefix" -DCMAKE_BUILD_TYPE="$Config" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DPONYC_VERSION="$Version" -DPONY_CPU="$PonyCpu" $lto_flag $useDefines --no-warn-unused-cli
        }
        $err = $LastExitCode
        if ($err -ne 0) { throw "Error: exit code $err" }
        break
    }
    "build"
    {
        # `--parallel` drives MSBuild's project-level parallelism (/m); the
        # per-source parallelism within a project comes from the /MP flag set in
        # CMakeLists.txt. Without both, the Windows build compiles serially.
        # The two combined can oversubscribe (up to jobs x cores cl.exe
        # processes); left uncapped because ponyc translation units are small
        # and the memory-heavy LLVM build is a separate, capped step (libs).
        # Cap this if a smaller CI runner shows memory pressure.
        $jobs = $env:NUMBER_OF_PROCESSORS
        if ([string]::IsNullOrEmpty($jobs)) { $jobs = [Environment]::ProcessorCount }
        Write-Output "cmake.exe --build `"$buildDir`" --config $Config --target ALL_BUILD --parallel $jobs"
        & cmake.exe --build "$buildDir" --config $Config --target ALL_BUILD --parallel $jobs
        $err = $LastExitCode
        if ($err -ne 0) { throw "Error: exit code $err" }
        break
    }
    "clean"
    {
        if (Test-Path $buildDir)
        {
            Write-Output "cmake.exe --build `"buildDir`" --config $Config --target clean"
            & cmake.exe --build "$buildDir" --config $Config --target clean
        }

        if (Test-Path $outDir)
        {
            Write-Output "Remove-Item -Path $outDir -Recurse -Force"
            Remove-Item -Path "$outDir" -Recurse -Force
        }
        break
    }
    "distclean"
    {
        if (Test-Path ($srcDir + "\build"))
        {
            Write-Output "Remove-Item -Path `"$srcDir\build`" -Recurse -Force"
            Remove-Item -Path "$srcDir\build" -Recurse -Force
        }
        break
    }
    "test"
    {
        $numTestSuitesRun = 0
        $failedTestSuites = @()

        & $outDir\ponyc.exe --version

        if ($Uselldb -eq "yes")
        {
            $lldbcmd = 'C:\msys64\mingw64\bin\lldb.exe'
            $lldbargs = @('--batch', '--one-line', 'run', '--one-line-on-crash', '"frame variable"', '--one-line-on-crash', '"register read"', '--one-line-on-crash', '"bt all"', '--one-line-on-crash', '"quit 1"', '--')
        }

        # libponyrt.tests
        if ($TestsToRun -match 'libponyrt.tests')
        {
            $numTestSuitesRun += 1;
            try
            {
                if ($Uselldb -eq "yes")
                {
                    Write-Output "$lldbcmd $lldbargs $outDir\libponyrt.tests.exe --gtest_shuffle"
                    $lldboutput = & $lldbcmd $lldbargs $outDir\libponyrt.tests.exe --gtest_shuffle
                    Write-Output $lldboutput
                    $err = Get-ProcessExitCodeFromLLDB -LLDBOutput $lldboutput
                }
                else
                {
                    Write-Output "$outDir\libponyrt.tests.exe --gtest_shuffle"
                    & $outDir\libponyrt.tests.exe --gtest_shuffle
                    $err = $LastExitCode
                }
            }
            catch
            {
                $err = -1
            }
            if ($err -ne 0) { $failedTestSuites += 'libponyrt.tests' }
        }

        # libponyc.tests
        if ($TestsToRun -match 'libponyc.tests')
        {
            $numTestSuitesRun += 1;
            try
            {
                if ($Uselldb -eq "yes")
                {
                    Write-Output "$lldbcmd $lldbargs $outDir\libponyc.tests.exe --gtest_shuffle"
                    $lldboutput = & $lldbcmd $lldbargs $outDir\libponyc.tests.exe --gtest_shuffle
                    Write-Output $lldboutput
                    $err = Get-ProcessExitCodeFromLLDB -LLDBOutput $lldboutput
                }
                else
                {
                    Write-Output "$outDir\libponyc.tests.exe --gtest_shuffle"
                    & $outDir\libponyc.tests.exe --gtest_shuffle
                    $err = $LastExitCode
                }
            }
            catch
            {
                $err = -1
            }
            if ($err -ne 0) { $failedTestSuites += 'libponyc.tests' }
        }

        # libponyc.run.tests
        if ($TestsToRun -match 'libponyc.run.tests')
        {
            foreach ($runConfig in ('debug', 'release'))
            {
                if (-not ($TestsToRun -match "libponyc.run.tests.$runConfig"))
                {
                    continue
                }
                $numTestSuitesRun += 1;

                $runOutDir = "$outDir\full-program-tests\$runConfig"
                $debugFlag = if ($runConfig -eq 'debug') { 'true' } else { 'false' }

                $debuggercmd = ''
                if ($Uselldb -eq "yes")
                {
                    $debuggercmd = "$lldbcmd $lldbargs"
                    $debuggercmd = $debuggercmd -replace ' ', '%20'
                    $debuggercmd = $debuggercmd -replace '"', '%22'
                }

                if (-not (Test-Path $runOutDir)) { New-Item -ItemType Directory -Force -Path $runOutDir }
                Write-Output "$buildDir\test\full-program-runner\full-program-runner.exe --debug=$debugFlag --debugger=$debuggercmd --timeout_s=120 --max_parallel=1 --debug=$debugFlag --test_lib=$outDir\test_lib --compiler=$outDir\ponyc.exe --output=$runOutDir $srcDir\test\full-program-tests"
                & $buildDir\test\full-program-runner\full-program-runner.exe --debugger=$debuggercmd --timeout_s=120 --max_parallel=1 --debug=$debugFlag --test_lib=$outDir\test_lib --compiler=$outDir\ponyc.exe --output=$runOutDir $srcDir\test\full-program-tests
                $err = $LastExitCode
                if ($err -ne 0) { $failedTestSuites += "libponyc.run.tests.$runConfig" }
            }
        }

        # stdlib-debug
        if ($TestsToRun -match 'stdlib-debug')
        {
            $numTestSuitesRun += 1;
            Write-Output "$outDir\ponyc.exe -d --checktree -b stdlib-debug -o $outDir $srcDir\packages\stdlib"
            & $outDir\ponyc.exe -d --checktree -b stdlib-debug -o $outDir $srcDir\packages\stdlib
            if ($LastExitCode -eq 0)
            {
                try
                {
                    if ($Uselldb -eq "yes")
                    {
                        Write-Output "$lldbcmd $lldbargs $outDir\stdlib-debug.exe --sequential"
                        $lldboutput = & $lldbcmd $lldbargs $outDir\stdlib-debug.exe --sequential
                        Write-Output $lldboutput
                        $err = Get-ProcessExitCodeFromLLDB -LLDBOutput $lldboutput
                    }
                    else
                    {
                        Write-Output "$outDir\stdlib-debug.exe --sequential"
                        & $outDir\stdlib-debug.exe --sequential
                        $err = $LastExitCode
                    }
                }
                catch
                {
                    $err = -1
                }
                if ($err -ne 0) { $failedTestSuites += 'stdlib-debug' }
            }
            else
            {
                $failedTestSuites += 'compile stdlib-debug'
            }
        }

        # stdlib-release
        if ($TestsToRun -match 'stdlib-release')
        {
            $numTestSuitesRun += 1;
            Write-Output "$outDir\ponyc.exe --checktree -b stdlib-release -o $outDir $srcDir\packages\stdlib"
            & $outDir\ponyc.exe --checktree -b stdlib-release -o $outDir $srcDir\packages\stdlib
            if ($LastExitCode -eq 0)
            {
                try
                {
                    if ($Uselldb -eq "yes")
                    {
                        Write-Output "$lldbcmd $lldbargs $outDir\stdlib-release.exe --sequential"
                        $lldboutput = & $lldbcmd $lldbargs $outDir\stdlib-release.exe --sequential
                        Write-Output $lldboutput
                        $err = Get-ProcessExitCodeFromLLDB -LLDBOutput $lldboutput
                    }
                    else
                    {
                        Write-Output "$outDir\stdlib-release.exe --sequential"
                        & $outDir\stdlib-release.exe --sequential
                        $err = $LastExitCode
                    }
                }
                catch
                {
                    $err = -1
                }
                if ($err -ne 0) { $failedTestSuites += 'stdlib-release' }
            }
            else
            {
                $failedTestSuites += 'compile stdlib-release'
            }
        }

        # grammar
        if ($TestsToRun -match 'grammar')
        {
            $numTestSuitesRun += 1
            Get-Content -Path "$srcDir\pony.g" -Encoding ASCII | Out-File -Encoding UTF8 "$outDir\pony.g.orig"
            & $outDir\ponyc.exe --antlr | Out-File -Encoding UTF8 "$outDir\pony.g.test"
            if ($LastExitCode -eq 0)
            {
                $origHash = (Get-FileHash -Path "$outDir\pony.g.orig").Hash
                $testHash = (Get-FileHash -Path "$outDir\pony.g.test").Hash

                Write-Output "grammar original hash:  $origHash"
                Write-Output "grammar generated hash: $testHash"

                if ($origHash -ne $testHash)
                {
                    $failedTestSuites += 'generated grammar file differs from baseline'
                }
            }
            else
            {
                $failedTestSuites += 'generate grammar'
            }
        }

        # pony-doc-tests
        if ($TestsToRun -match 'pony-doc-tests')
        {
            $numTestSuitesRun += 1;
            Write-Output "$outDir\ponyc.exe --path $srcDir\tools\lib\ponylang\pony_compiler\ -b pony-doc-tests -o $outDir $srcDir\tools\pony-doc\test"
            & $outDir\ponyc.exe --path $srcDir\tools\lib\ponylang\pony_compiler\ -b pony-doc-tests -o $outDir $srcDir\tools\pony-doc\test
            if ($LastExitCode -eq 0)
            {
                try
                {
                    Write-Output "$outDir\pony-doc-tests.exe --sequential"
                    & $outDir\pony-doc-tests.exe --sequential
                    $err = $LastExitCode
                }
                catch
                {
                    $err = -1
                }
                if ($err -ne 0) { $failedTestSuites += 'pony-doc-tests' }
            }
            else
            {
                $failedTestSuites += 'compile pony-doc-tests'
            }
        }

        # pony-lint-tests
        if ($TestsToRun -match 'pony-lint-tests')
        {
            $numTestSuitesRun += 1;
            Write-Output "$outDir\ponyc.exe --path $srcDir\tools\lib\ponylang\pony_compiler\ -b pony-lint-tests -o $outDir $srcDir\tools\pony-lint\test"
            & $outDir\ponyc.exe --path $srcDir\tools\lib\ponylang\pony_compiler\ -b pony-lint-tests -o $outDir $srcDir\tools\pony-lint\test
            if ($LastExitCode -eq 0)
            {
                $savePonyPath = $env:PONYPATH
                $env:PONYPATH = "$srcDir\packages;$savePonyPath"
                try
                {
                    Write-Output "$outDir\pony-lint-tests.exe --sequential"
                    & $outDir\pony-lint-tests.exe --sequential
                    $err = $LastExitCode
                }
                catch
                {
                    $err = -1
                }
                $env:PONYPATH = $savePonyPath
                if ($err -ne 0) { $failedTestSuites += 'pony-lint-tests' }
            }
            else
            {
                $failedTestSuites += 'compile pony-lint-tests'
            }
        }

        # pony-compiler-tests
        if ($TestsToRun -match 'pony-compiler-tests')
        {
            $numTestSuitesRun += 1;
            Write-Output "$outDir\ponyc.exe --path $srcDir\tools\lib\ponylang\pony_compiler\ -b pony-compiler-tests -o $outDir $srcDir\tools\lib\ponylang\pony_compiler\tests"
            & $outDir\ponyc.exe --path $srcDir\tools\lib\ponylang\pony_compiler\ -b pony-compiler-tests -o $outDir $srcDir\tools\lib\ponylang\pony_compiler\tests
            if ($LastExitCode -eq 0)
            {
                $savePonyPath = $env:PONYPATH
                $env:PONYPATH = "$srcDir\tools\lib\ponylang\pony_compiler;$srcDir\packages;$savePonyPath"
                try
                {
                    Write-Output "$outDir\pony-compiler-tests.exe --sequential"
                    & $outDir\pony-compiler-tests.exe --sequential
                    $err = $LastExitCode
                }
                catch
                {
                    $err = -1
                }
                $env:PONYPATH = $savePonyPath
                if ($err -ne 0) { $failedTestSuites += 'pony-compiler-tests' }
            }
            else
            {
                $failedTestSuites += 'compile pony-compiler-tests'
            }
        }

        # pony-lsp-tests
        if ($TestsToRun -match 'pony-lsp-tests')
        {
            $numTestSuitesRun += 1;
            Write-Output "$outDir\ponyc.exe --path $srcDir\tools\lib\ponylang\pony_compiler\ -b pony-lsp-tests -o $outDir $srcDir\tools"
            & $outDir\ponyc.exe --path $srcDir\tools\lib\ponylang\peg --path $srcDir\tools\lib\ponylang\pony_compiler\ -b pony-lsp-tests -o $outDir $srcDir\tools
            if ($LastExitCode -eq 0)
            {
                try
                {
                    Write-Output "$outDir\pony-lsp-tests.exe --sequential"
                    & $outDir\pony-lsp-tests.exe --sequential
                    $err = $LastExitCode
                }
                catch
                {
                    $err = -1
                }
                if ($err -ne 0) { $failedTestSuites += 'pony-lsp-tests' }
            }
            else
            {
                $failedTestSuites += 'compile pony-lsp-tests'
            }
        }

        #
        $numTestSuitesFailed = $failedTestSuites.Length
        Write-Output "Test suites run: $numTestSuitesRun, num failed: $numTestSuitesFailed"
        if ($numTestSuitesFailed -ne 0)
        {
            $failedTestSuitesList = [string]::Join(', ', $failedTestSuites)
            Write-Output "Test suites failed: ($failedTestSuitesList)"
            exit $numTestSuitesFailed
        }
        break
    }
    "build-examples"
    {
        # Find all example subdirectories that contain .pony files and build each one.
        # ffi-* examples need a separately-built C library, so they're skipped here.
        $examples = Get-ChildItem -Path "$srcDir\examples" -Directory |
                Where-Object { $_.Name -notlike "ffi-*" } |
                Where-Object { (Get-ChildItem -Path $_.FullName -Filter "*.pony" -File).Count -gt 0 } |
                Select-Object -ExpandProperty FullName

        $failed = @()
        foreach ($example in $examples)
        {
            Write-Output "Building example: $example"
            & $outDir\ponyc.exe -d -s --checktree -o "$example" "$example"
            if ($LastExitCode -ne 0)
            {
                $failed += $example
                Write-Output "Failed to build example: $example"
            }
        }

        if ($failed.Count -gt 0)
        {
            Write-Output "Failed to build the following examples:"
            $failed | ForEach-Object { Write-Output "  $_" }
            throw "Some examples failed to build"
        }
        break
    }
    "install"
    {
        Write-Output "cmake.exe --build `"$buildDir`" --config $Config --target install"
        & cmake.exe --build "$buildDir" --config $Config --target install
        $err = $LastExitCode
        if ($err -ne 0) { throw "Error: exit code $err" }
        break
    }
    "package-x86-64"
    {
        $package = "ponyc-x86-64-pc-windows-msvc.zip"
        Write-Output "Creating $buildDir\..\$package"

        # Remove unneeded files; we do it this way because Compress-Archive cannot add a single file to anything other than the root directory
        Get-ChildItem -File -Path "$Prefix\bin\*" | Where-Object { $_.Name -notin 'ponyc.exe','pony-doc.exe','pony-lint.exe','pony-lsp.exe' } | Remove-Item
        Compress-Archive -Path "$Prefix\bin", "$Prefix\lib", "$Prefix\packages", "$Prefix\examples" -DestinationPath "$buildDir\..\$package" -Force
        break
    }
    "package-arm64"
    {
        $package = "ponyc-arm64-pc-windows-msvc.zip"
        Write-Output "Creating $buildDir\..\$package"

        # Remove unneeded files; we do it this way because Compress-Archive cannot add a single file to anything other than the root directory
        Get-ChildItem -File -Path "$Prefix\bin\*" | Where-Object { $_.Name -notin 'ponyc.exe','pony-doc.exe','pony-lint.exe','pony-lsp.exe' } | Remove-Item
        Compress-Archive -Path "$Prefix\bin", "$Prefix\lib", "$Prefix\packages", "$Prefix\examples" -DestinationPath "$buildDir\..\$package" -Force
        break
    }
    default
    {
        throw "Unknown command '$Command'. use: {libs, cleanlibs, configure, build, clean, test, install, package, distclean}"
    }
}
