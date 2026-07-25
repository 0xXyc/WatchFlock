# === ESP32-C5 Auto Flasher Script By: AWOK ===

import sys
import subprocess
import os
import platform   # Placeholder for possible OS checks
import glob
import time
import shutil
import struct
import argparse

def ensure_package(pkg):
    try:
        __import__(pkg if pkg != 'gitpython' else 'git')
    except ImportError:
        print(f"Installing missing package: {pkg}")
        subprocess.check_call([sys.executable, '-m', 'pip', 'install', '--upgrade', pkg])

try:
    import serial.tools.list_ports
except ImportError:
    ensure_package('pyserial')
    import serial.tools.list_ports
try:
    import esptool
except ImportError:
    ensure_package('esptool')
    import esptool
try:
    from colorama import Fore, Style
except ImportError:
    ensure_package('colorama')
    from colorama import Fore, Style

# Dependency check and install if needed
REQUIRED_PACKAGES = [
    'pyserial',
    'esptool',
    'colorama'
]

def ensure_requirements():
    for pkg in REQUIRED_PACKAGES:
        ensure_package(pkg)
ensure_requirements()

# Finds the first file from a list of possible names in the bins folder
def find_file(name_options, bins_dir):
    for name in name_options:
        files = glob.glob(os.path.join(bins_dir, name))
        if files:
            return files[0]
    return None

# Partition entries are 32 bytes: magic(2) type(1) subtype(1) offset(4) size(4) label(16) flags(4)
PARTITION_MAGIC = b'\xaa\x50'
PARTITION_TYPE_APP = 0x00
PARTITION_TYPE_DATA = 0x01
PARTITION_SUBTYPE_OTA_DATA = 0x00

def read_partitions(partitions_bin):
    try:
        with open(partitions_bin, 'rb') as f:
            table = f.read()
    except OSError:
        return []
    entries = []
    for i in range(0, len(table) - 31, 32):
        entry = table[i:i + 32]
        if entry[:2] != PARTITION_MAGIC:
            break
        offset, size = struct.unpack('<II', entry[4:12])
        entries.append({
            'type': entry[2],
            'subtype': entry[3],
            'offset': offset,
            'size': size,
            'label': entry[12:28].rstrip(b'\x00').decode('utf-8', 'replace'),
        })
    return entries

# Offsets come from the table itself, so re-partitioning can't silently desync them
def find_partition(partitions, p_type, subtype=None):
    matches = [p for p in partitions
               if p['type'] == p_type and (subtype is None or p['subtype'] == subtype)]
    return min(matches, key=lambda p: p['offset']) if matches else None

# Picks a port without needing a hotplug: explicit --port first, then any USB serial device
# already connected, and only then fall back to waiting for one to appear.
def select_port(preferred=None):
    if preferred:
        return preferred

    def usb_ports():
        return [p for p in serial.tools.list_ports.comports() if p.vid is not None]

    found = usb_ports()
    if not found:
        print(Fore.YELLOW + "Waiting for ESP32-C5 device to be connected..." + Style.RESET_ALL)
        while not found:
            time.sleep(0.5)
            found = usb_ports()

    if len(found) == 1:
        print(Fore.GREEN + f"Detected {found[0].description} on port: {found[0].device}" + Style.RESET_ALL)
        return found[0].device

    print(Fore.YELLOW + "Multiple serial devices found:" + Style.RESET_ALL)
    for i, port in enumerate(found, 1):
        print(f"  {i}) {port.device}  {port.description}")
    while True:
        choice = input(Fore.YELLOW + f"Select device [1-{len(found)}]: " + Style.RESET_ALL).strip()
        if choice.isdigit() and 1 <= int(choice) <= len(found):
            return found[int(choice) - 1].device
        print(Fore.RED + "Invalid selection." + Style.RESET_ALL)

