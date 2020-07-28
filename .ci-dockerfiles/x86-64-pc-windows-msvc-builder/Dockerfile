#escape=`

FROM cirrusci/windowsservercore:2019

SHELL ["powershell", "-Command", "$ErrorActionPreference = 'Stop'; $ProgressPreference = 'SilentlyContinue';"]

ADD https://download.visualstudio.microsoft.com/download/pr/3105fcfe-e771-41d6-9a1c-fc971e7d03a7/67584dbce4956291c76a5deabbadc15c9ad6a50d6a5f4458765b6bd3a924aead/vs_BuildTools.exe C:\vs_BuildTools.exe

RUN C:\vs_BuildTools.exe --quiet --wait --norestart --nocache --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended
RUN choco install cmake -y --no-progress --installargs 'ADD_CMAKE_TO_PATH=System'
RUN choco install python3 -y --no-progress
RUN C:\Python39\python.exe -m pip install --upgrade cloudsmith-cli
