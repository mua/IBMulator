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
#include "vga_genregs.h"


const std::string & VGA_GenRegs::registers_to_string() const
{
	thread_local static std::string s;
	s = "";

	s += str_format("0x%02X %03u  Miscellaneous Output [%s]\n", (uint8_t)misc_output, (uint8_t)misc_output, (const char*)misc_output);
	s += str_format("0x%02X %03u  Video Subsystem Enable\n", video_enable, video_enable);

	return s;
}