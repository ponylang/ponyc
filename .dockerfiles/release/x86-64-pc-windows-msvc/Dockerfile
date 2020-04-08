#escape=`

FROM mcr.microsoft.com/windows/servercore:ltsc2019

SHELL ["powershell", "-Command", "$ErrorActionPreference = 'Stop'; $ProgressPreference = 'SilentlyContinue';"]

COPY .\git.ini C:\

ADD https://github.com/git-for-windows/git/releases/download/v2.26.0.windows.1/Git-2.26.0-64-bit.exe C:\Git-2.26.0-64-bit.exe
ADD https://download.visualstudio.microsoft.com/download/pr/220fe621-dd35-4fc0-a32e-10ff6f4551cd/4383a2e9eac72248bd56a25aed63051efb37393a7af3db4726868699c7cd99e4/vs_BuildTools.exe C:\vs_BuildTools.exe
ADD https://dl.cloudsmith.io/public/ponylang/releases/raw/versions/latest/ponyc-x86-64-pc-windows-msvc.zip C:\ponyc.zip

RUN C:\Git-2.26.0-64-bit.exe /VERYSILENT /NORESTART /NOCANCEL /SP- /CLOSEAPPLICATIONS /RESTARTAPPLICATIONS /LOADINF="C:\git.ini"
RUN C:\vs_BuildTools.exe --quiet --wait --norestart --nocache --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended
RUN Expand-Archive -Path C:\ponyc.zip -DestinationPath C:\ponyc
RUN setx /M PATH $($env:PATH + ';C:\ponyc\bin')

CMD C:\ponyc\bin\ponyc.exe
