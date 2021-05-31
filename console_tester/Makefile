MAKEFILE = Makefile

####### Compiler, tools and options
ifeq ($(CPU),X86)
CXX = g++
LINK = g++
TARGET = test_x86
LIBS = -L../eSPDI -leSPDI_X86_64_pp
endif

ifeq ($(BITS), 32)
	ifeq ($(CPU),MIPS)
		CXX = mipsel-linux-gnu-g++
		LINK = mipsel-linux-gnu-g++
		TARGET = test_mips
		LIBS = -L../eSPDI -leSPDI_MIPS_32_tiny
	endif

	ifeq ($(CPU),ARMEL)
		CXX = arm-linux-gnueabi-g++
		LINK = arm-linux-gnueabi-g++
		TARGET = test_armel
		LIBS = -L../eSPDI -leSPDI_ARMEL_32_tiny.so
	endif
	ifeq ($(CPU),ARMHF)
		CXX = ../toolchain/hypatia/toolchain-sunxi-glibc/toolchain/bin/arm-openwrt-linux-g++
		LINK = ../toolchain/hypatia/toolchain-sunxi-glibc/toolchain/bin/arm-openwrt-linux-g++
		TARGET = test_armhf
		LIBS = -L../eSPDI -leSPDI_hypatia_32 \
			   -L../eSPDI/alsa-release/arm/32-bits/lib/ -l:../eSPDI/alsa-release/arm/32-bits/lib/libasound.so.2 \
			   -L../eSPDI/turbojpeg/armhf_32/lib/ -lturbojpeg \
			   -L../eSPDI/opencl/armhf_32/lib/ -l:../eSPDI/opencl/armhf_32/lib/libOpenCL.so \
			   -L../eSPDI/opencv/ARMHF_32/lib/ -lopencv_core -lopencv_imgproc -lopencv_imgcodecs
	endif
	ifeq ($(CPU),TI)
		CXX = arm-linux-gnueabihf-g++
		LINK = arm-linux-gnueabihf-g++
		TARGET = test_armhf_ti
		LIBS = -L../eSPDI -leSPDI_$(CPU)_32 \
			   -L../eSPDI/alsa-release/armhf_ti_32/lib/ -l:../eSPDI/alsa-release/armhf_ti_32/lib/libasound.so.2 \
			   -L../eSPDI/turbojpeg/armhf_ti_32/lib/ -lturbojpeg \
			   -L../eSPDI/opencl/armhf_ti_32/lib/ -l:../eSPDI/opencl/armhf_ti_32/lib/libOpenCL.so \
			   -L../eSPDI/opencv/armhf_ti_32/lib/ -lopencv_core -lopencv_imgproc -lopencv_imgcodecs
	endif
endif
ifeq ($(BITS), 64)
	ifeq ($(CPU),X86)
		CXX = g++
		LINK = g++
		TARGET = test_x86
		LIBS = -L../eSPDI -leSPDI_X86_64 \
			   -L../eSPDI/turbojpeg/x86_64/lib/ -lturbojpeg \
			   -lrt -lm -ldl -lpthread -lX11 -ludev
	endif
	ifeq ($(CPU), NVIDIA)
		CXX = ../toolchain/tx2/gcc-linaro-7.5.0/bin/aarch64-linux-gnu-g++
		LINK = ../toolchain/tx2/gcc-linaro-7.5.0/bin/aarch64-linux-gnu-g++
		TARGET = test_arm64_tx2
		LIBS = -L../eSPDI -leSPDI_$(CPU)_64 \
			   -L../eSPDI/turbojpeg/TX2/lib/ -lturbojpeg \
			   -lrt
	endif
	ifeq ($(CPU), ARMHF)
		CXX =  aarch64-linux-gnu-g++
         	LINK =  aarch64-linux-gnu-g++
           	TARGET = test_arm64
             	LIBS = -L../eSPDI -leSPDI_ARMHF_64_tiny
	endif
endif


CFLAGS = -Wall -W -std=c++11
LFLAGS = -Wall -W 
LIBS += -lpthread
OBJECTS = main.o ColorPaletteGenerator.o RegisterSettings.o
INCPATH = -I../eSPDI -I../eSPDI/turbojpeg/armhf_32/include/


####### Implicit rules

.SUFFIXES: .o .cpp

.cpp.o:
	$(CXX) -c $(CFLAGS) $(INCPATH) -g -o $@ $<

all: $(TARGET)

####### Build rules
$(TARGET): $(OBJECTS)
	$(LINK) $(LFLAGS) -g -o $@ $^ $(LIBS) 

####### clean rules
clean: 
	rm $(OBJECTS) $(TARGET)
distclean:
	rm $(OBJECTS)
