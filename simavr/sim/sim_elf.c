/*
	sim_elf.c

	Loads a .elf file, extract the code, the data, the eeprom and
	the "mcu" specification section, also load usable code symbols
	to be able to print meaningful trace information.

	Copyright 2008, 2009 Michel Pollet <buserror@gmail.com>

 	This file is part of simavr.

	simavr is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	simavr is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with simavr.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#ifdef HAVE_LIBELF
#include <libelf.h>
#include <gelf.h>
#else
#undef ELF_SYMBOLS
#define ELF_SYMBOLS 0
#endif

#include "sim_elf.h"
#include "sim_vcd_file.h"
#include "avr_eeprom.h"
#include "avr_ioport.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif

/* Section names for log messages. */

static const char * const mem_image[] = { "Flash", "Data", "EEPROM", "Fuses",
										  "Lock", "Unknown" };

#if CONFIG_SIMAVR_TRACE && ELF_SYMBOLS
// Put a symbol name in a table, preferring names without leadling '_'.

static void
elf_set_preferred(const char **sp, const char *new)
{
	if (*sp) {
		const char *prev;

		// Replace duplicates definitions beginning '_' and '__'.

		prev = *sp;
		if (prev[0] && (prev[0] != '_' || (new[0] == '_' && prev[1] != '_')))
			return;

		// Most of array already leaks - FIXME
		//free((void *)*sp);
	}
	*sp = new;
}

// "Spread" the pointers for known symbols forward.

static void
avr_spread_lines(const char **table, int count)
{
	const char * last = NULL;

	for (int i = 0; i < count; i++) {
		if (!table[i])
			table[i] = last;
		else
			last = table[i];
	}
}
#endif

static void do_chunk(fw_chunk_t *chunk, uint8_t *buf,
					 uint32_t limit, uint32_t *end)
{
	uint32_t size, csize, fsize;

	csize = chunk->size;
	fsize = chunk->fill_size;
	if (fsize < csize)
		csize = fsize;	// Unlikely.
	size = (fsize > csize) ? fsize : csize;
	if (chunk->addr + size > limit) {
		AVR_LOG(NULL, LOG_ERROR,
				"Loading %d bytes of %s data at %x would overflow "
				"buffer (%d bytes).\n",
				size, mem_image[chunk->type], chunk->addr, limit);
		if (chunk->addr >= limit)
			return;
		size = limit - chunk->addr;
	}
	if (csize > size)
		csize = size;
	if (csize) {
		memcpy(buf + chunk->addr, chunk->data, csize);
		fsize -= csize;
	}
	if (fsize > size)
		fsize = size;
	if (fsize)
		memset(buf + chunk->addr + csize, 0, fsize);
	AVR_LOG(NULL, LOG_DEBUG, "Loaded %d bytes of %s data at %#x\n",
			size, mem_image[chunk->type], chunk->addr);
	if (end && chunk->addr + size > *end)
		*end = chunk->addr + size;
}

