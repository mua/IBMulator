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

#include "ibmulator.h"
#include "testfile.h"
#include "utils.h"

bool MachineTestResult::analyze(const MachineTest &_test)
{
	if(!is_valid) {
		analysis_log.push_back("invalid state result");
		return false;
	}

	bool result = true;

	if(_test.moo.has_exception != has_exception) {
		if(_test.moo.has_exception) {
			analysis_log.push_back(str_format("exception expected = 0x%02X", _test.moo.exception.number));
		} else {
			analysis_log.push_back(str_format("exception not expected = 0x%02X", exception.number));
		}
		result = false;
	} else if(_test.moo.has_exception && _test.moo.exception.number != exception.number) {
		analysis_log.push_back(str_format("exception, expected = 0x%02X, actual = 0x%02X",
				_test.moo.exception.number, exception.number));
		result = false;
	}

	if(_test.moo.final_state.regs.type == Moo::Reader::RegisterState::REG_16) {
		for(const auto r : Moo::REG16Range()) {
			auto [ expected, mask ] = _test.moo.GetFinalRegister(r);
			uint16_t actual = cpu_state.regs.GetRegister(r) & mask;
			if(expected != actual) {
				analysis_log.push_back(str_format("register = %s, expected = 0x%04X, actual = 0x%04X",
						Moo::GetRegister16Name(r), expected, actual));
				result = false;
			}
		}
	} else if(_test.moo.final_state.regs.type == Moo::Reader::RegisterState::REG_32) {
		for(const auto r : Moo::REG32Range()) {
			auto [ expected, mask ] = _test.moo.GetFinalRegister(r);
			uint32_t actual = cpu_state.regs.GetRegister(r) & mask;
			if(expected != actual) {
				analysis_log.push_back(str_format("register = %s, expected = 0x%08X, actual = 0x%08X",
						Moo::GetRegister32Name(r), expected, actual));
				result = false;
			}
		}
	} else {
		assert(false);
	}

	for(const auto expected : _test.moo.final_state.ram) {
		for(const auto actual : cpu_state.ram) {
			if(actual.address == expected.address) {
				if(actual.value != expected.value) {
					analysis_log.push_back(str_format("memory at [0x%05X], expected = 0x%02X, actual = 0x%02X",
							expected.address, expected.value, actual.value));
					result = false;
				}
				break;
			}
		}
	}

	return result;
}

void MachineTest::print()
{
	auto print_state = [&](Moo::Reader::CpuState &_state){
		PINFOF(LOG_V0, LOG_PROGRAM, "   registers:\n");
		const auto &regs = _state.regs;
		for(int i = 0; i < 32; i++) {
			if(regs.bitmask & (1 << i)) {
				PINFOF(LOG_V0, LOG_PROGRAM, "    %s = 0x%08X\n", Moo::Reader::GetRegisterName(i, moo.cpu_type), regs.values[i]);
			}
		}
		PINFOF(LOG_V0, LOG_PROGRAM, "   RAM entries: %u\n", _state.ram.size());
		for(size_t i = 0; i < _state.ram.size(); i++) {
			const auto &entry = _state.ram[i];
			PINFOF(LOG_V0, LOG_PROGRAM, "    [0x%05X] = 0x%02X\n", entry.address, entry.value);
		}
		if(_state.has_queue) {
			PINFOF(LOG_V0, LOG_PROGRAM, "    queue (%u bytes): ", _state.queue.bytes.size());
			for (const uint8_t byte : _state.queue.bytes) {
				PINFOF(LOG_V0, LOG_PROGRAM, "%02X ", byte);
			}
			PINFOF(LOG_V0, LOG_PROGRAM, "\n");
		}
	};
	
	PINFOF(LOG_V0, LOG_PROGRAM, "  instruction bytes (%u): ", moo.bytes.size());
	for(const uint8_t byte : moo.bytes) {
		PINFOF(LOG_V0, LOG_PROGRAM, "%02X ", byte);
	}
	PINFOF(LOG_V0, LOG_PROGRAM, "\n");
	PINFOF(LOG_V0, LOG_PROGRAM, "  instruction mnemonic: %s\n", moo.name.c_str());
	
	PINFOF(LOG_V0, LOG_PROGRAM, "  initial state:\n");
	print_state(moo.init_state);

	PINFOF(LOG_V0, LOG_PROGRAM, "  final state:\n");
	print_state(moo.final_state);
	
	if(moo.has_exception) {
		PINFOF(LOG_V0, LOG_PROGRAM, "   exception: \n");
		PINFOF(LOG_V0, LOG_PROGRAM, "    number = 0x%02X\n", moo.exception.number);
		PINFOF(LOG_V0, LOG_PROGRAM, "    flag address = [0x%05X]\n", moo.exception.flag_addr);
	}

	PINFOF(LOG_V0, LOG_PROGRAM, "  cycles: %u\n", moo.cycles.size());
	for (size_t i = 0; i < moo.cycles.size(); i++) {
		const auto &cycle = moo.cycles[i];
		PINFOF(LOG_V0, LOG_PROGRAM, "   [%u] Addr=0x%05X Data=0x%04X Bus=%s T=%s Q=%s\n",
				i, cycle.address_latch, cycle.data_bus,
				Moo::Reader::GetBusStatusName(cycle.bus_status, moo.cpu_type),
				Moo::Reader::GetBusStatusName(cycle.t_state, moo.cpu_type),
				Moo::Reader::GetBusStatusName(cycle.queue_op_status, moo.cpu_type)
				);
	}

	if(moo.has_exception) {
		PINFOF(LOG_V0, LOG_PROGRAM, "  exception: number=%u, flag addr.=0x%X\n", moo.exception.number, moo.exception.flag_addr);
	}
	if(moo.has_hash) {
		PINFOF(LOG_V0, LOG_PROGRAM, "  hash: ");
		for(int i = 0; i < 20; i++) {
			PINFOF(LOG_V0, LOG_PROGRAM, "%02X", moo.hash[i]);
		}
		PINFOF(LOG_V0, LOG_PROGRAM, "\n");
	}
}

MachineTest TestFile::get_test(size_t _index)
{
	return { m_reader[_index] };
}

void TestFile::load(std::string _path)
{
	m_path = _path;

	m_reader.AddFromFile(_path);

	const auto header = m_reader.GetHeader();
	PINFOF(LOG_V0, LOG_PROGRAM, "MOO test file information:\n");
	PINFOF(LOG_V0, LOG_PROGRAM, " version: %u.%u\n", header.version_major, header.version_minor);
	PINFOF(LOG_V0, LOG_PROGRAM, " CPU: %s\n", header.cpu_name.c_str());
	PINFOF(LOG_V0, LOG_PROGRAM, " test count: %u\n", header.test_count);
}

CPUFamily TestFile::cpu_family()
{
	switch(m_reader.GetHeader().cpu_type) {
		case Moo::Reader::CPU286:
			return CPU_286;
		case Moo::Reader::CPU386E:
			return CPU_386;
		default:
			throw std::runtime_error("unsupported CPU family");
	}
}

uint8_t TestFile::cpu_mode()
{
	return m_reader.GetMeta().cpu_mode;
}
