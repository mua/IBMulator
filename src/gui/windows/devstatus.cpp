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
#include "program.h"
#include "gui.h"
#include "machine.h"
#include "devstatus.h"
#include "filesys.h"
#include <RmlUi/Core.h>
#include <sstream>

#include "hardware/devices/pic.h"
#include "hardware/devices/pit.h"
#include "hardware/devices/vga.h"
#include "format.h"

event_map_t DevStatus::ms_evt_map = {
	GUI_EVT( "cmd_vga_dump_state", "click", DevStatus::on_cmd_vga_dump_state ),
	GUI_EVT( "cmd_vga_screenshot", "click", DevStatus::on_cmd_vga_screenshot ),
	GUI_EVT( "cmd_vga_skip_scanl", "click", DevStatus::on_cmd_vga_skip_scanl ),
	GUI_EVT( "cmd_vga_skip_frame", "click", DevStatus::on_cmd_vga_skip_frame ),
	GUI_EVT( "cmd_vga_update",     "click", DevStatus::on_cmd_vga_update ),
	GUI_EVT( "cmd_pit_update",     "click", DevStatus::on_cmd_pit_update ),
	GUI_EVT( "cmd_pic_update",     "click", DevStatus::on_cmd_pic_update ),
	GUI_EVT( "cmd_vga_genreg_show","click", DevStatus::on_cmd_vga_genreg_show ),
	GUI_EVT( "cmd_vga_seq_show",   "click", DevStatus::on_cmd_vga_seq_show ),
	GUI_EVT( "cmd_vga_crtc_show",  "click", DevStatus::on_cmd_vga_crtc_show ),
	GUI_EVT( "cmd_vga_stats_show", "click", DevStatus::on_cmd_vga_stats_show ),
	GUI_EVT( "close",              "click", DebugTools::DebugWindow::on_cancel ),
	GUI_EVT( "*",                  "keydown", Window::on_keydown )
};

DevStatus::DevStatus(GUI * _gui, Rml::Element *_button, Machine *_machine)
:
DebugTools::DebugWindow(_gui, "devstatus.rml", _button),
m_machine(_machine)
{
}

