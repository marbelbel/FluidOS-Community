"""Read-only structural checks for the VOS UEFI/GPT hybrid ISO."""

import argparse
import io
import pathlib
import struct
import sys
import zlib


SECTOR_SIZE = 512
GPT_SIGNATURE = b"EFI PART"
ISOHYBRID_DATA_PARTITION_GUID = bytes.fromhex(
    "a2 a0 d0 eb e5 b9 33 44 87 c0 68 b6 b7 26 99 c7"
)


def fail(message):
    raise SystemExit(f"ISO no valida: {message}")


def read_gpt_header(image, lba, sector_count):
    image.seek(lba * SECTOR_SIZE)
    header = bytearray(image.read(SECTOR_SIZE))
    if len(header) != SECTOR_SIZE or header[:8] != GPT_SIGNATURE:
        fail(f"falta una cabecera GPT en el sector {lba}")

    header_size = struct.unpack_from("<I", header, 12)[0]
    expected_crc = struct.unpack_from("<I", header, 16)[0]
    if header_size < 92 or header_size > SECTOR_SIZE:
        fail("tamano de cabecera GPT invalido")
    struct.pack_into("<I", header, 16, 0)
    if zlib.crc32(header[:header_size]) & 0xFFFFFFFF != expected_crc:
        fail(f"CRC de cabecera GPT incorrecto en el sector {lba}")
    struct.pack_into("<I", header, 16, expected_crc)

    current_lba, backup_lba = struct.unpack_from("<QQ", header, 24)
    first_usable, last_usable = struct.unpack_from("<QQ", header, 40)
    entries_lba = struct.unpack_from("<Q", header, 72)[0]
    count, entry_size, entries_crc = struct.unpack_from("<III", header, 80)
    if current_lba != lba or backup_lba >= sector_count or count < 2 or entry_size < 128:
        fail("campos de cabecera GPT incoherentes")
    table_sectors = (count * entry_size + SECTOR_SIZE - 1) // SECTOR_SIZE
    if entries_lba + table_sectors > sector_count:
        fail("tabla GPT fuera de la ISO")
    if first_usable > last_usable or last_usable >= sector_count:
        fail("limites de uso de GPT invalidos")
    if lba == 1 and entries_lba + table_sectors > first_usable:
        fail("la tabla GPT principal invade el espacio de particiones")
    if lba != 1 and entries_lba + table_sectors > current_lba:
        fail("la tabla GPT de respaldo invade su cabecera")
    image.seek(entries_lba * SECTOR_SIZE)
    table = image.read(count * entry_size)
    if zlib.crc32(table) & 0xFFFFFFFF != entries_crc:
        fail(f"CRC de tabla GPT incorrecto en el sector {lba}")
    return (header, table, backup_lba, entry_size, entries_lba,
            first_usable, last_usable, current_lba)


def verify_partition_tables(path):
    size = path.stat().st_size
    if size == 0 or size % SECTOR_SIZE:
        fail("el tamano no esta alineado a sectores de 512 bytes")
    sector_count = size // SECTOR_SIZE

    with path.open("rb") as image:
        image.seek(510)
        if image.read(2) != b"\x55\xaa":
            fail("falta la firma MBR 55 AA")

        image.seek(446 + 16)
        efi_entry = image.read(16)
        if len(efi_entry) != 16 or efi_entry[4] != 0xEF:
            fail("falta la particion EFI tipo EF en el MBR")
        mbr_efi_start, mbr_efi_size = struct.unpack_from("<II", efi_entry, 8)

        (primary, primary_table, backup_lba, entry_size, primary_entries_lba,
         first_usable, last_usable, primary_lba) = read_gpt_header(
            image, 1, sector_count
        )
        (backup, backup_table, unused_backup, backup_entry_size,
         backup_entries_lba, backup_first_usable, backup_last_usable,
         backup_current_lba) = read_gpt_header(
            image, backup_lba, sector_count
        )
        if (unused_backup != primary_lba or backup_current_lba != backup_lba or
                entry_size != backup_entry_size or
                backup_first_usable != first_usable or
                backup_last_usable != last_usable):
            fail("cabeceras GPT de respaldo incoherentes")
        if primary_table != backup_table:
            fail("la tabla GPT de respaldo no coincide con la principal")

        efi_partition = primary_table[entry_size:entry_size + 128]
        if efi_partition[:16] != ISOHYBRID_DATA_PARTITION_GUID:
            fail("falta la entrada hibrida de la imagen UEFI en GPT")
        efi_first, efi_last = struct.unpack_from("<QQ", efi_partition, 32)
        if (efi_first != mbr_efi_start or efi_last < efi_first or
                efi_first < first_usable or efi_last > last_usable):
            fail("las ubicaciones EFI de MBR y GPT no coinciden")
        if efi_last - efi_first + 1 != mbr_efi_size or efi_last >= sector_count:
            fail("los limites de la particion EFI no son validos")


