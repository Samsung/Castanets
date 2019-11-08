#!/bin/echo docker build . -f
# -*- docker-image-name: "castanets" -*-
# -*- coding: utf-8 -*-
# Copyright: 2018-present Samsung Electronics France SAS, and other contributors

FROM ubuntu:16.04 as builder
MAINTAINER Philippe Coval (p.coval@samsung.com)

ENV DEBIAN_FRONTEND noninteractive
ENV LC_ALL en_US.UTF-8
ENV LANG ${LC_ALL}

RUN echo "# log: Configuring locales" \
  && set -x \
  && apt-get update -y \
  && apt-get install -y locales \
  && echo "${LC_ALL} UTF-8" | tee /etc/locale.gen \
  && locale-gen ${LC_ALL} \
  && dpkg-reconfigure locales \
  && sync

RUN echo "# log: Setup system" \
  && set -x \
  && apt-get update -y \
  && apt-get install -y \
     apt-transport-https \
     git \
     lsb-release \
     sudo \
     ttf-mscorefonts-installer \
     checkinstall \
  && apt-get clean \
  && sync

WORKDIR /usr/local/opt/depot_tools
RUN echo "# log: ${project}: Preparing sources" \
  && set -x \
  && git clone --recursive --depth 1 \
     https://chromium.googlesource.com/chromium/tools/depot_tools . \
  && sync

ENV project castanets
ADD . /usr/local/opt/${project}/src/${project}/src

WORKDIR /usr/local/opt/${project}/src/${project}/src
RUN echo "# log: ${project}: Preparing sources" \
  && set -x \
  && export PATH="${PATH}:/usr/local/opt/depot_tools" \
  && yes | build/install-build-deps.sh \
  && sync

WORKDIR /usr/local/opt/${project}/src/${project}/src
RUN echo "# log: ${project}: Preparing sources" \
  && set -x \
  && export PATH="${PATH}:/usr/local/opt/depot_tools" \
  && build/create_gclient.sh \
  && sync

WORKDIR /usr/local/opt/${project}/src/${project}/src
RUN echo "# log: ${project}: Preparing sources" \
  && set -x \
  && export PATH="${PATH}:/usr/local/opt/depot_tools" \
  && gclient sync --with_branch_head \
  && sync

WORKDIR /usr/local/opt/${project}/src/${project}/src
RUN echo "# log: ${project}: Building sources" \
  && set -x \
  && export PATH="${PATH}:/usr/local/opt/depot_tools" \
  && make all \
  && ./out/Default/chrome -version \
  && sync

WORKDIR /usr/local/opt/${project}/src/${project}/src
RUN echo "# log: ${project}: Packaging" \
  && set -x \
  && export PATH="${PATH}:/usr/local/opt/depot_tools" \
  && make checkinstall/debian \
  && install -d /usr/local/opt/${project}/dist \
  && install *.deb /usr/local/opt/${project}/dist \
  && sync

ENTRYPOINT [ "/usr/lib/castanets/chrome" ]
CMD [ "--version" ]

FROM ubuntu:16.04
ENV project castanets
MAINTAINER Philippe Coval (p.coval@samsung.com)
COPY --from=builder /usr/local/opt/${project}/dist /usr/local/opt/${project}/dist

ENV DEBIAN_FRONTEND noninteractive
ENV LC_ALL en_US.UTF-8
ENV LANG ${LC_ALL}

WORKDIR /usr/local/opt/${project}/dist
RUN echo "# log: ${project}: Installing" \
  && set -x \
  && find ${PWD} \
  && dpkg -i --force-all *.deb \
  && apt-get update -y \
  && apt-get install -f -y \
  && dpkg -i *.deb \
  && apt-get clean \
  && cd ../.. && rm -rf ./${project} \
  && sync

ENTRYPOINT [ "/usr/lib/castanets/chrome" ]
CMD [ "--version" ]