void DevStatus::create()
{
	DebugTools::DebugWindow::create();

	m_vga.is_running = false;
	m_vga.panel = get_element("vga");

	m_vga.btn_update = get_element("cmd_vga_update");
	
	m_vga.mode   = get_element("vga_mode");
	m_vga.screen = get_element("vga_screen");

	m_vga.stats.frame_cnt  = get_element("vga_frame_cnt");
	m_vga.stats.pix_upd    = get_element("vga_pix_upd");
	m_vga.stats.upd        = get_element("vga_upd");
	m_vga.stats.saddr_line = get_element("vga_saddr_line");
	m_vga.stats.pal_line   = get_element("vga_pal_line");
	m_vga.stats.scanl = get_element("vga_scanl");
	m_vga.stats.disp_phase  = get_element("vga_disp_phase");
	m_vga.stats.hretr_phase = get_element("vga_hretr_phase");
	m_vga.stats.vretr_phase = get_element("vga_vretr_phase");

	m_vga.stats.is_visible = false;
	
	m_vga.gen.clock  = get_element("vga_clock");
	m_vga.gen.polarity = get_element("vga_polarity");
	m_vga.gen.registers = get_element("vga_gen_registers");

	m_vga.gen.is_visible = false;
	
	m_vga.seq.cwidth = get_element("vga_cwidth");
	m_vga.seq.dotclock = get_element("vga_dotclock");
	m_vga.seq.registers = get_element("vga_seq_registers");
	
	m_vga.seq.is_visible = false;
	
	m_vga.crtc.msl       = get_element("vga_msl");
	m_vga.crtc.dsc       = get_element("vga_dsc");
	m_vga.crtc.scanlimgl = get_element("vga_scanlimgl");
	
	m_vga.crtc.h.total   = get_element("vga_htotal");
	m_vga.crtc.h.lblank  = get_element("vga_hlblank");
	m_vga.crtc.h.lborder = get_element("vga_hlborder");
	m_vga.crtc.h.disp    = get_element("vga_hdisp");
	m_vga.crtc.h.rborder = get_element("vga_hrborder"); 
	m_vga.crtc.h.rblank  = get_element("vga_hrblank");
	m_vga.crtc.h.retr    = get_element("vga_hretr");
	m_vga.crtc.h.bend    = get_element("vga_hbend");
	m_vga.crtc.h.bstart  = get_element("vga_hbstart");
	m_vga.crtc.h.rstart  = get_element("vga_hrstart");
	m_vga.crtc.h.rend    = get_element("vga_hrend");

	m_vga.crtc.h.px.total   = get_element("vga_px_htotal");
	m_vga.crtc.h.px.lblank  = get_element("vga_px_hlblank");
	m_vga.crtc.h.px.lborder = get_element("vga_px_hlborder");
	m_vga.crtc.h.px.disp    = get_element("vga_px_hdisp");
	m_vga.crtc.h.px.rborder = get_element("vga_px_hrborder"); 
	m_vga.crtc.h.px.rblank  = get_element("vga_px_hrblank");
	m_vga.crtc.h.px.retr    = get_element("vga_px_hretr");
	m_vga.crtc.h.px.bend    = get_element("vga_px_hbend");
	m_vga.crtc.h.px.bstart  = get_element("vga_px_hbstart");
	m_vga.crtc.h.px.rstart  = get_element("vga_px_hrstart");
	m_vga.crtc.h.px.rend    = get_element("vga_px_hrend");
	
	m_vga.crtc.h.us.total   = get_element("vga_us_htotal");
	m_vga.crtc.h.us.disp    = get_element("vga_us_hdisp");
	m_vga.crtc.h.us.rblank  = get_element("vga_us_hrblank");
	m_vga.crtc.h.us.rborder = get_element("vga_us_hrborder");
	m_vga.crtc.h.us.bstart  = get_element("vga_us_hbstart");
	m_vga.crtc.h.us.retr    = get_element("vga_us_hretr");
	m_vga.crtc.h.us.rstart  = get_element("vga_us_hrstart");
	m_vga.crtc.h.us.rend    = get_element("vga_us_hrend");
	m_vga.crtc.h.us.lblank  = get_element("vga_us_hlblank");
	m_vga.crtc.h.us.bend    = get_element("vga_us_hbend");
	m_vga.crtc.h.us.lborder = get_element("vga_us_hlborder");

	m_vga.crtc.v.total   = get_element("vga_vtotal");
	m_vga.crtc.v.disp    = get_element("vga_vdisp");
	m_vga.crtc.v.tblank  = get_element("vga_vtblank");
	m_vga.crtc.v.tborder = get_element("vga_vtborder");
	m_vga.crtc.v.bstart  = get_element("vga_vbstart");
	m_vga.crtc.v.retr    = get_element("vga_vretr");
	m_vga.crtc.v.rstart  = get_element("vga_vrstart");
	m_vga.crtc.v.rend    = get_element("vga_vrend");
	m_vga.crtc.v.bblank  = get_element("vga_vbblank");
	m_vga.crtc.v.bend    = get_element("vga_vbend");
	m_vga.crtc.v.bborder = get_element("vga_vbborder");

	m_vga.crtc.v.ms.total   = get_element("vga_ms_vtotal");
	m_vga.crtc.v.ms.disp    = get_element("vga_ms_vdisp");
	m_vga.crtc.v.ms.tblank  = get_element("vga_ms_vtblank");
	m_vga.crtc.v.ms.tborder = get_element("vga_ms_vtborder");
	m_vga.crtc.v.ms.bstart  = get_element("vga_ms_vbstart");
	m_vga.crtc.v.ms.retr    = get_element("vga_ms_vretr");
	m_vga.crtc.v.ms.rstart  = get_element("vga_ms_vrstart");
	m_vga.crtc.v.ms.rend    = get_element("vga_ms_vrend");
	m_vga.crtc.v.ms.bblank  = get_element("vga_ms_vbblank");
	m_vga.crtc.v.ms.bend    = get_element("vga_ms_vbend");
	m_vga.crtc.v.ms.bborder = get_element("vga_ms_vbborder");

	m_vga.crtc.v.vblank_skip = get_element("vga_vblank_skip");
	m_vga.crtc.v.last_vis_sl = get_element("vga_last_vis_sl");

	m_vga.crtc.registers = get_element("vga_crtc_registers");
	m_vga.crtc.latches = get_element("vga_crtc_latches");

	m_vga.crtc.is_visible = false;


	m_pit.is_running = false;
	m_pit.panel = get_element("pit");
	m_pit.btn_update = get_element("cmd_pit_update");
	for(int i=0; i<3; i++) {
		m_pit.mode[i] = get_element(str_format("pit_%d_mode", i));
		m_pit.cnt[i]  = get_element(str_format("pit_%d_cnt", i));
		m_pit.gate[i] = get_element(str_format("pit_%d_gate", i));
		m_pit.out[i]  = get_element(str_format("pit_%d_out", i));
		m_pit.in[i]   = get_element(str_format("pit_%d_in", i));
	}

	
	m_pic.is_running = false;
	m_pic.panel = get_element("pic");
	m_pic.btn_update = get_element("cmd_pic_update");
	for(int i=0; i<16; i++) {
		m_pic.irq_e[i] = get_element(str_format("pic_irq_%d", i));
		m_pic.irr_e[i] = get_element(str_format("pic_irr_%d", i));
		m_pic.imr_e[i] = get_element(str_format("pic_imr_%d", i));
		m_pic.isr_e[i] = get_element(str_format("pic_isr_%d", i));
	}
	m_pic.irq = 0;
	m_pic.irr = 0;
	m_pic.imr = 0;
	m_pic.isr = 0;
	
	Rml::Event e;
	on_cmd_vga_stats_show(e);
}

