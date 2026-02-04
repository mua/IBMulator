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

#ifndef IBMULATOR_CPU_BUS_H
#define IBMULATOR_CPU_BUS_H

#include "core.h"
#include "mmu.h"
#include "../memory.h"

#define CPU_USE_PQ       true
#define CPU_PQ_MAX_SIZE  16

class CPUBus;
extern CPUBus g_cpubus;


class CPUBus
{
private:
	struct {
		// anatomy of the PQ:
		//                 ┌------[len]-----┐
		// addr: left    cseip             tail
		//       ╔══╤    ╤══╤══╤══╤══╤══╤══╤══╤    ╤══╗
		// data: ║  │ .. │  │  │  │  │  │  │  │ .. │  ║
		//       ╚══╧    ╧══╧══╧══╧══╧══╧══╧══╧    ╧══╝
		//        ^       ^                         ^
		// index: 0      pq_idx()                  size-1
		//
		// addresses are in linear space
		uint32_t cseip;   // address of the instruction pointer
		uint32_t eip;
		uint8_t  pq[CPU_PQ_MAX_SIZE];
		bool     pq_valid;
		uint32_t pq_left; // address of the head (always >= cseip)
		uint32_t pq_tail; // address of the tail (always >= cseip)
		int      pq_len;
	} m_s;

	int m_width = 0;
	int m_pq_size = 0;
	int m_pq_thres = 0;
	int m_paddress = 0; // pipelined address
	int m_cycles_ahead = 0;

	// an array containing the information about the PQ updates happened during prefilling
	struct PQCycles {
		uint32_t cycles;
		uint32_t tail;
	};
	PQCycles m_pq_cycles[CPU_PQ_MAX_SIZE / 2]; // the bus width is at least 16bit
	int m_pq_cycles_len = 0; // the amount of prefilling fetches

	// many copies of CPUBus can exist in the CPULogger
	std::shared_ptr<MemoryLogger> m_logger;

public:
	struct Counters {
		int fetch_cycles;
		int pq_avail_cycles;
		int mem_r_cycles;
		int mem_w_cycles;
		int pmem_cycles;   // pipelined memory cycles
		int pfetch_cycles; // pipelined fetch cycles

		void reset() {
			fetch_cycles = 0;
			pq_avail_cycles = 0;
			mem_r_cycles = 0;
			mem_w_cycles = 0;
			pmem_cycles = 0;
			pfetch_cycles = 0;
		}

		constexpr bool was_memory_accessed() const { return (mem_r_cycles || mem_w_cycles); }
		constexpr int  mem_tx_cycles() const { return mem_r_cycles + mem_w_cycles; }
	};

private:
	Counters m_counters;

public:
	CPUBus();

	void init();
	void reset();
	void config_changed();

	int  cycles_ahead() const { return m_cycles_ahead; }
	bool is_pq_valid() const { return m_s.pq_valid; }
	int  width() const { return m_width; }

	void update(int _cycles);
	void enable_paging(bool _enabled);
	void prefill_pq();

	//instruction fetching
	#if CPU_USE_PQ
	inline uint8_t  fetchb()  { return fetch<uint8_t, 1>(); }
	inline uint16_t fetchw()  { return fetch<uint16_t,2>(); }
	inline uint32_t fetchdw() { return fetch<uint32_t,4>(); }
	#else
	inline uint8_t  fetchb()  { return fetch_noqueue<uint8_t, 1>(); }
	inline uint16_t fetchw()  { return fetch_noqueue<uint16_t,2>(); }
	inline uint32_t fetchdw() { return fetch_noqueue<uint32_t,4>(); }
	#endif

	inline uint32_t eip() const { return m_s.eip; }
	inline uint32_t cseip() const { return m_s.cseip; }

	inline void invalidate_pq() {
		m_s.pq_valid = false;
		m_s.pq_len = 0;
		m_s.pq_left = m_s.pq_tail = m_s.cseip;
	}
	void reset_pq();

