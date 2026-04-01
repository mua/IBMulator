/*
 * Copyright (C) 2018-2026  Marco Bortolin
 *
 * This file is part of IBMulator.
 *
 * IBMulator is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * IBMulator is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with IBMulator.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ibmulator.h"
#include "vga_dac.h"
#include "utils.h"


const std::string & VGA_DAC::registers_to_string() const
{
	thread_local static std::string s;
	s = "";

	s += str_format("0x%02X %03u Palette Address (Write)\n", write_data_register, write_data_register);
	s += str_format("     %03u Write cycle\n", write_data_cycle);
	s += str_format("0x%02X %03u Palette Address (Read)\n", read_data_register, read_data_register);
	s += str_format("     %03u Read cycle\n", read_data_cycle);
	s += str_format("0x%02X %03u DAC State\n", state, state);
	s += str_format("0x%02X %03u PEL Mask\n", pel_mask, pel_mask);

	s += "      Palette\n";
	for(int i=0; i<256; i++) {
		s += str_format("0x%02X  %03u %03u %03u\n", i, palette[i].red, palette[i].green, palette[i].blue);
	}

	return s;
}
