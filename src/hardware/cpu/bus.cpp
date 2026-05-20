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

#include "ibmulator.h"
#include "bus.h"
#include "mmu.h"
#include "../cpu.h"
#include "program.h"
#include <cstring>

CPUBus g_cpubus;

#define PIPELINED_ADDR_286  0
#define PIPELINED_ADDR_386  0
#define MIN_MEM_CYCLES      2

CPUBus::CPUBus()
{
	memset(&m_s, 0, sizeof(m_s));
	m_counters.reset();
}

void CPUBus::init()
{
	m_logger = std::make_shared<MemoryLogger>();
}

void CPUBus::reset()
{
	invalidate_pq();
	m_counters.reset();
	m_pq_cycles_len = 0;
	m_cycles_ahead = 0;
	enable_paging(false);
}

void CPUBus::config_changed()
{
	ini_enum_map_t cpu_busw = {
		{ "286",   16 },
		{ "386SX", 16 },
		{ "386DX", 32 }
	};

	/* http://www.rcollins.org/secrets/PrefetchQueue.html
	 * The 80386 is documented as having a 16-byte prefetch queue. At one time,
	 * it did, but due to a bug in the pipelining architecture, Intel had to
	 * abandon the 16-byte queue, and only use a 12-byte queue. The change
	 * occurred (I believe) between the D0, and D1 step of the '386.
	 * The '386SX wasn't affected by the bug, and therefore hasn't changed.
	 */
	ini_enum_map_t cpu_pqs = {
		{ "286",   6  },
		{ "386SX", 16 },
		{ "386DX", 12 }
	};

	ini_enum_map_t cpu_pqt = {
		{ "286",   2 },
		{ "386SX", 4 },
		{ "386DX", 4 }
	};

	m_width = cpu_busw[g_cpu.model()];
	m_pq_size = cpu_pqs[g_cpu.model()];
	m_pq_thres = cpu_pqt[g_cpu.model()];

	if(g_cpu.family() >= CPU_386) {
		m_paddress = PIPELINED_ADDR_386;
	} else {
		m_paddress = PIPELINED_ADDR_286;
	}

	PINFOF(LOG_V1, LOG_CPU, "  Bus width: %d-bit, Prefetch Queue: %d byte\n",
			m_width, m_pq_size);
}

void CPUBus::save_state(StateBuf &_state)
{
	_state.write(&m_s, {sizeof(m_s), "CPUBus"});
}

void CPUBus::restore_state(StateBuf &_state)
{
	_state.read(&m_s, {sizeof(m_s), "CPUBus"});
	enable_paging(IS_PAGING());
}

void CPUBus::enable_paging(bool _enabled)
{
	if(_enabled) {
		if(m_width == 16) {
			fill_pq_fn = &CPUBus::fill_pq<2,true>;
		} else {
			fill_pq_fn = &CPUBus::fill_pq<4,true>;
		}
	} else {
		if(m_width == 16) {
			fill_pq_fn = &CPUBus::fill_pq<2,false>;
		} else {
			fill_pq_fn = &CPUBus::fill_pq<4,false>;
		}
	}
}

void CPUBus::reset_pq()
{
	m_s.eip = REG_EIP;
	m_s.cseip = REG_CS.desc.base + REG_EIP;
	invalidate_pq();
	m_cycles_ahead = 0;
	m_pq_cycles_len = 0;
}

void CPUBus::update(int _cycles)
{
	// This is where the PQ is updated, which happens either before decoding
	// (if an async CPU event occurred) of after execution.
	// _cycles is the amount of time that the prefetcher has to fill the queue.
	// _cycles is 0:
	//  1. always if before decoding
	//  2. after execution if the PQ is invalid
#if CPU_USE_PQ
		if(m_counters.was_memory_accessed()) {
			m_counters.pmem_cycles += m_cycles_ahead;
			m_cycles_ahead = 0;
		}

		m_counters.pq_avail_cycles = 0;
		if(m_s.pq_valid) {
			m_counters.pq_avail_cycles = _cycles;
			_cycles -= m_cycles_ahead;
			int i = 0;
			if(_cycles > 0) {
				m_cycles_ahead = 0;
				while(_cycles > 0 && i < m_pq_cycles_len) {
					_cycles -= m_pq_cycles[i].cycles;
					i++;
				}
			}
			if(i >= 0 && i < m_pq_cycles_len) {
				// rollback extra fetches
				m_s.pq_tail = m_pq_cycles[i].tail;
				m_s.pq_len = m_s.pq_tail - m_s.pq_left;
				assert(m_s.pq_len >= 0);
				#if CPULOG
				m_logger->rollback(MemoryLogger::CODE, m_pq_cycles_len-i);
				g_memory.logger().rollback(MemoryLogger::CODE, m_pq_cycles_len-i);
				#endif
			}
		} else if(m_pq_cycles_len) {
			#if CPULOG
			m_logger->rollback(MemoryLogger::CODE, m_pq_cycles_len);
			g_memory.logger().rollback(MemoryLogger::CODE, m_pq_cycles_len);
			#endif
		}
		m_pq_cycles_len = 0;

		if(_cycles <= 0) {
			m_cycles_ahead = (-1 * _cycles);
		}

		// this malus is compensated by the bonus at the top
		m_cycles_ahead += m_counters.mem_w_cycles;
#else
		UNUSED(_cycles);
		m_counters.mem_r_cycles += m_counters.mem_w_cycles;
#endif
}

