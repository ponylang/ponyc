#escape=`

FROM cirrusci/windowsservercore:2019

SHELL ["powershell", "-Command", "$ErrorActionPreference = 'Stop'; $ProgressPreference = 'SilentlyContinue';"]

ADD https://download.visualstudio.microsoft.com/download/pr/220fe621-dd35-4fc0-a32e-10ff6f4551cd/4383a2e9eac72248bd56a25aed63051efb37393a7af3db4726868699c7cd99e4/vs_BuildTools.exe C:\vs_BuildTools.exe

RUN C:\vs_BuildTools.exe --quiet --wait --norestart --nocache --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended
RUN choco install cmake -y --no-progress --installargs 'ADD_CMAKE_TO_PATH=System'
RUN choco install python3 -y --no-progress; `
  C:\Python38\python.exe -m pip install --upgrade cloudsmith-cli