void
avr_load_firmware(
		avr_t * avr,
		elf_firmware_t * firmware)
{
	fw_chunk_t *chunk, *ochunk;

	if (firmware->frequency)
		avr->frequency = firmware->frequency;
	if (firmware->vcc)
		avr->vcc = firmware->vcc;
	if (firmware->avcc)
		avr->avcc = firmware->avcc;
	if (firmware->aref)
		avr->aref = firmware->aref;
#if ELF_SYMBOLS
#if CONFIG_SIMAVR_TRACE
	/* Store the symbols read from the ELF file. */

	int           scount = avr->flashend >> 1;
	uint32_t      addr;
	const char ** table;

	// Allocate table of Flash address strings.

        table = calloc(scount, sizeof (char *));
	avr->trace_data->codeline = table;
	avr->trace_data->codeline_size = scount;

	// Re-allocate table of data address strings.

	if (firmware->highest_data_symbol >= avr->trace_data->data_names_size) {
		uint32_t new_size;

		new_size = firmware->highest_data_symbol + 1;
		avr->data_names = realloc(avr->data_names, new_size * sizeof (char *));
		memset(avr->data_names + avr->trace_data->data_names_size,
		       0,
		       (new_size - avr->trace_data->data_names_size) *
		           sizeof (char *));
		avr->trace_data->data_names_size = new_size;
	}

	for (int i = 0; i < firmware->symbolcount; i++) {
		const char **sp, *new;

		new = firmware->symbol[i]->symbol;
		addr = firmware->symbol[i]->addr;
		if (addr < avr->flashend) {
			// A code address.

			sp = &table[addr >> 1];
			elf_set_preferred(sp, new);
		} else if (addr >= AVR_SEGMENT_OFFSET_DATA &&
			   addr < AVR_SEGMENT_OFFSET_DATA +
                           	  avr->trace_data->data_names_size) {
			// Address in data space.

			addr -= AVR_SEGMENT_OFFSET_DATA;
			sp = &avr->data_names[addr];
			elf_set_preferred(sp, new);
		}
	}

	// Parse given ELF file for DWARF info.

	if (firmware->dwarf_file)
		avr_read_dwarf(avr, firmware->dwarf_file);
	free(firmware->dwarf_file);

	// Fill out the flash and data space name tables with duplicates.

	avr_spread_lines(table, scount);
	avr_spread_lines(avr->data_names + avr->ioend + 1,
			 avr->trace_data->data_names_size - (avr->ioend + 1));
#else
	// Parse given ELF file for DWARF info.

	if (firmware->dwarf_file)
		avr_read_dwarf(avr, firmware->dwarf_file);
	free(firmware->dwarf_file);
#endif
#endif // ELF_SYMBOLS

	/* Load. */

	for (chunk = firmware->chunks; chunk; ) {
		avr_eeprom_desc_t ed;

		if (chunk->type == UNKNOWN) {
			/* Classify chunk based on address and subtract base of region. */

			if (chunk->addr < AVR_SEGMENT_OFFSET_DATA) {
				/* Explicit flash section. Load it. */

				chunk->type = FLASH;
			} else if (chunk->addr < AVR_SEGMENT_OFFSET_EEPROM) {
				/* Data space.  Usually only for .bss, and that may be ignored.
				 * If there are initialised variables, they appear as
				 * extra initialised flash.  The C startup function
				 * understands that and will copy them to RAM, and also
				 * zeros .bss.  There might be some reason to load into
				 * data space ...
				 */

				chunk->type = DATA;
				chunk->addr -= AVR_SEGMENT_OFFSET_DATA;
			} else if (chunk->addr < AVR_SEGMENT_OFFSET_FUSES) {
				/* EEPROM. */

				chunk->type = EEPROM;
				chunk->addr -= AVR_SEGMENT_OFFSET_EEPROM;
			} else if (chunk->addr < AVR_SEGMENT_OFFSET_LOCK) {
				/* Fuses. */

				chunk->type = FUSES;
				chunk->addr -= AVR_SEGMENT_OFFSET_FUSES;
			} else if (chunk->addr < AVR_SEGMENT_OFFSET_LAST) {
				/* Lock bits. */

				chunk->type = LOCK;
				chunk->addr -= AVR_SEGMENT_OFFSET_LOCK;
			}
		}

		switch (chunk->type) {
		case FLASH:
			do_chunk(chunk, avr->flash, avr->flashend, &avr->codeend);
			break;
		case DATA:
			do_chunk(chunk, avr->data + avr->ioend, avr->ramend - avr->ioend,
					 NULL);
			break;
		case EEPROM:
			if (chunk->size) {
				ed.ee = chunk->data;
				ed.offset = chunk->addr;
				ed.size = chunk->size;
			}
			if (avr_ioctl(avr, AVR_IOCTL_EEPROM_SET, &ed) < 0) {
				AVR_LOG(avr, LOG_ERROR,
						"Failed to write %d bytes to EEPROM at offset %#x\n",
						chunk->size, chunk->addr);
			}
			break;
		case FUSES:
			do_chunk(chunk, avr->fuse, sizeof avr->fuse, NULL);
			break;
		case LOCK:
			do_chunk(chunk, &avr->lockbits, sizeof avr->lockbits, NULL);
			break;
		default:
			AVR_LOG(avr, LOG_ERROR, "Ignoring unknown (type %d) data chunk\n",
					chunk->type);
			break;
		}
		ochunk = chunk;
		chunk = chunk->next;
		free(ochunk);
	}
	// load the default pull up/down values for ports
	for (int i = 0; i < 8 && firmware->external_state[i].port; i++) {
		avr_ioport_external_t e = {
			.name = firmware->external_state[i].port,
			.mask = firmware->external_state[i].mask,
			.value = firmware->external_state[i].value,
		};
		avr_ioctl(avr, AVR_IOCTL_IOPORT_SET_EXTERNAL(e.name), &e);
	}
	avr_set_command_register(avr, firmware->command_register_addr);
	avr_set_console_register(avr, firmware->console_register_addr);

	// rest is initialization of the VCD file
	if (firmware->tracecount == 0)
		return;
	avr->vcd = malloc(sizeof(*avr->vcd));
	memset(avr->vcd, 0, sizeof(*avr->vcd));
	avr_vcd_init(avr,
		firmware->tracename[0] ? firmware->tracename: "gtkwave_trace.vcd",
		avr->vcd,
		firmware->traceperiod >= 1000 ? firmware->traceperiod : 1000);

	AVR_LOG(avr, LOG_TRACE, "ELF: Creating VCD trace file '%s'\n",
			avr->vcd->filename);

	for (int ti = 0; ti < firmware->tracecount; ti++) {
		if (firmware->trace[ti].kind == AVR_MMCU_TAG_VCD_PORTPIN) {
			avr_irq_t * irq = avr_io_getirq(avr,
					AVR_IOCTL_IOPORT_GETIRQ(firmware->trace[ti].mask),
					firmware->trace[ti].addr);
			if (irq) {
				char name[16];
				sprintf(name, "%c%d", firmware->trace[ti].mask,
						firmware->trace[ti].addr);
				avr_vcd_add_signal(avr->vcd, irq, 1,
					firmware->trace[ti].name[0] ?
						firmware->trace[ti].name : name);
			}
		} else if (firmware->trace[ti].kind == AVR_MMCU_TAG_VCD_IRQ) {
			avr_irq_t * bit = avr_get_interrupt_irq(avr, firmware->trace[ti].mask);
			if (bit && firmware->trace[ti].addr < AVR_INT_IRQ_COUNT)
				avr_vcd_add_signal(avr->vcd,
						&bit[firmware->trace[ti].addr],
						firmware->trace[ti].mask == 0xff ? 8 : 1,
						firmware->trace[ti].name);
		} else if (firmware->trace[ti].kind == AVR_MMCU_TAG_VCD_IO_IRQ) {
			avr_irq_t * irq = avr_io_getirq(avr, firmware->trace[ti].addr, firmware->trace[ti].mask);
			if (irq)
				avr_vcd_add_signal(avr->vcd, irq, 8, firmware->trace[ti].name);
		} else if ( (firmware->trace[ti].kind == AVR_MMCU_TAG_VCD_SRAM_8) ||
		    (firmware->trace[ti].kind == AVR_MMCU_TAG_VCD_SRAM_16) ) {
			if ((firmware->trace[ti].addr <= 31) || (firmware->trace[ti].addr > avr->ramend)) {
				AVR_LOG(avr, LOG_ERROR, "ELF: *** Invalid SRAM trace address (0x20 < 0x%04x < 0x%04x )\n", firmware->trace[ti].addr, avr->ramend);
			} else if (avr->sram_tracepoint_count >= ARRAY_SIZE(avr->sram_tracepoint)) {
				AVR_LOG(avr, LOG_ERROR, "ELF: *** Too many SRAM traces (limit = %d)\n", ARRAY_SIZE(avr->sram_tracepoint));
			} else {
				char name[20];
				sprintf(name, "sram_tracepoint_%d", avr->sram_tracepoint_count);
				const char *names[1] = {name};
				avr_irq_t *irq = avr_alloc_irq(&avr->irq_pool, 0, 1, names);
				if (irq) {
					AVR_LOG(avr, LOG_OUTPUT, "ELF: SRAM tracepoint added at '0x%04x' (%s, %d bits)\n", firmware->trace[ti].addr, firmware->trace[ti].name, firmware->trace[ti].kind == AVR_MMCU_TAG_VCD_SRAM_8 ? 8 : 16);
					avr->sram_tracepoint[avr->sram_tracepoint_count].irq = irq;
					avr->sram_tracepoint[avr->sram_tracepoint_count].width = firmware->trace[ti].kind == AVR_MMCU_TAG_VCD_SRAM_8 ? 8 : 16;
					avr->sram_tracepoint[avr->sram_tracepoint_count].addr = firmware->trace[ti].addr;
					avr_vcd_add_signal(avr->vcd,
						irq,
						avr->sram_tracepoint[avr->sram_tracepoint_count].width,
						firmware->trace[ti].name[0] ? firmware->trace[ti].name : names[0]
					);
					avr->sram_tracepoint_count++;
				}
			}
		} else if (firmware->trace[ti].mask == 0xff ||
					firmware->trace[ti].mask == 0) {
			// easy one
			avr_irq_t * all = avr_iomem_getirq(avr,
					firmware->trace[ti].addr,
					firmware->trace[ti].name,
					AVR_IOMEM_IRQ_ALL);
			if (!all) {
				AVR_LOG(avr, LOG_ERROR,
					"ELF: %s: unable to attach trace to address %04x\n",
					__FUNCTION__, firmware->trace[ti].addr);
			} else {
				avr_vcd_add_signal(avr->vcd, all, 8,
						firmware->trace[ti].name);
			}
		} else {
			int count = __builtin_popcount(firmware->trace[ti].mask);
		//	for (int bi = 0; bi < 8; bi++)
		//		if (firmware->trace[ti].mask & (1 << bi))
		//			count++;
			for (int bi = 0; bi < 8; bi++)
				if (firmware->trace[ti].mask & (1 << bi)) {
					avr_irq_t * bit = avr_iomem_getirq(avr,
							firmware->trace[ti].addr,
							firmware->trace[ti].name,
							bi);
					if (!bit) {
						AVR_LOG(avr, LOG_ERROR,
							"ELF: %s: unable to attach trace to address %04x\n",
							__FUNCTION__, firmware->trace[ti].addr);
						break;
					}

					if (count == 1) {
						avr_vcd_add_signal(avr->vcd,
								bit, 1, firmware->trace[ti].name);
						break;
					}
					char comp[128];
					sprintf(comp, "%s.%d", firmware->trace[ti].name, bi);
					avr_vcd_add_signal(avr->vcd, bit, 1, comp);
				}
		}
	}
	// if the firmware has specified a command register, do NOT start the trace here
	// the firmware probably knows best when to start/stop it
	if (!firmware->command_register_addr)
		avr_vcd_start(avr->vcd);
}

