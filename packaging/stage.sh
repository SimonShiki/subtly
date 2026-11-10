#!/usr/bin/env bash
set -euo pipefail

source_dir=$(cd "$(dirname "$0")/.." && pwd)
binary=$(readlink -f "${1:?Usage: packaging/stage.sh binary destination}")
destination=$(realpath -m "${2:?Usage: packaging/stage.sh binary destination}")
private="$destination/usr/lib/subtly"
licenses="$destination/usr/share/licenses/subtly"

install -d -m755 "$destination"
install -Dm755 "$binary" "$private/subtly"
install -Dm644 "$source_dir/LICENSE" "$licenses/LICENSE"
for notice in "$source_dir"/third_party/*; do
  [[ -f $notice ]] && install -Dm644 "$notice" "$licenses/${notice##*/}"
done

# Bundle the private ASR libraries and their dependencies, including the wheel's
# hashed ONNX Runtime SONAME. Use the native package manager for system libraries.
dependencies=$(LC_ALL=C ldd "$binary")
if [[ $dependencies == *'not found'* ]]; then
  echo "$dependencies" >&2
  exit 1
fi
while read -r soname library; do
  library=$(readlink -f "$library")
  case "$library" in
    /usr/lib/*|/usr/lib64/*|/lib/*|/lib64/*) continue ;;
  esac
  install -Dm755 "$library" "$private/$soname"
  sdk_dir=${library%/*}
  while [[ -n $sdk_dir && $sdk_dir != / ]]; do
    if [[ -d $sdk_dir/share/licenses ]]; then
      cp -a "$sdk_dir/share/licenses/." "$licenses/"
      break
    fi
    sdk_dir=${sdk_dir%/*}
  done
done < <(printf '%s\n' "$dependencies" |
  awk '$2 == "=>" && $3 ~ /^\// && $1 !~ /\// { print $1, $3 }')

# Remove build-machine paths and flatten the wheel's relative library layout.
for elf in "$private"/*; do
  patchelf --set-rpath '$ORIGIN' "$elf"
done

install -d "$destination/usr/bin"
ln -s ../lib/subtly/subtly "$destination/usr/bin/subtly"
install -Dm644 "$source_dir/subtly.desktop" "$destination/usr/share/applications/subtly.desktop"
install -Dm644 "$source_dir/qml/icons/subtly.svg" \
  "$destination/usr/share/icons/hicolor/scalable/apps/subtly.svg"
install -Dm644 "$source_dir/README.md" "$destination/usr/share/doc/subtly/README.md"