template<int len, bool paging>
uint32_t CPUBus::fill_pq_read_mem(uint32_t _address, int &_cycles)
{
	if(paging) {
		// #PF are thrown here.
		_address = g_cpummu.TLB_lookup(_address, len, IS_USER_PL, false);
	}
	uint32_t value = g_memory.read<len>(_address, _cycles, MemoryLogger::CODE);
	#if CPULOG
	m_logger->push_back({ MemoryLogger::CODE, len, _address, value });
	#endif
	return value;
}

template<int bytes, bool paging>
int CPUBus::fill_pq(const int _amount, const bool _paddress)
{
#if (PIPELINED_ADDR_286 || PIPELINED_ADDR_386)
	int paddress = _paddress * m_paddress;
#else
	UNUSED(_paddress);
#endif

	uint8_t *pq_ptr;

	// move valid bytes to the left
	if(m_s.pq_valid && m_s.pq_len) {
		const int move = pq_idx();
		int len = m_s.pq_len;
		pq_ptr = &m_s.pq[0];
		while(len--) {
			assert((pq_ptr + move) <= &m_s.pq[CPU_PQ_MAX_SIZE]);
			*pq_ptr = *(pq_ptr + move);
			pq_ptr++;
		}
	}
	m_s.pq_left = m_s.cseip;
	m_s.pq_valid = true;
	pq_ptr = &m_s.pq[m_s.pq_len]; // the next free byte slot

	m_pq_cycles_len = 0;

	// beware of 32bit int overflow
	const uint32_t free_space = pq_free_space();
	const uint32_t pq_tail_max {
		(LIKELY(0xffffffff - m_s.pq_tail >= free_space)) ?
		m_s.pq_tail + free_space
		: 0xffffffff
	};

	int amount = _amount;
	int cycles = 0;

	while(((_amount && amount>0) || !_amount) && m_s.pq_tail <= pq_tail_max) {
		const int adv = bytes - (m_s.pq_tail & (bytes-1));

		if(m_s.pq_tail > pq_tail_max - adv) {
			break;
		} else if(UNLIKELY(paging && (PAGE_OFFSET(m_s.pq_tail) + adv) > 4096)) {
			// reads must be inside dword boundaries
			break;
		}

		int c = 0;
		try {
			switch(adv) {
				case 2: { // word aligned
					const uint16_t v = fill_pq_read_mem<2,paging>(m_s.pq_tail, c);
					*pq_ptr = v;
					*(pq_ptr + 1) = v >> 8;
					break;
				}
				case 1: // 1-byte misaligned (right)
					*pq_ptr = fill_pq_read_mem<1,paging>(m_s.pq_tail, c);
					break;
				case 4: { // dword aligned
					const uint32_t v = fill_pq_read_mem<4,paging>(m_s.pq_tail, c);
					*pq_ptr = v;
					*(pq_ptr + 1) = v >> 8;
					*(pq_ptr + 2) = v >> 16;
					*(pq_ptr + 3) = v >> 24;
					break;
				}
				case 3: { // 1-byte misaligned (left)
					const uint32_t v = fill_pq_read_mem<4,paging>(m_s.pq_tail-1, c);
					*pq_ptr = v >> 8;
					*(pq_ptr + 1) = v >> 16;
					*(pq_ptr + 2) = v >> 24;
					break;
				}
				default: assert(false); break;
			}
		} catch(CPUException &) {
			// #PF are catched here
			if(_amount) {
				// a specific amount of data has been requested during instruction decoding.
				m_s.pq_valid = false;
				throw;
			} else {
				// no amount required, queue is filled with 0 or more valid bytes.
				// don't throw exceptions for code prefetching.
				// exceptions are thrown when instruction is executed (see CPUExecutor::execute())
				break;
			}
		}

		if(!_amount) {
			assert(m_pq_cycles_len < CPU_PQ_MAX_SIZE / 2);
			m_pq_cycles[m_pq_cycles_len].tail = m_s.pq_tail;
			m_pq_cycles[m_pq_cycles_len].cycles = c;
			m_pq_cycles_len++;
		}

#if (PIPELINED_ADDR_286 || PIPELINED_ADDR_386)
		c -= paddress;
		paddress = m_paddress;
		c = std::max(c, MIN_MEM_CYCLES);
#endif
		cycles += c;
		pq_ptr += adv;
		m_s.pq_tail += adv;
		amount -= adv;
		m_s.pq_len += adv;
	}

	if(UNLIKELY(amount > 0)) {
		m_s.pq_valid = false;
		throw CPUShutdown("code fetching error");
	}

	return cycles;
}

