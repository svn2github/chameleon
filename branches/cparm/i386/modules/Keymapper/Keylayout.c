/*
 *  Keymapper.c
 *  Chameleon
 *
 *  Created by JrCs on 28/08/11.
 *  Copyright 2011. All rights reserved.
 *
 */

#include "libsaio.h"
#include "term.h"
#include "modules.h"
#include "Keylayout.h"
#include "bootstruct.h"

#ifndef DEBUG_KLAYOUT
#define DEBUG_KLAYOUT 0
#endif

#if DEBUG_KLAYOUT
#define DBG(x...)	printf(x)
#else
#define DBG(x...)
#endif

#define kKeyboardLayout "KeyboardLayout"

//==========================================================================
// lseek() - Reposition the byte offset of the file descriptor from the
//           beginning of the file. Returns the relocated offset.

int key_b_lseek(int fdesc, int offset, int ptr)
{
    struct iob * io;
	
    if ((io = iob_from_fdesc(fdesc)) == NULL)
        return (-1);
	
    io->i_offset = offset;
	
    return offset;
}

struct keyboard_layout *current_layout = NULL;
int getchar_replacement(void);

int getchar_replacement(void) {
	int code   = bgetc();
	int status = readKeyboardShiftFlags();
	uint8_t  scancode = code >> 8;
	
	// Special scancode sent when alt + some keys are pressed
	if (scancode >= 0x78 && scancode <= 0x83)
		scancode -= 0x76;
	
	if (scancode < KEYBOARD_MAP_SIZE && !(status & (STATUS_LCTRL| STATUS_RCTRL))) {
		int key;
		if ((status & (STATUS_LALT|STATUS_RALT)) &&
			(status & (STATUS_LSHIFT|STATUS_RSHIFT|STATUS_CAPS))) {
			key=current_layout->keyboard_map_shift_alt[scancode];
		}
		else if (status & (STATUS_LSHIFT|STATUS_RSHIFT|STATUS_CAPS))
			key=current_layout->keyboard_map_shift[scancode];
		else if (status & (STATUS_LALT|STATUS_RALT))
			key=current_layout->keyboard_map_alt[scancode];
		else
			key=current_layout->keyboard_map[scancode];
		
		if (key != 0) // do the mapping
			code = key;
	}
	
	if (ASCII_KEY(code) != 0) // if ascii not null return it
		code = ASCII_KEY(code);
	
	//printf("Code: %04x\n",code);
	return (code);
}

static uint32_t load_keyboard_layout_file(const char *filename) {
	int      fd;
	char     magic[KEYBOARD_LAYOUTS_MAGIC_SIZE];
	uint32_t version;
	
	
	if ((fd = open_bvdev("bt(0,0)", filename)) < 0) {		
		goto fail; // fail
	}
	
	if (read(fd, magic, sizeof(magic)) != sizeof(magic)) {
		printf("Can't find magic in keyboard layout file: %s\n", filename);
		goto fail;
	}
	
	if (memcmp (magic, KEYBOARD_LAYOUTS_MAGIC, KEYBOARD_LAYOUTS_MAGIC_SIZE) != 0) {
		printf("Invalid magic code in keyboard layout file: %s\n", filename);
		goto fail;
    }
	
	if (read(fd, (char*) &version, sizeof(version)) != sizeof(version)) {
		printf("Can't get version of keyboard layout file: %s\n", filename);
		goto fail;
	}
	
	if (version != KEYBOARD_LAYOUTS_VERSION) {
		verbose("Bad version for keyboard layout file %s expected v%d found v%d\n",
				filename, KEYBOARD_LAYOUTS_VERSION, version);
		goto fail;
	}
	
	if (current_layout)
		free(current_layout);
	
	current_layout = malloc(sizeof(struct keyboard_layout));
	if (!current_layout)
		goto fail;
	bzero(current_layout,sizeof(struct keyboard_layout));
	
	key_b_lseek(fd, KEYBOARD_LAYOUTS_MAP_OFFSET, 0);
	
	if (read(fd, (char*) current_layout, sizeof(struct keyboard_layout)) != sizeof(struct keyboard_layout)) {
		printf("Wrong keyboard layout file %s size\n", filename);
		goto fail;
	}
	
	close(fd);
	
	return 1;
	
fail:
    
	if (current_layout) {
		free(current_layout);
		current_layout = NULL;
	}
	return 0;
}

uint32_t Keylayout_real_start(void)
{
	char  layoutPath[512];
	const char	*val;
	int			len;
	
	if (getValueForKey("KeyLayout", &val, &len, DEFAULT_BOOT_CONFIG))
	{
		snprintf(layoutPath, sizeof(layoutPath),"/Extra/Keymaps/%s", val);
		// Add the extension if needed
		if (len <= 4 || strncmp(val+len-4,".lyt", sizeof(".lyt")) != 0)
			strlcat(layoutPath, ".lyt", sizeof(layoutPath));
		
		if (!load_keyboard_layout_file(layoutPath))
		{
			DBG("Can't load %s keyboard layout file. Keylayout will not be used !\n",
				layoutPath);
			return 0;
		} 
		
		
		if (replace_system_function("_getc", &getchar_replacement) != EFI_SUCCESS ) 
		{
			printf("no function getc() to replace. Keylayout will not be used ! \n");
			return 0;
		}
		
		return 1;
		
	}
	return 0;
}

void Keylayout_start(void);
void Keylayout_start(void)
{
	Keylayout_real_start();
}