def verify_eltorito_and_files(path, pycdlib_root):
    sys.path.insert(0, str(pycdlib_root.resolve()))
    try:
        import pycdlib
    except ImportError as exc:
        fail(f"no se pudo cargar PyCdlib: {exc}")

    required_paths = (
        "/LIMINE.CONF;1",
        "/LIMINE_BIOS.SYS;1",
        "/BOOT/KERNEL.ELF;1",
        "/BOOT/LIMINE_BIOS_CD.BIN;1",
        "/BOOT/LIMINE_UEFI_CD.BIN;1",
        "/EFI/BOOT/BOOTX64.EFI;1",
        "/BOOT.CAT;1",
    )
    iso = pycdlib.PyCdlib()
    opened = False
    try:
        iso.open(str(path))
        opened = True
        for iso_path in required_paths:
            try:
                inode = iso.get_record(iso_path=iso_path)
            except Exception as exc:
                fail(f"falta {iso_path}: {exc}")
            if inode.data_length == 0:
                fail(f"archivo vacio: {iso_path}")

        config_stream = io.BytesIO()
        iso.get_file_from_iso_fp(config_stream, iso_path="/LIMINE.CONF;1")
        config = config_stream.getvalue().decode("ascii", errors="strict")
        if "timeout: 0" not in config or "quiet: yes" not in config:
            fail("Limine debe iniciar sin menu ni cuenta atras")

        efi_stream = io.BytesIO()
        iso.get_file_from_iso_fp(efi_stream, iso_path="/EFI/BOOT/BOOTX64.EFI;1")
        efi_app = efi_stream.getvalue()
        if len(efi_app) < 0x40 or efi_app[:2] != b"MZ":
            fail("BOOTX64.EFI no tiene cabecera PE valida")
        pe_offset = struct.unpack_from("<I", efi_app, 0x3C)[0]
        if (pe_offset + 26 > len(efi_app) or efi_app[pe_offset:pe_offset + 4] != b"PE\0\0" or
                struct.unpack_from("<H", efi_app, pe_offset + 4)[0] != 0x8664 or
                struct.unpack_from("<H", efi_app, pe_offset + 24)[0] != 0x20B):
            fail("BOOTX64.EFI no es una aplicacion PE32+ x64")

        catalog = iso.eltorito_boot_catalog
        if catalog is None or catalog.validation_entry.platform_id != 0:
            fail("falta la entrada BIOS El Torito para CD")
        if catalog.initial_entry.boot_indicator != 0x88:
            fail("la entrada BIOS El Torito no es arrancable")
        efi_sections = [section for section in catalog.sections
                        if section.platform_id == 0xEF]
        if (len(efi_sections) != 1 or not efi_sections[0].section_entries or
                efi_sections[0].section_entries[0].boot_indicator != 0x88):
            fail("falta la entrada UEFI El Torito")
        if iso.isohybrid_mbr is None or not iso.isohybrid_mbr.efi:
            fail("la ISO no contiene metadatos hibridos UEFI")
    finally:
        if opened:
            iso.close()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--iso", required=True, type=pathlib.Path)
    parser.add_argument("--pycdlib", required=True, type=pathlib.Path)
    args = parser.parse_args()

    path = args.iso.resolve()
    if not path.is_file():
        fail(f"no existe {path}")
    verify_partition_tables(path)
    verify_eltorito_and_files(path, args.pycdlib)
    print(f"ISO validada: {path} ({path.stat().st_size} bytes)")
    print("UEFI/GPT para Rufus y entradas El Torito BIOS/UEFI para CD: OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
