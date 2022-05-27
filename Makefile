# GNU Makefile
# This Makefile is only for competition on os.educg.net
# What a pity that they don't support cmake
# This Makefile is not meant to be maintance

OS_PLATFORM:=k210

ifeq ($(OS_PLATFORM), qemu)
	BIN_OFFSET:=2097152
else
	BIN_OFFSET:=131072
endif

PROJ_ROOT:=$(shell pwd)
BUILD_DIR:=${PROJ_ROOT}/build
TOOLCHAIN_PREFIX:=riscv64-unknown-elf-

CC:=${TOOLCHAIN_PREFIX}gcc
LD:=${TOOLCHAIN_PREFIX}ld
AR:=${TOOLCHAIN_PREFIX}ar
OBJCOPY:=${TOOLCHAIN_PREFIX}objcopy
OBJDUMP:=${TOOLCHAIN_PREFIX}objdump

KERNEL_HEADER:=${PROJ_ROOT}/kernel/header/
USER_HEADER:=${PROJ_ROOT}/userlib/header/
COMMON_HEADER:=${PROJ_ROOT}/header/

CC_FLAGS_GENERIC:= -fno-pic -nostdinc -static -fno-builtin -fno-strict-aliasing -fno-stack-protector -g -nostdlib -mcmodel=medany -Wstack-usage=8192 -fstack-usage
CC_FLAGS_KERNEL:= ${CC_FLAGS_GENERIC} -isystem ${KERNEL_HEADER} -isystem ${COMMON_HEADER}
CC_FLAGS_USER:= -Wall -Werror -O -fno-omit-frame-pointer -ggdb -g -MD -mcmodel=medany -ffreestanding -fno-common -nostdlib -mno-relax -fno-stack-protector -isystem ${USER_HEADER} -isystem ${COMMON_HEADER}

LD_FLAGS_KERNEL:= -N -T${PROJ_ROOT}/kernel/kernel-${OS_PLATFORM}.ld
LD_FLAGS_USER:= -z max-page-size=4096 -N -e main -Ttext 0

ifeq ($(OS_PLATFORM), qemu)
	CC_FLAGS_KERNEL += -DPLATFORM_QEMU
else
	CC_FLAGS_KERNEL += -DPLATFORM_K210
endif


.PHONY: all
all: prerequirement os.bin

.PHONY: prerequirement
prerequirement:
	mkdir -p ${BUILD_DIR}

.PHONY: clean
clean:
	rm -rf ${BUILD_DIR}
	rm -f os.bin

USERLIB_BUILD_DIR:=${BUILD_DIR}/userlib
KERNEL_BUILD_DIR:=${BUILD_DIR}/kernel
INIT_BUILD_DIR:=${BUILD_DIR}/init

# Kernel builds

KERNEL_ROOT:=${PROJ_ROOT}/kernel
KERNEL_C_SRCS:=$(shell find ${KERNEL_ROOT} -type f -name '*.c')
KERNEL_ASM_SRCS:=$(shell find ${KERNEL_ROOT} -type f -name '*.S')
KERNEL_SRCS:= ${KERNEL_ASM_SRCS} ${KERNEL_C_SRCS}
KERNEL_OBJS:=$(patsubst %, ${KERNEL_BUILD_DIR}/%.o, $(subst ${KERNEL_ROOT}/,,$(KERNEL_SRCS)))

${KERNEL_BUILD_DIR}/%.c.o: ${KERNEL_ROOT}/%.c
	@mkdir -p $(dir $@)
	@echo "Build $@"
	${CC} ${CC_FLAGS_KERNEL} -o $@ -c $<

${KERNEL_BUILD_DIR}/%.S.o: ${KERNEL_ROOT}/%.S
	@mkdir -p $(dir $@)
	@echo "Build Asmssembly $@"
	${CC} ${CC_FLAGS_KERNEL} -DASM_FILE -o $@ -c $<

${BUILD_DIR}/kernel.elf: ${BUILD_DIR}/init.o ${KERNEL_OBJS}
	@mkdir -p $(dir $@)
	@echo "Build kernel.elf"
	${LD} ${LD_FLAGS_KERNEL} -o $@ $^

# Userlib builds
USERLIB_ROOT:=${PROJ_ROOT}/userlib
USERLIB_SRCS:=$(shell find ${USERLIB_ROOT} -type f -name '*.c')
USERLIB_OBJS:=$(patsubst %, ${USERLIB_BUILD_DIR}/%.o, $(subst ${USERLIB_ROOT}/,,$(USERLIB_SRCS)))

${USERLIB_BUILD_DIR}/%.c.o: ${USERLIB_ROOT}/%.c
	@mkdir -p $(dir $@)
	@echo "Build $@"
	${CC} ${CC_FLAGS_GENERIC} -isystem ${COMMON_HEADER} -isystem ${USER_HEADER} -o $@ -c $<

${BUILD_DIR}/libuser.a: ${USERLIB_OBJS}
	@mkdir -p $(dir $@)
	@echo "Build libuser.a"
	${AR} -rcv $@ $^

# Init code builds

INIT_ROOT:=${PROJ_ROOT}/init
INIT_SRCS:=$(shell find ${INIT_ROOT} -type f -name '*.c')
INIT_OBJS:=$(patsubst %, ${INIT_BUILD_DIR}/%.o, $(subst ${INIT_ROOT}/,,$(INIT_SRCS)))

${INIT_BUILD_DIR}/%.c.o: ${INIT_ROOT}/%.c
	@mkdir -p $(dir $@)
	@echo "Build $@"
	${CC} ${CC_FLAGS_USER} -o $@ -c $<

${BUILD_DIR}/init.elf: ${BUILD_DIR}/libuser.a ${INIT_OBJS}
	@mkdir -p $(dir $@)
	@echo "Build init.elf"
	${LD} ${LD_FLAGS_USER} -o $@ ${INIT_OBJS} ${BUILD_DIR}/libuser.a

${BUILD_DIR}/init.o: ${BUILD_DIR}/init.elf ${INIT_ROOT}/package.S
	@mkdir -p $(dir $@)
	@echo "Build init.o"
	${CC} ${CC_FLAGS_GENERIC} -DINIT_ELF=\"${BUILD_DIR}/init.elf\" -o $@ -c ${INIT_ROOT}/package.S

# SBI Build

${BUILD_DIR}/SBI.elf:
	cd ${PROJ_ROOT}/SBI/${OS_PLATFORM}/rustsbi-${OS_PLATFORM}/ && \
		cargo make --release && \
		cp target/riscv64imac-unknown-none-elf/release/rustsbi-${OS_PLATFORM} \
		$@
	cd ${PROJ_ROOT}

${BUILD_DIR}/%.bin: ${BUILD_DIR}/%.elf
	${OBJCOPY} $< --strip-all -O binary $@

os.bin: ${BUILD_DIR}/kernel.bin ${BUILD_DIR}/SBI.bin
	rm -f $@
	dd conv=notrunc bs=1 if=${BUILD_DIR}/SBI.bin of=$@
	dd conv=notrunc bs=1 if=${BUILD_DIR}/kernel.bin of=$@ seek=${BIN_OFFSET}

