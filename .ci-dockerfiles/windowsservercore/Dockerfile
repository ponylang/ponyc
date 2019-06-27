FROM cirrusci/windowsservercore:2019

RUN choco install visualstudio2019buildtools -y --no-progress --package-parameters "--add Microsoft.VisualStudio.Workload.VCTools;includeRecommended --locale en-US" \
  && choco install python3 -y --no-progress \
  && C:\Python37\python.exe -m pip install --upgrade cloudsmith-cli \
  && choco install cmake -y --no-progress --installargs 'ADD_CMAKE_TO_PATH=System'
