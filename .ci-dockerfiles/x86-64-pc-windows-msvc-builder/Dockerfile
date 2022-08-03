#escape=`

FROM cirrusci/windowsservercore:2019

SHELL ["powershell", "-Command", "$ErrorActionPreference = 'Stop'; $ProgressPreference = 'SilentlyContinue';"]

ADD https://download.visualstudio.microsoft.com/download/pr/91cf5cbb-c34a-4766-bff6-aea28265d815/2b26805b6fcf60a3c0fab3700393868a9001d8efa9430fb5e772fbb195514382/vs_BuildTools.exe C:\vs_BuildTools.exe
ADD https://github.com/msys2/msys2-installer/releases/download/nightly-x86_64/msys2-base-x86_64-latest.sfx.exe C:\msys2.exe

RUN C:\vs_BuildTools.exe --quiet --wait --norestart --nocache --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended
RUN choco install cmake -y --no-progress --installargs 'ADD_CMAKE_TO_PATH=System'
RUN choco install python3 -y --no-progress
RUN C:\Python39\python.exe -m pip install --upgrade cloudsmith-cli
RUN C:\msys2.exe -y -oC:\ \
  Remove-Item msys2.exe \
  function msys() { C:\msys64\usr\bin\bash.exe @('-lc') + @Args } \
  msys ' ' \
  msys 'pacman --noconfirm -Syuu' \
  msys 'pacman --noconfirm -Syuu' \
  msys 'pacman --noconfirm -S --needed base-devel mingw-w64-x86_64-lldb' \
  msys 'pacman --noconfirm -Scc'
