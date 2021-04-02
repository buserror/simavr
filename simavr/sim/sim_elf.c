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
#include <libelf.h>
#include <gelf.h>

#include "sim_elf.h"
#include "sim_vcd_file.h"
#include "avr_eeprom.h"
#include "avr_ioport.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif

void
avr_load_firmware(
		avr_t * avr,
		elf_firmware_t * firmware)
{
	if (firmware->frequency)
		avr->frequency = firmware->frequency;
	if (firmware->vcc)
		avr->vcc = firmware->vcc;
	if (firmware->avcc)
		avr->avcc = firmware->avcc;
	if (firmware->aref)
		avr->aref = firmware->aref;
#if CONFIG_SIMAVR_TRACE && ELF_SYMBOLS
	int scount = firmware->flashsize >> 1;
	avr->trace_data->codeline = malloc(scount * sizeof(avr_symbol_t*));
	memset(avr->trace_data->codeline, 0, scount * sizeof(avr_symbol_t*));

	for (int i = 0; i < firmware->symbolcount; i++)
		if (firmware->symbol[i]->addr < firmware->flashsize)	// code address
			avr->trace_data->codeline[firmware->symbol[i]->addr >> 1] =
				firmware->symbol[i];
	// "spread" the pointers for known symbols forward
	avr_symbol_t * last = NULL;
	for (int i = 0; i < scount; i++) {
		if (!avr->trace_data->codeline[i])
			avr->trace_data->codeline[i] = last;
		else
			last = avr->trace_data->codeline[i];
	}
#endif

	avr_loadcode(avr, firmware->flash,
			firmware->flashsize, firmware->flashbase);
	avr->codeend = firmware->flashsize +
			firmware->flashbase - firmware->datasize;

	if (firmware->eeprom && firmware->eesize) {
		avr_eeprom_desc_t d = {
				.ee = firmware->eeprom,
				.offset = 0,
				.size = firmware->eesize
		};
		avr_ioctl(avr, AVR_IOCTL_EEPROM_SET, &d);
	}
	if (firmware->fuse)
		memcpy(avr->fuse, firmware->fuse, firmware->fusesize);
	if (firmware->lockbits)
		avr->lockbits = firmware->lockbits[0];
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

	AVR_LOG(avr, LOG_TRACE, "Creating VCD trace file '%s'\n",
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
					avr_vcd_add_signal(avr->vcd,
							bit, 1, firmware->trace[ti].name);
				}
		}
	}
	// if the firmware has specified a command register, do NOT start the trace here
	// the firmware probably knows best when to start/stop it
	if (!firmware->command_register_addr)
		avr_vcd_start(avr->vcd);
}

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
			case AVR_MMCU_TAG_VCD_TRACE: {
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

static int
elf_copy_section(
	const char *name,
	Elf_Data *data,
	uint8_t **dest)
{
	*dest = malloc(data->d_size);
	if (!*dest)
		return -1;

	memcpy(*dest, data->d_buf, data->d_size);
	AVR_LOG(NULL, LOG_DEBUG, "Loaded %zu %s\n", data->d_size, name);

	return 0;
}


/* The structure *firmware must be pre-initialised to zero, then optionally
 * with tracing and VCD information.
 */

int
elf_read_firmware(
	const char * file,
	elf_firmware_t * firmware)
{
	Elf32_Ehdr elf_header;			/* ELF header */
	Elf *elf = NULL;                       /* Our Elf pointer for libelf */
	int fd; // File Descriptor

	if ((fd = open(file, O_RDONLY | O_BINARY)) == -1 ||
			(read(fd, &elf_header, sizeof(elf_header))) < sizeof(elf_header)) {
		AVR_LOG(NULL, LOG_ERROR, "could not read %s\n", file);
		perror(file);
		close(fd);
		return -1;
	}

	Elf_Data *data_data = NULL,
		*data_text = NULL,
		*data_ee = NULL;                /* Data Descriptor */
	Elf_Data *data_fuse = NULL;
	Elf_Data *data_lockbits = NULL;

#if ELF_SYMBOLS
	firmware->symbolcount = 0;
	firmware->symbol = NULL;
#endif

	/* this is actually mandatory !! otherwise elf_begin() fails */
	if (elf_version(EV_CURRENT) == EV_NONE) {
			/* library out of date - recover from error */
	}
	// Iterate through section headers again this time well stop when we find symbols
	elf = elf_begin(fd, ELF_C_READ, NULL);
	//printf("Loading elf %s : %p\n", file, elf);

	Elf_Scn *scn = NULL;                   /* Section Descriptor */

	while ((scn = elf_nextscn(elf, scn)) != NULL) {
		GElf_Shdr shdr;                 /* Section Header */
		gelf_getshdr(scn, &shdr);
		char * name = elf_strptr(elf, elf_header.e_shstrndx, shdr.sh_name);
	//	printf("Walking elf section '%s'\n", name);

		if (!strcmp(name, ".text"))
			data_text = elf_getdata(scn, NULL);
		else if (!strcmp(name, ".data"))
			data_data = elf_getdata(scn, NULL);
		else if (!strcmp(name, ".eeprom"))
			data_ee = elf_getdata(scn, NULL);
		else if (!strcmp(name, ".fuse"))
			data_fuse = elf_getdata(scn, NULL);
		else if (!strcmp(name, ".lock"))
			data_lockbits = elf_getdata(scn, NULL);
		else if (!strcmp(name, ".bss")) {
			Elf_Data *s = elf_getdata(scn, NULL);
			firmware->bsssize = s->d_size;
		} else if (!strcmp(name, ".mmcu")) {
			Elf_Data *s = elf_getdata(scn, NULL);
			elf_parse_mmcu_section(firmware, s->d_buf, s->d_size);
			//printf("%s: avr_mcu_t size %ld / read %ld\n", __FUNCTION__, sizeof(struct avr_mcu_t), s->d_size);
		//	avr->frequency = f_cpu;
		}
#if ELF_SYMBOLS
		// When we find a section header marked SHT_SYMTAB stop and get symbols
		if (shdr.sh_type == SHT_SYMTAB) {
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

					// if its a bootloader, this symbol will be the entry point we need
					if (!strcmp(name, "__vectors"))
						firmware->flashbase = sym.st_value;
					avr_symbol_t * s = malloc(sizeof(avr_symbol_t) + strlen(name) + 1);
					strcpy((char*)s->symbol, name);
					s->addr = sym.st_value;
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
		}
#endif
	}
	uint32_t offset = 0;
	firmware->flashsize =
			(data_text ? data_text->d_size : 0) +
			(data_data ? data_data->d_size : 0);
	firmware->flash = malloc(firmware->flashsize);

	// using unsigned int for output, since there is no AVR with 4GB
	if (data_text) {
	//	hdump("code", data_text->d_buf, data_text->d_size);
		memcpy(firmware->flash + offset, data_text->d_buf, data_text->d_size);
		AVR_LOG(NULL, LOG_DEBUG, "Loaded %zu .text at address 0x%x\n",
				(unsigned int)data_text->d_size, firmware->flashbase);
		offset += data_text->d_size;
	}
	if (data_data) {
	//	hdump("data", data_data->d_buf, data_data->d_size);
		memcpy(firmware->flash + offset, data_data->d_buf, data_data->d_size);
		AVR_LOG(NULL, LOG_DEBUG, "Loaded %zu .data\n", data_data->d_size);
		offset += data_data->d_size;
		firmware->datasize = data_data->d_size;
	}
	if (data_ee) {
		if (elf_copy_section(".eeprom", data_ee, &firmware->eeprom))
			return -1;
		firmware->eesize = data_ee->d_size;
	}
	if (data_fuse) {
		if (elf_copy_section(".fuse", data_fuse, &firmware->fuse))
			return -1;
		firmware->fusesize = data_fuse->d_size;
	}
	if (data_lockbits) {
		if (elf_copy_section(".lock", data_lockbits, &firmware->lockbits))
			return -1;
	}
//	hdump("flash", avr->flash, offset);
	elf_end(elf);
	close(fd);
	return 0;
}

