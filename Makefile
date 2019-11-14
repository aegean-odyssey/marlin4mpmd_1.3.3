### simple(-ish) makefile to build Marlin4MPDM images

PROJECT = marlin4mpmd_1.3.3
VERSION = 133r1

DEFINES  = -DSTM32 -DSTM32F0 -DSTM32F070xB -DARM_MATH_CM0
DEFINES += -DUSE_HAL_DRIVER -DUSE_HAL_LEGACY
DEFINES += -DSTM_3DPRINT -DSTM32_MPMD -DMARLIN -DNO_WIFI

# (see patch.h)
_05A : DEFINES += -DMAKE_05ALIMIT
_10A : DEFINES += -DMAKE_10ALIMIT

# WORKAROUNDS

NOT_A_CLEAN_BUILD  = -Werror
NOT_A_CLEAN_BUILD += -Wno-error=discarded-qualifiers
NOT_A_CLEAN_BUILD += -Wno-error=incompatible-pointer-types

# warning only valid for C (not C++)
NOT_A_CLEAN_BUILD_C_ONLY += -Wno-error=implicit-function-declaration

ifeq (0,1) ###FYI###
/* MISSING.H - missing function declarations */
#define __HAL_RCC_ADC_FORCE_RESET  __HAL_RCC_ADC1_FORCE_RESET
#define __HAL_RCC_ADC_RELEASE_RESET  __HAL_RCC_ADC1_RELEASE_RESET
#ifdef __cplusplus
extern "C" {
#endif
void setup(void);
void loop(void);
void BSP_MotorControlBoard_GpioInit(unsigned char nbDevices);
#ifdef __cplusplus
}
#endif
endif

# ALSO, bug/typo in {TOP}/STM32F0xx_mpmd/stm32f0xx_mpmd.h -- workaround
# for the missing include file. (see configuration_STM.h rule, below)


# DIRECTORY ABBREVIATIONS

# use the path to the Makefile as root 
ZD := $(dir $(realpath $(lastword $(MAKEFILE_LIST))))

BUILD = ${ZD}build

TOP = ${ZD}Marlin4MPMD-1.3.3/MPMD_3dPrinter
HAL = ${TOP}/HAL_Driver
USB = ${TOP}/Middlewares/ST/STM32_USB_Device_Library
BSP = ${TOP}/Drivers/BSP
FFS = ${TOP}/FatFs/src
PRJ = ${TOP}/Marlin


# SOURCE FILES

LINKER_LD = ${TOP}/LinkerScript.ld