	template<unsigned S> inline uint32_t mem_read(uint32_t _phy)
	{
		#if CPULOG
		uint32_t value = p_mem_read<S>(_phy, m_counters.mem_r_cycles);
		m_logger->push_back({ MemoryLogger::MEMR, S, _phy, value });
		return value;
		#else
		return p_mem_read<S>(_phy, m_counters.mem_r_cycles);
		#endif
	}
	template<unsigned S> inline void mem_write(uint32_t _phy, uint32_t _data)
	{
		p_mem_write<S>(_phy, _data, m_counters.mem_w_cycles);
		#if CPULOG
		m_logger->push_back({ MemoryLogger::MEMW, S, _phy, _data });
		#endif
	}

	void save_state(StateBuf &_state);
	void restore_state(StateBuf &_state);

	int write_pq_to_logfile(FILE *_dest);

	Counters & counters() { return m_counters; }
	MemoryLogger & logger() const { return *m_logger.get(); }

private:
	template<unsigned> uint32_t p_mem_read(uint32_t _addr, int &_cycles) { assert(false); return 0; }
	template<unsigned> void p_mem_write(uint32_t _addr, uint32_t _data, int &_cycles) { assert(false); }

	constexpr int pq_free_space() const {
		return m_pq_size - m_s.pq_len;
	}
	constexpr int pq_idx() const {
		return m_s.cseip - m_s.pq_left;
	}
	constexpr bool pq_is_empty() const {
		return m_s.pq_len == 0;
	}
	template<int len, bool paging> uint32_t fill_pq_read_mem(uint32_t _linear, int &_cycles);
	template<int bytes, bool paging> int fill_pq(int _amount, bool _paddress);
	int (CPUBus::*fill_pq_fn)(int, bool) = nullptr;

	template<class T, int L>
	T fetch()
	{
		// code is always read from the PQ 
		if(m_s.pq_len < L) {
			// if there's not enough code available then fill the queue first
			m_counters.fetch_cycles += (this->*fill_pq_fn)(L-m_s.pq_len, m_counters.fetch_cycles>0);
			if(m_cycles_ahead) {
				m_counters.pfetch_cycles += m_cycles_ahead;
				m_cycles_ahead = 0;
			}
		}
		T data;
		switch(L) {
			case 1:
				data = m_s.pq[pq_idx()];
				break;
			case 2:
				data = m_s.pq[pq_idx()] | 
				       m_s.pq[pq_idx() + 1] << 8;
				break;
			case 4:
				data = m_s.pq[pq_idx()] | 
				       m_s.pq[pq_idx() + 1] << 8  |
				       m_s.pq[pq_idx() + 2] << 16 |
				       m_s.pq[pq_idx() + 3] << 24;
				break;
			default: assert(false); break;
		}
		m_s.pq_len -= L;
		m_s.cseip += L;
		m_s.eip += L;
		return data;
	}

	template<class T, int L>
	T fetch_noqueue()
	{
		T data;
		uint32_t phy = m_s.cseip;
		if(IS_PAGING()) {
			phy = g_cpummu.TLB_lookup(m_s.cseip, L, IS_USER_PL, false);
		}
		data = mem_read<L>(phy);
		m_s.cseip += L;
		m_s.eip += L;
		return data;
	}
};


template<> inline
uint32_t CPUBus::p_mem_read<1>(uint32_t _addr, int &_cycles)
{
	return g_memory.read_t<1>(_addr, 1, _cycles);
}
template<> uint32_t CPUBus::p_mem_read<2>(uint32_t _addr, int &_cycles);
template<> uint32_t CPUBus::p_mem_read<3>(uint32_t _addr, int &_cycles);
template<> uint32_t CPUBus::p_mem_read<4>(uint32_t _addr, int &_cycles);

template<> inline
void CPUBus::p_mem_write<1>(uint32_t _addr, uint32_t _data, int &_cycles)
{
	g_memory.write_t<1>(_addr, _data, 1, _cycles);
}
template<> void CPUBus::p_mem_write<2>(uint32_t _addr, uint32_t _data, int &_cycles);
template<> void CPUBus::p_mem_write<3>(uint32_t _addr, uint32_t _data, int &_cycles);
template<> void CPUBus::p_mem_write<4>(uint32_t _addr, uint32_t _data, int &_cycles);


#endif
