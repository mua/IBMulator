/*
 * Copyright (C) 2025-2026  Marco Bortolin
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
			analysis_log.push_back(
				str_format("exception expected = 0x%02X (%s)",
					_test.moo.exception.number,
					(_test.moo.exception.number <= 16) ? TestFile::EXC[_test.moo.exception.number] : "INT"
				)
			);
		} else {
			analysis_log.push_back(
				str_format("exception not expected = 0x%02X (%s)",
					exception.number,
					(has_soft_int) ? "INT" : TestFile::EXC[exception.number]
				)
			);
		}
		result = false;
	} else if(_test.moo.has_exception && _test.moo.exception.number != exception.number) {
		analysis_log.push_back(str_format("exception, expected = 0x%02X (%s), actual = 0x%02X (%s)",
				_test.moo.exception.number, (_test.moo.exception.number <= 16) ? TestFile::EXC[_test.moo.exception.number] : "INT",
				exception.number, (has_soft_int) ? "INT" : TestFile::EXC[exception.number]
		));
		result = false;
	}

	uint16_t flags_mask = _test.get_flags_mask();
	if(_test.moo.final_state.regs.type == Moo::Reader::RegisterState::REG_16) {
		for(const auto r : Moo::REG16Range()) {
			auto [ expected, mask ] = _test.moo.GetFinalRegister(r);
			if(r == Moo::REG16::FLAGS) {
				if(mask != 0xFFFF) {
					flags_mask = mask;
				} else {
					mask = flags_mask;
				}
			}
			expected &= mask;
			uint16_t actual = cpu_state.regs.GetRegister(r) & mask;
			if(expected != actual) {
				std::string mex;
				if(r == Moo::REG16::FLAGS) {
					std::string exp_flags = CPULogger::decode_eflags(expected, false, mask);
					std::string act_flags = CPULogger::decode_eflags(actual, false, mask);
					mex = str_format("register = %s, expected = 0x%04X [%s], actual = 0x%04X [%s], mask = 0x%04X",
							Moo::GetRegister16Name(r),
							expected, exp_flags.c_str(),
							actual, act_flags.c_str(),
							mask
						);
				} else {
					mex = str_format("register = %s, expected = 0x%04X, actual = 0x%04X",
							Moo::GetRegister16Name(r), expected, actual);
				}
				analysis_log.push_back(mex);
				result = false;
			}
		}
	} else if(_test.moo.final_state.regs.type == Moo::Reader::RegisterState::REG_32) {
		for(const auto r : Moo::REG32Range()) {
			auto [ expected, mask ] = _test.moo.GetFinalRegister(r);
			if(r == Moo::REG32::EFLAGS) {
				if(mask != 0xFFFFFFFF) {
					flags_mask = mask;
				} else {
					mask = flags_mask;
				}
			}
			expected &= mask;
			uint32_t actual = cpu_state.regs.GetRegister(r) & mask;
			if(expected != actual) {
				std::string mex;
				if(r == Moo::REG32::EFLAGS) {
					std::string exp_flags = CPULogger::decode_eflags(expected, true, mask);
					std::string act_flags = CPULogger::decode_eflags(actual, true, mask);
					mex = str_format("register = %s, expected = 0x%08X [%s], actual = 0x%08X [%s], mask = 0x%04X",
							Moo::GetRegister32Name(r),
							expected, exp_flags.c_str(),
							actual, act_flags.c_str(),
							mask
						);
				} else {
					mex = str_format("register = %s, expected = 0x%08X, actual = 0x%08X",
							Moo::GetRegister32Name(r), expected, actual);
				}
				analysis_log.push_back(mex);
				result = false;
			}
		}
	} else {
		assert(false);
	}
	uint16_t pushed_actual_flags = 0;
	uint16_t pushed_expected_flags = 0;
	for(const auto expected : _test.moo.final_state.ram) {
		for(const auto actual : cpu_state.ram) {
			if(actual.address == expected.address) {
				if(_test.moo.has_exception && (expected.address == _test.moo.exception.flag_addr || expected.address == _test.moo.exception.flag_addr+1)) {
					if(expected.address == _test.moo.exception.flag_addr) {
						pushed_expected_flags |= expected.value;
						pushed_actual_flags |= actual.value;
					} else {
						pushed_expected_flags |= int(expected.value) << 8;
						pushed_actual_flags |= int(actual.value) << 8;
					}
					continue;
				}
				if(actual.value != expected.value) {
					analysis_log.push_back(str_format("memory at [0x%05X], expected = 0x%02X, actual = 0x%02X",
							expected.address, expected.value, actual.value));
					result = false;
				}
				break;
			}
		}
	}
	if(_test.moo.has_exception) {
		pushed_actual_flags &= flags_mask;
		pushed_expected_flags &= flags_mask;
		if(pushed_actual_flags != pushed_expected_flags) {
			analysis_log.push_back(str_format("flags memory at [0x%05X], expected = 0x%04X, actual = 0x%04X",
					_test.moo.exception.flag_addr, pushed_expected_flags, pushed_actual_flags));
			result = false;
		}
	}

	if(has_cpu_log) {
		analysis_log.push_back("execution:");

		std::vector<MemoryLogger::LogData> mem_access;
		for(size_t i=0; i<cpu_log.size(); i++) {
			std::string bytes;
			const Instruction & instr = cpu_log[i].instr;
			for(size_t b=0; b < instr.size; b++) {
				bytes += str_format("%02X ", instr.bytes[b]);
			}
			analysis_log.push_back(str_format(" [%u] instruction bytes (%u): %s", i, instr.size, bytes.c_str()));
			analysis_log.push_back(str_format("  bus requests: %u", cpu_log[i].bus_logger.get_log_entries()));
			size_t entry_n = 0;
			for(size_t j=0; j<cpu_log[i].bus_logger.get_log_size(); j++) {
				auto & memlog = cpu_log[i].bus_logger.get_log()[j];
				entry_n += memlog.size == 0 ? 0 : 1;
				constexpr const char *op[] = { "MEMR", "MEMW", "CODE" };
				std::string data = "0x0000";
				switch(memlog.size) {
					case 1: data = str_format("0x%02X", memlog.data); break;
					case 2: data = str_format("0x%04X", memlog.data); break;
					case 4: data = str_format("0x%08X", memlog.data); break;
					default: break;
				}
				analysis_log.push_back(str_format("   [%s] Addr=0x%06X Data=%s Bus=%s Size=%u",
						memlog.size == 0 ? "  *" : str_format("%*u", 3, entry_n).c_str(),
						memlog.phy_addr,
						data.c_str(),
						op[memlog.operation],
						memlog.size
				));
				if(memlog.operation != MemoryLogger::CODE) {
					// moo data bus is 16bit and its log is of the single accesses.
					// eg: a misaligned dword read would have 3 MEMR ops (1+2+1 bytes) (usually)
					// bus_logger log is only of the data request, not the single ops.
					// i will only check the first requested data address and not the data values.
					// for now i'm only interested to see if data is trasferred from the the correct places
					// as a correct value might be only by chance.
					// TODO check ops and order of access from mem_logger instead
					mem_access.push_back(memlog);
				}
			}
			if(cpu_log[i].mem_logger.get_log_size()) {
				analysis_log.push_back(str_format("  data transfers: %u", cpu_log[i].mem_logger.get_log_entries()));
				unsigned rcount = 0, wcount = 0, ccount = 0;
				unsigned rsize = 0, wsize = 0, csize = 0;
				entry_n = 0;
				for(size_t j=0; j<cpu_log[i].mem_logger.get_log_size(); j++) {
					auto & memlog = cpu_log[i].mem_logger.get_log()[j];
					entry_n += memlog.size == 0 ? 0 : 1;
					constexpr const char *op[] = { "MEMR", "MEMW", "CODE" };
					analysis_log.push_back(str_format("   [%s] Addr=0x%06X Data=0x%04X Bus=%s Size=%u",
							memlog.size == 0 ? "  *" : str_format("%*u", 3, entry_n).c_str(),
							memlog.phy_addr,
							memlog.data,
							op[memlog.operation],
							memlog.size
					));
					if(memlog.operation == MemoryLogger::MEMR) {
						rcount++;
						rsize += memlog.size;
					} else if(memlog.operation == MemoryLogger::MEMW) {
						wcount++;
						wsize += memlog.size;
					} else {
						ccount++;
						csize += memlog.size;
					}
				}
				analysis_log.push_back(str_format("   R:%u (%u bytes), W:%u (%u bytes), C:%u (%u bytes)", 
						rcount, rsize, wcount, wsize, ccount, csize));
			}
			if(i == 0 && _test.moo.init_state.has_ea32) {
				if(!instr.modrm.is_valid) {
					analysis_log.push_back("  decoding error: ModRM missing");
					result = false;
				} else {
					uint8_t modrm = instr.modrm.rm | (instr.modrm.r << 3) | (instr.modrm.mod << 6);
					analysis_log.push_back(str_format("  ModRM = 0x%02X (MOD = %u, R/M = %u, r = %u), disp = 0x%08X",
							modrm, instr.modrm.mod, instr.modrm.rm, instr.modrm.r,
							instr.modrm.disp));
					if(cpu_log[i].instr.modrm.has_sib) {
						uint8_t sib = instr.modrm.base | (instr.modrm.index << 3) | (instr.modrm.scale << 6);
						bool undef_sib = (sib >= 0x60 && sib <= 0x67) ||
										 (sib >= 0xa0 && sib <= 0xa7) ||
										 (sib >= 0xe0 && sib <= 0xe7);
						analysis_log.push_back(str_format("  SIB = 0x%02X (Base = %u, Index = %u, SS = %u) %s",
								sib, instr.modrm.base, instr.modrm.index, instr.modrm.scale, undef_sib ? "UNDEFINED" : ""));
					}
				}
			}
		}

		std::vector<MemoryLogger::LogData> moo_mem_access;
		for(const auto & cycle : _test.moo.cycles) {
			if(_test.moo.cpu_type == Moo::Reader::CPU286 && (cycle.bus_status == 5 || cycle.bus_status == 6)) {
				// MEMR = 5, MEMW = 6
				moo_mem_access.push_back({
					cycle.bus_status == 5 ? 0u : 1u, 2, cycle.address_latch, cycle.data_bus
				});
			} else if(_test.moo.cpu_type == Moo::Reader::CPU386E && (cycle.bus_status == 6 || cycle.bus_status == 7)) {
				// MEMR = 6, MEMW = 7
				moo_mem_access.push_back({
					cycle.bus_status == 6 ? 0u : 1u, 2, cycle.address_latch, cycle.data_bus
				});
			}
		}
		// let's see if data is from the correct addresses
		// TODO for now it's only a presence check
		for(const auto & memop : mem_access) {
			bool found = false;
			for(const auto & moo_memop : moo_mem_access) {
				if(memop.phy_addr == moo_memop.phy_addr && memop.operation == moo_memop.operation) {
					// the value of data is verified above
					found = true;
					break;
				}
			}
			if(!found) {
				analysis_log.push_back(str_format("%s Addr=0x%06X not found",
					memop.operation == MemoryLogger::MEMR ? "MEMR" : "MEMW", memop.phy_addr));
				result = false;
			}
		}
	}

	return result;
}

void MachineTest::print(unsigned _log_pri)
{
	auto print_state = [&](Moo::Reader::CpuState &_state){
		PINFOF(_log_pri, LOG_PROGRAM, "   registers:\n");
		const auto &regs = _state.regs;
		const auto &masks = _state.masks;
		for(int i = 0; i < 32; i++) {
			if(regs.bitmask & (1 << i)) {
				PINFOF(_log_pri, LOG_PROGRAM, "    %s = 0x%08X",
						Moo::Reader::GetRegisterName(i, moo.cpu_type),
						regs.values[i]);
				if(masks.is_populated && (masks.bitmask & (1 << i))) {
					PINFOF(_log_pri, LOG_PROGRAM, ", mask = 0x%08X", masks.values[i]);
				}
				PINFOF(_log_pri, LOG_PROGRAM, "\n");
			}
		}
		PINFOF(_log_pri, LOG_PROGRAM, "   RAM entries: %u\n", _state.ram.size());
		for(size_t i = 0; i < _state.ram.size(); i++) {
			const auto &entry = _state.ram[i];
			PINFOF(_log_pri, LOG_PROGRAM, "    [0x%05X] = 0x%02X\n", entry.address, entry.value);
		}
		if(_state.has_queue) {
			PINFOF(_log_pri, LOG_PROGRAM, "   Instruction queue (%u bytes): ", _state.queue.bytes.size());
			for (const uint8_t byte : _state.queue.bytes) {
				PINFOF(_log_pri, LOG_PROGRAM, "%02X ", byte);
			}
			PINFOF(_log_pri, LOG_PROGRAM, "\n");
		}
		if(_state.has_ea32) {
			PINFOF(_log_pri, LOG_PROGRAM, "   Effective Address:\n");
			if(_state.ea32.segreg > 5) {
				PINFOF(_log_pri, LOG_PROGRAM, "    invalid\n");
			} else {
				static constexpr const char *segregs[]{
					"CS","SS","DS","ES","FS","GS"
				};
				PINFOF(_log_pri, LOG_PROGRAM, "    register: %s\n", segregs[_state.ea32.segreg]);
				PINFOF(_log_pri, LOG_PROGRAM, "    segment selector value: 0x%04X\n", _state.ea32.segsel);
				PINFOF(_log_pri, LOG_PROGRAM, "    segment base address: 0x%08X\n", _state.ea32.segbase);
				PINFOF(_log_pri, LOG_PROGRAM, "    segment limit: 0x%08X\n", _state.ea32.seglimit);
				PINFOF(_log_pri, LOG_PROGRAM, "    offset: 0x%08X\n", _state.ea32.offset);
				PINFOF(_log_pri, LOG_PROGRAM, "    linear address: 0x%08X\n", _state.ea32.linaddr);
				PINFOF(_log_pri, LOG_PROGRAM, "    physical address: 0x%08X\n", _state.ea32.phyaddr);
			}
		}
	};
	
	PINFOF(_log_pri, LOG_PROGRAM, "  instruction bytes (%u): ", moo.bytes.size());
	for(const uint8_t byte : moo.bytes) {
		PINFOF(_log_pri, LOG_PROGRAM, "%02X ", byte);
	}
	PINFOF(_log_pri, LOG_PROGRAM, "\n");
	PINFOF(_log_pri, LOG_PROGRAM, "  instruction mnemonic: %s\n", moo.name.c_str());
	
	PINFOF(_log_pri, LOG_PROGRAM, "  initial state:\n");
	print_state(moo.init_state);

	PINFOF(_log_pri, LOG_PROGRAM, "  final state:\n");
	print_state(moo.final_state);
	
	if(moo.has_exception) {
		PINFOF(_log_pri, LOG_PROGRAM, "   exception: \n");
		PINFOF(_log_pri, LOG_PROGRAM, "    number = 0x%02X %s\n",
				moo.exception.number,
				(moo.exception.number <= 16) ? TestFile::EXC[moo.exception.number] : "(INT)"
		);
		PINFOF(_log_pri, LOG_PROGRAM, "    flag address = [0x%05X]\n", moo.exception.flag_addr);
	}

	PINFOF(_log_pri, LOG_PROGRAM, "  bus cycles: %u\n", moo.cycles.size());
	std::vector<MemoryLogger::LogData> tx;
	for(size_t i = 0; i < moo.cycles.size(); i++) {
		const auto &cycle = moo.cycles[i];
		std::string busstatus = Moo::Reader::GetBusStatusName(cycle.bus_status, moo.cpu_type);
		if(cycle.t_state == 0) {
			PINFOF(_log_pri, LOG_PROGRAM, "   [%*u] Bus=%s T=%s\n", 3,
				i+1, busstatus.c_str(), Moo::Reader::GetTStateName(cycle.t_state, moo.cpu_type)
			);
		} else {
			PINFOF(_log_pri, LOG_PROGRAM, "   [%*u] Addr=0x%06X Data=0x%04X Bus=%s T=%s ALE=%d BHE=%d\n", 3,
				i+1, cycle.address_latch, cycle.data_bus,
				busstatus.c_str(),
				Moo::Reader::GetTStateName(cycle.t_state, moo.cpu_type),
				cycle.pin_bitfield0.ale,
				cycle.pin_bitfield0.bhe // Active = 0, Inactive = 1
			);
			MemoryLogger::LogData op;
			if(cycle.t_state == 1 && (busstatus == "MEMR" || busstatus == "MEMW" || busstatus == "CODE")) {
				op.phy_addr = cycle.address_latch;
				if(busstatus == "MEMR") {
					op.operation = MemoryLogger::MEMR;
				} else if(busstatus == "MEMW") {
					op.operation = MemoryLogger::MEMW;
				} else {
					op.operation = MemoryLogger::CODE;
				}
				op.size = (cycle.pin_bitfield0.bhe == 0 && (cycle.address_latch & 1) == 0) ? 2 : 1;
				assert(i+1 < moo.cycles.size());
				if(op.size == 2) {
					op.data = moo.cycles[i+1].data_bus;
				} else if(cycle.pin_bitfield0.bhe == 1) {
					op.data = moo.cycles[i+1].data_bus & 0xff;
				} else {
					op.data = moo.cycles[i+1].data_bus >> 8;
				}
				tx.push_back(op);
			}
		}
	}

	PINFOF(_log_pri, LOG_PROGRAM, "  data transfers: %u\n", tx.size());
	unsigned rcount = 0, wcount = 0, ccount = 0;
	unsigned rsize = 0, wsize = 0, csize = 0;
	for(size_t i = 0; i < tx.size(); i++) {
		constexpr const char *op[] = { "MEMR", "MEMW", "CODE" };
		PINFOF(_log_pri, LOG_PROGRAM, "   [%*u] Addr=0x%06X Data=0x%04X Bus=%s Size=%u\n", 3,
			i+1, tx[i].phy_addr, tx[i].data, op[tx[i].operation], tx[i].size
		);
		if(tx[i].operation == MemoryLogger::MEMR) {
			rcount++;
			rsize += tx[i].size;
		} else if(tx[i].operation == MemoryLogger::MEMW) {
			wcount++;
			wsize += tx[i].size;
		} else {
			ccount++;
			csize += tx[i].size;
		}
	}
	PINFOF(_log_pri, LOG_PROGRAM, str_format("   R:%u (%u bytes), W:%u (%u bytes), C:%u (%u bytes)\n",
			rcount, rsize, wcount, wsize, ccount, csize).c_str());

	if(moo.has_hash) {
		PINFOF(_log_pri, LOG_PROGRAM, "  hash: ");
		for(int i = 0; i < 20; i++) {
			PINFOF(_log_pri, LOG_PROGRAM, "%02X", moo.hash[i]);
		}
		PINFOF(_log_pri, LOG_PROGRAM, "\n");
	}
}

uint32_t MachineTest::get_flags_mask() const
{
	// for special cases only, based on current version of MOO tests that lack masks

	uint32_t mask = FMASK_VALID;

	std::vector<uint8_t> prefixes{
		0x26, 0x2E, 0x36, 0x3E, 0x64, 0x65, 0x66, 0x67, 0xF0, 0xF1, 0xF2, 0xF3
	};

	bool addr32 = false;
	bool op32 = false;
	size_t b = 0;
	while(std::find(prefixes.begin(), prefixes.end(), moo.bytes[b]) != prefixes.end()) {
		switch(moo.bytes[b++]) {
			case 0x66:
				op32 = true;
				break;
			case 0x67:
				addr32 = true;
				break;
			default:
				break;
		}
	}
	ModRM modrm;
	switch(moo.bytes[b++]) {
		case 0x08: case 0x09: case 0x0A: case 0x0B: case 0x0C: case 0x0D: // OR
			mask = FMASK_OF | FMASK_SF | FMASK_ZF | FMASK_PF | FMASK_CF;
			break;
		case 0x8D: // LEA
			break;
		case 0x0F: {
			switch(moo.bytes[b++]) {
				case 0xA3: // BT
				case 0xAB: // BTS
				case 0xB3: // BTR
				case 0xBA: // BT/BTS/BTR/BTC
				case 0xBB: // BTC
					mask = FMASK_CF | FMASK_ZF;
					break;
				case 0xBC: // BSF
				case 0xBD: // BSR
					mask = FMASK_ZF;
					break;
				case 0xA4: // SHLD
				case 0xAC: // SHRD
				{
					modrm.load(moo.bytes, b, addr32);
					uint32_t sh_amnt = moo.bytes[b++] % 32;
					if(!op32 && sh_amnt > 16) {
						PDEBUGF(LOG_V0, LOG_PROGRAM, "MOO: '%s': SHLD/SHRD count > 16 (%u) is undefined.\n",
								moo.name.c_str(), sh_amnt);
						mask = 0;
					} else {
						mask = FMASK_OF | FMASK_SF | FMASK_ZF | FMASK_PF | FMASK_CF;
					}
					break;
				}
				case 0xA5: // SHLD
				case 0xAD: // SHRD
				{
					uint32_t sh_amnt = (moo.GetInitialRegister(Moo::REG32::ECX) & 0xFF) % 32;
					if(!op32 && sh_amnt > 16) {
						PDEBUGF(LOG_V0, LOG_PROGRAM, "MOO: '%s': SHLD/SHRD count > 16 (%u) is undefined.\n",
								moo.name.c_str(), sh_amnt);
						mask = 0;
					} else {
						mask = FMASK_OF | FMASK_SF | FMASK_ZF | FMASK_PF | FMASK_CF;
					}
					break;
				}
				case 0xAF: // IMUL
					mask = FMASK_OF | FMASK_CF;
					break;
				default: break;
			}
			break;
		}
		case 0x37: // AAA
			// we want to test all the flags because we implement UB
			break;
		case 0x62: // BOUND
			mask = FMASK_IF;
			break;
		case 0x69: // IMUL
		case 0x6B: // IMUL
			mask = FMASK_OF | FMASK_CF;
			break;
		case 0xC0: case 0xC1:
		case 0xD0: case 0xD1: case 0xD2: case 0xD3:
		{
			modrm.load(moo.bytes, b, addr32);
			switch(modrm.n) {
				case 0: // ROL
				case 1: // ROR
					mask = FMASK_OF | FMASK_SF | FMASK_ZF | FMASK_AF | FMASK_PF | FMASK_CF;
					break;
				case 2: // RCL
				case 3: // RCR:
				case 4: // SHL/SAL
				case 5: // SHR
				case 6: // SHL/SAL
					// we want to test all the flags because we implement UB
					break;
				case 7: // SAR
					mask = FMASK_OF | FMASK_SF | FMASK_ZF | FMASK_PF | FMASK_CF;
					break;
				default:
					break;
			}
			break;
		}
		case 0xF6:
		case 0xF7: {
			modrm.load(moo.bytes, b, addr32);
			switch(modrm.n) {
				case 3: // NEG
					mask = FMASK_OF | FMASK_SF | FMASK_ZF | FMASK_AF | FMASK_PF | FMASK_CF;
					break;
				case 4: // MUL
				case 5: // IMUL
					mask = FMASK_OF | FMASK_CF;
					break;
				case 6: // DIV
				case 7: // IDIV
					mask = 0;
					break;
				default:
					break;
			}
			break;
		}

		default:
			break;
	}
	return mask;
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
	const auto meta = m_reader.GetMeta();
	PINFOF(LOG_V0, LOG_PROGRAM, "MOO test file information:\n");
	PINFOF(LOG_V0, LOG_PROGRAM, " version: %u.%u\n", header.version_major, header.version_minor);
	PINFOF(LOG_V0, LOG_PROGRAM, " CPU: %s\n", header.cpu_name.c_str());
	PINFOF(LOG_V0, LOG_PROGRAM, " opcode: 0x%08X\n", meta.opcode);
	PINFOF(LOG_V0, LOG_PROGRAM, " mnemonic: %s\n", meta.mnemonic.c_str());
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
