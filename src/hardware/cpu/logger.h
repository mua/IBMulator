/*
 * Copyright (C) 2015-2026  Marco Bortolin
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

#ifndef IBMULATOR_CPU_LOGGER_H
#define IBMULATOR_CPU_LOGGER_H

#include "core.h"
#include "decoder.h"
#include "state.h"
#include "exception.h"

struct CPULogIRQ
{
	uint8_t irq;
	uint8_t vector;
};

struct CPULogEntry
{
	uint64_t time;
	CPUState state;
	CPUCore core;
	CPUException exc;
	CPUBus bus;
	MemoryLogger bus_logger;
	MemoryLogger mem_logger;
	Instruction instr;
	CPUCycles cycles;
	CPULogIRQ irq;
};

class CPULogger
{
public:
	using CPULog = std::vector<CPULogEntry>;

private:
	uint m_log_idx = 0;
	uint m_log_size = 0;
	CPULog m_log;
	uint32_t m_iret_address = 0;
	CPULogIRQ m_irq = {0xFF,0};
	FILE *m_log_file = nullptr;
	std::string m_log_filename;
	std::map<int,uint64_t> m_global_counters;
	std::map<int,uint64_t> m_file_counters;

	static int get_opcode_index(const Instruction &_instr);
	static int write_entry(FILE *_dest, CPULogEntry &_entry);
	static const std::string & disasm(CPULogEntry &_log_entry);
	static void write_counters(const std::string _filename, std::map<int,uint64_t> &_cnt);
	static int write_segreg(FILE *_dest, const CPUCore &_core, const SegReg &_segreg, const char *_name);
public:
	static const char* decode_eflags(uint32_t _eflags, bool _32bit, uint32_t _mask = 0xFFFFFFFF);

public:

	CPULogger();
	~CPULogger();

	void add_entry(
		uint64_t _time,
		const Instruction &_instr,
		const CPUState &_state,
		const CPUException &_exc,
		const CPUCore &_core,
		const CPUBus &_bus,
		const MemoryLogger &_mem_logger,
		const CPUCycles &_cycles
	);
	void set_prev_i_exc(const CPUException &_exc, uint32_t _cseip);
	void set_next_i_irq(uint8_t _irq, uint8_t _vector);
	void open_file(const std::string _filename);
	void close_file();
	void set_iret_address(uint32_t _address);
	void reset_iret_address();
	void reset_global_counters();
	void reset_file_counters();
	void dump(const std::string _filename);
	unsigned get_log_size() const { return m_log_size; }
	const CPULogEntry & get_log_entry(size_t _idx) const;
};

#endif
