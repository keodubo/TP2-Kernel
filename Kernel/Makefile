# include Makefile.inc

# KERNEL=kernel.bin
# SOURCES=$(wildcard *.c)
# SOURCES_ASM=$(wildcard asm/*.asm)
# OBJECTS=$(SOURCES:.c=.o)
# OBJECTS_ASM=$(SOURCES_ASM:.asm=.o)
# LOADERSRC=loader.asm

# LOADEROBJECT=$(LOADERSRC:.asm=.o)
# STATICLIBS=

# all: $(KERNEL)

# $(KERNEL): $(LOADEROBJECT) $(OBJECTS) $(STATICLIBS) $(OBJECTS_ASM)
# 	$(LD) $(LDFLAGS) -T kernel.ld -o $(KERNEL) $(LOADEROBJECT) $(OBJECTS) $(OBJECTS_ASM) $(STATICLIBS)

# %.o: %.c
# 	$(GCC) $(GCCFLAGS) -I./include -c $< -o $@

# %.o : %.asm
# 	$(ASM) $(ASMFLAGS) $< -o $@

# $(LOADEROBJECT):
# 	$(ASM) $(ASMFLAGS) $(LOADERSRC) -o $(LOADEROBJECT)

# clean:
# 	rm -rf asm/*.o *.o *.bin

# .PHONY: all clean

include Makefile.inc

KERNEL=kernel.bin
KERNEL_ELF=kernel.elf
SOURCES=$(wildcard *.c)
SOURCES_INTERRUPT=$(wildcard interrupt/*.c)
SOURCES_DRIVERS=$(wildcard drivers/*.c)
SOURCES_MM=$(wildcard mm/*.c)
SOURCES_IPC=$(wildcard ipc/*.c)
SOURCES_PROCESS=$(wildcard process/*.c)
SOURCES_FS=$(wildcard fs/*.c)
SOURCES_LIB=$(wildcard lib/*.c)
SOURCES_SYS=$(wildcard sys/*.c)
SOURCES_ASM=$(wildcard asm/*.asm)

OBJECTS=$(SOURCES:.c=.o)
OBJECTS_INTERRUPT=$(SOURCES_INTERRUPT:.c=.o)
OBJECTS_DRIVERS=$(SOURCES_DRIVERS:.c=.o)
OBJECTS_MM=$(SOURCES_MM:.c=.o)
OBJECTS_IPC=$(SOURCES_IPC:.c=.o)
OBJECTS_PROCESS=$(SOURCES_PROCESS:.c=.o)
OBJECTS_FS=$(SOURCES_FS:.c=.o)
OBJECTS_LIB=$(SOURCES_LIB:.c=.o)
OBJECTS_SYS=$(SOURCES_SYS:.c=.o)
OBJECTS_ASM=$(SOURCES_ASM:.asm=.o)

LOADERSRC=loader.asm

LOADEROBJECT=$(LOADERSRC:.asm=.o)
STATICLIBS=
ALL_OBJECTS= $(LOADEROBJECT) $(OBJECTS) $(OBJECTS_ASM) $(OBJECTS_INTERRUPT) $(OBJECTS_DRIVERS) $(OBJECTS_MM) $(OBJECTS_IPC) $(OBJECTS_PROCESS) $(OBJECTS_FS) $(OBJECTS_LIB) $(OBJECTS_SYS)

# Seleccion del memory manager
# Por defecto usa First Fit, se puede sobrescribir con MM_FLAG
MM_FLAG ?=

# Agregar el flag al compilador si esta definido
ifneq ($(MM_FLAG),)
	GCCFLAGS += $(MM_FLAG)
endif

ifneq ($(findstring USE_BUDDY_SYSTEM,$(MM_FLAG)),)
	GCCFLAGS += -DCONFIG_MM_BUDDY
endif

all: $(KERNEL) $(KERNEL_ELF)

$(KERNEL): $(STATICLIBS) $(ALL_OBJECTS)
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

$(KERNEL_ELF): $(STATICLIBS) $(ALL_OBJECTS)
	$(LD) $(LDFLAGS) -T kernel.ld --oformat=elf64-x86-64 -o $@ $^

%.o: %.c
	$(GCC) $(GCCFLAGS) -I./include -I./interruptions -c $< -o $@
%.o : %.asm
	$(ASM) $(ASMFLAGS) $< -o $@

$(LOADEROBJECT):
	$(ASM) $(ASMFLAGS) $(LOADERSRC) -o $(LOADEROBJECT)

clean:
	rm -rf $(ALL_OBJECTS) *.bin *.elf

# Target para compilar con Buddy System
buddy:
	@echo "Compilando con Buddy System..."
	$(MAKE) MM_FLAG=-DUSE_BUDDY_SYSTEM all

.PHONY: all clean buddy
