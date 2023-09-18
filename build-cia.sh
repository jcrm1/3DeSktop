make clean
make
$DEVKITARM/bin/arm-none-eabi-strip 3DeSktop.elf
makerom -f cia -o 3DeSktop.cia -rsf cia.rsf -target t -exefslogo -elf 3DeSktop.elf -icon 3DeSktop.smdh -banner banner.bin
