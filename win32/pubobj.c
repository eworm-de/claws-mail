/*
 * Copyright (C) 2004 by Alfons Hoogervorst. 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <assert.h>

/* Microsoft COFF definitions - inferred from data found in 
 * "Windows 95 System Programming Secrets" by Matt Pietrek */

#define IMAGE_SHORT_NAME_SIZE (8)

#define FILE_MACHINE_INTEL_I386	(0x14C)

#define FILE_CHARS_NO_RELOC	(0x1)
#define FILE_CHARS_EXECUTABLE	(0x2)
#define FILE_CHARS_DLL		(0x2000)

/* header */
typedef struct {
	unsigned short		machine;
	unsigned short		sections;
	unsigned long		time_stamp;
	unsigned long		symbol_table_offset;
	unsigned long		symbols;
	unsigned short		optional_header_size;
	unsigned short		characteristics;
} image_file;

/* symbol */
typedef struct {
	union {
		unsigned char	short_name[IMAGE_SHORT_NAME_SIZE];
		struct {
			unsigned long	short_offset;
			unsigned long	long_offset;
		} name;
		unsigned long	long_name[2];
	} N;
	unsigned long		value;
	short			section_number;
	unsigned short		type;
	unsigned char		storage_class;
	unsigned char		number_of_aux_symbols;
} image_symbol;

#define IMAGE_SYMBOL_SIZE ((sizeof (*((image_symbol *)(0))).N.short_name)      + \
			   (sizeof (*((image_symbol *)(0))).value)             + \
			   (sizeof (*((image_symbol *)(0))).section_number)    + \
			   (sizeof (*((image_symbol *)(0))).type)              + \
			   (sizeof (*((image_symbol *)(0))).storage_class)     + \
			   (sizeof (*((image_symbol *)(0))).number_of_aux_symbols))

/***/

/* XXX: Need to byte swap for big endian machines */
#define READ_WORD(f, data)	fread(&data, sizeof(data), 1, f)
#define READ_DWORD(f, data)	fread(&data, sizeof(data), 1, f)

int main(int argc, char **argv)
{
	assert(IMAGE_SYMBOL_SIZE == 18);

	fprintf(stderr, "pubobj - list public C declarations in Win32 COFF objects\n"
		        "See http://claws-w32.sf.net\n");
	
	for (argc -= 1, argv = &argv[1]; argc; argc--, argv++) {
		FILE *fin;
		image_file imf;
		long n;
		unsigned long table_size = 0;
		char *string_table = NULL;

		if (NULL == (fin = fopen(argv[0], "rb"))) 
			continue;	

		/* read informational data */
		READ_WORD(fin,	imf.machine);
		READ_WORD(fin,	imf.sections);
		READ_DWORD(fin, imf.time_stamp);
		READ_DWORD(fin, imf.symbol_table_offset);
		READ_DWORD(fin, imf.symbols);

		/* check for overflows, so we're limited to 2Gb files... */
		assert(imf.symbol_table_offset < (unsigned long) LONG_MAX);
		assert(imf.symbols < (unsigned long) LONG_MAX);

		
		/* seek to symbol table to read symbol table size */
		fseek(fin, (long) imf.symbol_table_offset, SEEK_SET);
		fseek(fin, (long) imf.symbols * IMAGE_SYMBOL_SIZE, SEEK_CUR);
		READ_DWORD(fin, table_size);
		
		/* table size dword is part of the table size */
		table_size -= 4;
		if (table_size) {
			/* allocate enough memory and read all symbols; they 
			 * are just ASCIIZ strings. */
			string_table = calloc(table_size, 1);
			fread(string_table, table_size, 1, fin);
		}

		/* good, iterate each symbol */
		fseek(fin, (long) imf.symbol_table_offset, SEEK_SET);

		n = 0;
		while (n < imf.symbols) {
			image_symbol ims;
			
			READ_DWORD(fin, ims.N.name.short_offset);
			READ_DWORD(fin, ims.N.name.long_offset);
			READ_DWORD(fin, ims.value);
			READ_WORD(fin,  ims.section_number); /* XXX: signed */	
			READ_WORD(fin,  ims.type);
			ims.storage_class = fgetc(fin);
			ims.number_of_aux_symbols = fgetc(fin);

			/* external symbol */
			if (table_size && ims.storage_class == 2) {
				char safe_buf[IMAGE_SHORT_NAME_SIZE + 1];
				char *name;
				
				/*
				 * 1) if type == 0x20, section_number != 0, it's a
				 *    function that's externally visible (non-static).
				 *
				 * 2) if type == 0x00, section_number == 0, value != 0,
				 *    it's a global variable. Note that since all 
				 *    global variables are merged into one .data, this
				 *    would even include variables that are not defined
				 *    in this object
				 */
				if ((ims.type == 0x20 && ims.section_number) ||  
				    (ims.type == 0x00 && ims.section_number == 0 && ims.value)) {
					if (ims.N.name.short_offset) {
						_snprintf(safe_buf, IMAGE_SHORT_NAME_SIZE + 1,
							 "%s", ims.N.short_name);
						name = safe_buf;	
					} else 
						name = &string_table[ims.N.name.long_offset - 
								     sizeof(unsigned long)];

					/* We're only interested in _cdecls */
					if (*name == '_' && !strchr(name, '@'))
						printf("%s\n", name + 1);
				}
			}
			
			/* skip auxillary symbols (uninteresting junk) */
			if (ims.number_of_aux_symbols) {
				fseek(fin, ims.number_of_aux_symbols * IMAGE_SYMBOL_SIZE, 
				      SEEK_CUR);
				n += ims.number_of_aux_symbols;
			} else
				n++;
		}
		free(string_table);
		fclose(fin);
	}		
	return 0;
}


