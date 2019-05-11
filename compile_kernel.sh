#!/bin/bash

#find files modified in the past 24 hours, ingnore hidden files (get only .c files)
files=`find . -not -path "*/\.*" \( -name "*.c" -o -name "*.h" -o -name "*.S" \) -mtime -1 -print`

#for every file copy to the correct place in the kernel library
for file in $files; do
        cp $file "/usr/src/linux-2.4.18-14custom${file:1}"      
done

cd /usr/src/linux-2.4.18-14custom
make bzImage
cd /mnt/hgfs/shared_folder/os234123
cp /usr/src/linux-2.4.18-14custom/arch/i386/boot/bzImage /boot/vmlinuz-2.4.18-14custom
