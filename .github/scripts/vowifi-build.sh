#!/bin/sh
#
# Build Asterisk (vowifi branch) on a Debian based system and stage the
# installed tree as a tarball.  Used by .github/workflows/VowifiBuild.yml
# for the Linux x64 and Raspberry Pi (arm64) builds, but can be run
# by hand inside a Debian/Raspberry Pi OS container as well.
#
# Usage: vowifi-build.sh <artifact-name>
#
set -eu

NAME="${1:-asterisk-$(uname -m)}"
SRCDIR="$(cd "$(dirname "$0")/../.." && pwd)"
OUTDIR="${OUTDIR:-$SRCDIR/out}"
DESTDIR="$OUTDIR/$NAME"
# The VoLTE/VoWiFi patches need sysmocom's patched pjproject (e.g. the
# pjsip_tcp_transport_cfg.bind_if field), the stock tarball will not do.
PJPROJECT_REMOTE="${PJPROJECT_REMOTE:-https://gitea.sysmocom.de/sysmocom/pjproject.git}"
PJPROJECT_BRANCH="${PJPROJECT_BRANCH:-sysmocom/2.14}"

if [ "${SKIP_DEPS:-0}" != 1 ]; then
	export DEBIAN_FRONTEND=noninteractive
	SUDO=
	[ "$(id -u)" = 0 ] || SUDO=sudo
	$SUDO apt-get update -q
	$SUDO apt-get install -y -q --no-install-recommends \
		build-essential pkg-config autoconf automake ca-certificates \
		bzip2 patch wget file git xmlstarlet python3 \
		libedit-dev libjansson-dev libsqlite3-dev uuid-dev \
		libxml2-dev libxslt1-dev libssl-dev libsrtp2-dev \
		libcurl4-openssl-dev libunbound-dev liburiparser-dev \
		libspeex-dev libspeexdsp-dev libogg-dev libvorbis-dev \
		libopusfile-dev libgsm1-dev libspandsp-dev libcodec2-dev \
		libical-dev libpopt-dev libnewt-dev libcap-dev zlib1g-dev \
		libopencore-amrnb-dev libopencore-amrwb-dev libvo-amrwbenc-dev
fi

cd "$SRCDIR"

# The checkout may be owned by another user (e.g. when bind mounted into a
# container), make sure build_tools/make_version can still run git.
git config --global --add safe.directory "$SRCDIR" 2>/dev/null || true

# Use pjproject from git instead of downloading the release tarball, the
# same way osmocom's docker-playground builds this tree.
if [ ! -e third-party/pjproject/source ]; then
	git clone --depth=1 -b "$PJPROJECT_BRANCH" "$PJPROJECT_REMOTE" \
		third-party/pjproject/source
fi
git -C third-party/pjproject/source log -1 --format='pjproject: %H %s'

# The AMR codec patches touch configure.ac but do not ship a regenerated
# configure script, so always regenerate it.
./bootstrap.sh

./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--localstatedir=/var \
	--libdir=/usr/lib \
	--with-pjproject-bundled

make menuselect.makeopts
menuselect/menuselect \
	--disable BUILD_NATIVE \
	--enable codec_amr \
	--enable codec_vevs \
	menuselect.makeopts

# Fail early if the AMR codec, a core piece of the VoWiFi setup, would be
# silently skipped because a dependency was not found.
# MENUSELECT_CODECS lists the *disabled* codec modules.
if grep -q '^MENUSELECT_CODECS=.*codec_amr' menuselect.makeopts; then
	echo "codec_amr is not enabled, check the AMR libraries" >&2
	exit 1
fi

make -j"$(nproc)"
make -C codec_server

rm -rf "$DESTDIR"
make install samples DESTDIR="$DESTDIR"
install -D -m 755 codec_server/cserver "$DESTDIR/usr/bin/cserver"

tar -C "$OUTDIR" -czf "$OUTDIR/$NAME.tar.gz" "$NAME"
echo "Built $OUTDIR/$NAME.tar.gz"
