#!/usr/bin/env bash
#
# install.sh - vieno komandos paleidimas, kuris:
#   1) idiegia libcurl dev paketa (jei jo dar nera)
#   2) atsisiuncia cJSON biblioteka i src/ (jei jos dar nera)
#   3) sukompiliuoja programa (make)
#
# Naudojimas:
#   ./install.sh
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "==> Tikrinami kompiliavimo irankiai (gcc, make)..."
if ! command -v gcc >/dev/null 2>&1 || ! command -v make >/dev/null 2>&1; then
    echo "==> gcc/make nerasta - idiegiamas build-essential"
    if command -v apt-get >/dev/null 2>&1; then
        sudo apt-get update -qq
        sudo apt-get install -y build-essential
    elif command -v dnf >/dev/null 2>&1; then
        sudo dnf groupinstall -y "Development Tools"
    elif command -v pacman >/dev/null 2>&1; then
        sudo pacman -Sy --noconfirm base-devel
    fi
else
    echo "    OK - gcc ir make jau prieinami."
fi

echo "==> Tikrinama libcurl vystymo biblioteka..."
if ! echo '#include <curl/curl.h>' | ${CC:-cc} -E - >/dev/null 2>&1; then
    echo "==> libcurl/curl.h nerastas - bandoma idiegti per apt-get (reikalingas sudo)"
    if command -v apt-get >/dev/null 2>&1; then
        sudo apt-get update -qq
        sudo apt-get install -y libcurl4-openssl-dev
    elif command -v dnf >/dev/null 2>&1; then
        sudo dnf install -y libcurl-devel
    elif command -v pacman >/dev/null 2>&1; then
        sudo pacman -Sy --noconfirm curl
    else
        echo "!! Nepavyko automatiskai nustatyti paketu tvarkykles."
        echo "!! Idiekite libcurl vystymo paketa rankiniu budu ir paleiskite is naujo."
        exit 1
    fi
else
    echo "    OK - libcurl jau prieinamas."
fi

echo "==> Tikrinama cJSON biblioteka..."
if [ ! -f "src/cJSON.h" ] || [ ! -f "src/cJSON.c" ]; then
    echo "==> Atsisiunciama cJSON is https://github.com/DaveGamble/cJSON"
    curl -fsSL -o src/cJSON.h https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.h
    curl -fsSL -o src/cJSON.c https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.c
else
    echo "    OK - cJSON jau yra src/ kataloge."
fi

echo "==> Kompiliuojama (make)..."
make

echo ""
echo "==> Baigta! Vykdomasis failas: bin/speedtest"
echo "==> Bandykite: ./bin/speedtest --help"
