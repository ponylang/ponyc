#escape=`

FROM mcr.microsoft.com/windows/servercore:1909

SHELL ["powershell", "-Command", "$ErrorActionPreference = 'Stop'; $ProgressPreference = 'SilentlyContinue';"]

ADD https://download.visualstudio.microsoft.com/download/pr/220fe621-dd35-4fc0-a32e-10ff6f4551cd/4383a2e9eac72248bd56a25aed63051efb37393a7af3db4726868699c7cd99e4/vs_BuildTools.exe C:\vs_BuildTools.exe
ADD https://dl.cloudsmith.io/public/ponylang/nightlies/raw/versions/latest/ponyc-x86-64-pc-windows-msvc.zip C:\ponyc.zip

RUN Expand-Archive -Path C:\ponyc.zip -DestinationPath C:\ponyc; `
    & C:\vs_BuildTools.exe --quiet --wait --norestart --nocache --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --installPath C:\BuildTools; `    
    setx /M PATH $($env:PATH + ';C:\ponyc\bin')

CMD C:\ponyc\bin\ponyc.exe
