# restored_external64patcher
A tool for patching a 64-bit restored_external binary to skip the sealing of the rootfs

## Build
`make`

## Usage
1. Extract binary from an iOS ramdisk (macOS only):
    - `img4 -i <ramdisk> -o ramdisk.dmg`
        - `img4` can be found [here](https://github.com/xerub/img4lib)
    - `hdiutil attach ramdisk.dmg -mountpoint ramdisk`
    - `cp ramdisk/usr/local/bin/restored_external .`
    - `hdiutil detach ramdisk`

2. Run `restored_external64_patcher`:
    - `restored_external64_patcher restored_external restored_external_patched`

3. Resign patched restored_external binary
    - `ldid -e restored_external > ents.plist`
    - `ldid -Sents.plist restored_external_patched`