void CPUBus::prefill_pq()
{
	// we are just before instruction execution.
	// pre-emptively fill up the PQ to anticipate the prefetching that happens
	// during execution.
	// at this point we don't know the available time, fetches that exceeded it
	// will be pruned in update()
	if(m_s.pq_valid && pq_free_space() >= m_pq_thres) {
		(this->*fill_pq_fn)(0, 0);
	}
}

template<>
uint32_t CPUBus::p_mem_read<2>(uint32_t _addr, int &_cycles)
{
	if(((_addr&0x1)==0) || (m_width==32 && ((_addr&0x3)==1))) {
		// even address or a word inside a dword boundary on a 32bit bus
		return g_memory.read_t<2>(_addr, 2, _cycles);
	}
	// odd address and (not 32-bit bus or between dwords)
	int c = -m_paddress;
	uint16_t v = g_memory.read_t<1>(_addr,  2, c) |
	             g_memory.read<1>(  _addr+1,   c) << 8;
	c = std::max(c,MIN_MEM_CYCLES*2);
	_cycles += c;
	g_memory.check_trap(_addr, MEM_TRAP_READ, v, 2);
	return v;
}

template<>
uint32_t CPUBus::p_mem_read<3>(uint32_t _addr, int &_cycles)
{
	// this is called only for unaligned cross page dword reads
	// see cpu/executor/memory.cpp
	if(m_width == 16) {
		if(_addr&0x1) {
			return g_memory.read<1>(_addr,   _cycles) |
			       g_memory.read<2>(_addr+1, _cycles) << 8;
		}
		return g_memory.read<2>(_addr,   _cycles) |
		       g_memory.read<1>(_addr+2, _cycles) << 16;
	} else {
		if(_addr&0x1) {
			return g_memory.read<4>(_addr-1, _cycles) >> 8;
		}
		assert((_addr&0x3) == 0);
		return g_memory.read<4>(_addr, _cycles) & 0x00FFFFFF;
	}
}

template<>
uint32_t CPUBus::p_mem_read<4>(uint32_t _addr, int &_cycles)
{
	uint32_t v;
	int c;
	if(m_width == 16) {
		if((_addr&0x1) == 0) {
			// word aligned
			c = -m_paddress;
			v =	g_memory.read<2>(_addr,   c) |
				g_memory.read<2>(_addr+2, c) << 16;
			c = std::max(c,MIN_MEM_CYCLES*2);
			_cycles += c;
			g_memory.check_trap(_addr, MEM_TRAP_READ, v, 4);
			return v;
		} else {
			c = -(m_paddress*2);
			v =	g_memory.read<1>(_addr,   _cycles) |
				g_memory.read<2>(_addr+1, _cycles) << 8   |
				g_memory.read<1>(_addr+3, _cycles) << 24;
			c = std::max(c,MIN_MEM_CYCLES*3);
			_cycles += c;
			g_memory.check_trap(_addr, MEM_TRAP_READ, v, 4);
			return v;
		}
	} else {
		if((_addr&0x3) == 0) {
			// dword aligned
			return g_memory.read_t<4>(_addr, 4, _cycles);
		}
		c = -m_paddress;
		if((_addr&0x3) == 2) {
			// word aligned
			v = g_memory.read<2>(_addr,   c) |
				g_memory.read<2>(_addr+2, c) << 16;
		} else if((_addr&0x3) == 1) {
			// 1-byte unaligned (left)
			v = g_memory.read<4>(_addr-1, c) >> 8 |
				g_memory.read<1>(_addr+3, c) << 24;
		} else {
			// 1-byte unaligned (right)
			v = g_memory.read<1>(_addr,   c) |
				g_memory.read<4>(_addr+1, c) << 8;
		}
		c = std::max(c,MIN_MEM_CYCLES*2);
		_cycles += c;
		g_memory.check_trap(_addr, MEM_TRAP_READ, v, 4);
		return v;
	}
}

