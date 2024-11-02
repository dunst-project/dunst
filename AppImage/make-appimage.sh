#!/bin/sh

# This script is meant to be ran on alpine linux, dependencies:
# wget build-base patchelf libxscrnsaver-dev libxinerama-dev libxrandr-dev libnotify-dev
# dbus-dev wayland-dev perl pango-dev wayland-protocols dbus librsvg desktop-file-utils

set -e
export ARCH="$(uname -m)"
export APPIMAGE_EXTRACT_AND_RUN=1
APPIMAGETOOL="https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage"
export COMPLETIONS=0

# BUILD DUNST
mkdir -p ./AppDir
make -j$(nproc) PREFIX="$PWD"/AppDir/usr
make install PREFIX="$PWD"/AppDir/usr
cd ./AppDir

# AppRun
echo "Creating AppRun..."
cat >> ./AppRun << 'EOF'
#!/bin/sh
CURRENTDIR="$(dirname "$(readlink -f "$0")")"
export PATH="$CURRENTDIR/usr/bin:$PATH"
[ -z "$APPIMAGE" ] && APPIMAGE="$0"
BIN="${ARGV0#./}"
unset ARGV0
if [ -f "$CURRENTDIR/usr/bin/$BIN" ]; then
	if [ "$BIN" = "dunstctl" ]; then
		exec "$CURRENTDIR/usr/bin/$BIN" "$@"
	else
		exec "$CURRENTDIR/ld-musl-x86_64.so.1" "$CURRENTDIR/usr/bin/$BIN" "$@"
	fi
elif [ "$1" = "--notify" ]; then
	shift
	exec "$CURRENTDIR/ld-musl-x86_64.so.1" "$CURRENTDIR"/usr/bin/dunstify "$@"
elif [ "$1" = "--ctl" ]; then
	shift
	exec "$CURRENTDIR"/usr/bin/dunstctl "$@"
else
	if [ -z "$1" ]; then
		echo "AppImage commands:"
		echo " \"$APPIMAGE\"            runs dunst"
		echo " \"$APPIMAGE --notify\"   runs dunstify"
		echo " \"$APPIMAGE --ctl\"      runs dunstctl"
		echo "You can also make and run symlinks to the AppImage with the names"
		echo "dunstify and dunstctl to launch them automatically without extra args"
		echo "running dunst..."
	fi
	exec "$CURRENTDIR/ld-musl-x86_64.so.1" "$CURRENTDIR/usr/bin/dunst" "$@"
fi
EOF
chmod +x ./AppRun

# Dummy Icon & Desktop
echo "Placing .desktop and icon..."
touch ./dunst.png && ln -s ./dunst.png ./.DirIcon # No official icon?
cat >> ./dunst.desktop << 'EOF' # No official .desktop?
[Desktop Entry]
Type=Application
Name=dunst
Icon=dunst
Exec=dunst
Categories=System
Hidden=true
EOF

# DEPLOY ALL LIBS
echo "Deploying dependencies..."
mkdir -p ./usr/lib
ldd ./usr/bin/* | awk -F"[> ]" '{print $4}' | xargs -I {} cp -vf {} ./usr/lib
if [ -f ./usr/lib/ld-musl-x86_64.so.1 ]; then
	mv ./usr/lib/ld-musl-x86_64.so.1 ./
else
	cp /lib/ld-musl-x86_64.so.1 ./
fi
find ./usr/bin -type f -exec patchelf --set-rpath '$ORIGIN/../lib' {} ';'
find ./usr/lib -type f -exec patchelf --set-rpath '$ORIGIN' {} ';'
find ./usr/lib ./usr/bin -type f -exec strip -s -R .comment --strip-unneeded {} ';'

# Do the thing!
echo "Generating AppImage..."
#export VERSION="$(./AppRun --version | awk '{print $(NF-1)}')" # This breaks for some reason?
cd ..
wget -q "$APPIMAGETOOL" -O appimagetool
chmod +x ./appimagetool
./appimagetool --comp zstd \
	--mksquashfs-opt -Xcompression-level --mksquashfs-opt 22 \
	-n ./AppDir ./dunst-"$ARCH".AppImage
ls
echo "All Done!"
