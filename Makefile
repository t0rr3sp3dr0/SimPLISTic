export RC_ProjectSourceVersion=1.0.0
all:
	gcc *.c  -o jlutil -g2  -DWANT_MAIN
	strip  jlutil

arm64:
	gcc-arm64 *.c -o jlutil.arm64  -DWANT_MAIN /Users/morpheus/Documents/Work/ProcExp/crt0.arm64.o  -miphoneos-version-min=8.0 -DWANT_MAIN
	strip  jlutil.arm64
	jtool --sign --inplace jlutil.arm64 
arm64e:
	gcc-arm64e *.c -o jlutil.arm64e -DWANT_MAIN /Users/morpheus/Documents/Work/ProcExp/crt0.arm64e.o  -miphoneos-version-min=8.0 -DWANT_MAIN
	strip  jlutil.arm64e
	jtool --sign --inplace jlutil.arm64e
	
arm32:
	gcc-armv7 *.c -o jlutil.armv7 -DWANT_MAIN
	jtool --sign --inplace jlutil.armv7


universal: all arm64 arm64e
	lipo -create -arch arm64e jlutil.arm64e -arch arm64 jlutil.arm64 -arch x86_64 jlutil -output jlutil.universal

linux:
	gcc -DLINUX *.c -o jlutil.ELF64 -Wall -DWANT_MAIN
	strip -x jlutil.ELF64

dist:
	tar cvf simplist.tar jlutil.ELF64 jlutil.universal
test:
	./test.sh
backup:
	tar zcvf ~/simplistSrc.tgz *
