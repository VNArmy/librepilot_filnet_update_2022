#!/bin/bash -e
#
# gcs-bundle-osg-linux.sh - bundle OpenSceneGraph and osgEarth into a Linux
# GCS build and pack bin, lib and share into a standalone tar.gz.
# Copyright (c) 2026, The LibrePilot Project, http://www.librepilot.org
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
# for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
#

# The following environment variables must be set
: ${OSG_SDK_DIR?} ${OSGEARTH_SDK_DIR?} ${OSG_VERSION?} ${GCS_DIR?} ${GCS_SMALL_NAME?} ${DL_DIR?}

PLUGINS_DIR=osgPlugins-${OSG_VERSION}
DEST=${GCS_DIR}/lib/${GCS_SMALL_NAME}
RESOURCES_DIR=${GCS_DIR}/share/${GCS_SMALL_NAME}/osgearth-resources
TARBALL=${GCS_DIR}/${GCS_SMALL_NAME}-with-osgearth-linux-$(uname -m).tar.gz
PNG_SRC_DIR=${DL_DIR}/osg-3.6.5-png-src
PNG_SRC=${PNG_SRC_DIR}/src/osgPlugins/png/ReaderWriterPNG.cpp

for dir in "${OSG_SDK_DIR}/lib64/${PLUGINS_DIR}" "${OSGEARTH_SDK_DIR}/lib64/${PLUGINS_DIR}" \
           "${OSGEARTH_SDK_DIR}/resources" "${DEST}"; do
    if [ ! -d "${dir}" ]; then
        echo "Missing directory: ${dir}" >&2
        exit 1
    fi
done

# Core OSG and osgEarth libraries, next to libGCSOsgEarth
cp -a "${OSG_SDK_DIR}"/lib64/*.so* "${OSGEARTH_SDK_DIR}"/lib64/*.so* "${DEST}/"
for f in "${DEST}"/libosg*.so* "${DEST}"/libOpenThreads.so*; do
    if [ -f "${f}" ] && [ ! -L "${f}" ]; then
        patchelf --set-rpath '$ORIGIN' "${f}"
    fi
done

# osgEarth plugins: a source build installs them in lib64/osgPlugins-<version>,
# they go next to the core libraries so that '$ORIGIN' finds libosgEarth
for f in "${OSGEARTH_SDK_DIR}/lib64/${PLUGINS_DIR}"/*.so; do
    cp "${f}" "${DEST}/"
    patchelf --set-rpath '$ORIGIN' "${DEST}/$(basename "${f}")"
done

# OSG reader/writer plugins (.3ds, .png, .osg, ...)
mkdir -p "${DEST}/${PLUGINS_DIR}"
for f in "${OSG_SDK_DIR}/lib64/${PLUGINS_DIR}"/*.so; do
    cp "${f}" "${DEST}/${PLUGINS_DIR}/"
    patchelf --set-rpath '$ORIGIN/..' "${DEST}/${PLUGINS_DIR}/$(basename "${f}")"
done

# The prebuilt osgdb_png.so links libpng12, which current distributions no
# longer ship, so rebuild it against the system libpng from OSG 3.6.5 sources
if [ ! -f "${PNG_SRC}" ]; then
    rm -rf "${PNG_SRC_DIR}"
    git clone --branch OpenSceneGraph-3.6.5 --depth 1 --filter=blob:none --sparse \
        https://github.com/openscenegraph/osg.git "${PNG_SRC_DIR}"
    git -C "${PNG_SRC_DIR}" sparse-checkout set src/osgPlugins/png
fi
g++ -std=c++11 -fPIC -shared -O2 -I"${OSG_SDK_DIR}/include" "${PNG_SRC}" \
    -o "${DEST}/${PLUGINS_DIR}/osgdb_png.so" \
    -L"${OSG_SDK_DIR}/lib64" -Wl,-rpath-link,"${OSG_SDK_DIR}/lib64" \
    -losg -losgDB -lOpenThreads -lpng -lz -Wl,-rpath,'$ORIGIN/..'

# osgEarth shaders and resources
mkdir -p "${RESOURCES_DIR}"
cp -r "${OSGEARTH_SDK_DIR}"/resources/* "${RESOURCES_DIR}/"

# The archive must carry bin, lib and share together
rm -f "${TARBALL}"
tar -C "${GCS_DIR}" -czf "${TARBALL}" bin lib share
echo "Created ${TARBALL}"
