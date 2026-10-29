"""Registra la placa de la mascota dentro del firmware xiaozhi-esp32.

Uso (lo ejecuta GitHub Actions automáticamente):
    python aplicar_mascota.py <carpeta_de_xiaozhi> <carpeta_de_la_placa>
"""
import shutil
import sys
from pathlib import Path

BOARD_DIR = "mascota-esp32-s3-lcd-2"
SYMBOL = "BOARD_TYPE_MASCOTA_ESP32_S3_LCD_2"

KCONFIG_BLOCK = (
    f"    config {SYMBOL}\n"
    f"        bool \"Mascota IA (Waveshare ESP32-S3-LCD-2)\"\n"
    f"        depends on IDF_TARGET_ESP32S3\n"
)

CMAKE_BLOCK = (
    f"elseif(CONFIG_{SYMBOL})\n"
    f"    set(BOARD_DIR \"{BOARD_DIR}\")\n"
    f"    set(BUILTIN_TEXT_FONT font_noto_sans_basic_20_4)\n"
    f"    set(BUILTIN_ICON_FONT font_material_symbols_20_4)\n"
    f"    set(DEFAULT_EMOJI_COLLECTION noto-color-emoji_128)\n"
)


def insert_before(path: Path, anchor: str, block: str) -> None:
    text = path.read_text(encoding="utf-8")
    if SYMBOL in text:
        print(f"{path.name}: ya contiene la placa, no se toca")
        return
    index = text.find(anchor)
    if index < 0:
        sys.exit(f"ERROR: no encuentro '{anchor.strip()}' en {path}")
    path.write_text(text[:index] + block + text[index:], encoding="utf-8")
    print(f"{path.name}: placa añadida")


def main() -> None:
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    xiaozhi = Path(sys.argv[1]).resolve()
    board_src = Path(sys.argv[2]).resolve()
    main_dir = xiaozhi / "main"

    destination = main_dir / "boards" / BOARD_DIR
    if destination.exists():
        shutil.rmtree(destination)
    shutil.copytree(board_src, destination)
    print(f"Placa copiada en {destination}")

    insert_before(
        main_dir / "Kconfig.projbuild",
        "    config BOARD_TYPE_BREAD_COMPACT_WIFI_LCD\n",
        KCONFIG_BLOCK,
    )
    insert_before(
        main_dir / "CMakeLists.txt",
        "elseif(CONFIG_BOARD_TYPE_BREAD_COMPACT_WIFI_LCD)\n",
        CMAKE_BLOCK,
    )


if __name__ == "__main__":
    main()
