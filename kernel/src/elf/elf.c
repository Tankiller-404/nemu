#include "common.h"
#include "memory.h"
#include <string.h>
#include <elf.h>

#define ELF_OFFSET_IN_DISK 0

#ifdef HAS_DEVICE
void ide_read(uint8_t *, uint32_t, uint32_t);
#else
void ramdisk_read(uint8_t *, uint32_t, uint32_t);
#endif

#define STACK_SIZE (1 << 20)

void create_video_mapping();
uint32_t get_ucr3();

uint32_t loader() {
	Elf32_Ehdr *elf;
	Elf32_Phdr ph;
	int i;

	uint8_t buf[4096];

#ifdef HAS_DEVICE
	ide_read(buf, ELF_OFFSET_IN_DISK, 4096);
#else
	ramdisk_read(buf, ELF_OFFSET_IN_DISK, 4096);
#endif

	elf = (void*)buf;

	const uint32_t elf_magic = 0x464c457f;
	uint32_t *p_magic = (void *)buf;
	nemu_assert(*p_magic == elf_magic);
	nemu_assert(elf->e_phentsize == sizeof(Elf32_Phdr));

	/* Load each program segment */
	for(i = 0; i < elf->e_phnum; i ++) {
		uint32_t offset = elf->e_phoff + i * elf->e_phentsize;
#ifdef HAS_DEVICE
		ide_read((uint8_t *)&ph, ELF_OFFSET_IN_DISK + offset, sizeof(ph));
#else
		ramdisk_read((uint8_t *)&ph, ELF_OFFSET_IN_DISK + offset, sizeof(ph));
#endif

		/* Scan the program header table, load each segment into memory */
		if(ph.p_type == PT_LOAD) {
			uint8_t *segment = (uint8_t *)ph.p_vaddr;
			nemu_assert(ph.p_filesz <= ph.p_memsz);

			#ifdef HAS_DEVICE
			ide_read(segment, ELF_OFFSET_IN_DISK + ph.p_offset, ph.p_filesz);
			#else
			ramdisk_read(segment, ELF_OFFSET_IN_DISK + ph.p_offset, ph.p_filesz);
			#endif
			memset(segment + ph.p_filesz, 0, ph.p_memsz - ph.p_filesz);


#ifdef IA32_PAGE
			/* Record the program break for future use. */
			extern uint32_t cur_brk, max_brk;
			uint32_t new_brk = ph.p_vaddr + ph.p_memsz - 1;
			if(cur_brk < new_brk) { max_brk = cur_brk = new_brk; }
#endif
		}
	}

	volatile uint32_t entry = elf->e_entry;

#ifdef IA32_PAGE
	mm_malloc(KOFFSET - STACK_SIZE, STACK_SIZE);

#ifdef HAS_DEVICE
	create_video_mapping();
#endif

	write_cr3(get_ucr3());
#endif

	return entry;
}
