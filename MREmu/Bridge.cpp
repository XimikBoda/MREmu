#include "Bridge.h"
#include "Memory.h"
#include "ARModule.h"
#include "Cpu.h"
#include "GDB.h"
#include <string>
#include <iostream>
#include <unicorn/unicorn.h>

#include <vmgraph.h>
#include <vmres.h>
#include <vmtimer.h>
#include <vmpromng.h>
#include <vmgettag.h>
#include <vmchset.h>
#include <vmsim.h>
#include <vmstdlib.h>
#include <vmmm.h>

#include "MREngine/Sock.h"

VMINT vm_get_res_header();//tmp

namespace Cpu {
	void printREG(uc_engine* uc);
}

const unsigned char bxlr[2] = { 0x70, 0x47 };
const unsigned char idle_bin[2] = { 0xfe, 0xe7 };

extern uc_engine* uc;
uc_hook trace;

namespace Bridge {
	struct br_func {
		std::string name;
		void (*f)(uc_engine* uc);
	};

	unsigned char* func_ptr;
	uint32_t idle_p = 0;

	ARModule armodule;

	static unsigned int read_reg(uc_engine* uc, int reg) {
		unsigned int r = 0;
		uc_reg_read(uc, reg, &r);
		return r;
	}

	static void write_reg(uc_engine* uc, int reg, unsigned int val) {
		uc_reg_write(uc, reg, &val);
	}

	void put_to_reg(uc_engine* uc, unsigned int val) {
		unsigned int sp = read_reg(uc, UC_ARM_REG_SP);
		sp -= 4;
		write_reg(uc, UC_ARM_REG_SP, sp);
		uc_mem_write(uc, sp, &val, 4);
	}

	unsigned int top_sp(uc_engine* uc, int l) {
		unsigned int el, sp = read_reg(uc, UC_ARM_REG_SP);
		uc_mem_read(uc, sp + 4 * l, &el, 4);
		return el;
	}

	unsigned int top_from_r(uc_engine* uc, unsigned int r, int l) {
		unsigned int el;
		uc_mem_read(uc, r + 4 * l, &el, 4);
		return el;
	}

	static void write_ret(uc_engine* uc, unsigned int val) {
		write_reg(uc, UC_ARM_REG_R0, val);
	}

	unsigned int read_arg(uc_engine* uc, int ind) {
		if (ind < 4)
			return read_reg(uc, UC_ARM_REG_R0 + ind);
		else
			return top_sp(uc, ind - 4);
	}


