#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

format=${1:?Usage: packaging/build.sh deb|rpm|archlinux|stage}
case "$format" in
  deb|rpm|archlinux|stage) ;;
  *) echo "Unknown format: $format" >&2; exit 1 ;;
esac
source_version=$(sed -n 's/^set_version("\([^"]*\)").*/\1/p' xmake.lua)
version=${SUBTLY_VERSION:-$source_version}
export SUBTLY_VERSION=${version#v}
if [[ $SUBTLY_VERSION != "$source_version" ]]; then
  echo "Package version must match set_version in xmake.lua ($source_version)" >&2
  exit 1
fi
x
if [[ ${SUBTLY_NEMOTRON:-n} == y ]]; then
  export SUBTLY_PACKAGE_SUFFIX=-nemotron
  export SUBTLY_PACKAGE_CONFLICT=subtly
  export SUBTLY_PACKAGE_PROVIDES=subtly
else
  export SUBTLY_PACKAGE_SUFFIX=
  export SUBTLY_PACKAGE_CONFLICT=subtly-nemotron
  export SUBTLY_PACKAGE_PROVIDES=
fi

xmake f -y -m release --nemotron="${SUBTLY_NEMOTRON:-n}"
xmake build subtly
mkdir -p dist
stage="$PWD/dist/root"
rm -rf -- "$stage"
bash packaging/stage.sh build/linux/x86_64/release/subtly "$stage"

if [[ $format != stage ]]; then
  nfpm package --config packaging/nfpm.yaml --packager "$format" --target dist/
fi