template<>
void CPUBus::p_mem_write<2>(uint32_t _addr, uint32_t _data, int &_cycles)
{
	if(((_addr&0x1)==0) || (m_width==32 && ((_addr&0x3)==1))) {
		// even address or a word inside a dword boundary on a 32bit bus
		g_memory.write_t<2>(_addr, _data, 2, _cycles);
		return;
	}
	// odd address or word across two dwords on 32-bit bus
	int c = -m_paddress;
	g_memory.write_t<1>(_addr,   _data,   2, c);
	g_memory.write<1>(  _addr+1, _data>>8,   c);
	c = std::max(c,MIN_MEM_CYCLES*2);
	_cycles += c;
}

template<>
void CPUBus::p_mem_write<3>(uint32_t _addr, uint32_t _data, int &_cycles)
{
	// this is called only for unaligned cross page dword writes
	// see cpu/executor/memory.cpp
	if(_addr&0x1) {
		g_memory.write<1>(_addr,   _data,    _cycles);
		g_memory.write<2>(_addr+1, _data>>8, _cycles);
	} else {
		g_memory.write<2>(_addr,   _data,     _cycles);
		g_memory.write<1>(_addr+2, _data>>16, _cycles);
	}
}

template<>
void CPUBus::p_mem_write<4>(uint32_t _addr, uint32_t _data, int &_cycles)
{
	int c;
	if(m_width == 16) {
		if((_addr&0x1) == 0) {
			// word aligned
			c = -m_paddress;
			g_memory.write_t<2>(_addr,   _data,    4, c);
			g_memory.write<2>(  _addr+2, _data>>16,   c);
			c = std::max(c,MIN_MEM_CYCLES*2);
		} else {
			c = -(m_paddress*2);
			g_memory.write_t<1>(_addr,   _data,    4, c);
			g_memory.write<2>(  _addr+1, _data>>8,    c);
			g_memory.write<1>(  _addr+3, _data>>24,   c);
			c = std::max(c,MIN_MEM_CYCLES*3);
		}
		_cycles += c;
	} else {
		if((_addr&0x3) == 0) {
			// dword aligned
			g_memory.write_t<4>(_addr, _data, 4, _cycles);
			return;
		}
		c = -m_paddress;
		if((_addr&0x3) == 2) {
			// word aligned
			g_memory.write<2>(_addr,   _data,     c);
			g_memory.write<2>(_addr+2, _data>>16, c);
		} else if((_addr&0x3) == 1) {
			// 1-byte unaligned (left)
			int t = 0;
			uint32_t v = g_memory.read<1>(_addr-1, t);
			v |= (_data<<8);
			g_memory.write<4>(_addr-1, v,         c);
			g_memory.write<1>(_addr+3, _data>>24, c);
		} else {
			// 1-byte unaligned (right)
			g_memory.write<1>(_addr, _data, c);
			int t = 0;
			uint32_t v = g_memory.read<1>(_addr+4, t);
			v = (v<<24) | (_data>>8);
			g_memory.write<4>(_addr+1, v, c);
		}
		c = std::max(c,MIN_MEM_CYCLES*2);
		_cycles += c;
		g_memory.check_trap(_addr, MEM_TRAP_READ, _data, 4);
	}
}

int CPUBus::write_pq_to_logfile(FILE *_dest)
{
	int res = 0;
	if(is_pq_valid()) {
		res = fprintf(_dest, "v");
	} else {
		res = fprintf(_dest, " ");
	}
	if(res<0) {
		return -1;
	}

	if(pq_is_empty()) {
		res = fprintf(_dest, "e");
	} else {
		res = fprintf(_dest, " ");
	}
	if(res<0) {
		return -1;
	}

	if(fprintf(_dest, " %2d [ ", m_s.pq_len) < 0) {
		return -1;
	}
	for(int i=0; i<m_pq_size; i++) {
		if(!m_s.pq_len || (i < pq_idx() || i >= pq_idx() + m_s.pq_len)) {
			if(fprintf(_dest, "-- ") < 0) {
				return -1;
			}
		} else {
			if(fprintf(_dest, "%02X ", m_s.pq[i]) < 0) {
				return -1;
			}
		}
	}
	if(fprintf(_dest, "]") < 0) {
		return -1;
	}

	return 0;
}