def main():
    parser = argparse.ArgumentParser(description="ESP32-C5 Auto Flasher (bins subdir)")
    parser.add_argument('firmware', nargs='?',
                        help="Application .bin to flash. Defaults to the largest .bin in the bins folder.")
    parser.add_argument('--bins-dir', default=os.path.join(os.path.dirname(__file__), 'bins'),
                        help="Folder holding the bootloader, partition table and app image.")
    parser.add_argument('--port', help="Serial port to flash. Autodetected when omitted.")
    parser.add_argument('--baud', default='921600', help="Flash baud rate (default: 921600).")
    args = parser.parse_args()

    bins_dir = args.bins_dir
    if not os.path.isdir(bins_dir):
        print(Fore.RED + f"Bins directory not found: {bins_dir}\nPlease create a 'bins' folder with your .bin files." + Style.RESET_ALL)
        exit(1)

    # Logo and splash, both centered and purple
    terminal_width = shutil.get_terminal_size((100, 20)).columns
    def center(text): return text.center(terminal_width)
    logo_lines = [
        "                                            @@@@@@                              ",
        "                                @@@@@@@@  @@@@@ @@@@                            ",
        "                               @@@    @@@@@@@     @@@     @@@@@@@@              ",
        "                    @@@@@@@@@@@@@      @@@@@       @@@  @@@@    @@@             ",
        "                   @@@      @@@@@       @@@         @@@@@@       @@@            ",
        "                  @@@         @@@        @@         @@@@         @@@            ",
        "         @@@@@@@@@@@@          @@        @          @@@         @@@     @@      ",
        "        @@@     @@@@@           @@           @@@    @@          @@@@@@@@@@@@@@  ",
        "       @@@         @@@           @@          @@@               @@@@@        @@@@",
        "       @@@           @@                     @@@@              @@@            @@@",
        "      @@@@@            @@                   @@@@                             @@@",
        "   @@@@@@  @@                @@         @   @@@                            @@@@ ",
        "  @@@                         @@       @@         @                     @@@@@@  ",
        " @@@                           @@     @@@        @@            @@@@@@@@@@@@@@@  ",
        "@@@       @@@@      @          @@@@  @@@@@     @@@        @      @@@@  @@@ @@@  ",
        "@@@       @@@@@@@    @@@       @@@@@@@@@@@@@@@@@@@       @@        @@@ @@@      ",
        "@@@        @@@@@ @@@  @@@@@  @@@@ @@@@  @@@@@@@@@@      @@@@        @@@         ",
        "@@@@            @@@@@@@@@@@@@@@@  @@@   @@@ @@  @@@@@@@@@@@@         @@@        ",
        " @@@@          @@@@@@@@@@@@@@@@   @@@       @@   @@@@@@@@@@@@        @@@        ",
        "  @@@@@@     @@@@@   @@@     @@    @               @@  @@  @@@@    @@@@         ",
        "   @@@@@@@@@@@@@     @@@     @@                    @@  @@   @@@@@@@@@@@         ",
        "   @@@@@@@@@@@@      @@@     @@                   @@@  @     @@@@@@@@           ",
        "        @@  @@                                    @@@        @@  @@@            ",
        "        @   @@                                               @@  @@@            ",
        "           @@@                                                   @@@            ",
        "           @@@                                                                  ",
        ""
    ]
    splash_lines = [
        "--  ESP32 C5 Flasher --",
        "By AWOK",
        "Inspired from LordSkeletonMans ESP32 FZEasyFlasher",
        "Shout out to JCMK for the inspiration on setting up the C5",
        ""
    ]
    print(Fore.MAGENTA + "\n" + "\n".join(center(line) for line in logo_lines + splash_lines) + Style.RESET_ALL)

    serial_port = select_port(args.port)

    # Find bin files for each firmware component. The wildcards also match an Arduino
    # build folder, where everything is prefixed with the sketch name.
    bootloader = find_file(['bootloader.bin', '*.bootloader.bin'], bins_dir)
    partitions = find_file(['partition-table.bin', 'partitions.bin',
                            '*.partitions.bin', '*.partition-table.bin'], bins_dir)
    ota_data = find_file(['ota_data_initial.bin', '*.ota_data_initial.bin'], bins_dir)

    if args.firmware:
        app_bin = args.firmware
        if not os.path.isfile(app_bin):
            print(Fore.RED + f"Firmware file not found: {app_bin}" + Style.RESET_ALL)
            exit(1)
    else:
        # Main firmware: largest bin in the folder that's not bootloader, partition, or OTA.
        # Merged images are whole-flash dumps, not app images, so they must never win here.
        all_bins = glob.glob(os.path.join(bins_dir, "*.bin"))
        exclude = {bootloader, partitions, ota_data}
        firmware_bins = [f for f in all_bins
                         if f not in exclude and not f.endswith('merged.bin') and os.path.isfile(f)]
        if not firmware_bins:
            print(Fore.RED + "No application firmware .bin file found in the 'bins' folder!" + Style.RESET_ALL)
            exit(1)
        app_bin = max(firmware_bins, key=lambda f: os.path.getsize(f))

    # Print summary, ask for confirmation before flashing
    print(Fore.CYAN + f"\nBootloader:   {bootloader or 'NOT FOUND'}")
    print(f"Partitions:   {partitions or 'NOT FOUND'}")
    print(f"OTA Data:     {ota_data or 'NOT FOUND'}")
    print(f"App (main):   {app_bin}\n" + Style.RESET_ALL)
    if not (bootloader and partitions):
        print(Fore.RED + "Missing bootloader or partition table. Both are required for a complete flash!" + Style.RESET_ALL)
        exit(1)

    # Take every offset from the partition table so it can't drift out of sync with the layout
    table = read_partitions(partitions)
    app_part = find_partition(table, PARTITION_TYPE_APP)
    otadata_part = find_partition(table, PARTITION_TYPE_DATA, PARTITION_SUBTYPE_OTA_DATA)
    if not app_part:
        print(Fore.RED + f"No app partition found in {partitions}. Is that a valid partition table?" + Style.RESET_ALL)
        exit(1)
    if ota_data and not otadata_part:
        print(Fore.RED + "Found ota_data_initial.bin but the partition table has no otadata partition." + Style.RESET_ALL)
        exit(1)

    app_size = os.path.getsize(app_bin)
    if app_size > app_part['size']:
        print(Fore.RED + f"App image is {app_size:,} bytes but the '{app_part['label']}' partition "
                         f"holds only {app_part['size']:,}. Wrong file or wrong partition scheme." + Style.RESET_ALL)
        exit(1)

    print(Fore.CYAN + f"Flashing to '{app_part['label']}' at 0x{app_part['offset']:x} "
                      f"({app_size:,} of {app_part['size']:,} bytes used)\n" + Style.RESET_ALL)
    confirm = input(Fore.YELLOW + "Ready to flash these files to ESP32-C5? (y/N): " + Style.RESET_ALL)
    if confirm.strip().lower() != 'y':
        print("Aborting.")
        exit(0)

    # Flash using esptool. The C5 second-stage bootloader lives at 0x2000, not 0x0 like the C3/C6.
    esptool_args = [
        '--chip', 'esp32c5',
        '--port', serial_port,
        '--baud', args.baud,
        '--before', 'default_reset',
        '--after', 'hard_reset',
        'write_flash', '-z',
        '0x2000', bootloader,
        '0x8000', partitions,
    ]
    if ota_data:
        esptool_args += [hex(otadata_part['offset']), ota_data]
    esptool_args += [hex(app_part['offset']), app_bin]

    print(Fore.YELLOW + "Flashing ESP32-C5 with bootloader, partition table, and application..." + Style.RESET_ALL)
    try:
        esptool.main(esptool_args)
        print(Fore.GREEN + "Flashing complete!" + Style.RESET_ALL)
    except Exception as e:
        print(Fore.RED + f"Flashing failed: {e}" + Style.RESET_ALL)

if __name__ == "__main__":
    main()