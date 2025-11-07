include ../Makefile.inc

MODULE=0000-sampleCodeModule.bin
MODULE_ELF=0000-sampleCodeModule.elf
SOURCES=$(wildcard [^_]*.c)
SOURCES_TESTS=$(wildcard tests/*.c)
SOURCES_SHELL=$(wildcard shell/*.c)
SOURCES_PROGRAMS=$(wildcard programs/*.c)
SOURCES_LIB=$(wildcard lib/*.c)
SOURCES_ASM=$(wildcard asm/*.asm)
OBJECTS_ASM=$(SOURCES_ASM:asm/%.asm=obj/%.asm.o)

all: $(MODULE) $(MODULE_ELF)

$(MODULE): $(SOURCES) $(SOURCES_TESTS) $(SOURCES_SHELL) $(SOURCES_PROGRAMS) $(SOURCES_LIB) $(OBJECTS_ASM)
	$(GCC) $(GCCFLAGS) -I./include -I../include -I./tests -I./shell -I./programs -I./lib -T sampleCodeModule.ld _loader.c $(OBJECTS_ASM) $(SOURCES) $(SOURCES_TESTS) $(SOURCES_SHELL) $(SOURCES_PROGRAMS) $(SOURCES_LIB) -o ../$(MODULE)

$(MODULE_ELF): $(SOURCES) $(SOURCES_TESTS) $(SOURCES_SHELL) $(SOURCES_PROGRAMS) $(SOURCES_LIB) $(OBJECTS_ASM)
	$(GCC) $(GCCFLAGS) -I./include -I../include -I./tests -I./shell -I./programs -I./lib -T sampleCodeModule.ld -Wl,--oformat=elf64-x86-64 _loader.c $(OBJECTS_ASM) $(SOURCES) $(SOURCES_TESTS) $(SOURCES_SHELL) $(SOURCES_PROGRAMS) $(SOURCES_LIB) -o ../$(MODULE_ELF)

obj/%.asm.o : asm/%.asm
	mkdir -p obj
	$(ASM) $(ASMFLAGS) $< -o $@

clean:
	rm -rf *.o
	rm -rf obj
	rm -rf ../$(MODULE) ../$(MODULE_ELF)

.PHONY: all clean print
