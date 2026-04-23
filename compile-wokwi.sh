#!/usr/bin/env bash
# Gera o firmware *local* para o Wokwi (nao passa pelos servidores do site).
# Requisito: arduino-cli + nucleo esp32:esp32
#   curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
#   arduino-cli core update-index
#   arduino-cli core install esp32:esp32
# Depois, na IDE Wokwi: F1 -> Wokwi: Start Simulator
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"
# Nucleo 3.x: "ESP32 Dev Module" = esp32:esp32:esp32 (o antigo "esp32dev" ja nao existe)
FQBN="${FQBN:-esp32:esp32:esp32}"
OUT="build"
mkdir -p "$OUT"
if ! arduino-cli lib list | grep -qF 'LiquidCrystal'; then
  echo "A instalar biblioteca LiquidCrystal (uma vez)..."
  arduino-cli lib install "LiquidCrystal"
fi
echo "Compilando: $FQBN  (pasta sketch/)"
arduino-cli compile -b "$FQBN" --output-dir "$OUT" sketch
echo "Concluido. Binarios em: $OUT/"
find "$OUT" -name '*.bin' -o -name '*.elf' 2>/dev/null | head -20
echo "Ajusta wokwi.toml se o caminho de firmware/elf nao bater (varia com FQBN)."
