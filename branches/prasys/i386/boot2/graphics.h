/*
 *  graphics.h
 *  
 *
 *  Created by fassl on 22.12.08.
 *  Copyright 2008 __MyCompanyName__. All rights reserved.
 *
 */

#include "boot.h"
#include "bootstruct.h"
#include "graphic_utils.h"
#include "vbe.h"


#ifndef __BOOT_GRAPHICS_H
#define __BOOT_GRAPHICS_H

#define DEFAULT_SCREEN_WIDTH 1024
#define DEFAULT_SCREEN_HEIGHT 768

int loadPngImage(const char *filename, uint16_t *width, uint16_t *height, uint8_t **imageData);
int loadPixmapFromPng(const char *filename, pixmap_t *p);
int loadPixmapFromEmbeddedPng(uint8_t *pngData, uint32_t pngSize, pixmap_t *p);

unsigned long lookUpCLUTIndex( unsigned char index, unsigned char depth );

void drawColorRectangle( unsigned short x, unsigned short y, unsigned short width, unsigned short height, unsigned char  colorIndex );
void drawDataRectangle( unsigned short x, unsigned short y, unsigned short width, unsigned short height, unsigned char * data );
int convertImage( unsigned short width, unsigned short height, const unsigned char *imageData, unsigned char **newImageData );

int initGraphicsMode ();

void drawCheckerBoard();

void blendImage(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t *data);

void drawCheckerBoard();
void blendImage(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t *data);

int loadEmbeddedPngImage(uint8_t *pngData, int pngSize, uint16_t *width, uint16_t *height, uint8_t **imageData);


char *getVBEInfoString();
char *getVBEModeInfoString();
void getGraphicModeParams(unsigned long params[]);


unsigned short getVESAModeWithProperties( unsigned short     width,
						  unsigned short     height,
						  unsigned char      bitsPerPixel,
						  unsigned short     attributesSet,
						  unsigned short     attributesClear,
						  VBEModeInfoBlock * outModeInfo,
						  unsigned short *   vesaVersion );


#endif /* !__BOOT_GRAPHICS_H */