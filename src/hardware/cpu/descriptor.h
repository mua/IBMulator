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

#ifndef IBMULATOR_CPU_DESCRIPTOR_H
#define IBMULATOR_CPU_DESCRIPTOR_H


/*
 * Segment descriptor Access Rights masks
 */
#define SEG_ACCESSED    0x1
#define SEG_READWRITE   0x2
#define SEG_CONFORMING  0x4
#define SEG_EXP_DOWN    0x4
#define SEG_EXECUTABLE  0x8
#define SEG_CODE        0x8
#define SEG_SEGMENT     0x10
#define SEG_PRESENT     0x80
#define SEG_REAL_MODE   SEG_SEGMENT|SEG_PRESENT|SEG_READWRITE|SEG_ACCESSED

/* Segment descriptor type masks
 */
#define SEG_TYPE_DATA       0x0
#define SEG_TYPE_READWRITE  0x1
#define SEG_TYPE_READABLE   0x1
#define SEG_TYPE_WRITABLE   0x1
#define SEG_TYPE_CONFORMING 0x2
#define SEG_TYPE_EXP_DOWN   0x2
#define SEG_TYPE_EXECUTABLE 0x4
#define SEG_TYPE_CODE       0x4

/* cf. 386:6-4 */
enum DescriptorType {
	DESC_TYPE_INVALID_0      = 0x0,
	DESC_TYPE_AVAIL_286_TSS  = 0x1,
	DESC_TYPE_LDT_DESC       = 0x2,
	DESC_TYPE_BUSY_286_TSS   = 0x3,
	DESC_TYPE_286_CALL_GATE  = 0x4,
	DESC_TYPE_TASK_GATE      = 0x5,
	DESC_TYPE_286_INTR_GATE  = 0x6,
	DESC_TYPE_286_TRAP_GATE  = 0x7,
	DESC_TYPE_INVALID_8      = 0x8,
	DESC_TYPE_AVAIL_386_TSS  = 0x9,
	DESC_TYPE_INVALID_A      = 0xA,
	DESC_TYPE_BUSY_386_TSS   = 0xB,
	DESC_TYPE_386_CALL_GATE  = 0xC,
	DESC_TYPE_INVALID_D      = 0xD,
	DESC_TYPE_386_INTR_GATE  = 0xE,
	DESC_TYPE_386_TRAP_GATE  = 0xF
};

#define TSS_BUSY_BIT   0x2
#define DESC_GATE_MASK 0x4


struct Descriptor
{
	union {
		uint32_t limit;     // Segment / TSS
		uint32_t offset;    // Call Gate / Trap-Int Gate
	};
	union {
		uint16_t base_15_0; // Segment / System / TSS
		uint16_t selector;  // Call Gate / Task Gate / Trap-Int Gate
	};
	union {
		uint8_t base_23_16; // Segment / System / TSS
		uint8_t word_count; // Call Gate
	};
	uint32_t base;
	// Access Rights (AR) bits:
	bool accessed; // | bit0 (if segment, 1=has been accessed)
	uint8_t type;  // | bit0,1,2,3 (b0 if gate)
	bool segment;  // | bit4 (1=segment, 0=control)
	uint8_t dpl;   // | bit5,6 (Descriptor Privilege Level)
	bool present;  // | bit7 (1=present in real memory)
	///
	bool big;
	bool page_granular;
	bool valid;

	inline uint8_t get_AR() const {
		uint8_t ar = segment << 4 | dpl << 5 | present << 7;
		if(segment) {
			ar |= type<<1 | accessed;
		} else {
			ar |= type;
		}
		return ar;
	}

	inline void set_AR(uint8_t _AR) {
		valid = true;
		segment = _AR & SEG_SEGMENT;
		if(segment) {
			//Code or Data Segment Descriptor
			accessed = _AR & 1;
			type = (_AR>>1) & 7;
		} else {
			//System Segment Descriptor or Gate Descriptor
			type = (_AR & 0xF);
			if(type==DESC_TYPE_INVALID_0 || type==DESC_TYPE_INVALID_8
			|| type==DESC_TYPE_INVALID_A || type==DESC_TYPE_INVALID_D) {
				valid = false;
			}
		}
		dpl = (_AR>>5) & 3;
		present = _AR & 0x80;
	}

