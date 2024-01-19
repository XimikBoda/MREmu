GCC = $(GCC_ARM_NONE_EABI)\arm-none-eabi-gcc
GPP = $(GCC_ARM_NONE_EABI)\arm-none-eabi-g++

GCC_PARAM = -c -fpic -march=armv5te -fvisibility=hidden -O2 -mlittle-endian 

LINK_PARAM =  -fno-threadsafe-statics -fpic -fpcc-struct-return -T "$(CURDIR)\scat.ld" -fvisibility=hidden  -Wl,--gc-sections --disable-libstdcxx-verbose -Wl,--wrap,_malloc_r  -pie
#-l:libc.a
PROJECT_NAME = stdio_module

APP_NAME = "stdio_module"
AUTHOR = "Ximik_Boda"

SOURCES = main.c
OBJECTS=$(SOURCES:.c=.o)

all: stdio_module.axf
#	xxd -i stdio_module.axf > "ARModuleBin.h"

	
stdio_module.axf: $(OBJECTS)
	$(GCC) -o $(CURDIR)\stdio_module.axf $(OBJECTS) $(LINK_PARAM)

.c.o:
	$(GCC) $(GCC_PARAM) -c $< -o $(CURDIR)\$@

clean:
	rm -f  *.o
	rm -f  *.axf

