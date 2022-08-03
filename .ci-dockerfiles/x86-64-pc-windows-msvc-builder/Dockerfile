#escape=`

FROM cirrusci/windowsservercore:2019

SHELL ["powershell", "-Command", "$ErrorActionPreference = 'Stop'; $ProgressPreference = 'SilentlyContinue';"]

ADD https://download.visualstudio.microsoft.com/download/pr/91cf5cbb-c34a-4766-bff6-aea28265d815/2b26805b6fcf60a3c0fab3700393868a9001d8efa9430fb5e772fbb195514382/vs_BuildTools.exe C:\vs_BuildTools.exe

RUN C:\vs_BuildTools.exe --quiet --wait --norestart --nocache --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended
RUN choco install cmake -y --no-progress --installargs 'ADD_CMAKE_TO_PATH=System'
RUN choco install python --version=3.10.0 -y --no-progress
RUN C:\Python310\python.exe -m pip install --upgrade cloudsmith-cli
