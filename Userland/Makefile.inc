GCC=x86_64-linux-gnu-gcc
GPP=x86_64-linux-gnu-g++
LD=x86_64-linux-gnu-ld
AR=x86_64-linux-gnu-ar
ASM=nasm

# Seleccion del memory manager (heredado del Kernel si esta definido)
MM_FLAG ?=

GCCFLAGS=-m64 -fno-exceptions -std=c99 -Wall -Wextra -Werror -ffreestanding -nostdlib -fno-common -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -fno-builtin-malloc -fno-builtin-free -fno-builtin-realloc -g $(MM_FLAG)
ARFLAGS=rvs
ASMFLAGS=-felf64
LDFLAGS=--warn-common -z max-page-size=0x1000
