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
#include "avr_eeprom.h"

void avr_load_firmware(avr_t * avr, elf_firmware_t * firmware)
{
	avr->frequency = firmware->mmcu.f_cpu;
	avr->codeline = firmware->codeline;
	avr_loadcode(avr, firmware->flash, firmware->flashsize, 0);
	avr->codeend = firmware->flashsize - firmware->datasize;
	if (firmware->eeprom && firmware->eesize) {
		avr_eeprom_desc_t d = { .ee = firmware->eeprom, .offset = 0, .size = firmware->eesize };
		avr_ioctl(avr, AVR_IOCTL_EEPROM_SET, &d);
	}
}

int elf_read_firmware(const char * file, elf_firmware_t * firmware)
{
	Elf32_Ehdr elf_header;			/* ELF header */
	Elf *elf = NULL;                       /* Our Elf pointer for libelf */
	int fd; // File Descriptor

	if ((fd = open(file, O_RDONLY)) == -1 ||
			(read(fd, &elf_header, sizeof(elf_header))) < sizeof(elf_header)) {
		printf("could not read %s\n", file);
		perror(file);
		close(fd);
		return -1;
	}

	Elf_Data *data_data = NULL, 
		*data_text = NULL,
		*data_ee = NULL;                /* Data Descriptor */

	memset(firmware, 0, sizeof(*firmware));
#if ELF_SYMBOLS
	//int bitesize = ((avr->flashend+1) >> 1) * sizeof(avr_symbol_t);
	firmware->codesize = 32768;
	int bitesize = firmware->codesize * sizeof(avr_symbol_t);
	firmware->codeline = malloc(bitesize);
	memset(firmware->codeline,0, bitesize);
#endif

	/* this is actualy mandatory !! otherwise elf_begin() fails */
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
		else if (!strcmp(name, ".bss")) {
			Elf_Data *s = elf_getdata(scn, NULL);
			firmware->bsssize = s->d_size;
		} else if (!strcmp(name, ".mmcu")) {
			Elf_Data *s = elf_getdata(scn, NULL);
			firmware->mmcu = *((struct avr_mcu_t*)s->d_buf);
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
			//	printf("%08x %08d ", sym.st_value, sym.st_size);
				if (ELF32_ST_BIND(sym.st_info) == STB_GLOBAL || 
						ELF32_ST_TYPE(sym.st_info) == STT_FUNC || 
						ELF32_ST_TYPE(sym.st_info) == STT_OBJECT) {
					const char * name = elf_strptr(elf, shdr.sh_link, sym.st_name);

					// type of symbol
					if (sym.st_value & 0xfff00000) {

					} else {
						// code
						if (firmware->codeline[sym.st_value >> 1] == NULL) {
							avr_symbol_t * s = firmware->codeline[sym.st_value >> 1] = malloc(sizeof(avr_symbol_t*));
							s->symbol = strdup(name);
							s->addr = sym.st_value;
						}
					}
				}
			}
		}
#endif
	}
#if ELF_SYMBOLS
	avr_symbol_t * last = NULL;
	for (int i = 0; i < firmware->codesize; i++) {
		if (!firmware->codeline[i])
			firmware->codeline[i] = last;
		else
			last = firmware->codeline[i];
	}
#endif
	uint32_t offset = 0;
	firmware->flashsize =
			(data_text ? data_text->d_size : 0) +
			(data_data ? data_data->d_size : 0);
	firmware->flash = malloc(firmware->flashsize);
	if (data_text) {
	//	hdump("code", data_text->d_buf, data_text->d_size);
		memcpy(firmware->flash + offset, data_text->d_buf, data_text->d_size);
		offset += data_text->d_size;
		printf("Loaded %d .text\n", data_text->d_size);
	}
	if (data_data) {
	//	hdump("data", data_data->d_buf, data_data->d_size);
		memcpy(firmware->flash + offset, data_data->d_buf, data_data->d_size);
		printf("Loaded %d .data\n", data_data->d_size);
		offset += data_data->d_size;
		firmware->datasize = data_data->d_size;
	}
	if (data_ee) {
	//	hdump("eeprom", data_ee->d_buf, data_ee->d_size);
		firmware->eeprom = malloc(data_ee->d_size);
		memcpy(firmware->eeprom, data_ee->d_buf, data_ee->d_size);
		printf("Loaded %d .eeprom\n", data_ee->d_size);
		firmware->eesize = data_ee->d_size;
	}
//	hdump("flash", avr->flash, offset);
	elf_end(elf);
	close(fd);
	return 0;
}