	inline void operator=(uint64_t _data) {
		set_AR(uint8_t(_data>>40));
		if(segment || !(type&DESC_GATE_MASK)) {
			// for code/data segments and not call/task/int/trap gates
			limit = (_data & 0xFFFF) | ((_data >> 32) & 0xF0000);
			base_15_0 = _data >> 16;
			base_23_16 = _data >> 32;
			base = base_15_0 | (uint32_t(base_23_16) << 16) | ((_data >> 32) & 0xFF000000 );
			big = (_data >> 54) & 1;
			page_granular = (_data >> 55) & 1;
			if(page_granular) {
				limit = (limit << 12) | 0xFFF;
			}
		} else {
			// gates
			offset = _data & 0xFFFF;
			if(type >= DESC_TYPE_386_CALL_GATE) {
				offset |= ((_data >> 32) & 0xFFFF0000);
			}
			selector = _data >> 16;
			word_count = (_data >> 32) & 0x1F;
		}
	}

	void set_from_286_cache(uint16_t _cache[3]) {
		uint64_t data;
		data = _cache[2]; // limit
		data |= uint64_t(_cache[0]) << 16; // base 15-0
		data |= uint64_t(_cache[1]) << 32; // AR | base 23-16
		(*this) = data;
		/*
		 * Access rights byte is in the format of the access byte in a descriptor.
		 * The only difference is that the present bit becomes a valid bit.
		 * If zero, the descriptor is considered invalid and any memory reference
		 * using the descriptor will cause exception 13 with an error code of zero.
		 */
		valid = (_cache[1]>>8) & 0x80;
	}

	inline bool is_data_segment() const   { return (segment && !(type & SEG_TYPE_CODE)); }
	inline bool is_code_segment() const   { return (segment && (type & SEG_TYPE_CODE)); }
	inline bool is_system_segment() const { return !segment; }
	inline bool is_conforming() const     { return (segment && (type & SEG_TYPE_CONFORMING)); }
	inline bool is_expand_down() const    { return (segment && (type & SEG_TYPE_EXP_DOWN)); }
	inline bool is_readable() const       { return (segment && (type & SEG_TYPE_READABLE)); }
	inline bool is_writeable() const      { return (segment && (type & SEG_TYPE_WRITABLE)); }
	inline bool is_286_system() const     { return (!segment && type <= DESC_TYPE_286_TRAP_GATE); }
	inline bool is_286_TSS() const        { return (!segment && ((type & 0xD) == 0x1)); }
	inline bool is_386_system() const     { return (!segment && type >= DESC_TYPE_AVAIL_386_TSS); }
	inline bool is_386_TSS() const        { return (!segment && ((type & 0xD) == 0x9)); }
};