#ifdef HAVE_LIBELF
static void
elf_parse_mmcu_section(
		elf_firmware_t * firmware,
		uint8_t * src,
		uint32_t size)
{
//	hdump(".mmcu", src, size);
	while (size) {
		uint8_t tag = *src++;
		uint8_t ts = *src++;
		int next = size > 2 + ts ? 2 + ts : size;
	//	printf("elf_parse_mmcu_section %2d, size %2d / remains %3d\n",
	//			tag, ts, size);
		switch (tag) {
			case AVR_MMCU_TAG_FREQUENCY:
				firmware->frequency =
					src[0] | (src[1] << 8) | (src[2] << 16) | (src[3] << 24);
				break;
			case AVR_MMCU_TAG_NAME:
				strcpy(firmware->mmcu, (char*)src);
				break;
			case AVR_MMCU_TAG_VCC:
				firmware->vcc =
					src[0] | (src[1] << 8) | (src[2] << 16) | (src[3] << 24);
				break;
			case AVR_MMCU_TAG_AVCC:
				firmware->avcc =
					src[0] | (src[1] << 8) | (src[2] << 16) | (src[3] << 24);
				break;
			case AVR_MMCU_TAG_AREF:
				firmware->aref =
					src[0] | (src[1] << 8) | (src[2] << 16) | (src[3] << 24);
				break;
			case AVR_MMCU_TAG_PORT_EXTERNAL_PULL: {
				for (int i = 0; i < 8; i++)
					if (!firmware->external_state[i].port) {
						firmware->external_state[i].port = src[2];
						firmware->external_state[i].mask = src[1];
						firmware->external_state[i].value = src[0];
#if 0
						AVR_LOG(NULL, LOG_DEBUG,
							"AVR_MMCU_TAG_PORT_EXTERNAL_PULL[%d] %c:%02x:%02x\n",
							i, firmware->external_state[i].port,
							firmware->external_state[i].mask,
							firmware->external_state[i].value);
#endif
						break;
					}
			}	break;
			case AVR_MMCU_TAG_VCD_PORTPIN:
			case AVR_MMCU_TAG_VCD_IRQ:
			case AVR_MMCU_TAG_VCD_TRACE:
			case AVR_MMCU_TAG_VCD_SRAM_8:
			case AVR_MMCU_TAG_VCD_SRAM_16: {
				uint8_t mask = src[0];
				uint16_t addr = src[1] | (src[2] << 8);
				char * name = (char*)src + 3;
#if 0
				AVR_LOG(NULL, LOG_DEBUG,
						"VCD_TRACE %d %04x:%02x - %s\n", tag,
						addr, mask, name);
#endif
				firmware->trace[firmware->tracecount].kind = tag;
				firmware->trace[firmware->tracecount].mask = mask;
				firmware->trace[firmware->tracecount].addr = addr;
				strncpy(firmware->trace[firmware->tracecount].name, name,
					sizeof(firmware->trace[firmware->tracecount].name));
				firmware->tracecount++;
			}	break;
			case AVR_MMCU_TAG_VCD_IO_IRQ: {
				uint8_t   mask = src[0];
				uint32_t  ioctl;
				char     *name;

				ioctl = AVR_IOCTL_DEF(src[1], src[2], src[3], src[4]);
				name = (char*)src + 6;
				firmware->trace[firmware->tracecount].kind = tag;
				firmware->trace[firmware->tracecount].mask = mask;
				firmware->trace[firmware->tracecount].addr = ioctl;
				strncpy(firmware->trace[firmware->tracecount].name, name,
					sizeof(firmware->trace[firmware->tracecount].name));
				firmware->tracecount++;
			}	break;
			case AVR_MMCU_TAG_VCD_FILENAME: {
				strcpy(firmware->tracename, (char*)src);
			}	break;
			case AVR_MMCU_TAG_VCD_PERIOD: {
				firmware->traceperiod =
					src[0] | (src[1] << 8) | (src[2] << 16) | (src[3] << 24);
			}	break;
			case AVR_MMCU_TAG_SIMAVR_COMMAND: {
				firmware->command_register_addr = src[0] | (src[1] << 8);
			}	break;
			case AVR_MMCU_TAG_SIMAVR_CONSOLE: {
				firmware->console_register_addr = src[0] | (src[1] << 8);
			}	break;
		}
		size -= next;
		src += next - 2; // already incremented
	}
}

