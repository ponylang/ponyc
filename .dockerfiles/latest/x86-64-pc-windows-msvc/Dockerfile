#escape=`

FROM mcr.microsoft.com/windows/servercore:ltsc2019

SHELL ["powershell", "-Command", "$ErrorActionPreference = 'Stop'; $ProgressPreference = 'SilentlyContinue';"]

COPY .\git.ini C:\

ADD https://github.com/git-for-windows/git/releases/download/v2.29.2.windows.3/Git-2.29.2.3-64-bit.exe C:\Git-2.29.2.3-64-bit.exe
ADD https://download.visualstudio.microsoft.com/download/pr/9b3476ff-6d0a-4ff8-956d-270147f21cd4/ccfb9355f4f753315455542f966025f96de734292d3908c8c3717e9685b709f0/vs_BuildTools.exe C:\vs_BuildTools.exe
ADD https://dl.cloudsmith.io/public/ponylang/nightlies/raw/versions/latest/ponyc-x86-64-pc-windows-msvc.zip C:\ponyc.zip

RUN C:\Git-2.29.2.3-64-bit.exe /VERYSILENT /NORESTART /NOCANCEL /SP- /CLOSEAPPLICATIONS /RESTARTAPPLICATIONS /LOADINF="C:\git.ini"
RUN C:\vs_BuildTools.exe --quiet --wait --norestart --nocache --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended
RUN Expand-Archive -Path C:\ponyc.zip -DestinationPath C:\ponyc
RUN setx /M PATH $($env:PATH + ';C:\ponyc\bin')

CMD ["C:\ponyc\bin\ponyc.exe"]