void DevStatus::on_cmd_vga_update(Rml::Event &)
{
	m_vga.is_running = !m_vga.is_running;
	m_vga.btn_update->SetClass("on", m_vga.is_running);
}

void DevStatus::on_cmd_vga_stats_show(Rml::Event &)
{
	m_vga.stats.is_visible = !m_vga.stats.is_visible;
	get_element("vga_stats")->SetClass("d-none", !m_vga.stats.is_visible);
	get_element("cmd_vga_stats_show")->SetClass("expanded", m_vga.stats.is_visible);
}

void DevStatus::on_cmd_vga_genreg_show(Rml::Event &)
{
	m_vga.gen.is_visible = !m_vga.gen.is_visible;
	get_element("vga_genreg")->SetClass("d-none", !m_vga.gen.is_visible);
	get_element("cmd_vga_genreg_show")->SetClass("expanded", m_vga.gen.is_visible);
}

void DevStatus::on_cmd_vga_seq_show(Rml::Event &)
{
	m_vga.seq.is_visible = !m_vga.seq.is_visible;
	get_element("vga_seq")->SetClass("d-none", !m_vga.seq.is_visible);
	get_element("cmd_vga_seq_show")->SetClass("expanded", m_vga.seq.is_visible);
}

void DevStatus::on_cmd_vga_crtc_show(Rml::Event &)
{
	m_vga.crtc.is_visible = !m_vga.crtc.is_visible;
	get_element("vga_crtc")->SetClass("d-none", !m_vga.crtc.is_visible);
	get_element("cmd_vga_crtc_show")->SetClass("expanded", m_vga.crtc.is_visible);
}