#if 0
static int
elf_copy_segment(int fd, Elf32_Phdr *php, uint8_t **dest)
{
	int rv;

	if (*dest == NULL)
		*dest = malloc(php->p_filesz);
	if (!*dest)
		return -1;

	lseek(fd, php->p_offset, SEEK_SET);
	rv = read(fd, *dest, php->p_filesz);
	if (rv != php->p_filesz) {
		AVR_LOG(NULL, LOG_ERROR,
				"Got %d when reading %d bytes for %x at offset %d "
				"from ELF file\n",
				rv, php->p_filesz, php->p_vaddr, php->p_offset);
		return -1;
	}
	AVR_LOG(NULL, LOG_DEBUG, "ELF: Loaded %d bytes at %x\n",
			php->p_filesz, php->p_vaddr);
	return 0;
}

static int
elf_handle_segment(int fd, Elf32_Phdr *php, uint8_t **dest, const char *name)
{
	if (*dest) {
		AVR_LOG(NULL, LOG_ERROR,
				"Unexpected extra %s data: %d bytes at %x.\n",
				name, php->p_filesz, php->p_vaddr);
		return -1;
	} else {
		elf_copy_segment(fd, php, dest);
		return 0;
	}
}
#endif

/* The structure *firmware must be pre-initialised to zero, then optionally
 * tracing and VCD information may be added.
 */

