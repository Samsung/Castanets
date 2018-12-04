#! /usr/bin/make -f
# -*- makefile -*-
# ex: set tabstop=4 noexpandtab:

default: help all
	sync

project=castanets
url?=https://github.com/Samsung/castanets

dir=out/Default
exe_basename=chrome
exe=${dir}/${exe_basename}
depot_tools_dir?=/usr/local/opt/depot_tools
PATH:=${PATH}:${depot_tools_dir}
gclient_file?=${CURDIR}/../.gclient
gn?=gn
maintainer?=p.coval@samsung.com
pkglicense?=BSD-3-clause and others
pkgversion?=$(shell git describe --tag | tr '-' '+' || echo 0.0.0)
pkgrelease?=0~${USER}0
pkgsummary?=Web engine distributed among multiple devices
debian_requires?= \
 libasound2, \
 libgconf-2-4, \
 libgtk-3-0, \
 libnss3, \
 libxss1, \
 libgl1 \
 # EOL

%: help
	@echo "# log: $@"
	sync

help:
	@echo "# log: Check 'README.md' for more details"
	@echo "# PATH=${PATH}"
	gn --version || echo "# TODO: install gn"

all: ${exe}
	ls $<

${gclient_file}: build/create_gclient.sh
	${CURDIR}/$<
	ls -l $< $@ || gclient sync --with_branch_head
	ls -l $< $@

${dir}/build.ninja: ${gclient_file}
	${gn} gen ${@D}

${dir}/args.gn: ${dir}/build.ninja
	echo 'enable_castanets=true' | tee $@
	echo 'enable_nacl=false' | tee -a $@

${exe}: ${dir}/args.gn
	ninja -C ${@D} ${@F}

install: ${exe}
	install -d ${DESTDIR}/usr/lib/${project}
	install $< ${DESTDIR}/usr/lib/${project}
	ldd ${exe} | grep -o "${CURDIR}/[^ ]*" | \
 while read file; do \
  install $${file} ${DESTDIR}/usr/lib/${project}; \
 done
	install ${<D}/*.bin ${DESTDIR}/usr/lib/${project}
	install ${<D}/*.dat ${DESTDIR}/usr/lib/${project}
	install ${<D}/*.pak ${DESTDIR}/usr/lib/${project}
	cp -rfa ${<D}/locales ${DESTDIR}/usr/lib/${project}

checkinstall/debian: ${exe}
	@echo "${pkgsummary}" > description-pak
	checkinstall --version
	checkinstall \
 --backup="no" \
 --conflicts="${project}" \
 --default \
 --install=no \
 --maintainer="${maintainer}" \
 --nodoc \
 --pkglicense="${pkglicense}" \
 --pkgname="${project}-snapshot" \
 --pkgrelease="${pkgrelease}" \
 --pkgsource="${url}" \
 --pkgversion="${pkgversion}" \
 --requires="${debian_requires}" \
 --type ${@F} \
 # EOL