void DevStatus::on_cmd_vga_dump_state(Rml::Event &)
{
	try {
		std::string captpath = g_program.config().find_file(CAPTURE_SECTION, CAPTURE_DIR);
		std::string statefile = FileSys::get_next_filename(captpath, "vga_state_", ".txt");
		if(!statefile.empty()) {
			m_gui->save_framebuffer(statefile + ".png", statefile + ".pal.png");
			m_machine->devices().vga()->state_to_textfile(statefile);
			std::string mex = "VGA state dumped to " + statefile;
			PINFOF(LOG_V0, LOG_GUI, "%s\n", mex.c_str());
			m_gui->show_message(mex.c_str());
		}
	} catch(std::exception &) {}
}

void DevStatus::on_cmd_vga_screenshot(Rml::Event &)
{
	m_gui->take_screenshot(true);
}

void DevStatus::on_cmd_vga_skip_scanl(Rml::Event &)
{
	VGA *vga = m_machine->devices().vga();
	if(vga) {
		vga->dbg_scanl_step();
		if(g_machine.is_paused()) {
			g_machine.cmd_resume(false);
		}
	}
}

void DevStatus::on_cmd_vga_skip_frame(Rml::Event &)
{
	VGA *vga = m_machine->devices().vga();
	if(vga) {
		vga->dbg_frame_step();
		if(g_machine.is_paused()) {
			g_machine.cmd_resume(false);
		}
	}
}

void DevStatus::on_cmd_pit_update(Rml::Event &)
{
	m_pit.is_running = !m_pit.is_running;
	m_pit.btn_update->SetClass("on", m_pit.is_running);
}

void DevStatus::on_cmd_pic_update(Rml::Event &)
{
	m_pic.is_running = !m_pic.is_running;
	m_pic.btn_update->SetClass("on", m_pic.is_running);
}

void DevStatus::update_pic()
{
	PIC *pic = m_machine->devices().pic();
	uint16_t pic_irq = pic->get_irq();
	uint16_t pic_irr = pic->get_irr();
	uint16_t pic_imr = pic->get_imr();
	uint16_t pic_isr = pic->get_isr();
	for(uint i=0; i<16; i++) {
		update_pic(pic_irq, pic_irr, pic_imr, pic_isr, i);
	}
	m_pic.irq = pic_irq;
	m_pic.irr = pic_irr;
	m_pic.imr = pic_imr;
	m_pic.isr = pic_isr;
}

void DevStatus::update_pic(uint16_t _irq, uint16_t _irr, uint16_t _imr, uint16_t _isr, uint _irqn)
{
	assert(_irqn<16);
	bool mybit, picbit;

	picbit = (_irq>>_irqn) & 1;
	mybit = (m_pic.irq>>_irqn) & 1;

	if(mybit && !picbit) {
		m_pic.irq_e[_irqn]->SetClass("led_active", false);
	} else if(!mybit && picbit) {
		m_pic.irq_e[_irqn]->SetClass("led_active", true);
	}

	picbit = (_irr>>_irqn) & 1;
	mybit = (m_pic.irr>>_irqn) & 1;

	if(mybit && !picbit) {
		m_pic.irr_e[_irqn]->SetClass("led_active", false);
	} else if(!mybit && picbit) {
		m_pic.irr_e[_irqn]->SetClass("led_active", true);
	}

	picbit = (_imr>>_irqn) & 1;
	mybit = (m_pic.imr>>_irqn) & 1;

	if(mybit && !picbit) {
		m_pic.imr_e[_irqn]->SetClass("led_active", false);
	} else if(!mybit && picbit) {
		m_pic.imr_e[_irqn]->SetClass("led_active", true);
	}

	picbit = (_isr >>_irqn) & 1;
	mybit = (m_pic.isr>>_irqn) & 1;

	if(mybit && !picbit) {
		m_pic.isr_e[_irqn]->SetClass("led_active", false);
	} else if(!mybit && picbit) {
		m_pic.isr_e[_irqn]->SetClass("led_active", true);
	}

}

void DevStatus::update_pit()
{
	for(unsigned i=0; i<3; i++) {
		update_pit(i);
	}
}

