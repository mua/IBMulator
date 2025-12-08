/*
 * Copyright (C) 2025  Marco Bortolin
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

#ifndef IBMULATOR_TEST_FILE_H
#define IBMULATOR_TEST_FILE_H

#include "hardware/cpu.h"

#if HAVE_ZLIB
#define MOO_USE_ZLIB
#endif
#include "mooreader.h"

struct MachineTest 
{
	Moo::Reader::Test moo;

	void print();
};

struct MachineTestResult
{
	bool is_valid = false;
	Moo::Reader::CpuState cpu_state;
	bool has_exception = false;
	Moo::Reader::Exception exception;

	bool analyze(const MachineTest &_test);
	std::vector<std::string> analysis_log;
};

class TestFile
{
protected:
	std::string m_path;
	Moo::Reader m_reader;
	
public:
	virtual ~TestFile() {}

	const char *path() { return m_path.c_str(); }
	void load(std::string _path);
	CPUFamily cpu_family();
	uint8_t cpu_mode();
	unsigned test_count() { return m_reader.GetHeader().test_count; }
	MachineTest get_test(size_t _index);
};

#endif
