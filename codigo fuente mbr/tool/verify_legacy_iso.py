"""Read-only checks for the Limine BIOS/MBR ISO written as a raw image."""

import argparse
import pathlib
import sys


def fail(message):
    raise SystemExit(f"ISO Legacy no valida: {message}")


def verify(path, pycdlib_root):
    if not path.is_file():
        fail(f"no existe {path}")
    if path.stat().st_size < 64 * 2048:
        fail("el archivo es demasiado pequeno")

    with path.open("rb") as image:
        mbr = image.read(512)
        if len(mbr) != 512 or mbr[510:512] != b"\x55\xaa":
            fail("falta la firma de arranque MBR 55 AA")
        if b"isolinux.bin missing or corrupt" in mbr[:440]:
            fail("el MBR aun contiene el cargador generico de ISO, no Limine")
        if not any(mbr[offset + 4] != 0 for offset in (446, 462, 478, 494)):
            fail("la tabla MBR no contiene ninguna particion")
        image.seek(16 * 2048 + 1)
        if image.read(5) != b"CD001":
            fail("se ha dañado el volumen ISO9660")

    sys.path.insert(0, str(pycdlib_root.resolve()))
    try:
        import pycdlib
    except ImportError as exc:
        fail(f"no se pudo cargar PyCdlib: {exc}")

    iso = pycdlib.PyCdlib()
    opened = False
    try:
        iso.open(str(path))
        opened = True
        required = (
            "/LIMINE.CONF;1",
            "/LIMINE_BIOS.SYS;1",
            "/BOOT/KERNEL.ELF;1",
            "/BOOT/LIMINE_BIOS_CD.BIN;1",
            "/BOOT/LIMINE_UEFI_CD.BIN;1",
            "/EFI/BOOT/BOOTX64.EFI;1",
            "/BOOT.CAT;1",
        )
        for iso_path in required:
            try:
                if iso.get_record(iso_path=iso_path).data_length == 0:
                    fail(f"archivo vacio: {iso_path}")
            except Exception as exc:
                fail(f"falta {iso_path}: {exc}")

        catalog = iso.eltorito_boot_catalog
        if catalog is None or catalog.validation_entry.platform_id != 0:
            fail("falta la entrada El Torito BIOS")
        if catalog.initial_entry.boot_indicator != 0x88:
            fail("la entrada El Torito BIOS no es arrancable")
        efi_sections = [section for section in catalog.sections
                        if section.platform_id == 0xEF]
        if (len(efi_sections) != 1 or not efi_sections[0].section_entries or
                efi_sections[0].section_entries[0].boot_indicator != 0x88):
            fail("falta la entrada El Torito UEFI")
    except Exception as exc:
        fail(f"no se pudo leer la estructura ISO9660: {exc}")
    finally:
        if opened:
            iso.close()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--iso", required=True, type=pathlib.Path)
    parser.add_argument("--pycdlib", required=True, type=pathlib.Path)
    args = parser.parse_args()
    path = args.iso.resolve()
    verify(path, args.pycdlib)
    print(f"ISO BIOS/MBR hibrida creada y revisada: {path} ({path.stat().st_size} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
