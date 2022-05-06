# Project MyFirstTouchToRISCV
# If I could run a hello world at today (4/18) this project may be continue
CMAKE_MINIMUM_REQUIRED(VERSION 3.0.0)

SET(CMAKE_SYSTEM_NAME Generic)
SET(CMAKE_SYSTEM_PROCESSOR RV64)
SET(CMAKE_EXECUTABLE_SUFFIX ".elf")

# Find cross compiler for RISCV64
FIND_PROGRAM(RISCV_C_COMPILER "riscv64-elf-gcc")
message("What: ${RISCV_C_COMPILER}")

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
SET(CMAKE_VERBOSE_MAKEFILE ON)

FILE(GLOB_RECURSE ASM_SRCS "kernel/*/*.S")
FILE(GLOB_RECURSE C_SRCS "kernel/*/*.c")

FOREACH (SRC IN ITEMS ${ASM_SRCS})
    SET_SOURCE_FILES_PROPERTIES(${SRC} PROPERTIES COMPILE_DEFINITIONS ASM_FILE)
ENDFOREACH ()

SET(SRCS ${ASM_SRCS} ${C_SRCS})

# Generate kernel.elf
ADD_EXECUTABLE(kernel.elf ${SRCS})
TARGET_INCLUDE_DIRECTORIES(kernel.elf PRIVATE "kernel/header")
SET_TARGET_PROPERTIES(kernel.elf PROPERTIES LINK_FLAGS "-N -T${CMAKE_SOURCE_DIR}/kernel/kernel.ld ")
TARGET_LINK_LIBRARIES(kernel.elf init_code)
ADD_CUSTOM_COMMAND(TARGET kernel.elf
        POST_BUILD
        COMMAND ${CMAKE_OBJDUMP} -S ${CMAKE_BINARY_DIR}/kernel.elf > ${CMAKE_BINARY_DIR}/kernel.disasm)
# End of kernel.elf

# Generate static userlib
FILE(GLOB_RECURSE ULIB_SRCS "userlib/*.c")
ADD_LIBRARY(user STATIC ${ULIB_SRCS})
TARGET_INCLUDE_DIRECTORIES(user PUBLIC "userlib/header")
# End of userlib

# Init code
FILE(GLOB_RECURSE INIT_SRCS "init/*.c")
ADD_EXECUTABLE(init.elf ${INIT_SRCS})
TARGET_COMPILE_OPTIONS(init.elf PRIVATE -Wall -Werror -O -fno-omit-frame-pointer
        -ggdb -g -MD -mcmodel=medany -ffreestanding -fno-common -nostdlib
        -mno-relax -fno-stack-protector)
TARGET_LINK_OPTIONS(init.elf PRIVATE -z max-page-size=4096 -N -e main -Ttext 0)
TARGET_LINK_LIBRARIES(init.elf user)
ADD_LIBRARY(init_code OBJECT init/package.S)
SET_SOURCE_FILES_PROPERTIES(init/package.S PROPERTIES LANGUAGE ASM)
TARGET_COMPILE_DEFINITIONS(init_code PRIVATE INIT_ELF="${CMAKE_BINARY_DIR}/init.elf")
ADD_DEPENDENCIES(init_code init.elf)
# End of init