int
elf_read_firmware(
	const char * file,
	elf_firmware_t * firmware)
{
	Elf32_Ehdr   elf_header;				/* ELF header */
	Elf         *elf = NULL;				/* Our Elf pointer for libelf */
	Elf32_Phdr  *php;						/* Program header. */
	Elf_Scn     *scn = NULL;				/* Section Descriptor */
	fw_chunk_t **ncp;						/* Loadable file contents. */
	size_t       ph_count;					/* Program Header entry count. */
	int          fd, i;						/* File Descriptor */

	if ((fd = open(file, O_RDONLY | O_BINARY)) == -1 ||
			(read(fd, &elf_header, sizeof(elf_header))) < sizeof(elf_header)) {
		AVR_LOG(NULL, LOG_ERROR, "could not read %s\n", file);
		perror(file);
		close(fd);
		return -1;
	}

#if ELF_SYMBOLS
	firmware->symbolcount = 0;
	firmware->symbol = NULL;
#endif

	/* this is actually mandatory !! otherwise elf_begin() fails */
	if (elf_version(EV_CURRENT) == EV_NONE) {
		/* library out of date - recover from error */
		return -1;
	}
	// Iterate through section headers again this time well stop when we find symbols
	elf = elf_begin(fd, ELF_C_READ, NULL);
	//printf("Loading elf %s : %p\n", file, elf);

	if (!elf)
		return -1;
	if (elf_kind(elf) != ELF_K_ELF) {
		AVR_LOG(NULL, LOG_ERROR, "Unexpected ELF file type\n");
		return -1;
	}

	/* Scan the Program Header Table. */

	if (elf_getphdrnum(elf, &ph_count) != 0 || ph_count == 0 ||
		(php = elf32_getphdr(elf)) == NULL) {
		AVR_LOG(NULL, LOG_ERROR, "No ELF Program Headers\n");
		return -1;
	}

	ncp = &firmware->chunks;
	for (i = 0; i < (int)ph_count; ++i, ++php) {
		fw_chunk_t    *chunk;
#if 0
		printf("Header %d type %d addr %x/%x size %d/%d flags %x\n",
			   i, php->p_type, php->p_vaddr, php->p_paddr,
			   php->p_filesz, php->p_memsz, php->p_flags);
#endif
		if (php->p_type != PT_LOAD || php->p_filesz == 0)
			continue;
		if (php->p_paddr >= AVR_SEGMENT_OFFSET_LAST) {
			continue; // Probably .mmcu
		}

		if (php->p_filesz == 0 && php->p_memsz == 0)
			continue;		// Unlikely!

		/* Allocate a chunk to store this information. */

		chunk = (fw_chunk_t *)malloc(sizeof (fw_chunk_t) + php->p_filesz);
		if (!chunk) {
			AVR_LOG(NULL, LOG_ERROR, "No memory for firmware!\n");
			exit(1);
		}
		chunk->type = UNKNOWN; // Classify later,
		chunk->addr = php->p_paddr;
		chunk->size = php->p_filesz;
		chunk->fill_size = php->p_memsz;
		chunk->next = NULL;
		if (php->p_filesz) {
			int rv;

			/* Read the data. */

			if (lseek(fd, php->p_offset, SEEK_SET) < 0) {
				AVR_LOG(NULL, LOG_ERROR,
						"Error when seeking %d bytes for %x at offset %d "
						"from ELF file: %s\n",
						php->p_filesz, php->p_vaddr, php->p_offset,
						strerror(errno));
				exit(1);
			}
			rv = read(fd, chunk->data, php->p_filesz);
			if (rv != php->p_filesz) {
				AVR_LOG(NULL, LOG_ERROR,
						"Got %d when reading %d bytes for %x at offset %d "
						"from ELF file: %s\n",
						rv, php->p_filesz, php->p_vaddr, php->p_offset,
						strerror(errno));
				exit(1);
			}
		}
		*ncp = chunk;
		ncp = &chunk->next;
	}

	/* Scan the section table for .mmcu magic and symbols. */

	if (!firmware->dwarf_file)
		firmware->dwarf_file = strdup(file);	// Parse later.

	while ((scn = elf_nextscn(elf, scn)) != NULL) {
		GElf_Shdr shdr;                 /* Section Header */
		gelf_getshdr(scn, &shdr);
		char * name = elf_strptr(elf, elf_header.e_shstrndx, shdr.sh_name);
		//	printf("Walking elf section '%s'\n", name);

		if (name && !strcmp(name, ".mmcu")) {
			Elf_Data *s = elf_getdata(scn, NULL);

			elf_parse_mmcu_section(firmware, s->d_buf, s->d_size);
			if (shdr.sh_addr < 0x860000)
				AVR_LOG(NULL, LOG_WARNING,
						"Warning: ELF .mmcu section at %x may be loaded.\n",
						shdr.sh_addr);
		//	printf("%s: size %ld\n", __FUNCTION__, s->d_size);
		//	avr->frequency = f_cpu;
		}

#if ELF_SYMBOLS
		// When we find a section header marked SHT_SYMTAB stop and get symbols
		if (shdr.sh_type == SHT_SYMTAB) {
			uint32_t highest_data = 0;

			// edata points to our symbol table
			Elf_Data *edata = elf_getdata(scn, NULL);

			// how many symbols are there? this number comes from the size of
			// the section divided by the entry size
			int symbol_count = shdr.sh_size / shdr.sh_entsize;

			// loop through to grab all symbols
			for (int i = 0; i < symbol_count; i++) {
				GElf_Sym sym;			/* Symbol */
				// libelf grabs the symbol data using gelf_getsym()
				gelf_getsym(edata, i, &sym);

				// print out the value and size
				if (ELF32_ST_BIND(sym.st_info) == STB_GLOBAL ||
                                    ELF32_ST_TYPE(sym.st_info) == STT_FUNC ||
                                    ELF32_ST_TYPE(sym.st_info) == STT_OBJECT) {
					const char * name = elf_strptr(elf, shdr.sh_link, sym.st_name);
#if VERBOSE
					printf("Symbol %s bind %d type %d value %lx size %ld visibility %d\n",
					       name, ELF32_ST_BIND(sym.st_info),
					       ELF32_ST_TYPE(sym.st_info),
                                               sym.st_value, sym.st_size,
                                               GELF_ST_VISIBILITY(sym.st_other));
#endif
					// Some names are lengths of data areas
					// that are not obviously distinguished
					// from labels like __vectors.

					if (ELF32_ST_TYPE(sym.st_info) == STT_NOTYPE &&
					    sym.st_size == 0) {
						int n;

						n = strlen(name);
						if (n > 9 &&
						    !strcmp(&name[n - 9],
							    "_LENGTH__")) {
							//printf(" Ignored\n");
							continue;
						}
					}

					// Look for the highest RAM sysmbol.

					if (sym.st_value >
					    AVR_SEGMENT_OFFSET_DATA +
					        highest_data &&
					    sym.st_value <
					        AVR_SEGMENT_OFFSET_EEPROM) {
						highest_data =
                                                    sym.st_value + sym.st_size -
						    AVR_SEGMENT_OFFSET_DATA;
					}

					// if its a bootloader, this symbol will be the entry point we need
					if (!strcmp(name, "__vectors"))
						firmware->flashbase = sym.st_value;
					avr_symbol_t * s = malloc(sizeof(avr_symbol_t) + strlen(name) + 1);
					strcpy((char*)s->symbol, name);
					s->addr = sym.st_value;
					s->size = sym.st_size;
					if (!(firmware->symbolcount % 8))
						firmware->symbol = realloc(
							firmware->symbol,
							(firmware->symbolcount + 8) * sizeof(firmware->symbol[0]));

					// insert new element, keep the array sorted
					int insert = -1;
					for (int si = 0; si < firmware->symbolcount && insert == -1; si++)
						if (firmware->symbol[si]->addr >= s->addr)
							insert = si;
					if (insert == -1)
						insert = firmware->symbolcount;
					else
						memmove(firmware->symbol + insert + 1,
								firmware->symbol + insert,
								(firmware->symbolcount - insert) * sizeof(firmware->symbol[0]));
					firmware->symbol[insert] = s;
					firmware->symbolcount++;
				}
			}
			firmware->highest_data_symbol = highest_data;
		}
#endif // ELF_SYMBOLS
	}
	elf_end(elf);
	close(fd);
	return 0;
}
#else //  HAVE_LIBELF not defined.
int
elf_read_firmware(const char * file, elf_firmware_t * firmware)
{
	AVR_LOG(NULL, LOG_ERROR, "ELF format is not supported by this build.\n");
	return -1;
}
#endif
