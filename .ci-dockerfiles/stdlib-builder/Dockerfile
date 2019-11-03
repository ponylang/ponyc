FROM ponylang/ponyc:latest

RUN apt-get update \
 && apt-get install -y \
  python3 \
  python3-pip \
 && pip3 install mkdocs

RUN pip3 install --user mkdocs-ponylang
