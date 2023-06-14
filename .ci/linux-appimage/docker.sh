#!/bin/bash

#Building Citra
mkdir build
cd build
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=/usr/lib/ccache/clang -DCMAKE_CXX_COMPILER=/usr/lib/ccache/clang++ -DCMAKE_C_FLAGS="-fuse-ld=lld" -DCMAKE_CXX_FLAGS="-fuse-ld=lld" -DENABLE_QT_TRANSLATION=ON -DCITRA_ENABLE_COMPATIBILITY_REPORTING=${ENABLE_COMPATIBILITY_REPORTING:-"OFF"} -DENABLE_COMPATIBILITY_LIST_DOWNLOAD=ON -DUSE_DISCORD_PRESENCE=ON -DENABLE_FFMPEG_VIDEO_DUMPER=ON -DENABLE_LTO=ON
ninja

ctest -VV -C Release

#Circumvent missing LibFuse in Docker, by extracting the AppImage
export APPIMAGE_EXTRACT_AND_RUN=1

#Building AppDir
DESTDIR="./AppDir" ninja install
mv ./AppDir/usr/local/bin ./AppDir/usr
mv ./AppDir/usr/local/share ./AppDir/usr
rm -rf ./AppDir/usr/local
QMAKE=/usr/lib/qt6/bin/qmake DEPLOY_PLATFORM_THEMES=1 /linuxdeploy-x86_64.AppImage --appdir AppDir --plugin qt --plugin checkrt
sed -i 's/*XFCE*/*X-Cinnamon*|*XFCE*/g' ./AppDir/apprun-hooks/linuxdeploy-plugin-qt-hook.sh
sed -i '/export QT_QPA_PLATFORMTHEME=gtk3/a \ \ \ \ \ \ \ \ export GDK_BACKEND=x11' ./AppDir/apprun-hooks/linuxdeploy-plugin-qt-hook.sh

#Build AppImage
QMAKE=/usr/lib/qt6/bin/qmake /linuxdeploy-x86_64.AppImage --appdir AppDir --output appimage