/*
 * Code/Data Segment (S=1) (cf. 286:6-5; 386:5-3,6-2)

   7                             0 7                             0
  ╔═══════════════════════════════╤═══╤═══╤═══╤═══╤═══════════════╗
+7║            BASE 31-24         │ G │B/D│ 0 │AVL│  LIMIT 19-16  ║+6 (386 only)
  ╟───┬───────┬───┬───────────┬───┼───┴───┴───┴───┴───┴───┴───┴───╢
+5║ P │  DPL  │S=1│   TYPE    │ A │          BASE 23-16           ║+4
  ╟───┴───┴───┴───┴───┴───┴───┴───┴───────────────────────────────╢
+3║                           BASE 15-0                           ║+2
  ╟───────────────────────────────┴───────────────────────────────╢
+1║                           LIMIT 15-0                          ║ 0
  ╚═══════════════════════════════╧═══════════════════════════════╝
   15                                                            0

	A: Accessed, P: Present, B/D: Big/Default, G: Granularity

	Data segments AR byte:
	┌───┬───────┬───┬───────────┬───┐
	│ P │  DPL  │ 1 │ 0   E   W │ A │
	└───┴───┴───┴───┴───┴───┴───┴───┘
	E: Expand-Down, W: Writeable

	Code segments AR byte:
	┌───┬───────┬───┬───────────┬───┐
	│ P │  DPL  │ 1 │ 1   C   R │ A │
	└───┴───┴───┴───┴───┴───┴───┴───┘
	C: Conforming, R: Readable


 * System Segment (S=0) (cf. 286:6-6; 386:5-3,6-2)

   7                             0 7                             0
  ╔═══════════════════════════════╤═══╤═══╤═══╤═══╤═══════════════╗
+7║            BASE 31-24         │ G │ x │ 0 │AVL│  LIMIT 19-16  ║+6 (386 only)
  ╟───┬───────┬───┬───────────────┼───┴───┴───┴───┴───┴───┴───┴───╢
+5║ P │  DPL  │S=0│     TYPE      │           BASE 23-16          ║+4
  ╟───┴───┴───┴───┴───┴───┴───┴───┴───────────────────────────────╢
+3║                           BASE 15-0                           ║+2
  ╟───────────────────────────────┴───────────────────────────────╢
+1║                           LIMIT 15-0                          ║ 0
  ╚═══════════════════════════════╧═══════════════════════════════╝
   15                                                            0

 * Call Gate (cf. 286:8-4; 386:6-11)

   7                             0 7                             0
  ╔═══════════════════════════════════════════════════════════════╗
+7║                  DESTINATION OFFSET 31-16                     ║+6 (386 only)
  ╟───┬───────┬───┬───────────────┬───────────┬───────────────────╢
+5║ P │  DPL  │ 0 │ T   1   0   0 │ x   x   x │  WORD COUNT 4-0   ║+4
  ╟───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───────────┬───────╢
+3║                 DESTINATION SELECTOR 15-2             │ x   x ║+2
  ╟───────────────────────────────┴───────────────────────┴───┴───╢
+1║                  DESTINATION OFFSET 15-0                      ║ 0
  ╚═══════════════════════════════╧═══════════════════════════════╝
   15                                                            0

  T=0 for 286, 1 for 386

 * TSS (cf. 286:8-4; 386:7-3)

   7                             0 7                             0
  ╔═══════════════════════════════╤═══╤═══╤═══╤═══╤═══════════════╗
+7║            BASE 31-24         │ G │ 0 │ 0 │AVL│  LIMIT 19-16  ║+6 (386 only)
  ╟───┬───────┬───┬───────────────┼───┴───┴───┴───┴───┴───┴───┴───╢
+5║ P │  DPL  │ 0 │ T   0   B   1 │         TSS BASE 23-16        ║+4
  ╟───┴───┴───┴───┴───┴───┴───┴───┴───────────────────────────────╢
+3║                          TSS BASE 15-0                        ║+2
  ╟───────────────────────────────┴───────────────────────────────╢
+1║                          TSS LIMIT 15-0                       ║ 0
  ╚═══════════════════════════════╧═══════════════════════════════╝
   15                                                            0

  T=0 for 286, 1 for 386, B: Busy

 * Task Gate (cf. 286:8-8; 386:7-6)

   7                             0 7                             0
  ╔═══════════════════════════════╤═══════════════════════════════╗
+7║                             UNUSED                            ║+6
  ╟───┬───────┬───┬───────────────┬───────────────────────────────╢
+5║ P │  DPL  │ 0 │ 0   1   0   1 │            UNUSED             ║+4
  ╟───┴───┴───┴───┴───┴───┴───┴───┴───────────────────────────────╢
+3║                          TSS SELECTOR                         ║+2
  ╟───────────────────────────────┴───────────────────────────────╢
+1║                             UNUSED                            ║ 0
  ╚═══════════════════════════════╧═══════════════════════════════╝
   15                                                            0

 * Trap/Interrupt Gate (cf. 286:9-4; 386:9-6)

   7                             0 7                             0
  ╔═══════════════════════════════╤═══════════════════════════════╗
+7║                          OFFSET 31-16                         ║+6 (386 only)
  ╟───┬───────┬───┬───────────────┬───────────────────────────────╢
+5║ P │  DPL  │ 0 │ T   1   1   I │            UNUSED             ║+4
  ╟───┴───┴───┴───┴───┴───┴───┴───┴───────────────────────────────╢
+3║                          TSS SELECTOR                         ║+2
  ╟───────────────────────────────┴───────────────────────────────╢
+1║                          OFFSET 15-0                          ║ 0 (386 only)
  ╚═══════════════════════════════╧═══════════════════════════════╝
   15                                                            0

  T=0 for 286, 1 for 386, I=0 for Interrupt, 1 for Trap

 */

#endif
