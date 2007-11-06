SUFFIXES = .asm

noinst_LIBRARIES = lib386.a

lib386_a_SOURCES = 2xSaImmx.asm

.asm.o:
	$(NASM) -f elf -o $@ $<