	std::vector<br_func> func_map = 
	{
		{"vm_get_sym_entry", [](uc_engine* uc) {
			write_ret(uc, vm_get_sym_entry((char*)ADDRESS_FROM_EMU(read_arg(uc, 0))));
		}},



		// System
		{"vm_get_time", [](uc_engine* uc) {
			write_ret(uc, vm_get_time((vm_time_t*)ADDRESS_FROM_EMU(read_arg(uc, 0))));
		}},
		{"vm_get_curr_utc", [](uc_engine* uc) {
			write_ret(uc, vm_get_curr_utc((VMUINT*)ADDRESS_FROM_EMU(read_arg(uc, 0))));
		}},
		{"vm_get_sys_time_zone", [](uc_engine* uc) {
			write_ret(uc, vm_get_sys_time_zone());
		}},
		{"vm_get_malloc_stat", [](uc_engine* uc) {
			write_ret(uc, ADDRESS_TO_EMU(vm_get_malloc_stat()));
		}},
		{"vm_malloc", [](uc_engine* uc) {
			write_ret(uc, ADDRESS_TO_EMU(vm_malloc(read_arg(uc, 0))));
		}},
		{"vm_calloc", [](uc_engine* uc) {
			write_ret(uc, ADDRESS_TO_EMU(vm_calloc(read_arg(uc, 0))));
		}},
		{"vm_realloc", [](uc_engine* uc) {
			write_ret(uc, ADDRESS_TO_EMU(
				vm_realloc((void*)ADDRESS_FROM_EMU(read_arg(uc, 0)),
					read_arg(uc, 1))));
		}},
		{"vm_free", [](uc_engine* uc) {
			vm_free((void*)ADDRESS_FROM_EMU(read_arg(uc, 0)));
		}},
		{"vm_reg_sysevt_callback", [](uc_engine* uc) {
			vm_reg_sysevt_callback((void (*)(VMINT message, VMINT param))read_arg(uc, 0));
		}},
		{"vm_get_mre_total_mem_size", [](uc_engine* uc) {
			write_ret(uc, ADDRESS_TO_EMU(vm_get_mre_total_mem_size()));
		}},
		{"vm_get_tick_count", [](uc_engine* uc) {
			write_ret(uc, vm_get_tick_count());
		}},
		{"vm_get_exec_filename", [](uc_engine* uc) {
			write_ret(uc, ADDRESS_TO_EMU(
				vm_get_exec_filename(
					(VMWSTR)ADDRESS_FROM_EMU(read_arg(uc, 0)))));
		}},
		{"vm_get_sys_property", [](uc_engine* uc) {
			write_ret(uc, ADDRESS_TO_EMU(
				vm_get_sys_property(
					read_arg(uc, 0),
					(VMCHAR*)ADDRESS_FROM_EMU(read_arg(uc, 1)),
					read_arg(uc, 2))));
		}},
		{"vm_get_vm_tag", [](uc_engine* uc) {
			write_ret(uc,
				vm_get_vm_tag(
					(short*)ADDRESS_FROM_EMU(read_arg(uc, 0)),
					read_arg(uc, 1),
					(void*)ADDRESS_FROM_EMU(read_arg(uc, 2)),
					(int*)ADDRESS_FROM_EMU(read_arg(uc, 3))));
		}},
		{"vm_switch_power_saving_mode", [](uc_engine* uc) {
			write_ret(uc, vm_switch_power_saving_mode((power_saving_mode_enum)read_arg(uc, 0)));
		}},
		{"vm_appmgr_is_installed", [](uc_engine* uc) {
			write_ret(uc,
				vm_appmgr_is_installed(
					(VMWSTR)ADDRESS_FROM_EMU(read_arg(uc, 0)),
					(VMCHAR*)ADDRESS_FROM_EMU(read_arg(uc, 1))));
		}},
		{"vm_appmgr_get_installed_list", [](uc_engine* uc) {
			write_ret(uc,
				vm_appmgr_get_installed_list(
					read_arg(uc, 0),
					(vm_install_id*)ADDRESS_FROM_EMU(read_arg(uc, 1)),
					(VMUINT*)ADDRESS_FROM_EMU(read_arg(uc, 2))));
		}},



		// Program manager
		{"vm_pmng_get_current_handle", [](uc_engine* uc) {
			write_ret(uc, vm_pmng_get_current_handle());
		}},
		{"vm_reg_msg_proc", [](uc_engine* uc) {
			vm_reg_msg_proc((VM_MESSAGE_PROC)read_arg(uc, 0));
		}},
		{"vm_post_msg", [](uc_engine* uc) {
			write_ret(uc,
				vm_post_msg(
					read_arg(uc, 0),
					read_arg(uc, 1),
					read_arg(uc, 2),
					read_arg(uc, 3)));
		}},



		// Timer
		{"vm_create_timer", [](uc_engine* uc) {
			write_ret(uc, vm_create_timer(read_arg(uc, 0),
				(VM_TIMERPROC_T)read_arg(uc, 1)));
		}},
		{"vm_delete_timer", [](uc_engine* uc) {
			write_ret(uc, vm_delete_timer(read_arg(uc, 0)));
		}},
		{"vm_create_timer_ex", [](uc_engine* uc) {
			write_ret(uc, vm_create_timer_ex(read_arg(uc, 0),
				(VM_TIMERPROC_T)read_arg(uc, 1)));
		}},
		{"vm_delete_timer_ex", [](uc_engine* uc) {
			write_ret(uc, vm_delete_timer_ex(read_arg(uc, 0)));
		}}, // done



		// IO
		{"vm_reg_keyboard_callback", [](uc_engine* uc) {
			vm_reg_keyboard_callback(
				(vm_key_handler_t)read_arg(uc, 0));
		}},
		{"vm_file_open", [](uc_engine* uc) {
			write_ret(uc, vm_file_open(
				(VMWSTR)ADDRESS_FROM_EMU(read_arg(uc, 0)),
				read_arg(uc, 1),
				read_arg(uc, 2)));
		}},
		{"vm_file_close", [](uc_engine* uc) {
			vm_file_close(read_arg(uc, 0));
		}},
		{"vm_file_read", [](uc_engine* uc) {
			write_ret(uc, vm_file_read(
				read_arg(uc, 0),
				(void*)ADDRESS_FROM_EMU(read_arg(uc, 1)),
				read_arg(uc, 2),
				(VMUINT*)ADDRESS_FROM_EMU(read_arg(uc, 3))));
		}},
		{"vm_file_write", [](uc_engine* uc) {
			write_ret(uc, vm_file_write(
				read_arg(uc, 0),
				(void*)ADDRESS_FROM_EMU(read_arg(uc, 1)),
				read_arg(uc, 2),
				(VMUINT*)ADDRESS_FROM_EMU(read_arg(uc, 3))));
		}},
		{"vm_file_commit", [](uc_engine* uc) {
			write_ret(uc, vm_file_commit(read_arg(uc, 0)));
		}},
		{"vm_file_seek", [](uc_engine* uc) {
			write_ret(uc, vm_file_seek(
				read_arg(uc, 0),
				read_arg(uc, 1),
				read_arg(uc, 2)));
		}},
		{"vm_file_tell", [](uc_engine* uc) {
			write_ret(uc, vm_file_tell(read_arg(uc, 0)));
		}},
		{"vm_file_is_eof", [](uc_engine* uc) {
			write_ret(uc, vm_file_is_eof(read_arg(uc, 0)));
		}},
		{"vm_file_getfilesize", [](uc_engine* uc) {
			write_ret(uc, vm_file_getfilesize(
				read_arg(uc, 0),
				(VMUINT*)ADDRESS_FROM_EMU(read_arg(uc, 1))));
		}},
		{"vm_file_delete", [](uc_engine* uc) {
			write_ret(uc, vm_file_delete(
				(VMWSTR)ADDRESS_FROM_EMU(read_arg(uc, 0))));
		}},
		{"vm_file_rename", [](uc_engine* uc) {
			write_ret(uc, vm_file_rename(
				(VMWSTR)ADDRESS_FROM_EMU(read_arg(uc, 0)),
				(VMWSTR)ADDRESS_FROM_EMU(read_arg(uc, 1))));
		}},
		{"vm_file_mkdir",  [](uc_engine* uc) {
			write_ret(uc, vm_file_mkdir((VMWSTR)ADDRESS_FROM_EMU(read_arg(uc, 0))));
		}},
		{"vm_file_set_attributes",  [](uc_engine* uc) {
			write_ret(uc, vm_file_set_attributes(
				(VMWSTR)ADDRESS_FROM_EMU(read_arg(uc, 0)),
				read_arg(uc, 1)));
		}},
		{"vm_file_get_attributes",  [](uc_engine* uc) {
			write_ret(uc, vm_file_get_attributes((VMWSTR)ADDRESS_FROM_EMU(read_arg(uc, 0))));
		}},
		{"vm_find_first",  [](uc_engine* uc) {
			write_ret(uc, vm_find_first(
				(VMWSTR)ADDRESS_FROM_EMU(read_arg(uc, 0)),
				(vm_fileinfo_t*)ADDRESS_FROM_EMU(read_arg(uc, 1))));
		}},
		{"vm_find_next",  [](uc_engine* uc) {
			write_ret(uc, vm_find_next(
				read_arg(uc, 0),
				(vm_fileinfo_t*)ADDRESS_FROM_EMU(read_arg(uc, 1))));
		}},
		{"vm_find_close",  [](uc_engine* uc) {
			vm_find_close(read_arg(uc, 0));
		}},
		{"vm_find_first_ext",  [](uc_engine* uc) {
			write_ret(uc, vm_find_first_ext(
				(VMWSTR)ADDRESS_FROM_EMU(read_arg(uc, 0)),
				(vm_fileinfo_ext*)ADDRESS_FROM_EMU(read_arg(uc, 1))));
		}},
		{"vm_find_next_ext",  [](uc_engine* uc) {
			write_ret(uc, vm_find_next_ext(
				read_arg(uc, 0),
				(vm_fileinfo_ext*)ADDRESS_FROM_EMU(read_arg(uc, 1))));
		}},
		{"vm_find_close_ext",  [](uc_engine* uc) {
			vm_find_close_ext(read_arg(uc, 0));
		}},
		{"vm_file_get_modify_time",  [](uc_engine* uc) {
			write_ret(uc, vm_file_get_modify_time(
				(VMWSTR)ADDRESS_FROM_EMU(read_arg(uc, 0)),
				(vm_time_t*)ADDRESS_FROM_EMU(read_arg(uc, 1))));
		}},
		{"vm_get_removeable_driver", [](uc_engine* uc) {
			write_ret(uc, vm_get_removable_driver());
		}},
		{"vm_get_system_driver", [](uc_engine* uc) {
			write_ret(uc, vm_get_system_driver());
		}},
		{"vm_get_disk_free_space", [](uc_engine* uc) {
			write_ret(uc, vm_get_disk_free_space((VMWSTR)ADDRESS_FROM_EMU(read_arg(uc, 0))));
		}},
		{"vm_get_disk_info", [](uc_engine* uc) {
			write_ret(uc, vm_get_disk_info(
				(VMCHAR*)ADDRESS_FROM_EMU(read_arg(uc, 0)),
				(vm_fs_disk_info*)ADDRESS_FROM_EMU(read_arg(uc, 1)),
				(vm_fs_di_enum)read_arg(uc, 2)
			));
		}},
		{"vm_is_support_keyborad", [](uc_engine* uc) {
			write_ret(uc, vm_is_support_keyborad());
		}},



		// SIM
		{"vm_has_sim_card", [](uc_engine* uc) {
			write_ret(uc, vm_has_sim_card());
		}},
		{"vm_get_imei", [](uc_engine* uc) {
			write_ret(uc, ADDRESS_TO_EMU(vm_get_imei()));
		}},
		{"vm_get_imsi", [](uc_engine* uc) {
			write_ret(uc, ADDRESS_TO_EMU(vm_get_imsi()));
		}},
		{"vm_sim_card_count", [](uc_engine* uc) {
			write_ret(uc, vm_sim_card_count());
		}},
		{"vm_set_active_sim_card", [](uc_engine* uc) {
			write_ret(uc, vm_set_active_sim_card(
				read_arg(uc, 0)));
		}},
		{"vm_get_sim_card_status", [](uc_engine* uc) {
			write_ret(uc, vm_get_sim_card_status(
				read_arg(uc, 0)));
		}},
		{"vm_query_operator_code", [](uc_engine* uc) {
			write_ret(uc, vm_query_operator_code(
				(VMCHAR*)ADDRESS_FROM_EMU(read_arg(uc, 0)),
				read_arg(uc, 1)));
		}},
		{"vm_sim_get_active_sim_card", [](uc_engine* uc) {
			write_ret(uc, vm_sim_get_active_sim_card());
		}},
		{"vm_sim_max_card_count", [](uc_engine* uc) {
			write_ret(uc, vm_sim_max_card_count());
		}},
		{"vm_sim_get_prefer_sim_card", [](uc_engine* uc) {
			write_ret(uc, vm_sim_max_card_count());
		}},



		// Graphic
		{"vm_graphic_get_screen_width", [](uc_engine* uc) {
			write_ret(uc, vm_graphic_get_screen_width());
		}},
		{"vm_graphic_get_screen_height", [](uc_engine* uc) {
			write_ret(uc, vm_graphic_get_screen_height());
		}},
		{"vm_graphic_create_layer", [](uc_engine* uc) {
			write_ret(uc,
				vm_graphic_create_layer(
					read_arg(uc, 0),
					read_arg(uc, 1),
					read_arg(uc, 2),
					read_arg(uc, 3),
					read_arg(uc, 4)));
		}},
		{"vm_graphic_delete_layer", [](uc_engine* uc) {
			write_ret(uc, vm_graphic_delete_layer(read_arg(uc, 0)));
		}},
		{"vm_graphic_active_layer", [](uc_engine* uc) {
			write_ret(uc, vm_graphic_active_layer(read_arg(uc, 0)));
		}},
		{"vm_graphic_get_layer_buffer", [](uc_engine* uc) {
			write_ret(uc, ADDRESS_TO_EMU(vm_graphic_get_layer_buffer(read_arg(uc, 0))));
		}},
		{"vm_graphic_flush_layer", [](uc_engine* uc) {
			write_ret(uc, vm_graphic_flush_layer(
				(VMINT*)ADDRESS_FROM_EMU(read_arg(uc, 0)),
				read_arg(uc, 1)));
		}},
		{"vm_graphic_flatten_layer", [](uc_engine* uc) {
			write_ret(uc, vm_graphic_flatten_layer(
				(VMINT*)ADDRESS_FROM_EMU(read_arg(uc, 0)),
				read_arg(uc, 1)));
		}},
		{"vm_graphic_translate_layer", [](uc_engine* uc) {
			write_ret(uc,
				vm_graphic_translate_layer(
					read_arg(uc, 0),
					read_arg(uc, 1),
					read_arg(uc, 2)));
		}},
		{"vm_graphic_get_bits_per_pixel", [](uc_engine* uc) {
			write_ret(uc, vm_graphic_get_bits_per_pixel());
		}},
		{"vm_graphic_create_canvas", [](uc_engine* uc) {
			write_ret(uc,
				vm_graphic_create_canvas(
					read_arg(uc, 0),
					read_arg(uc, 1)));
		}},
		{"vm_graphic_create_canvas_cf", [](uc_engine* uc) {
			write_ret(uc,
				vm_graphic_create_canvas_cf(
					read_arg(uc, 0),
					read_arg(uc, 1),
					read_arg(uc, 2)));
		}},
		{"vm_graphic_release_canvas", [](uc_engine* uc) {
			vm_graphic_release_canvas(
				read_arg(uc, 0));
		}},
		{"vm_graphic_get_canvas_buffer", [](uc_engine* uc) {
			write_ret(uc, ADDRESS_TO_EMU(
				vm_graphic_get_canvas_buffer(
					read_arg(uc, 0))));
		}},
		{"vm_graphic_create_layer_ex", [](uc_engine* uc) {
			write_ret(uc, vm_graphic_create_layer_ex(
				read_arg(uc, 0),
				read_arg(uc, 1),
				read_arg(uc, 2),
				read_arg(uc, 3),
				read_arg(uc, 4),
				read_arg(uc, 5),
				(VMUINT8*)ADDRESS_FROM_EMU(read_arg(uc, 6))));
		}},
		{"vm_graphic_create_layer_cf", [](uc_engine* uc) {
			write_ret(uc, vm_graphic_create_layer_cf(
				read_arg(uc, 0),
				read_arg(uc, 1),
				read_arg(uc, 2),
				read_arg(uc, 3),
				read_arg(uc, 4),
				(vm_graphic_color_argb*)read_arg(uc, 5),
				read_arg(uc, 6),
				(VMUINT8*)ADDRESS_FROM_EMU(read_arg(uc, 7)),
				read_arg(uc, 8)));
		}},
		{"vm_graphic_load_image", [](uc_engine* uc) {
			write_ret(uc,
				vm_graphic_load_image(
					(VMUINT8*)ADDRESS_FROM_EMU(read_arg(uc, 0)),
					read_arg(uc, 1)));
		}},
		{"vm_graphic_get_img_property", [](uc_engine* uc) {
			write_ret(uc, ADDRESS_TO_EMU(
				vm_graphic_get_img_property(
					read_arg(uc, 0),
					read_arg(uc, 1))));
		}},
		{"vm_graphic_blt", [](uc_engine* uc) {
			vm_graphic_blt(
				(VMBYTE*)ADDRESS_FROM_EMU(read_arg(uc, 0)),
				read_arg(uc, 1),
				read_arg(uc, 2),
				(VMBYTE*)ADDRESS_FROM_EMU(read_arg(uc, 3)),
				read_arg(uc, 4),
				read_arg(uc, 5),
				read_arg(uc, 6),
				read_arg(uc, 7),
				read_arg(uc, 8)
			);
		}},
		{"vm_graphic_blt_ex", [](uc_engine* uc) {
			vm_graphic_blt_ex(
				(VMBYTE*)ADDRESS_FROM_EMU(read_arg(uc, 0)),
				read_arg(uc, 1),
				read_arg(uc, 2),
				(VMBYTE*)ADDRESS_FROM_EMU(read_arg(uc, 3)),
				read_arg(uc, 4),
				read_arg(uc, 5),
				read_arg(uc, 6),
				read_arg(uc, 7),
				read_arg(uc, 8),
				read_arg(uc, 9)
			);
		}},
		{"vm_graphic_rotate", [](uc_engine* uc) { //WRONG
			vm_graphic_rotate(
				(VMBYTE*)ADDRESS_FROM_EMU(read_arg(uc, 0)),
				read_arg(uc, 1),
				read_arg(uc, 2),
				(VMBYTE*)ADDRESS_FROM_EMU(read_arg(uc, 3)),
				read_arg(uc, 4),
				read_arg(uc, 5)
			);
		}},
		{"vm_graphic_mirror", [](uc_engine* uc) {
			vm_graphic_mirror(
				(VMBYTE*)ADDRESS_FROM_EMU(read_arg(uc, 0)),
				read_arg(uc, 1),
				read_arg(uc, 2),
				(VMBYTE*)ADDRESS_FROM_EMU(read_arg(uc, 3)),
				read_arg(uc, 4),
				read_arg(uc, 5)
			);
		}},
		{"vm_graphic_set_pixel", [](uc_engine* uc) {
			vm_graphic_set_pixel(
				(VMUINT8*)ADDRESS_FROM_EMU(read_arg(uc, 0)),
				read_arg(uc, 1),
				read_arg(uc, 2),
				read_arg(uc, 3));
		}},
		{"vm_graphic_set_pixel_ex", [](uc_engine* uc) {
			vm_graphic_set_pixel_ex(
				read_arg(uc, 0),
				read_arg(uc, 1),
				read_arg(uc, 2));
		}},
		{"vm_graphic_line", [](uc_engine* uc) {
			vm_graphic_line(
				(VMUINT8*)ADDRESS_FROM_EMU(read_arg(uc, 0)),
				read_arg(uc, 1),
				read_arg(uc, 2),
				read_arg(uc, 3),
				read_arg(uc, 4),
				read_arg(uc, 5));
		}},
		{"vm_graphic_line_ex", [](uc_engine* uc) {
			vm_graphic_line_ex(
				read_arg(uc, 0),
				read_arg(uc, 1),
				read_arg(uc, 2),
				read_arg(uc, 3),
				read_arg(uc, 4));
		}},
		{"vm_graphic_fill_rect", [](uc_engine* uc) {
			vm_graphic_fill_rect(
				(VMUINT8*)ADDRESS_FROM_EMU(read_arg(uc, 0)),
				read_arg(uc, 1),
				read_arg(uc, 2),
				read_arg(uc, 3),
				read_arg(uc, 4),
				read_arg(uc, 5),
				read_arg(uc, 6));
		}},
		{"vm_graphic_fill_rect_ex", [](uc_engine* uc) {
			vm_graphic_fill_rect_ex(
				read_arg(uc, 0),
				read_arg(uc, 1),
				read_arg(uc, 2),
				read_arg(uc, 3),
				read_arg(uc, 4));
		}},
		{"vm_graphic_roundrect", [](uc_engine* uc) {
			vm_graphic_roundrect(
				(VMUINT8*)ADDRESS_FROM_EMU(read_arg(uc, 0)),
				read_arg(uc, 1),
				read_arg(uc, 2),
				read_arg(uc, 3),
				read_arg(uc, 4),
				read_arg(uc, 5),
				read_arg(uc, 6));
		}},
		{"vm_graphic_roundrect_ex", [](uc_engine* uc) {
			vm_graphic_roundrect_ex(
				read_arg(uc, 0),
				read_arg(uc, 1),
				read_arg(uc, 2),
				read_arg(uc, 3),
				read_arg(uc, 4),
				read_arg(uc, 5));
		}},
		{"vm_graphic_fill_roundrect", [](uc_engine* uc) {
			vm_graphic_fill_roundrect(
				(VMUINT8*)ADDRESS_FROM_EMU(read_arg(uc, 0)),
				read_arg(uc, 1),
				read_arg(uc, 2),
				read_arg(uc, 3),
				read_arg(uc, 4),
				read_arg(uc, 5),
				read_arg(uc, 6));
		}},
		{"vm_graphic_fill_roundrect_ex", [](uc_engine* uc) {
			vm_graphic_fill_roundrect_ex(
				read_arg(uc, 0),
				read_arg(uc, 1),
				read_arg(uc, 2),
				read_arg(uc, 3),
				read_arg(uc, 4),
				read_arg(uc, 5));
		}},
		{"vm_graphic_rect", [](uc_engine* uc) {
			vm_graphic_rect(
				(VMUINT8*)ADDRESS_FROM_EMU(read_arg(uc, 0)),
				read_arg(uc, 1),
				read_arg(uc, 2),
				read_arg(uc, 3),
				read_arg(uc, 4),
				read_arg(uc, 5));
		}},
		{"vm_graphic_rect_ex", [](uc_engine* uc) {
			vm_graphic_rect_ex(
				read_arg(uc, 0),
				read_arg(uc, 1),
				read_arg(uc, 2),
				read_arg(uc, 3),
				read_arg(uc, 4));
		}},
		{"vm_graphic_fill_polygon", [](uc_engine* uc) {
			vm_graphic_fill_polygon(
				read_arg(uc, 0),
				(vm_graphic_point*)ADDRESS_FROM_EMU(read_arg(uc, 1)),
				read_arg(uc, 2));
		}},
		{"vm_graphic_set_clip", [](uc_engine* uc) {
			vm_graphic_set_clip(
				read_arg(uc, 0),
				read_arg(uc, 1),
				read_arg(uc, 2),
				read_arg(uc, 3));
		}},
		{"vm_graphic_reset_clip", [](uc_engine* uc) {
			vm_graphic_reset_clip();
		}},
		{"vm_graphic_flush_screen", [](uc_engine* uc) {
			vm_graphic_flush_screen();
		}},
		{"vm_graphic_is_r2l_state", [](uc_engine* uc) {
			write_ret(uc, vm_graphic_is_r2l_state());
		}},
		{"vm_graphic_setcolor", [](uc_engine* uc) {
			write_ret(uc, vm_graphic_setcolor(
				(vm_graphic_color*)ADDRESS_FROM_EMU(read_arg(uc, 0))));
		}},
		{"vm_graphic_canvas_set_trans_color", [](uc_engine* uc) {
			write_ret(uc, vm_graphic_canvas_set_trans_color(
				read_arg(uc, 0),
				read_arg(uc, 1)));
		}},



		// Textout
		{"vm_graphic_get_character_height", [](uc_engine* uc) {
			write_ret(uc, vm_graphic_get_character_height());
		}},
		{"vm_graphic_get_character_width", [](uc_engine* uc) {
			write_ret(uc, vm_graphic_get_character_width(
				read_arg(uc, 0)));
		}},
		{"vm_graphic_get_string_width", [](uc_engine* uc) {
			write_ret(uc, vm_graphic_get_string_width(
				(VMWSTR)ADDRESS_FROM_EMU(read_arg(uc, 0))));
		}},
		{"vm_graphic_get_string_height", [](uc_engine* uc) {
			write_ret(uc, vm_graphic_get_string_height(
				(VMWSTR)ADDRESS_FROM_EMU(read_arg(uc, 0))));
		}},
		{"vm_graphic_measure_character", [](uc_engine* uc) {
			write_ret(uc, vm_graphic_measure_character(
				read_arg(uc, 0),
				(VMINT*)ADDRESS_FROM_EMU(read_arg(uc, 1)),
				(VMINT*)ADDRESS_FROM_EMU(read_arg(uc, 2))));
		}},
		{"vm_graphic_get_character_info", [](uc_engine* uc) {
			write_ret(uc, vm_graphic_get_character_info(
				read_arg(uc, 0),
				(vm_graphic_char_info*)ADDRESS_FROM_EMU(read_arg(uc, 1))));
		}},
		{"vm_graphic_set_font", [](uc_engine* uc) {
			vm_graphic_set_font(
				(font_size_t)read_arg(uc, 0));
		}},
		{"vm_graphic_textout", [](uc_engine* uc) {
			vm_graphic_textout(
				(VMUINT8*)ADDRESS_FROM_EMU(read_arg(uc, 0)),
				read_arg(uc, 1),
				read_arg(uc, 2),
				(VMWSTR)ADDRESS_FROM_EMU(read_arg(uc, 3)),
				read_arg(uc, 4),
				read_arg(uc, 5));
		}},
		{"vm_graphic_textout_by_baseline", [](uc_engine* uc) {
			vm_graphic_textout_by_baseline(
				(VMUINT8*)ADDRESS_FROM_EMU(read_arg(uc, 0)),
				read_arg(uc, 1),
				read_arg(uc, 2),
				(VMWSTR)ADDRESS_FROM_EMU(read_arg(uc, 3)),
				read_arg(uc, 4),
				read_arg(uc, 5),
				read_arg(uc, 6));
		}},
		{"vm_font_set_font_size", [](uc_engine* uc) {
			write_ret(uc, vm_font_set_font_size(
				read_arg(uc, 0)));
		}},
		{"vm_font_set_font_style", [](uc_engine* uc) {
			write_ret(uc, vm_font_set_font_style(
				read_arg(uc, 0),
				read_arg(uc, 1),
				read_arg(uc, 2)));
		}},
		{"vm_graphic_textout_to_layer", [](uc_engine* uc) {
			write_ret(uc, vm_graphic_textout_to_layer(
				read_arg(uc, 0),
				read_arg(uc, 1),
				read_arg(uc, 2),
				(VMWSTR)ADDRESS_FROM_EMU(read_arg(uc, 3)),
				read_arg(uc, 4)));
		}},
		{"vm_graphic_get_string_baseline", [](uc_engine* uc) {
			write_ret(uc, vm_graphic_get_string_baseline(
				(VMWSTR)ADDRESS_FROM_EMU(read_arg(uc, 0))));
		}},
		{"vm_graphic_is_use_vector_font", [](uc_engine* uc) {
			write_ret(uc, vm_graphic_is_use_vector_font());
		}},
		{"vm_graphic_get_char_num_in_width", [](uc_engine* uc) {
			write_ret(uc, vm_graphic_get_char_num_in_width(
				(VMWSTR)ADDRESS_FROM_EMU(read_arg(uc, 0)),
				read_arg(uc, 1),
				read_arg(uc, 2),
				read_arg(uc, 3)));
		}},



		// Resources
		{"vm_load_resource", [](uc_engine* uc) {
			write_ret(uc,
				ADDRESS_TO_EMU(vm_load_resource(
					(char*)ADDRESS_FROM_EMU(read_arg(uc, 0)),
					(VMINT*)ADDRESS_FROM_EMU(read_arg(uc, 1))
				)));
		}},
		{"vm_resource_get_data", [](uc_engine* uc) {
			write_ret(uc,
				ADDRESS_TO_EMU(vm_resource_get_data(
					(VMUINT8*)ADDRESS_FROM_EMU(read_arg(uc, 0)),
					read_arg(uc, 1),
					read_arg(uc, 2)
				)));
		}},
		{"vm_get_res_header", [](uc_engine* uc) {
			write_ret(uc,
				vm_get_res_header());
		}},



		// CharSet
		{"vm_ucs2_to_gb2312", [](uc_engine* uc) {
			write_ret(uc,
				vm_ucs2_to_ascii(
					(VMSTR)ADDRESS_FROM_EMU(read_arg(uc, 0)),
					read_arg(uc, 1),
					(VMWSTR)ADDRESS_FROM_EMU(read_arg(uc, 2))
				));
		}},
		{"vm_gb2312_to_ucs2", [](uc_engine* uc) {
			write_ret(uc,
				vm_gb2312_to_ucs2(
					(VMWSTR)ADDRESS_FROM_EMU(read_arg(uc, 0)),
					read_arg(uc, 1),
					(VMSTR)ADDRESS_FROM_EMU(read_arg(uc, 2))
				));
		}},
		{"vm_ucs2_to_ascii", [](uc_engine* uc) {
			write_ret(uc,
				vm_ucs2_to_ascii(
					(VMSTR)ADDRESS_FROM_EMU(read_arg(uc, 0)),
					read_arg(uc, 1),
					(VMWSTR)ADDRESS_FROM_EMU(read_arg(uc, 2))
				));
		}},
		{"vm_ascii_to_ucs2", [](uc_engine* uc) {
			write_ret(uc,
				vm_ascii_to_ucs2(
					(VMWSTR)ADDRESS_FROM_EMU(read_arg(uc, 0)),
					read_arg(uc, 1),
					(VMSTR)ADDRESS_FROM_EMU(read_arg(uc, 2))
				));
		}},
		{"vm_chset_convert", [](uc_engine* uc) {
			write_ret(uc,
				vm_chset_convert(
					(vm_chset_enum)read_arg(uc, 0),
					(vm_chset_enum)read_arg(uc, 1),
					(VMCHAR*)ADDRESS_FROM_EMU(read_arg(uc, 2)),
					(VMCHAR*)ADDRESS_FROM_EMU(read_arg(uc, 3)),
					read_arg(uc, 4)
				));
		}},
		{"vm_get_language", [](uc_engine* uc) {
			write_ret(uc,
				vm_get_language());
		}},
		{"vm_get_language_ssc", [](uc_engine* uc) {
			write_ret(uc,
				vm_get_language_ssc(
					(VMINT8*)ADDRESS_FROM_EMU(read_arg(uc, 0))));
		}},



		// STDLib
		{"vm_wstrlen", [](uc_engine* uc) {
			write_ret(uc,vm_wstrlen(
					(VMWSTR)ADDRESS_FROM_EMU(read_arg(uc, 0))));
		}},
		{"vm_wstrcpy", [](uc_engine* uc) {
			write_ret(uc,vm_wstrcpy(
					(VMWSTR)ADDRESS_FROM_EMU(read_arg(uc, 0)),
					(VMWSTR)ADDRESS_FROM_EMU(read_arg(uc, 1))
				));
		}},



		// Audio
		{"vm_set_volume", [](uc_engine* uc) {
			vm_set_volume(read_arg(uc, 0));
		}},
		{"vm_midi_play_by_bytes", [](uc_engine* uc) {
			write_ret(uc,
				vm_midi_play_by_bytes(
					(VMUINT8*)ADDRESS_FROM_EMU(read_arg(uc, 0)),
					read_arg(uc, 1),
					read_arg(uc, 2),
					(void(*)(VMINT, VMINT))read_arg(uc, 3)
				));
		}},
		{"vm_midi_play_by_bytes_ex", [](uc_engine* uc) {
			write_ret(uc,
				vm_midi_play_by_bytes_ex(
					(VMUINT8*)ADDRESS_FROM_EMU(read_arg(uc, 0)),
					read_arg(uc, 1),
					read_arg(uc, 2),
					read_arg(uc, 3),
					read_arg(uc, 4),
					(void(*)(VMINT, VMINT))read_arg(uc, 5)
				));
		}},
		{"vm_midi_pause", [](uc_engine* uc) {
			write_ret(uc,
				vm_midi_pause(
					read_arg(uc, 0)));
		}},
		{"vm_midi_get_time", [](uc_engine* uc) {
			write_ret(uc,
				vm_midi_get_time(
					read_arg(uc, 0),
					(VMUINT*)ADDRESS_FROM_EMU(read_arg(uc, 1))
				));
		}},
		{"vm_midi_stop", [](uc_engine* uc) {
			vm_midi_stop(read_arg(uc, 0));
		}},
		{"vm_midi_stop_all", [](uc_engine* uc) {
			vm_midi_stop_all();
		}},


		// Sock
		{"vm_is_support_wifi", [](uc_engine* uc) {
			write_ret(uc, vm_is_support_wifi());
		}},
		{"vm_wifi_is_connected", [](uc_engine* uc) {
			write_ret(uc, vm_wifi_is_connected());
		}},
		{"vm_soc_get_host_by_name", [](uc_engine* uc) {
			write_ret(uc,
				vm_soc_get_host_by_name(
					read_arg(uc, 0),
					(const VMCHAR*)ADDRESS_FROM_EMU(read_arg(uc, 1)),
					(vm_soc_dns_result*)ADDRESS_FROM_EMU(read_arg(uc, 2)),
					(VMINT(*)(vm_soc_dns_result*))read_arg(uc, 3)
				));
		}},
		{"vm_tcp_connect", [](uc_engine* uc) {
			write_ret(uc,
				vm_tcp_connect(
					(const char*)ADDRESS_FROM_EMU(read_arg(uc, 0)),
					read_arg(uc, 1),
					read_arg(uc, 2),
					(void (*)(VMINT, VMINT))read_arg(uc, 3)
				));
		}},
		{"vm_tcp_close", [](uc_engine* uc) {
			vm_tcp_close(read_arg(uc, 0));
		}},
		{"vm_tcp_read", [](uc_engine* uc) {
			write_ret(uc,
				vm_tcp_read(
					read_arg(uc, 0),
					(void*)ADDRESS_FROM_EMU(read_arg(uc, 1)),
					read_arg(uc, 2)
				));
		}},
		{"vm_tcp_write", [](uc_engine* uc) {
			write_ret(uc,
				vm_tcp_write(
					read_arg(uc, 0),
					(void*)ADDRESS_FROM_EMU(read_arg(uc, 1)),
					read_arg(uc, 2)
				));
		}},



		// Some
		{"srand", [](uc_engine* uc) {
			srand(read_arg(uc, 0));
		}},
		{"rand", [](uc_engine* uc) {
			write_ret(uc, rand());
		}},



		// ARModule
		{"armodule_malloc", [](uc_engine* uc) {
			write_ret(uc, ADDRESS_TO_EMU(armodule.app_memory.malloc(read_arg(uc, 0))));
		}},
		{"armodule_realloc", [](uc_engine* uc) {
			write_ret(uc, ADDRESS_TO_EMU(
				armodule.app_memory.realloc(
					(size_t)ADDRESS_FROM_EMU(read_arg(uc, 0)),
					read_arg(uc, 1))));
		}},
		{"armodule_free", [](uc_engine* uc) {
			armodule.app_memory.free((size_t)ADDRESS_FROM_EMU(read_arg(uc, 0)));
		}},
	};