void DevStatus::update_pit(unsigned cnt)
{
	const PIT *pit = m_machine->devices().pit();
	if(!pit) {
		assert(false);
		return;
	}

	m_pit.mode[cnt]->SetInnerRML(format_uint16(pit->read_mode(cnt)));
	m_pit.cnt[cnt]->SetInnerRML(format_hex32(pit->read_CNT(cnt)));
	bool gate = pit->read_GATE(cnt);
	bool out = pit->read_OUT(cnt);
	m_pit.gate[cnt]->SetInnerRML(format_bit(gate));
	m_pit.out[cnt]->SetInnerRML(format_bit(out));
	if(gate && out) {
		m_pit.out[cnt]->SetClass("led_active", true);
	} else {
		m_pit.out[cnt]->SetClass("led_active", false);
	}
	m_pit.in[cnt]->SetInnerRML(format_hex16(pit->read_inlatch(cnt)));
}

void DevStatus::update_vga(bool _force)
{
	VGA *vga = m_machine->devices().vga();
	const VideoModeInfo & vm = vga->video_mode();
	static std::string str(100,'0');

	if(vm.mode == VGA_M_TEXT) {
		str_format(str, "%ux%u %s %ux%u %ux%u",
			vm.imgw, vm.imgh, vga->current_mode_string(),
			vm.textcols, vm.textrows,
			vm.cwidth, vm.cheight);
	} else {
		str_format(str, "%ux%u %s",
			vm.imgw, vm.imgh, vga->current_mode_string());
	}
	m_vga.mode->SetInnerRML(str);

	const VideoTimings & vt = vga->timings();
	m_vga.screen->SetInnerRML(str_format(str, "%ux%u (%ux%u) H:%.2fkHz V:%.2fHz",
		vm.xres, vm.yres,
		vm.framew, vm.frameh,
		vt.hfreq, vt.vfreq));

	// Status
	if(m_vga.stats.is_visible || _force) {
		const VideoStats & stats = vga->stats();
		m_vga.stats.frame_cnt->SetInnerRML(str_format(str, "%d", stats.frame_cnt));

		m_vga.stats.upd->SetClass("led_active", stats.updated_pix);
		if(stats.updated_pix) {
			m_vga.stats.pix_upd->SetInnerRML(str_format(str, "%d", stats.updated_pix));
		}

		m_vga.stats.saddr_line->SetInnerRML(str_format(str, "%d", stats.last_saddr_line));
		m_vga.stats.pal_line->SetInnerRML(str_format(str, "%d", stats.last_pal_line));

		bool disp = false, vret = false, hret = false;
		double scanline = vga->current_scanline(disp, hret, vret);
		m_vga.stats.scanl->SetInnerRML(str_format(str, "%.2f", scanline));
		m_vga.stats.disp_phase->SetClass("led_active", disp);
		m_vga.stats.hretr_phase->SetClass("led_active", hret);
		m_vga.stats.vretr_phase->SetClass("led_active", vret);
	}

	// Gen.Regs.
	if(m_vga.gen.is_visible || _force) {
		m_vga.gen.clock->SetInnerRML(str_format("%.0f Hz", vt.clock));
		constexpr const char *polstr[]{
			"H+ V+ (340 lines)", // POL_340 = 0,
			"H- V+ (400 lines)", // POL_400 = 1,
			"H+ V- (350 lines)", // POL_350 = 2,
			"H- V- (480 lines)"  // POL_480 = 3
		};
		m_vga.gen.polarity->SetInnerRML(str_format("%s", polstr[vga->gen_regs().misc_output.POL]));
		auto gen_registers = str_to_html(vga->gen_regs().registers_to_string());
		m_vga.gen.registers->SetInnerRML(gen_registers);
	}

	// Sequencer
	if(m_vga.seq.is_visible || _force) {
		m_vga.seq.cwidth->SetInnerRML(str_format("%u pixels", vt.cwidth));
		m_vga.seq.dotclock->SetInnerRML(str_format("%u", vga->sequencer().clocking.DC));
		auto seq_registers = str_to_html(vga->sequencer().registers_to_string());
		m_vga.seq.registers->SetInnerRML(seq_registers);
	}

	// CRTC
	if(m_vga.crtc.is_visible || _force) {
		m_vga.crtc.msl->SetInnerRML(str_format("%u", vga->crtc().max_scanline.MSL + 1));
		m_vga.crtc.dsc->SetInnerRML(str_format("%u", vga->crtc().max_scanline.DSC));
		if(vm.imgh != 0) {
			m_vga.crtc.scanlimgl->SetInnerRML(str_format("%u", vm.yres/vm.imgh));
		}

		/////////////////////
		// HORIZONTAL timings
		/// characters
		m_vga.crtc.h.total->SetInnerRML(str_format("%u", vt.htotal));
		m_vga.crtc.h.disp->SetInnerRML(str_format("%u", vt.hdend));
		// front porch
		m_vga.crtc.h.rblank->SetInnerRML(str_format("%d", vt.blank.right + vt.overscan.right));
		m_vga.crtc.h.rborder->SetInnerRML(str_format("%d", vt.overscan.right));
		m_vga.crtc.h.bstart->SetInnerRML(str_format("%d", vt.hbstart));
		// retrace
		m_vga.crtc.h.retr->SetInnerRML(str_format("%d", vt.blank.hretr));
		m_vga.crtc.h.rstart->SetInnerRML(str_format("%d", vt.hrstart));
		m_vga.crtc.h.rend->SetInnerRML(str_format("%d", vt.hrend));
		// back porch
		m_vga.crtc.h.lblank->SetInnerRML(str_format("%d", vt.blank.left + vt.overscan.left));
		m_vga.crtc.h.bend->SetInnerRML(str_format("%d", vt.hbend));
		m_vga.crtc.h.lborder->SetInnerRML(str_format("%d", vt.overscan.left));
	
		/// pixels
		m_vga.crtc.h.px.total->SetInnerRML(str_format("%u", vt.htotal * vt.cwidth));
		m_vga.crtc.h.px.disp->SetInnerRML(str_format("%u", vt.hdend * vt.cwidth));
		// front porch
		m_vga.crtc.h.px.rblank->SetInnerRML(str_format("%d", (vt.overscan.right + vt.blank.right) * vt.cwidth));
		m_vga.crtc.h.px.rborder->SetInnerRML(str_format("%d", vt.overscan.right * vt.cwidth));
		m_vga.crtc.h.px.bstart->SetInnerRML(str_format("%d", vt.hbstart * vt.cwidth));
		// retrace
		m_vga.crtc.h.px.retr->SetInnerRML(str_format("%d", vt.blank.hretr * vt.cwidth));
		m_vga.crtc.h.px.rstart->SetInnerRML(str_format("%d", vt.hrstart * vt.cwidth));
		m_vga.crtc.h.px.rend->SetInnerRML(str_format("%d", vt.hrend * vt.cwidth));
		// back porch
		m_vga.crtc.h.px.lblank->SetInnerRML(str_format("%d", (vt.overscan.left + vt.blank.left) * vt.cwidth));
		m_vga.crtc.h.px.bend->SetInnerRML(str_format("%d", vt.hbend * vt.cwidth));
		m_vga.crtc.h.px.lborder->SetInnerRML(str_format("%d", vt.overscan.left * vt.cwidth));
	
		/// microseconds
		m_vga.crtc.h.us.total->SetInnerRML(str_format("%.3f", vt.ns.htotal/1000.0));
		m_vga.crtc.h.us.disp->SetInnerRML(str_format("%.3f", vt.ns.hdend/1000.0));
		// front porch
		if(vt.ns.hrstart > vt.ns.hdend) {
			m_vga.crtc.h.us.rblank->SetInnerRML(str_format("%.3f", (vt.ns.hrstart - vt.ns.hdend)/1000.0));
		} else {
			m_vga.crtc.h.us.rblank->SetInnerRML("0");
		}
		if(vt.overscan.right) {
			m_vga.crtc.h.us.rborder->SetInnerRML(str_format("%.3f", (vt.ns.hbstart - vt.ns.hdend)/1000.0));
		} else {
			m_vga.crtc.h.us.rborder->SetInnerRML("0");
		}
		m_vga.crtc.h.us.bstart->SetInnerRML(str_format("%.3f", vt.ns.hbstart/1000.0));
		// retrace
		if(vt.ns.hrend > vt.ns.hrstart) {
			m_vga.crtc.h.us.retr->SetInnerRML(str_format("%.3f", (vt.ns.hrend - vt.ns.hrstart)/1000.0));
		} else {
			m_vga.crtc.h.us.retr->SetInnerRML("0");
		}
		m_vga.crtc.h.us.rstart->SetInnerRML(str_format("%.3f", vt.ns.hrstart/1000.0));
		m_vga.crtc.h.us.rend->SetInnerRML(str_format("%.3f", vt.ns.hrend/1000.0));
		// back porch
		if(vt.ns.htotal > vt.ns.hrend) {
			m_vga.crtc.h.us.lblank->SetInnerRML(str_format("%.3f", (vt.ns.htotal- vt.ns.hrend)/1000.0));
		} else {
			m_vga.crtc.h.us.lblank->SetInnerRML("0");
		}
		m_vga.crtc.h.us.bend->SetInnerRML(str_format("%.3f", vt.ns.hbend/1000.0));
		if(vt.overscan.left) {
			m_vga.crtc.h.us.lborder->SetInnerRML(str_format("%.3f", (vt.ns.htotal - vt.ns.hbend)/1000.0));
		} else {
			m_vga.crtc.h.us.lborder->SetInnerRML("0");
		}
	
		if(vt.hrend > vt.htotal) {
			m_vga.crtc.h.rend->SetClass("overflow", true);
			m_vga.crtc.h.px.rend->SetClass("overflow", true);
			m_vga.crtc.h.us.rend->SetClass("overflow", true);
		} else {
			m_vga.crtc.h.rend->SetClass("overflow", false);
			m_vga.crtc.h.px.rend->SetClass("overflow", false);
			m_vga.crtc.h.us.rend->SetClass("overflow", false);
		}
		if(vt.hbend > vt.htotal) {
			m_vga.crtc.h.bend->SetClass("overflow", true);
			m_vga.crtc.h.px.bend->SetClass("overflow", true);
			m_vga.crtc.h.us.bend->SetClass("overflow", true);
		} else {
			m_vga.crtc.h.bend->SetClass("overflow", false);
			m_vga.crtc.h.px.bend->SetClass("overflow", false);
			m_vga.crtc.h.us.bend->SetClass("overflow", false);
		}
	
	
		////////////////////
		/// VERTICAL timings
		m_vga.crtc.v.total->SetInnerRML(str_format("%u", vt.vtotal));
		m_vga.crtc.v.disp->SetInnerRML(str_format("%u", vt.vdend));
		// front porch
		m_vga.crtc.v.bblank->SetInnerRML(str_format("%d", vt.blank.bottom + vt.overscan.bottom));
		m_vga.crtc.v.bborder->SetInnerRML(str_format("%d", vt.overscan.bottom));
		m_vga.crtc.v.bstart->SetInnerRML(str_format("%u", vt.vbstart));
		// retrace
		m_vga.crtc.v.retr->SetInnerRML(str_format("%d", vt.blank.vretr));
		m_vga.crtc.v.rstart->SetInnerRML(str_format("%u", vt.vrstart));
		m_vga.crtc.v.rend->SetInnerRML(str_format("%u", vt.vrend));
		// back porch
		if(vt.vbend > vt.vtotal) {
			m_vga.crtc.v.tblank->SetInnerRML(str_format("%d + %d", (vt.blank.top - vt.vblank_skip), vt.vblank_skip));
		} else {
			m_vga.crtc.v.tblank->SetInnerRML(str_format("%d", vt.blank.top + vt.overscan.top));
		}
		m_vga.crtc.v.bend->SetInnerRML(str_format("%u", vt.vbend));
		m_vga.crtc.v.tborder->SetInnerRML(str_format("%d", vt.overscan.top));
	
		/// milliseconds
		m_vga.crtc.v.ms.total->SetInnerRML(str_format("%.3f", vt.ns.vtotal/1000000.0));
		m_vga.crtc.v.ms.disp->SetInnerRML(str_format("%.3f", vt.ns.vdend/1000000.0));
		// front porch
		m_vga.crtc.v.ms.bblank->SetInnerRML(str_format("%.3f", (vt.ns.vrstart - vt.ns.vdend)/1000000.0));
		if(vt.overscan.bottom) {
			m_vga.crtc.v.ms.bborder->SetInnerRML(str_format("%.3f", (vt.ns.vbstart - vt.ns.vdend)/1000000.0));
		} else {
			m_vga.crtc.v.ms.bborder->SetInnerRML("0");
		}
		m_vga.crtc.v.ms.bstart->SetInnerRML(str_format("%.3f", vt.ns.vbstart/1000000.0));
		// retrace
		m_vga.crtc.v.ms.retr->SetInnerRML(str_format("%.3f", (vt.ns.vrend - vt.ns.vrstart)/1000000.0));
		m_vga.crtc.v.ms.rstart->SetInnerRML(str_format("%.3f", vt.ns.vrstart/1000000.0));
		m_vga.crtc.v.ms.rend->SetInnerRML(str_format("%.3f", vt.ns.vrend/1000000.0));
		// back porch
		m_vga.crtc.v.ms.tblank->SetInnerRML(str_format("%.3f", (vt.ns.vtotal- vt.ns.vrend)/1000000.0));
		m_vga.crtc.v.ms.bend->SetInnerRML(str_format("%.3f", vt.ns.vbend/1000000.0));
		if(vt.overscan.top) {
			m_vga.crtc.v.ms.tborder->SetInnerRML(str_format("%.3f", (vt.ns.vtotal - vt.ns.vbend)/1000000.0));
		} else {
			m_vga.crtc.v.ms.tborder->SetInnerRML("0");
		}

		if(vt.vbend > vt.vtotal) {
			m_vga.crtc.v.bend->SetClass("overflow", true);
			m_vga.crtc.v.ms.bend->SetClass("overflow", true);
		} else {
			m_vga.crtc.v.bend->SetClass("overflow", false);
			m_vga.crtc.v.ms.bend->SetClass("overflow", false);
		}

		m_vga.crtc.v.vblank_skip->SetInnerRML(str_format("%u", vt.vblank_skip));
		m_vga.crtc.v.last_vis_sl->SetInnerRML(str_format("%u", vt.last_vis_sl));

		auto crtc_registers = str_to_html(vga->crtc().registers_to_string());
		m_vga.crtc.registers->SetInnerRML(crtc_registers);

		auto crtc_latches = str_to_html(vga->crtc().latches_to_string());
		m_vga.crtc.latches->SetInnerRML(crtc_latches);
	}
}

void DevStatus::update()
{
	if(!m_enabled) {
		return;
	}

	static bool updated = false;
	if(m_machine->is_paused() && !updated) {
		update_vga(true);
		update_pic();
		update_pit();
	} else {
		if(m_vga.is_running && m_vga.panel->IsVisible()) {
			update_vga();
		}
		if(m_pic.is_running && m_pic.panel->IsVisible()) {
			update_pic();
		}
		if(m_pit.is_running && m_pit.panel->IsVisible()) {
			update_pit();
		}
	}
	
	if(m_machine->is_paused()) {
		updated = true;
	} else {
		updated = false;
	}
}
