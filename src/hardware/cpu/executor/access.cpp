/*
 * Copyright (C) 2016-2026  Marco Bortolin
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
#include "hardware/cpu/executor.h"

bool CPUExecutor::seg_check_read(SegReg & _seg, uint32_t _offset, unsigned _len)
{
	assert(_len > 0);

	_len--;

	switch(_seg.desc.type) {
		case SEG_TYPE_DATA:
		case SEG_TYPE_DATA | SEG_TYPE_WRITABLE:
		case SEG_TYPE_CODE | SEG_TYPE_READABLE:
		case SEG_TYPE_CODE | SEG_TYPE_READABLE | SEG_TYPE_CONFORMING:
			if(_seg.desc.limit == 0xffffffff && _seg.desc.base == 0) {
				break;
			}
			if(_offset > (_seg.desc.limit - _len) || _len > _seg.desc.limit) {
				PDEBUGF(LOG_V2, LOG_CPU, "seg_check_read(): segment limit violation.\n");
				return false;
			}
			break;
		case SEG_TYPE_DATA | SEG_TYPE_EXP_DOWN:
		case SEG_TYPE_DATA | SEG_TYPE_WRITABLE | SEG_TYPE_EXP_DOWN:
		{
			uint32_t upper_limit;
			if(_seg.desc.big) {
				upper_limit = 0xffffffff;
			} else {
				upper_limit = 0x0000ffff;
			}
			if(_offset <= _seg.desc.limit || _offset > upper_limit || (upper_limit - _offset) < _len) {
				PDEBUGF(LOG_V2, LOG_CPU, "seg_check_read(): segment limit violation expand down.\n");
				return false;
			}
			break;
		}
		case SEG_TYPE_CODE:
		case SEG_TYPE_CODE | SEG_TYPE_CONFORMING:
			PDEBUGF(LOG_V2, LOG_CPU, "seg_check_read(): code segment execute only.\n");
			return false;
		default:
			PDEBUGF(LOG_V2, LOG_CPU, "seg_check_read(): invalid segment type!\n");
			return false;
	}

	return true;
}

bool CPUExecutor::seg_check_write(SegReg & _seg, uint32_t _offset, unsigned _len)
{
	assert(_len > 0);

	_len--;

	switch(_seg.desc.type) {
		case SEG_TYPE_DATA:
		case SEG_TYPE_DATA | SEG_TYPE_EXP_DOWN:
		case SEG_TYPE_CODE:
		case SEG_TYPE_CODE | SEG_TYPE_CONFORMING:
		case SEG_TYPE_CODE | SEG_TYPE_READABLE:
		case SEG_TYPE_CODE | SEG_TYPE_READABLE | SEG_TYPE_CONFORMING:
			PDEBUGF(LOG_V2, LOG_CPU, "seg_check_write(): segment not writeable.\n");
			return false;
		case SEG_TYPE_DATA | SEG_TYPE_WRITABLE:
			if(_seg.desc.limit == 0xffffffff && _seg.desc.base == 0) {
				break;
			}
			if(_offset > (_seg.desc.limit - _len) || _len > _seg.desc.limit) {
				PDEBUGF(LOG_V2, LOG_CPU, "seg_check_write(): segment limit violation.\n");
				return false;
			}
			break;
		case SEG_TYPE_DATA | SEG_TYPE_WRITABLE | SEG_TYPE_EXP_DOWN:
		{
			uint32_t upper_limit;
			if(_seg.desc.big) {
				upper_limit = 0xffffffff;
			} else {
				upper_limit = 0x0000ffff;
			}
			if(_offset <= _seg.desc.limit || _offset > upper_limit || (upper_limit - _offset) < _len) {
				PDEBUGF(LOG_V2, LOG_CPU, "seg_check_write(): segment limit violation expand down.\n");
				return false;
			}
			break;
		}
		default:
			PDEBUGF(LOG_V2, LOG_CPU, "seg_check_write(): invalid segment type!\n");
			return false;
	}

	return true;
}

void CPUExecutor::seg_check(SegReg & _seg, uint32_t _offset, unsigned _len,
		bool _write, uint8_t _vector, uint16_t _errcode)
{
	if(_vector == CPU_INVALID_INT) {
		_vector = _seg.is(REG_SS) ? CPU_SS_EXC : CPU_GP_EXC;
	}

	if(!_seg.desc.valid) {
		PDEBUGF(LOG_V2, LOG_CPU, "seg_check(): segment not valid.\n");
		throw CPUException(_vector, _errcode);
	}
	if(!_seg.desc.present) {
		PDEBUGF(LOG_V2, LOG_CPU, "seg_check(): segment not present.\n");
		throw CPUException(_vector, _errcode);
	}

	bool result;
	if(_write) {
		result = seg_check_write(_seg, _offset, _len);
	} else {
		result = seg_check_read(_seg, _offset, _len);
	}
	if(!result) {
		throw CPUException(_vector, _errcode);
	}
}

void CPUExecutor::io_check(uint16_t _port, unsigned _len)
{
	if((IS_PMODE() && (CPL > FLAG_IOPL)) || IS_V8086()) {
		if(CPU_FAMILY <= CPU_286) {
			/* #GP(O) if the current privilege level is bigger (has less privilege)
			 * than IOPL; which is the privilege level found in the flags register.
			 */
			PDEBUGF(LOG_V2, LOG_CPU, "io_check(): I/O access not allowed.\n");
			throw CPUException(CPU_GP_EXC, 0);
		}
		if(!REG_TR.desc.valid || !REG_TR.desc.is_system_segment() || (
			REG_TR.desc.type != DESC_TYPE_AVAIL_386_TSS &&
			REG_TR.desc.type != DESC_TYPE_BUSY_386_TSS
		))
		{
			PDEBUGF(LOG_V2, LOG_CPU, "io_check(): TR doesn't point to a valid 32bit TSS.\n");
			throw CPUException(CPU_GP_EXC, 0);
		}
		if(REG_TR.desc.limit < 103) {
			PDEBUGF(LOG_V2, LOG_CPU, "io_check(): TR.limit < 103\n");
			throw CPUException(CPU_GP_EXC, 0);
		}

		uint32_t io_base = read_word(REG_TR.desc.base + 102);
		uint32_t offset = (io_base + _port / 8);
		if(offset >= REG_TR.desc.limit) {
			PDEBUGF(LOG_V2, LOG_CPU, "io_check(): TR.limit violation.\n");
			throw CPUException(CPU_GP_EXC, 0);
		}
		uint16_t permission = read_word(REG_TR.desc.base + offset);
		unsigned bit_index = _port & 0x7;
		unsigned mask = (1 << _len) - 1;
		if((permission >> bit_index) & mask) {
			throw CPUException(CPU_GP_EXC, 0);
		}
	}
}
