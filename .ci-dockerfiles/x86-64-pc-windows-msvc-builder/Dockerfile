#escape=`

FROM cirrusci/windowsservercore:2019

SHELL ["powershell", "-Command", "$ErrorActionPreference = 'Stop'; $ProgressPreference = 'SilentlyContinue';"]

ADD https://download.visualstudio.microsoft.com/download/pr/9b3476ff-6d0a-4ff8-956d-270147f21cd4/ccfb9355f4f753315455542f966025f96de734292d3908c8c3717e9685b709f0/vs_BuildTools.exe C:\vs_BuildTools.exe

RUN C:\vs_BuildTools.exe --quiet --wait --norestart --nocache --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended
RUN choco install cmake -y --no-progress --installargs 'ADD_CMAKE_TO_PATH=System'
RUN choco install python3 -y --no-progress
RUN C:\Python39\python.exe -m pip install --upgrade cloudsmith-cli