SOURCES = \
	${TOP}/startup/startup_stm32.s \
	$(wildcard ${HAL}/Src/*.c) \
	$(wildcard ${USB}/Core/Src/*.c) \
	$(wildcard ${USB}/Class/CDC/Src/*.c) \
	$(wildcard ${BSP}/STM32F0xx-3dPrinter/*.c) \
	$(wildcard ${FFS}/*.c) \
	${FFS}/drivers/sd_diskio.c \
	${FFS}/option/syscall.c \
	${FFS}/option/unicode.c \
	$(wildcard ${TOP}/src/*.c) \
	$(wildcard ${TOP}/STM32F0xx_mpmd/*.c) \
	$(wildcard ${PRJ}/*.cpp) \
	${PRJ}/binGcode/binGcodeCommand.cpp \
	${PRJ}/binGcode/binGcodePar.cpp

EXCLUDE = $(wildcard ${HAL}/Src/*_template.c)


# TARGET LISTS

PROJ = ${PROJECT}-${VERSION}
_05A = ${PROJECT}-${VERSION}-05Alimit
_10A = ${PROJECT}-${VERSION}-10Alimit

SRCS = $(filter-out $(EXCLUDE),$(SOURCES))
OBJS = $(addsuffix .o,$(basename $(notdir $(SRCS))))
DEPS = $(OBJS:.o=.d)


# SOURCE FILE SEARCH PATHS

VPATH = $(sort $(dir $(SRCS)))


# INCLUDE FILE SEARCH PATHS

INCLUDE = \
	-include ${ZD}patch.h -I${TOP}/Marlin \
	-I${TOP}/inc \
	-I${TOP}/Marlin/binGcode \
	-I${TOP}/uzLib \
	-I${TOP}/FatFs/src/drivers \
	-I${TOP}/FatFs/src/option \
	-I${TOP}/FatFs/src \
	-I${TOP}/Middlewares/ST/STM32_USB_Device_Library/Core/Inc \
	-I${TOP}/Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc \
	-I${TOP}/Drivers/BSP/Components/A4985 \
	-I${TOP}/Drivers/BSP/Components/l6474 \
	-I${TOP}/Drivers/BSP/Components/Common \
	-I${TOP}/STM32F0xx_mpmd \
	-I${TOP}/HAL_Driver/Inc \
	-I${TOP}/HAL_Driver \
	-I${TOP}/CMSIS/device \
	-I${TOP}/CMSIS/core \
	-I${TOP}/Drivers/BSP/MotorControl \
	-I${TOP}/HAL_Driver/Inc/Legacy \
	-I${TOP}/Drivers/BSP/STM32F0xx-3dPrinter \
	-I${TOP}/Drivers/BSP/NUCLEO-F070RB-3dPrinter \
	-I${TOP}/Drivers/BSP/MPMD-3dPrinter \
	-I${BUILD}


# BUILD FLAGS

LDFLAGS += -L${TOP}/CMSIS
LDLIBS  += -larm_cortexM0l_math

CPPFLAGS += $(DEFINES) $(NOT_A_CLEAN_BUILD)
CPPFLAGS += -fsingle-precision-constant -fmerge-all-constants 
__CFLAGS  = -Os -fdata-sections -ffunction-sections -flto $(INCLUDE)
CFLAGS   += $(NOT_A_CLEAN_BUILD_C_ONLY) $(__CFLAGS)
CXXFLAGS += $(__CFLAGS) -fno-exceptions -fno-rtti
LDFLAGS  += -specs=nano.specs -u _printf_float -Wl,--gc-sections -flto


# BUILD TOOLS

CXX = arm-none-eabi-g++ -mcpu=cortex-m0 -mthumb -mfloat-abi=soft
CC  = arm-none-eabi-gcc -mcpu=cortex-m0 -mthumb -mfloat-abi=soft
AS  = arm-none-eabi-as  -mcpu=cortex-m0 -mthumb -mfloat-abi=soft
AR  = arm-none-eabi-ar

OBJCOPY = arm-none-eabi-objcopy
BINSIZE = arm-none-eabi-size


# MAKE RULES

.PHONY : one all clean realclean distclean depends PROJECT _05A _10A

one :
ifeq (,$(realpath ${BUILD}))
#	# create the build directory, if necessary
	mkdir ${BUILD}
endif
ifneq ($(words $(DEPS)),$(words $(wildcard ${BUILD}/*.d)))
#	# something's missing, re-create dependencies
	$(MAKE) -C ${BUILD} -f ${ZD}Makefile depends
endif
	$(MAKE) -C ${BUILD} -f ${ZD}Makefile PROJECT

all :   distclean realclean
	mkdir ${BUILD}
	$(MAKE) -C ${BUILD} -f ${ZD}Makefile depends
	$(MAKE) -C ${BUILD} -f ${ZD}Makefile _05A
	$(MAKE) -C ${BUILD} -f ${ZD}Makefile clean
	$(MAKE) -C ${BUILD} -f ${ZD}Makefile depends
	$(MAKE) -C ${BUILD} -f ${ZD}Makefile _10A
	rm -fR ${BUILD} *.MAP *.map

distclean : clean
	rm -fR ${BUILD}

depends : configuration_STM.h $(DEPS)

# workaround for an errant include file reference
configuration_STM.h :
	echo "#include \"Configuration_STM.h\"" >$@

_05A : ${_05A}.map ${_05A}.bin ${_05A}.MAP
	@cp -u $^ ${ZD} && cat $<

_10A : ${_10A}.map ${_10A}.bin ${_10A}.MAP
	@cp -u $^ ${ZD} && cat $<

PROJECT : ${PROJ}.map ${PROJ}.bin ${PROJ}.MAP
	@cp -u $^ ${ZD} && cat $<

%.elf : ${LINKER_LD} $(OBJS)
	$(CXX) $(LDFLAGS) -Wl,-Map=$(@:.elf=.MAP) -o $@ -T$^ $(LDLIBS)

%.map : %.elf
	$(BINSIZE) -A -B $< >$@

%.bin : %.elf
	$(OBJCOPY) -O binary $< $@

%.MAP : %.elf

realclean : clean
	rm -f *.elf *.MAP *.map *.bin

clean :
	rm -f *~ *.o *.d *.a


# AUTOMATIC PREREQUISITES
# ignore this stuff if our target is clean, realclean, or distclean
ifeq (,$(findstring ${MAKECMDGOALS},clean realclean distclean)) 

%.d : %.s
	echo "$(@:.d=.o) $@: $<" >$@                

%.d : %.c
	$(CC) -MM -MF $@ -MT '$(@:.d=.o) $@' $(CPPFLAGS) $(CFLAGS) $<

%.d : %.cpp
	$(CXX) -MM -MF $@ -MT '$(@:.d=.o) $@' $(CPPFLAGS) $(CXXFLAGS) $<

ifeq (${MAKECMDGOALS},depends)

define depends_rule =
$(basename $(notdir $(1))).d : $(1)
endef

# for depends, automatically create a rule for each source file
$(foreach f,$(SRCS),$(eval $(call depends_rule,$(f))))

else

# otherwise, include our rule for each source file (if it exists)
include $(notdir $(realpath $(DEPS)))

endif
endif
