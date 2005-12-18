/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * bitmap.c - PSPLINK kernel module bitmap code
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 *
 * $HeadURL$
 * $Id$
 */

#include <pspkernel.h>
#include <pspdisplay.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define BMP_ID "BM"

struct BitmapHeader
{
	char id[2];
	uint32_t filesize;
	uint32_t reserved;
	uint32_t offset;
	uint32_t headsize;
	uint32_t width;
	uint32_t height;
	uint16_t planes;
	uint16_t bpp;
	uint32_t comp;
	uint32_t bitmapsize;
	uint32_t hres;
	uint32_t vres;
	uint32_t colors;
	uint32_t impcolors;
} __attribute__((packed));

static int fixed_write(int fd, void *data, int len)
{
	int writelen = 0;

	while(writelen < len)
	{
		int ret;

		ret = sceIoWrite(fd, data + writelen, len - writelen);
		if(ret <= 0)
		{
			writelen = -1;
			break;
		}
		writelen += ret;
	}

	return writelen;
}

int write_8888_data(int fd, void *frame)
{
	uint8_t line[480*3];
	uint8_t *p;
	int i;
	int h;

	for(h = 271; h >= 0; h--)
	{
		p = frame + (h*512*4);
		for(i = 0; i < 480; i++)
		{
			line[(i*3) + 2] = p[i*4];
			line[(i*3) + 1] = p[(i*4) + 1];
			line[(i*3) + 0] = p[(i*4) + 2];
		}

		fixed_write(fd, line, sizeof(line));
	}

	return 0;
}

int write_5551_data(int fd, void *frame)
{
	uint8_t line[480*3];
	uint16_t *p;
	int i;
	int h;

	for(h = 271; h >= 0; h--)
	{
		p = frame;
		p += (h * 512);
		for(i = 0; i < 480; i++)
		{
			line[(i*3) + 2] = (p[i] & 0x1F) << 3;
			line[(i*3) + 1] = ((p[i] >> 5) & 0x1F) << 3;
			line[(i*3) + 0] = ((p[i] >> 10) & 0x1F) << 3;
		}

		fixed_write(fd, line, sizeof(line));
	}

	return 0;
}

int write_565_data(int fd, void *frame)
{
	uint8_t line[480*3];
	uint16_t *p;
	int i;
	int h;

	for(h = 271; h >= 0; h--)
	{
		p = frame;
		p += (h * 512);
		for(i = 0; i < 480; i++)
		{
			line[(i*3) + 2] = (p[i] & 0x1F) << 3;
			line[(i*3) + 1] = ((p[i] >> 5) & 0x3F) << 2;
			line[(i*3) + 0] = ((p[i] >> 11) & 0x1F) << 3;
		}

		fixed_write(fd, line, sizeof(line));
	}

	return 0;
}

int write_4444_data(int fd, void *frame)
{
	uint8_t line[480*3];
	uint16_t *p;
	int i;
	int h;

	for(h = 271; h >= 0; h--)
	{
		p = frame;
		p += (h * 512);
		for(i = 0; i < 480; i++)
		{
			line[(i*3) + 2] = (p[i] & 0xF) << 4;
			line[(i*3) + 1] = ((p[i] >> 4) & 0xF) << 4;
			line[(i*3) + 0] = ((p[i] >> 8) & 0xF) << 4;
		}

		fixed_write(fd, line, sizeof(line));
	}

	return 0;
}

int bitmapWrite(void *frame_addr, int format, const char *file)
{
	struct BitmapHeader bmp;
	int fd;

	memset(&bmp, 0, sizeof(bmp));
	memcpy(&bmp, BMP_ID, sizeof(bmp.id));
	bmp.filesize = 480*272*3 + sizeof(bmp);
	bmp.offset = sizeof(bmp);
	bmp.headsize = 0x28;
	bmp.width = 480;
	bmp.height = 272;
	bmp.planes = 1;
	bmp.bpp = 24;
	bmp.bitmapsize = 480*272*3;
	bmp.hres = 2834;
	bmp.vres = 2834;

	fd = sceIoOpen(file, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
	if(fd < 0)
	{
		printf("Could not open file '%s' for writing\n", file);
	}

	fixed_write(fd, &bmp, sizeof(bmp));
	switch(format)
	{
		case PSP_DISPLAY_PIXEL_FORMAT_565: write_565_data(fd, frame_addr);
										   break;
		case PSP_DISPLAY_PIXEL_FORMAT_5551: write_5551_data(fd, frame_addr); 
										   break;
		case PSP_DISPLAY_PIXEL_FORMAT_4444: write_4444_data(fd, frame_addr);
											break;
		case PSP_DISPLAY_PIXEL_FORMAT_8888: write_8888_data(fd, frame_addr);
											break;
	};

	sceIoClose(fd);

	return 0;
}
