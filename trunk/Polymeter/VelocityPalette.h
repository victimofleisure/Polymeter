// Copyleft 2014 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      24aug15	initial version
		01		07jun21	rename rounding functions

        palette for mapping note velocities to colors
 
*/

/*
	// create 128-color palette that changes hue spectrally from blue to red
	// (via cyan, green, and yellow) at medium lightness and full saturation
	FILE	*fp = fopen("VelocityPalette.h", "w");
	for (int i = 0; i < MIDI_NOTES; i++) {
		double	h, r, g, b;
		h = (1 - double(i) / MIDI_NOTE_MAX) * 240;
		hls2rgb(h, 0.5, 1.0, r, g, b);
		fprintf(fp, "RGB(0x%02X,0x%02X,0x%02X), // 0x%02X\n", Round(r * 255), Round(g * 255), Round(b * 255), i);
	}
	fclose(fp);
*/

RGB(0x00,0x00,0xFF), // 0x00
RGB(0x00,0x08,0xFF), // 0x01
RGB(0x00,0x10,0xFF), // 0x02
RGB(0x00,0x18,0xFF), // 0x03
RGB(0x00,0x20,0xFF), // 0x04
RGB(0x00,0x28,0xFF), // 0x05
RGB(0x00,0x30,0xFF), // 0x06
RGB(0x00,0x38,0xFF), // 0x07
RGB(0x00,0x40,0xFF), // 0x08
RGB(0x00,0x48,0xFF), // 0x09
RGB(0x00,0x50,0xFF), // 0x0A
RGB(0x00,0x58,0xFF), // 0x0B
RGB(0x00,0x60,0xFF), // 0x0C
RGB(0x00,0x68,0xFF), // 0x0D
RGB(0x00,0x70,0xFF), // 0x0E
RGB(0x00,0x78,0xFF), // 0x0F
RGB(0x00,0x81,0xFF), // 0x10
RGB(0x00,0x89,0xFF), // 0x11
RGB(0x00,0x91,0xFF), // 0x12
RGB(0x00,0x99,0xFF), // 0x13
RGB(0x00,0xA1,0xFF), // 0x14
RGB(0x00,0xA9,0xFF), // 0x15
RGB(0x00,0xB1,0xFF), // 0x16
RGB(0x00,0xB9,0xFF), // 0x17
RGB(0x00,0xC1,0xFF), // 0x18
RGB(0x00,0xC9,0xFF), // 0x19
RGB(0x00,0xD1,0xFF), // 0x1A
RGB(0x00,0xD9,0xFF), // 0x1B
RGB(0x00,0xE1,0xFF), // 0x1C
RGB(0x00,0xE9,0xFF), // 0x1D
RGB(0x00,0xF1,0xFF), // 0x1E
RGB(0x00,0xF9,0xFF), // 0x1F
RGB(0x00,0xFF,0xFD), // 0x20
RGB(0x00,0xFF,0xF5), // 0x21
RGB(0x00,0xFF,0xED), // 0x22
RGB(0x00,0xFF,0xE5), // 0x23
RGB(0x00,0xFF,0xDD), // 0x24
RGB(0x00,0xFF,0xD5), // 0x25
RGB(0x00,0xFF,0xCD), // 0x26
RGB(0x00,0xFF,0xC5), // 0x27
RGB(0x00,0xFF,0xBD), // 0x28
RGB(0x00,0xFF,0xB5), // 0x29
RGB(0x00,0xFF,0xAD), // 0x2A
RGB(0x00,0xFF,0xA5), // 0x2B
RGB(0x00,0xFF,0x9D), // 0x2C
RGB(0x00,0xFF,0x95), // 0x2D
RGB(0x00,0xFF,0x8D), // 0x2E
RGB(0x00,0xFF,0x85), // 0x2F
RGB(0x00,0xFF,0x7C), // 0x30
RGB(0x00,0xFF,0x74), // 0x31
RGB(0x00,0xFF,0x6C), // 0x32
RGB(0x00,0xFF,0x64), // 0x33
RGB(0x00,0xFF,0x5C), // 0x34
RGB(0x00,0xFF,0x54), // 0x35
RGB(0x00,0xFF,0x4C), // 0x36
RGB(0x00,0xFF,0x44), // 0x37
RGB(0x00,0xFF,0x3C), // 0x38
RGB(0x00,0xFF,0x34), // 0x39
RGB(0x00,0xFF,0x2C), // 0x3A
RGB(0x00,0xFF,0x24), // 0x3B
RGB(0x00,0xFF,0x1C), // 0x3C
RGB(0x00,0xFF,0x14), // 0x3D
RGB(0x00,0xFF,0x0C), // 0x3E
RGB(0x00,0xFF,0x04), // 0x3F
RGB(0x04,0xFF,0x00), // 0x40
RGB(0x0C,0xFF,0x00), // 0x41
RGB(0x14,0xFF,0x00), // 0x42
RGB(0x1C,0xFF,0x00), // 0x43
RGB(0x24,0xFF,0x00), // 0x44
RGB(0x2C,0xFF,0x00), // 0x45
RGB(0x34,0xFF,0x00), // 0x46
RGB(0x3C,0xFF,0x00), // 0x47
RGB(0x44,0xFF,0x00), // 0x48
RGB(0x4C,0xFF,0x00), // 0x49
RGB(0x54,0xFF,0x00), // 0x4A
RGB(0x5C,0xFF,0x00), // 0x4B
RGB(0x64,0xFF,0x00), // 0x4C
RGB(0x6C,0xFF,0x00), // 0x4D
RGB(0x74,0xFF,0x00), // 0x4E
RGB(0x7C,0xFF,0x00), // 0x4F
RGB(0x85,0xFF,0x00), // 0x50
RGB(0x8D,0xFF,0x00), // 0x51
RGB(0x95,0xFF,0x00), // 0x52
RGB(0x9D,0xFF,0x00), // 0x53
RGB(0xA5,0xFF,0x00), // 0x54
RGB(0xAD,0xFF,0x00), // 0x55
RGB(0xB5,0xFF,0x00), // 0x56
RGB(0xBD,0xFF,0x00), // 0x57
RGB(0xC5,0xFF,0x00), // 0x58
RGB(0xCD,0xFF,0x00), // 0x59
RGB(0xD5,0xFF,0x00), // 0x5A
RGB(0xDD,0xFF,0x00), // 0x5B
RGB(0xE5,0xFF,0x00), // 0x5C
RGB(0xED,0xFF,0x00), // 0x5D
RGB(0xF5,0xFF,0x00), // 0x5E
RGB(0xFD,0xFF,0x00), // 0x5F
RGB(0xFF,0xF9,0x00), // 0x60
RGB(0xFF,0xF1,0x00), // 0x61
RGB(0xFF,0xE9,0x00), // 0x62
RGB(0xFF,0xE1,0x00), // 0x63
RGB(0xFF,0xD9,0x00), // 0x64
RGB(0xFF,0xD1,0x00), // 0x65
RGB(0xFF,0xC9,0x00), // 0x66
RGB(0xFF,0xC1,0x00), // 0x67
RGB(0xFF,0xB9,0x00), // 0x68
RGB(0xFF,0xB1,0x00), // 0x69
RGB(0xFF,0xA9,0x00), // 0x6A
RGB(0xFF,0xA1,0x00), // 0x6B
RGB(0xFF,0x99,0x00), // 0x6C
RGB(0xFF,0x91,0x00), // 0x6D
RGB(0xFF,0x89,0x00), // 0x6E
RGB(0xFF,0x81,0x00), // 0x6F
RGB(0xFF,0x78,0x00), // 0x70
RGB(0xFF,0x70,0x00), // 0x71
RGB(0xFF,0x68,0x00), // 0x72
RGB(0xFF,0x60,0x00), // 0x73
RGB(0xFF,0x58,0x00), // 0x74
RGB(0xFF,0x50,0x00), // 0x75
RGB(0xFF,0x48,0x00), // 0x76
RGB(0xFF,0x40,0x00), // 0x77
RGB(0xFF,0x38,0x00), // 0x78
RGB(0xFF,0x30,0x00), // 0x79
RGB(0xFF,0x28,0x00), // 0x7A
RGB(0xFF,0x20,0x00), // 0x7B
RGB(0xFF,0x18,0x00), // 0x7C
RGB(0xFF,0x10,0x00), // 0x7D
RGB(0xFF,0x08,0x00), // 0x7E
RGB(0xFF,0x00,0x00), // 0x7F
