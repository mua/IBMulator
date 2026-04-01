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

#ifndef IBMULATOR_GUI_DEVSTATUS_H
#define IBMULATOR_GUI_DEVSTATUS_H

#include "debugtools.h"

class Machine;
class GUI;


class DevStatus final : public DebugTools::DebugWindow
{
private:
	Machine *m_machine;

	struct {
		bool is_running;
		Rml::Element *panel;
		Rml::Element *btn_update;
		Rml::Element *mode, *screen;

		struct {
			bool is_visible;
			Rml::Element *frame_cnt;
			Rml::Element *pix_upd, *upd, *saddr_line, *pal_line;
			Rml::Element *scanl, *disp_phase, *hretr_phase, *vretr_phase;
		} stats;

		struct {
			bool is_visible;
			Rml::Element *clock, *polarity, *registers;
		} gen;

		struct {
			bool is_visible;
			Rml::Element *cwidth, *dotclock, *registers;
		} seq;

		struct {
			bool is_visible;
			Rml::Element *msl, *dsc, *scanlimgl;
			struct {
				Rml::Element *total, *lblank, *lborder, *disp, *rborder, *rblank, *retr;
				Rml::Element *bend, *bstart, *rstart, *rend;
				struct {
					Rml::Element *total, *lblank, *lborder, *disp, *rborder, *rblank, *retr;
					Rml::Element *bend, *bstart, *rstart, *rend;
				} px;
				struct {
					Rml::Element *total, *lblank, *lborder, *disp, *rborder, *rblank, *retr;
					Rml::Element *bend, *bstart, *rstart, *rend;
				} us;
			} h;
			struct {
				Rml::Element *total, *tblank, *tborder, *disp, *bborder, *bblank, *retr;
				Rml::Element *bend, *bstart, *rstart, *rend;
				struct {
					Rml::Element *total, *tblank, *tborder, *disp, *bborder, *bblank, *retr;
					Rml::Element *bend, *bstart, *rstart, *rend;
				} ms;
				Rml::Element *vblank_skip, *last_vis_sl;
			} v;
			Rml::Element *registers, *latches;
		} crtc;

		struct {
			bool is_visible;
			Rml::Element *registers;
		} gfx;

		struct {
			bool is_visible;
			Rml::Element *registers;
		} att;

		struct {
			bool is_visible;
			Rml::Element *registers;
			SDL_Surface *palette;
		} dac;

	} m_vga = {};

	struct {
		bool is_running;
		Rml::Element *panel;
		Rml::Element *btn_update;
		Rml::Element *irq_e[16], *irr_e[16], *imr_e[16], *isr_e[16];
		uint16_t irq, irr, imr, isr;
	} m_pic = {};

	struct {
		bool is_running;
		Rml::Element *panel;
		Rml::Element *btn_update;
		Rml::Element *mode[3], *cnt[3], *gate[3], *out[3], *in[3];
	} m_pit = {};

	static event_map_t ms_evt_map;

	void on_cmd_vga_update(Rml::Event &);
	void on_cmd_vga_dump_state(Rml::Event &);
	void on_cmd_vga_screenshot(Rml::Event &);
	void on_cmd_vga_skip_scanl(Rml::Event &);
	void on_cmd_vga_skip_frame(Rml::Event &);
	void on_cmd_vga_stats_show(Rml::Event &);
	void on_cmd_vga_genreg_show(Rml::Event &);
	void on_cmd_vga_seq_show(Rml::Event &);
	void on_cmd_vga_crtc_show(Rml::Event &);
	void on_cmd_vga_gfx_show(Rml::Event &);
	void on_cmd_vga_att_show(Rml::Event &);
	void on_cmd_vga_dac_show(Rml::Event &);
	void on_cmd_pit_update(Rml::Event &);
	void on_cmd_pic_update(Rml::Event &);
	void update_pit();
	void update_pit(unsigned cnt);
	void update_pic();
	void update_pic(uint16_t _irq, uint16_t _irr, uint16_t _imr, uint16_t _isr, uint _irqn);
	void update_vga(bool _force = false);

public:
	DevStatus(GUI * _gui, Rml::Element *_button, Machine *_machine);

	void update() override;
	SDL_Surface * get_vga_dac_palette_surface();

protected:
	void create() override;
	event_map_t & get_event_map() override { return DevStatus::ms_evt_map; }
};


#endif