	int vm_get_sym_entry(const char* symbol) {
		std::string str = symbol;

		int ret = 0;

		for (int i = 0; i < func_map.size(); ++i)
			if (func_map[i].name == str) {
				ret = (ADDRESS_TO_EMU(func_ptr) + i * 2) | 1;
				break;
			}

		if (ret == 0)
			ret = armodule.vm_get_sym_entry(symbol);

		printf("vm_get_sym_entry(%s) -> %08x\n", symbol, ret);

		return ret;
	}

	void bridge_hoock(uc_engine* uc, uint64_t address, uint32_t size, void* user_data) {
		int ind = (address - ADDRESS_TO_EMU(func_ptr)) / 2;

		if (ind > func_map.size())
			abort();
		//if(ind) printf("--%s-- called -> ", func_map[ind].name.c_str());
		func_map[ind].f(uc);
		//if (ind) printf("%08x\n", read_arg(uc, 0));
	}

	void init() {
		size_t func_count = func_map.size();

		func_ptr = (unsigned char*)Memory::shared_malloc(func_count * 2 + 2);

		for (int i = 0; i < func_count; ++i)
			func_ptr[i * 2] = bxlr[0], func_ptr[i * 2 + 1] = bxlr[1];

		func_ptr[func_count * 2] = idle_bin[0], func_ptr[func_count * 2 + 1] = idle_bin[1];

		idle_p = ADDRESS_TO_EMU(func_ptr + func_count * 2) | 1;

		uc_hook_add(uc, &trace, UC_HOOK_CODE, (void*)bridge_hoock, 0,
			ADDRESS_TO_EMU(func_ptr), ADDRESS_TO_EMU(func_ptr + func_count * 2 - 1));

		armodule.init(vm_get_sym_entry("armodule_malloc"),
			vm_get_sym_entry("armodule_realloc"),
			vm_get_sym_entry("armodule_free"));
	}

