"""Create the UEFI/GPT ISO; bios-install is applied to its Legacy copy."""

import argparse
import pathlib
import struct
import sys
import zlib


SECTOR_SIZE = 512
GPT_SIGNATURE = b"EFI PART"


def normalize_gpt_copies(image_path):
    """Make the backup GPT entries match the primary entries exactly."""
    with image_path.open("r+b") as image:
        image.seek(0, 2)
        image_size = image.tell()
        if image_size == 0 or image_size % SECTOR_SIZE:
            raise ValueError("La ISO no tiene un tamano valido para GPT")

        image.seek(SECTOR_SIZE)
        primary = bytearray(image.read(SECTOR_SIZE))
        if primary[:8] != GPT_SIGNATURE:
            raise ValueError("No se encontro la cabecera GPT principal")
        backup_lba = struct.unpack_from("<Q", primary, 32)[0]
        primary_entries_lba = struct.unpack_from("<Q", primary, 72)[0]
        count, entry_size = struct.unpack_from("<II", primary, 80)
        if count < 2 or entry_size < 128:
            raise ValueError("La tabla GPT no contiene la particion UEFI esperada")

        table_size = count * entry_size
        image.seek(backup_lba * SECTOR_SIZE)
        backup = bytearray(image.read(SECTOR_SIZE))
        if backup[:8] != GPT_SIGNATURE:
            raise ValueError("No se encontro la cabecera GPT de respaldo")
        backup_entries_lba = struct.unpack_from("<Q", backup, 72)[0]
        if (primary_entries_lba * SECTOR_SIZE + table_size > image_size or
                backup_entries_lba * SECTOR_SIZE + table_size > image_size):
            raise ValueError("La tabla GPT queda fuera de la ISO")

        image.seek(primary_entries_lba * SECTOR_SIZE)
        primary_table = bytearray(image.read(table_size))
        image.seek(backup_entries_lba * SECTOR_SIZE)
        backup_table = bytearray(image.read(table_size))
        if len(primary_table) != table_size or len(backup_table) != table_size:
            raise ValueError("No se pudieron leer ambas tablas GPT")
        # PyCdlib generates separate GUID values in its two GPT copies. Keep
        # the primary's partition identifiers and make the backup identical.
        for index in range(count):
            start = index * entry_size
            end = start + entry_size
            p_entry = primary_table[start:end]
            b_entry = backup_table[start:end]
            if p_entry[:16] != b_entry[:16] or p_entry[32:] != b_entry[32:]:
                raise ValueError("Las entradas GPT principal y de respaldo difieren")

        table_crc = zlib.crc32(primary_table) & 0xFFFFFFFF
        for header in (primary, backup):
            header_size = struct.unpack_from("<I", header, 12)[0]
            if header_size < 92 or header_size > SECTOR_SIZE:
                raise ValueError("Tamano de cabecera GPT no valido")
            struct.pack_into("<I", header, 88, table_crc)
            struct.pack_into("<I", header, 16, 0)
            struct.pack_into("<I", header, 16,
                             zlib.crc32(header[:header_size]) & 0xFFFFFFFF)

        image.seek(primary_entries_lba * SECTOR_SIZE)
        image.write(primary_table)
        image.seek(backup_entries_lba * SECTOR_SIZE)
        image.write(primary_table)
        image.seek(SECTOR_SIZE)
        image.write(primary)
        image.seek(backup_lba * SECTOR_SIZE)
        image.write(backup)


def add_directory(iso, iso_path, rr_name, joliet_path):
    iso.add_directory(iso_path=iso_path, rr_name=rr_name, joliet_path=joliet_path)


def add_file(iso, stage, source_path, iso_path, rr_name, joliet_path):
    filename = stage.joinpath(*source_path.split("/"))
    if not filename.is_file():
        raise FileNotFoundError(f"Missing staged file: {filename}")
    iso.add_file(
        str(filename),
        iso_path=iso_path,
        rr_name=rr_name,
        joliet_path=joliet_path,
    )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--stage", required=True, type=pathlib.Path)
    parser.add_argument("--output", required=True, type=pathlib.Path)
    parser.add_argument("--pycdlib", required=True, type=pathlib.Path)
    args = parser.parse_args()

    stage = args.stage.resolve()
    output = args.output.resolve()
    package_root = args.pycdlib.resolve()
    sys.path.insert(0, str(package_root))

    try:
        import pycdlib
        from pycdlib.isohybrid import IsoHybrid
    except ImportError as exc:
        raise SystemExit(f"No se encuentra PyCdlib dentro de {package_root}") from exc

    if not stage.is_dir():
        raise SystemExit(f"No existe la carpeta temporal: {stage}")
    if output.exists():
        raise SystemExit(f"La ISO ya existe y no se sobrescribirá: {output}")
    output.parent.mkdir(parents=True, exist_ok=True)

    iso = pycdlib.PyCdlib()
    try:
        iso.new(interchange_level=3, vol_ident="VOS", joliet=3, rock_ridge="1.09")

        add_directory(iso, "/BOOT", "boot", "/boot")
        add_directory(iso, "/EFI", "efi", "/EFI")
        add_directory(iso, "/EFI/BOOT", "boot", "/EFI/BOOT")

        add_file(
            iso, stage, "limine.conf", "/LIMINE.CONF;1", "limine.conf", "/limine.conf"
        )
        add_file(
            iso, stage, "limine-bios.sys", "/LIMINE_BIOS.SYS;1",
            "limine-bios.sys", "/limine-bios.sys",
        )
        add_file(
            iso,
            stage,
            "boot/limine-bios-cd.bin",
            "/BOOT/LIMINE_BIOS_CD.BIN;1",
            "limine-bios-cd.bin",
            "/boot/limine-bios-cd.bin",
        )
        add_file(
            iso,
            stage,
            "boot/kernel.elf",
            "/BOOT/KERNEL.ELF;1",
            "kernel.elf",
            "/boot/kernel.elf",
        )
        add_file(
            iso,
            stage,
            "boot/limine-uefi-cd.bin",
            "/BOOT/LIMINE_UEFI_CD.BIN;1",
            "limine-uefi-cd.bin",
            "/boot/limine-uefi-cd.bin",
        )
        add_file(
            iso,
            stage,
            "EFI/BOOT/BOOTX64.EFI",
            "/EFI/BOOT/BOOTX64.EFI;1",
            "BOOTX64.EFI",
            "/EFI/BOOT/BOOTX64.EFI",
        )

        iso.add_eltorito(
            bootfile_path="/BOOT/LIMINE_BIOS_CD.BIN;1",
            bootcatfile="/BOOT.CAT;1",
            boot_load_size=4,
            boot_info_table=True,
        )
        iso.add_eltorito(
            bootfile_path="/BOOT/LIMINE_UEFI_CD.BIN;1",
            bootcatfile="/BOOT.CAT;1",
            platform_id=0xEF,
            efi=True,
        )
        # Rufus ISO mode can extract these files to a normal UEFI FAT32 USB.
        # This generic hybrid MBR is not Limine BIOS USB boot code; the build
        # script applies Limine's installer to a separate Legacy image copy.
        hybrid = IsoHybrid()
        hybrid.new(True, False, 1, None, 0, 32, 64, 0)
        iso.isohybrid_mbr = hybrid
        iso.write(str(output))
    finally:
        iso.close()

    normalize_gpt_copies(output)

    print(f"ISO creada: {output} ({output.stat().st_size} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
