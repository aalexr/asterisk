#!/bin/sh
#
# Build this tree (vowifi branch) the way the vowifi_gateway engine image
# does, and stage the installed tree as a tarball that the engine's
# Containerfile can unpack over its Fedora base instead of compiling.
#
# Must run inside the same base image as the engine (FEDORA_RELEASE below,
# matching the FROM line of vowifi_gateway/engine/Containerfile) so the
# binaries link against the same library versions.  Used by
# .github/workflows/VowifiBuild.yml; can be run by hand with e.g.
#
#   podman run --rm -v "$PWD:/src:Z" -w /src \
#       registry.fedoraproject.org/fedora:40 .github/scripts/vowifi-build.sh
#
# Keep the package list, configure flags and menuselect options in sync
# with engine/Containerfile in vowifi_gateway.
#
# Output: $OUTDIR/asterisk-fedora<rel>-<arch>-pj<pjproject sha12>.tar.gz
#
set -eu

PJPROJECT_REPO="${PJPROJECT_REPO:-https://gitea.sysmocom.de/sysmocom/pjproject.git}"
PJPROJECT_REF="${PJPROJECT_REF:?set PJPROJECT_REF to the pjproject commit SHA}"
FEDORA_RELEASE="${FEDORA_RELEASE:-40}"

SRCDIR="$(cd "$(dirname "$0")/../.." && pwd)"
OUTDIR="${OUTDIR:-$SRCDIR/out}"
ARCH="$(uname -m)"
PJ_SHORT="$(printf %.12s "$PJPROJECT_REF")"
ASSET="asterisk-fedora${FEDORA_RELEASE}-${ARCH}-pj${PJ_SHORT}"
STAGE="$OUTDIR/stage-$ARCH"

# subshell: os-release sets NAME, VERSION, ... which must not leak into this script
OS="$(. /etc/os-release && echo "$ID $VERSION_ID")"
if [ "$OS" != "fedora $FEDORA_RELEASE" ]; then
	echo "expected fedora $FEDORA_RELEASE, running on $OS" >&2
	exit 1
fi

if [ "${SKIP_DEPS:-0}" != 1 ]; then
	dnf -y update
	dnf -y install git gcc g++ openssl-devel patch libedit-devel opus-devel libuuid-devel autoconf \
	automake libtool gettext perl python3 flex bison pkg-config m4 gettext-devel gperf gmp-devel \
	jansson-devel libxml2-devel sqlite-devel opencore-amr-devel vo-amrwbenc-devel \
	libmnl-devel speex-devel speexdsp-devel libvorbis-devel alsa-lib-devel portaudio-devel \
	libcurl-devel xmlstarlet postgresql-devel unixODBC-devel neon-devel gmime-devel lua-devel \
	uriparser-devel libxslt-devel mysql-devel bluez-libs-devel radcli-devel freetds-devel \
	jack-audio-connection-kit-devel libcap-devel net-snmp-devel corosynclib-devel newt-devel \
	popt-devel libical-devel spandsp-devel openldap-devel codec2-devel fftw-devel libsndfile-devel \
	unbound-devel wget subversion libresample-devel binutils-devel libsrtp-devel gsm-devel \
	systemd-devel bzip2 tar gzip
fi

cd "$SRCDIR"
# build_tools/make_version runs git; the checkout may belong to another uid.
git config --global --add safe.directory "$SRCDIR" 2>/dev/null || true
ASTERISK_SHA="$(git rev-parse HEAD)"

# sysmocom's pjproject (the VoLTE patches need e.g. pjsip_tcp_transport_cfg.bind_if),
# pinned by commit and used as the bundled pjproject source.
PJDIR="$OUTDIR/pjproject"
if [ ! -d "$PJDIR/.git" ]; then
	git init -q "$PJDIR"
	git -C "$PJDIR" fetch -q --depth=1 "$PJPROJECT_REPO" "$PJPROJECT_REF"
	git -C "$PJDIR" checkout -q FETCH_HEAD
fi
[ "$(git -C "$PJDIR" rev-parse HEAD)" = "$PJPROJECT_REF" ] || {
	echo "pjproject checkout is not $PJPROJECT_REF" >&2; exit 1; }
rm -f third-party/pjproject/source
ln -s "$PJDIR" third-party/pjproject/source

# The AMR commit changes configure.ac without regenerating configure.
sh bootstrap.sh
./configure --enable-binary-modules
make menuselect/menuselect menuselect-tree menuselect.makeopts
./menuselect/menuselect --enable codec_opus --disable BUILD_NATIVE
make -j"$(nproc)"

rm -rf "$STAGE"
make install DESTDIR="$STAGE"

INFO="$STAGE/usr/share/doc/asterisk-vowifi/BUILDINFO"
mkdir -p "$(dirname "$INFO")"
cat > "$INFO" <<INFO
ASTERISK_SHA=$ASTERISK_SHA
PJPROJECT_REPO=$PJPROJECT_REPO
PJPROJECT_REF=$PJPROJECT_REF
FEDORA_RELEASE=$FEDORA_RELEASE
ARCH=$ARCH
BUILT=$(date -u +%Y-%m-%dT%H:%M:%SZ)
INFO

# Paths in the tarball are relative to / (usr/..., var/..., etc/...).
tar -C "$STAGE" --numeric-owner --owner=0 --group=0 -czf "$OUTDIR/$ASSET.tar.gz" .
echo "Built $OUTDIR/$ASSET.tar.gz"