	int ads_start(uint32_t entry, uint32_t vm_get_sym_entry_p, uint32_t data_base) {
		//data_base += 0x80;
		uint32_t base_it = data_base - 0x80;

		write_reg(uc, UC_ARM_REG_R9, data_base);

		put_to_reg(uc, (uint64_t)idle_p);
		put_to_reg(uc, 0);

		*(uint32_t*)ADDRESS_FROM_EMU(base_it) = read_reg(uc, UC_ARM_REG_SP);
		base_it += 4;

		*(uint32_t*)ADDRESS_FROM_EMU(base_it) = vm_get_sym_entry_p;
		base_it += 4;

		*(uint32_t*)ADDRESS_FROM_EMU(base_it) = data_base + 1024;
		base_it += 4;

		*(uint32_t*)ADDRESS_FROM_EMU(base_it) = data_base + 1024 + 2 * 1024;
		base_it += 4;

		*(uint32_t*)ADDRESS_FROM_EMU(base_it) = 3 * 1024;
		base_it += 4;

		return run_cpu(entry, 0);
	}

	int run_cpu(uint32_t adr, int n, ...) {
		va_list factor;

		va_start(factor, n);
		if (n > 4)
			throw 1;
		for (int i = 0; i < n; i++) {
			unsigned int t = va_arg(factor, unsigned int);
			if (i >= 0)
				uc_reg_write(uc, UC_ARM_REG_R0 + i, &t);
		}
		va_end(factor);
		//write_reg(uc, UC_ARM_REG_LR, (uint64_t)stack_p);
		write_reg(uc, UC_ARM_REG_LR, (uint64_t)idle_p);

		if (!GDB::gdb_mode) {
			uc_err err = uc_emu_start(uc, adr, (uint64_t)idle_p & ~1, 0, 0);
			if (err) {
				printf("uc_emu_start returned %d (%s)\n", err, uc_strerror(err));
				Cpu::printREG(uc);
				while (1) {
					Sleep(1000);
				}
			}
		}
		else
		{
			write_reg(uc, UC_ARM_REG_PC, (uint64_t)adr & ~1LL);
			uint32_t cpsr = read_reg(uc, UC_ARM_REG_CPSR);
			cpsr = cpsr & ~(1L << 5);
			if (adr & 1)
				cpsr |= (1L << 5);

			write_reg(uc, UC_ARM_REG_CPSR, cpsr);

			while (read_reg(uc, UC_ARM_REG_PC) != (idle_p & ~1)) {
				uint32_t cpsr = read_reg(uc, UC_ARM_REG_CPSR);
				bool thumb = (cpsr & (1 << 5)) >> 5;
				uc_err err = UC_ERR_OK;

				switch (GDB::cpu_state) {
					case GDB::Stop:
						GDB::update();
						break;
					case GDB::Step:
						err = uc_emu_start(uc, read_reg(uc, UC_ARM_REG_PC) | thumb, (uint64_t)idle_p & ~1, 0, 1);
						GDB::cpu_state = GDB::Stop;
						break;
					case GDB::Work:
						err = uc_emu_start(uc, read_reg(uc, UC_ARM_REG_PC) | thumb, (uint64_t)idle_p & ~1, 0, 0);
						break;
				}

				if (err) {
					GDB::cpu_state = GDB::Stop;
					GDB::make_answer("S05");
					printf("uc_emu_start returned %d (%s)\n", err, uc_strerror(err));
				}
			}
		}
		return read_reg(uc, UC_ARM_REG_R0);
	}
}