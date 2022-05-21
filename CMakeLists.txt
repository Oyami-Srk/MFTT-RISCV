# Project MyFirstTouchToRISCV
# If I could run a hello world at today (4/18) this project may be continue
CMAKE_MINIMUM_REQUIRED(VERSION 3.0.0)

SET(CMAKE_SYSTEM_NAME Generic)
SET(CMAKE_SYSTEM_PROCESSOR RV64)
SET(CMAKE_EXECUTABLE_SUFFIX ".elf")

# Find cross compiler for RISCV64
FIND_PROGRAM(RISCV_C_COMPILER "riscv64-elf-gcc")

if (EXISTS "${RISCV_C_COMPILER}")
    SET(RISCV_TOOLCHAIN_PREFIX "riscv64-elf-")
ELSE ()
    MESSAGE(FATAL_ERROR "Cannot found RISC-V Toolchain.")
ENDIF ()

MESSAGE("Found RISC-V Toolchain prefix: ${RISCV_TOOLCHAIN_PREFIX}")

SET(CMAKE_ASM_COMPILER ${RISCV_TOOLCHAIN_PREFIX}gcc)
SET(CMAKE_C_COMPILER ${RISCV_TOOLCHAIN_PREFIX}gcc)
SET(CMAKE_CXX_COMPILER ${RISCV_TOOLCHAIN_PREFIX}g++)
SET(CMAKE_AR ${RISCV_TOOLCHAIN_PREFIX}ar)
SET(CMAKE_RANLIB ${RISCV_TOOLCHAIN_PREFIX}ranlib)
SET(CMAKE_OBJCOPY ${RISCV_TOOLCHAIN_PREFIX}objcopy)
SET(CMAKE_OBJDUMP ${RISCV_TOOLCHAIN_PREFIX}objdump)
# Disable compiler check for cross-compiler.
SET(CMAKE_C_COMPILER_WORKS TRUE)
SET(CMAKE_CXX_COMPILER_WORKS TRUE)
SET(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
# Enable asm
ENABLE_LANGUAGE(ASM)

SET(CMAKE_C_FLAGS "-fno-pic -nostdinc -static -fno-builtin -fno-strict-aliasing -fno-stack-protector -g -nostdlib -mcmodel=medany")

PROJECT(MyFirstTouchToRISCV)
#SET(CMAKE_VERBOSE_MAKEFILE ON CACHE BOOL "ON")
SET(IMG_UTILS ${PROJECT_SOURCE_DIR}/tools/scripts/img_utils)
SET(DISK_IMAGE fs.img)

FILE(GLOB_RECURSE ASM_SRCS "kernel/*/*.S")
FILE(GLOB_RECURSE C_SRCS "kernel/*/*.c")

FOREACH (SRC IN ITEMS ${ASM_SRCS})
    SET_SOURCE_FILES_PROPERTIES(${SRC} PROPERTIES COMPILE_DEFINITIONS ASM_FILE)
ENDFOREACH ()

SET(SRCS ${ASM_SRCS} ${C_SRCS})
INCLUDE_DIRECTORIES(SYSTEM header)

# Generate kernel.elf
ADD_EXECUTABLE(kernel.elf ${SRCS})
TARGET_INCLUDE_DIRECTORIES(kernel.elf SYSTEM PRIVATE "kernel/header")
SET_TARGET_PROPERTIES(kernel.elf PROPERTIES LINK_FLAGS "-N -T${CMAKE_SOURCE_DIR}/kernel/kernel.ld ")
TARGET_LINK_LIBRARIES(kernel.elf init_code)
ADD_DEPENDENCIES(kernel.elf init_code)
ADD_CUSTOM_COMMAND(TARGET kernel.elf
        POST_BUILD
        COMMAND ${CMAKE_OBJDUMP} -S ${CMAKE_BINARY_DIR}/kernel.elf > ${CMAKE_BINARY_DIR}/kernel.disasm)
# End of kernel.elf

SET(USER_PROG_COMPILE_OPTIONS "-Wall;-Werror;-O;-fno-omit-frame-pointer;"
        "-ggdb;-g;-MD;-mcmodel=medany;-ffreestanding;-fno-common;-nostdlib;"
        ";-mno-relax;-fno-stack-protector")
# TODO: use _start and cutom crt0 as entry.
SET(USER_PROG_LINK_OPTIONS "-z;max-page-size=4096;-N;-e;main;-Ttext;0")

# Generate static userlib
FILE(GLOB_RECURSE ULIB_SRCS "userlib/*.c")
ADD_LIBRARY(user STATIC ${ULIB_SRCS})
TARGET_INCLUDE_DIRECTORIES(user PUBLIC "userlib/header")
TARGET_COMPILE_OPTIONS(user INTERFACE ${USER_PROG_COMPILE_OPTIONS})
TARGET_LINK_OPTIONS(user INTERFACE ${USER_PROG_LINK_OPTIONS})
# End of userlib


# Init code
FILE(GLOB_RECURSE INIT_SRCS "init/*.c")
ADD_EXECUTABLE(init.elf ${INIT_SRCS})
TARGET_LINK_LIBRARIES(init.elf user)

ADD_LIBRARY(init_code STATIC init/package.S)
SET_SOURCE_FILES_PROPERTIES(init/package.S PROPERTIES LANGUAGE ASM)
SET_SOURCE_FILES_PROPERTIES(init/package.S OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/init.elf)
TARGET_COMPILE_DEFINITIONS(init_code PRIVATE INIT_ELF="${CMAKE_BINARY_DIR}/init.elf")
# End of init

# User Prog
ADD_EXECUTABLE(prog1 progs/prog1.c)
TARGET_LINK_LIBRARIES(prog1 user)
SET_TARGET_PROPERTIES(prog1 PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/userprogs")
SET(USER_PROGS prog1)
# End of user prog

# Generate HD.img
IF ("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Linux")
    FIND_PROGRAM(UDISKSCTL "udisksctl")
    FIND_PROGRAM(RSYNC "rsync")
    IF (UDISKSCTL AND RSYNC)
        SET(IMG "${CMAKE_BINARY_DIR}/${DISK_IMAGE}")

        ADD_CUSTOM_COMMAND(OUTPUT ${DISK_IMAGE}
                COMMAND ${IMG_UTILS} attach ${IMG}
                COMMAND ${IMG_UTILS} sync ${CMAKE_BINARY_DIR}/userprogs/ ${IMG} .
                COMMAND ${IMG_UTILS} detach ${IMG}
                WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
                )

        ADD_CUSTOM_TARGET(generate_img ALL DEPENDS ${DISK_IMAGE})
        ADD_DEPENDENCIES(kernel.elf generate_img)

        ADD_CUSTOM_COMMAND(OUTPUT ${DISK_IMAGE}
                DEPENDS ${USER_PROGS}
                APPEND
                )
    ELSE ()
        MESSAGE(WARNING "Generate disk image require udisks2 (udisksctl command) and rsync.")
    ENDIF ()
ELSE ()
    MESSAGE(WARNING "Generate disk image is only support under Linux platform")
ENDIF ()

# End of HD.img



