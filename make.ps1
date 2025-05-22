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

    [Parameter(HelpMessage="Whether or not to run tests in LLDB debugger")]
    [string]
    $Uselldb = "no",

    [Parameter(HelpMessage="Tests to run")]
    [string]
    $TestsToRun = 'libponyrt.tests,libponyc.tests,libponyc.run.tests.debug,libponyc.run.tests.release,stdlib-debug,stdlib-release,grammar'
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
$outDir = Join-Path -Path $srcDir -ChildPath "build\$config_lower"

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

if (($Command.ToLower() -ne "libs") -and ($Command.ToLower() -ne "distclean") -and !(Test-Path -Path $libsDir))
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
            & cmake.exe -B "$libsBuildDir" -S "$libsSrcDir" -G "$Generator" -A $Arch -Thost="$Thost" -DCMAKE_INSTALL_PREFIX="$libsDir" -DCMAKE_BUILD_TYPE=Release -DLLVM_TARGETS_TO_BUILD="X86;ARM;AArch64;WebAssembly;RISCV"
            $err = $LastExitCode
        }
        else
        {
            & cmake.exe -B "$libsBuildDir" -S "$libsSrcDir" -G "$Generator" -Thost="$Thost" -DCMAKE_INSTALL_PREFIX="$libsDir" -DCMAKE_BUILD_TYPE=Release -DLLVM_TARGETS_TO_BUILD="X86;ARM;AArch64;WebAssembly;RISCV"
            $err = $LastExitCode
        }
        if ($err -ne 0) { throw "Error: exit code $err" }

        # Write-Output "Building libraries..."
        Write-Output "cmake.exe --build `"$libsBuildDir`" --target install --config Release"
        & cmake.exe --build "$libsBuildDir" --target install --config Release
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

        if ($Arch.Length -gt 0)
        {
            Write-Output "cmake.exe -B `"$buildDir`" -S `"$srcDir`" -G `"$Generator`" -A $Arch -Thost="$Thost" -DCMAKE_INSTALL_PREFIX=`"$Prefix`" -DCMAKE_BUILD_TYPE=`"$Config`" -DPONYC_VERSION=`"$Version`""
            & cmake.exe -B "$buildDir" -S "$srcDir" -G "$Generator" -A $Arch -Thost="$Thost" -DCMAKE_INSTALL_PREFIX="$Prefix" -DCMAKE_BUILD_TYPE="$Config" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DPONYC_VERSION="$Version" $lto_flag --no-warn-unused-cli
        }
        else
        {
            Write-Output "cmake.exe -B `"$buildDir`" -S `"$srcDir`" -G `"$Generator`" -Thost="$Thost" -DCMAKE_INSTALL_PREFIX=`"$Prefix`" -DCMAKE_BUILD_TYPE=`"$Config`" -DPONYC_VERSION=`"$Version`""
            & cmake.exe -B "$buildDir" -S "$srcDir" -G "$Generator" -Thost="$Thost" -DCMAKE_INSTALL_PREFIX="$Prefix" -DCMAKE_BUILD_TYPE="$Config" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DPONYC_VERSION="$Version" $lto_flag --no-warn-unused-cli
        }
        $err = $LastExitCode
        if ($err -ne 0) { throw "Error: exit code $err" }
        break
    }
    "build"
    {
        Write-Output "cmake.exe --build `"$buildDir`" --config $Config --target ALL_BUILD"
        & cmake.exe --build "$buildDir" --config $Config --target ALL_BUILD
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
        # Find all .pony files in examples directory, get their unique directories, and build each one
        $examples = Get-ChildItem -Path "$srcDir\examples\*\*" -Filter "*.pony" -Recurse |
                Select-Object -ExpandProperty Directory -Unique |
                Where-Object { $_.FullName -notlike "*ffi-*" } |
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
    "stress-test-ubench-release"
    {
        $lldbcmd = 'C:\msys64\mingw64\bin\lldb.exe'
        $lldbargs = @('--batch', '--one-line', 'run', '--one-line-on-crash', '"frame variable"', '--one-line-on-crash', '"register read"', '--one-line-on-crash', '"bt all"', '--one-line-on-crash', '"quit 1"', '--')

        & $outDir\ponyc.exe --bin-name=ubench --output=$outDir test\rt-stress\string-message-ubench
        $lldboutput = & $lldbcmd $lldbargs $outDir\ubench.exe --pingers 320 --initial-pings 5 --report-count 40 --report-interval 300 --ponynoscale --ponynoblock
        Write-Output $lldboutput
        $err = Get-ProcessExitCodeFromLLDB -LLDBOutput $lldboutput
        exit $err
        break
    }
    "stress-test-ubench-with-cd-release"
    {
        $lldbcmd = 'C:\msys64\mingw64\bin\lldb.exe'
        $lldbargs = @('--batch', '--one-line', 'run', '--one-line-on-crash', '"frame variable"', '--one-line-on-crash', '"register read"', '--one-line-on-crash', '"bt all"', '--one-line-on-crash', '"quit 1"', '--')

        & $outDir\ponyc.exe --bin-name=ubench --output=$outDir test\rt-stress\string-message-ubench
        $lldboutput = & $lldbcmd $lldbargs $outDir\ubench.exe --pingers 320 --initial-pings 5 --report-count 40 --report-interval 300 --ponynoscale
        Write-Output $lldboutput
        $err = Get-ProcessExitCodeFromLLDB -LLDBOutput $lldboutput
        exit $err
        break
    }
    "stress-test-ubench-debug"
    {
        $lldbcmd = 'C:\msys64\mingw64\bin\lldb.exe'
        $lldbargs = @('--batch', '--one-line', 'run', '--one-line-on-crash', '"frame variable"', '--one-line-on-crash', '"register read"', '--one-line-on-crash', '"bt all"', '--one-line-on-crash', '"quit 1"', '--')

        & $outDir\ponyc.exe --debug --bin-name=ubench --output=$outDir test\rt-stress\string-message-ubench
        $lldboutput = & $lldbcmd $lldbargs $outDir\ubench.exe --pingers 320 --initial-pings 5 --report-count 40 --report-interval 300 --ponynoscale --ponynoblock
        Write-Output $lldboutput
        $err = Get-ProcessExitCodeFromLLDB -LLDBOutput $lldboutput
        exit $err
        break
    }
    "stress-test-ubench-with-cd-debug"
    {
        $lldbcmd = 'C:\msys64\mingw64\bin\lldb.exe'
        $lldbargs = @('--batch', '--one-line', 'run', '--one-line-on-crash', '"frame variable"', '--one-line-on-crash', '"register read"', '--one-line-on-crash', '"bt all"', '--one-line-on-crash', '"quit 1"', '--')

        & $outDir\ponyc.exe --debug --bin-name=ubench --output=$outDir test\rt-stress\string-message-ubench
        $lldboutput = & $lldbcmd $lldbargs $outDir\ubench.exe --pingers 320 --initial-pings 5 --report-count 40 --report-interval 300 --ponynoscale
        Write-Output $lldboutput
        $err = Get-ProcessExitCodeFromLLDB -LLDBOutput $lldboutput
        exit $err
        break
    }
    "stress-test-tcp-open-close-release"
    {
        $lldbcmd = 'C:\msys64\mingw64\bin\lldb.exe'
        $lldbargs = @('--batch', '--one-line', 'run', '--one-line-on-crash', '"frame variable"', '--one-line-on-crash', '"register read"', '--one-line-on-crash', '"bt all"', '--one-line-on-crash', '"quit 1"', '--')

        & $outDir\ponyc.exe --bin-name=open-close --output=$outDir test\rt-stress\tcp-open-close
        $lldboutput = & $lldbcmd $lldbargs $outDir\open-close.exe --ponynoblock 1000
        Write-Output $lldboutput
        $err = Get-ProcessExitCodeFromLLDB -LLDBOutput $lldboutput
        exit $err
        break
    }
    "stress-test-tcp-open-close-with-cd-release"
    {
        $lldbcmd = 'C:\msys64\mingw64\bin\lldb.exe'
        $lldbargs = @('--batch', '--one-line', 'run', '--one-line-on-crash', '"frame variable"', '--one-line-on-crash', '"register read"', '--one-line-on-crash', '"bt all"', '--one-line-on-crash', '"quit 1"', '--')

        & $outDir\ponyc.exe --bin-name=open-close --output=$outDir test\rt-stress\tcp-open-close
        $lldboutput = & $lldbcmd $lldbargs $outDir\open-close.exe 1000
        Write-Output $lldboutput
        $err = Get-ProcessExitCodeFromLLDB -LLDBOutput $lldboutput
        exit $err
        break
    }
    "stress-test-tcp-open-close-debug"
    {
        $lldbcmd = 'C:\msys64\mingw64\bin\lldb.exe'
        $lldbargs = @('--batch', '--one-line', 'run', '--one-line-on-crash', '"frame variable"', '--one-line-on-crash', '"register read"', '--one-line-on-crash', '"bt all"', '--one-line-on-crash', '"quit 1"', '--')

        & $outDir\ponyc.exe --debug --bin-name=open-close --output=$outDir test\rt-stress\tcp-open-close
        $lldboutput = & $lldbcmd $lldbargs $outDir\open-close.exe --ponynoblock 1000
        Write-Output $lldboutput
        $err = Get-ProcessExitCodeFromLLDB -LLDBOutput $lldboutput
        exit $err
        break
    }
    "stress-test-tcp-open-close-with-cd-debug"
    {
        $lldbcmd = 'C:\msys64\mingw64\bin\lldb.exe'
        $lldbargs = @('--batch', '--one-line', 'run', '--one-line-on-crash', '"frame variable"', '--one-line-on-crash', '"register read"', '--one-line-on-crash', '"bt all"', '--one-line-on-crash', '"quit 1"', '--')

        & $outDir\ponyc.exe --debug --bin-name=open-close --output=$outDir test\rt-stress\tcp-open-close
        $lldboutput = & $lldbcmd $lldbargs $outDir\open-close.exe 1000
        Write-Output $lldboutput
        $err = Get-ProcessExitCodeFromLLDB -LLDBOutput $lldboutput
        exit $err
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
        Get-ChildItem -File -Path "$Prefix\bin\*" -Exclude ponyc.exe | Remove-Item
        Compress-Archive -Path "$Prefix\bin", "$Prefix\lib", "$Prefix\packages", "$Prefix\examples" -DestinationPath "$buildDir\..\$package" -Force
        break
    }
    "package-arm64"
    {
        $package = "ponyc-arm64-pc-windows-msvc.zip"
        Write-Output "Creating $buildDir\..\$package"

        # Remove unneeded files; we do it this way because Compress-Archive cannot add a single file to anything other than the root directory
        Get-ChildItem -File -Path "$Prefix\bin\*" -Exclude ponyc.exe | Remove-Item
        Compress-Archive -Path "$Prefix\bin", "$Prefix\lib", "$Prefix\packages", "$Prefix\examples" -DestinationPath "$buildDir\..\$package" -Force
        break
    }
    default
    {
        throw "Unknown command '$Command'. use: {libs, cleanlibs, configure, build, clean, test, install, package, distclean}"
    }
}
