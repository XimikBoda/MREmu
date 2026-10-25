#include "Bridge.h"
#include "Memory.h"
#include "ARModule.h"
#include "Log.h"

#include "MREngine/Audio.h"
#include "MREngine/Resources.h"

#include "Cpu.h"
#include "GDB.h"
#include <string>
#include <iostream>
#include <thread>
#include <chrono>
#include <unicorn/unicorn.h>

#include <vmgraph_fix.h>
#include <vmgfxold.h>
#include <vmres.h>
#include <vm4res.h>
#include <vmtimer.h>
#include <vmpromng.h>
#include <vmgettag.h>
#include <vmchset.h>
#include <vmsim.h>
#include <vmstdlib.h>
#include <vmmm.h>
#include <vmbitstream.h>
#include <vmsensor.h>
#include <vmtel.h>
#include <vmsock_fix.h>
#include <vmmissing.h>

namespace Cpu {
	void printREG(uc_engine* uc);
}

const unsigned char bxlr[2] = { 0x70, 0x47 };
const unsigned char idle_bin[2] = { 0xfe, 0xe7 };

extern uc_engine* uc;

namespace Bridge {
	struct br_func {
		std::string name;
		void* p;
		void (*f)(uc_engine* uc);
	};

	unsigned char* func_ptr;
	uint32_t idle_p = 0;

	ARModule armodule;

	static uint32_t read_reg(uc_engine* uc, int reg) {
		uint32_t r = 0;
		uc_reg_read(uc, reg, &r);
		return r;
	}

	static void write_reg(uc_engine* uc, int reg, uint32_t val) {
		uc_reg_write(uc, reg, &val);
	}

	void put_to_reg(uc_engine* uc, uint32_t val) {
		uint32_t sp = read_reg(uc, UC_ARM_REG_SP);
		sp -= 4;
		write_reg(uc, UC_ARM_REG_SP, sp);
		uc_mem_write(uc, sp, &val, 4);
	}

	uint32_t top_sp(uc_engine* uc, int l) {
		uint32_t el, sp = read_reg(uc, UC_ARM_REG_SP);
		uc_mem_read(uc, sp + 4 * l, &el, 4);
		return el;
	}

	uint32_t top_from_r(uc_engine* uc, uint32_t r, int l) {
		uint32_t el;
		uc_mem_read(uc, r + 4 * l, &el, 4);
		return el;
	}

	static void write_ret(uc_engine* uc, uint32_t val) {
		write_reg(uc, UC_ARM_REG_R0, val);
	}

	uint32_t read_arg(uc_engine* uc, int ind) {
		if (ind < 4)
			return read_reg(uc, UC_ARM_REG_R0 + ind);
		else
			return top_sp(uc, ind - 4);
	}

	uint64_t read_arg_64(uc_engine* uc, int ind1, int ind2) {
		uint64_t val1 = (uint64_t)read_arg(uc, ind1);
		uint64_t val2 = (uint64_t)read_arg(uc, ind2);
		return (val2 << 32) | val1;
	}

	template <typename T>
	T extract_arg(uc_engine* uc, int index) {
		static_assert(std::is_pointer_v<T> || sizeof(T) <= 4
		        || std::is_same_v<T, long> || std::is_same_v<T, unsigned long>,
			"[BIND_FUNC ERROR]: Argument is > 32 bits (requires read_arg_64). Use a manual lambda!");

		static_assert(!(std::is_pointer_v<T> && std::is_pointer_v<std::remove_pointer_t<T>>),
			"[BIND_FUNC ERROR]: Double pointer (T**) detected. Needs manual out-param translation. Use a manual lambda!");

		auto raw_val = read_arg(uc, index);

		if constexpr (std::is_pointer_v<T>)
			return reinterpret_cast<T>(ADDRESS_FROM_EMU(raw_val));
		else
			return static_cast<T>(raw_val);
	}

	template <typename T, T Func>
	struct ApiWrapper;

	template <typename Ret, typename... Args, Ret(*Func)(Args...)>
	struct ApiWrapper<Ret(*)(Args...), Func> {
		template <std::size_t... Is>
		static void call_impl(uc_engine* uc, std::index_sequence<Is...>) {
			if constexpr (!std::is_void_v<Ret>) {
                static_assert(std::is_pointer_v<Ret> || sizeof(Ret) <= 4
                              || std::is_same_v<Ret, long> || std::is_same_v<Ret, unsigned long>,
					"[BIND_FUNC ERROR]: Return type > 32 bits. Use a manual lambda!");

				static_assert(!(std::is_pointer_v<Ret> && std::is_pointer_v<std::remove_pointer_t<Ret>>),
					"[BIND_FUNC ERROR]: Returning a double pointer. Use a manual lambda!");
			}

			if constexpr (std::is_void_v<Ret>)
				Func(extract_arg<Args>(uc, Is)...);
			else {
				auto result = Func(extract_arg<Args>(uc, Is)...);

				if constexpr (std::is_pointer_v<Ret>)
					write_ret(uc, ADDRESS_TO_EMU(result));
				else 
					write_ret(uc, static_cast<uint32_t>(result));
			}
		}

		static void call(uc_engine* uc) {
			call_impl(uc, std::index_sequence_for<Args...>{});
		}
	};

	template <auto Func>
	constexpr auto wrap_api() {
		return ApiWrapper<decltype(Func), Func>::call;
	}

#define BIND_FUNC(name) { #name, (void*)name, wrap_api<name>() }

#define BIND_FUNC_FIX(name) { #name, (void*)name##_FIX, wrap_api<name##_FIX>() }

#define FUNCN(name) #name, (void*)name
#define FUNCN_FIX(name) #name, (void*)name##_FIX

	std::vector<br_func> func_map =
	{
		BIND_FUNC(vm_get_sym_entry),

		// System
		BIND_FUNC(vm_get_time),
		BIND_FUNC(vm_get_curr_utc),
		BIND_FUNC(vm_get_sys_time_zone),
		BIND_FUNC(vm_get_sys_scene),
		BIND_FUNC(vm_get_malloc_stat),
		BIND_FUNC(vm_malloc),
		BIND_FUNC(vm_calloc),
		BIND_FUNC(vm_realloc),
		BIND_FUNC(vm_free),
		BIND_FUNC(vm_reg_sysevt_callback),
		BIND_FUNC(vm_get_mre_total_mem_size),
		BIND_FUNC(vm_get_tick_count),
		BIND_FUNC(vm_get_exec_filename),
		BIND_FUNC(vm_get_sys_property),
		BIND_FUNC(vm_get_vm_tag),
		BIND_FUNC(vm_switch_power_saving_mode),
		BIND_FUNC(vm_appmgr_is_installed),
		BIND_FUNC(vm_appmgr_get_installed_list),
		BIND_FUNC(vm_open_wap_url),
		BIND_FUNC(vm_app_log),

		// Program manager
		BIND_FUNC(vm_pmng_get_current_handle),
		BIND_FUNC(vm_reg_msg_proc),
		BIND_FUNC(vm_post_msg),
		BIND_FUNC(vm_start_app),
		BIND_FUNC(vm_start_app_with_para),
		BIND_FUNC(vm_exit_app),

		// Timer
		BIND_FUNC(vm_create_timer),
		BIND_FUNC(vm_delete_timer),
		BIND_FUNC(vm_create_timer_ex),
		BIND_FUNC(vm_delete_timer_ex),

		// IO
		BIND_FUNC(vm_reg_keyboard_callback),
		BIND_FUNC(vm_reg_pen_callback),
		BIND_FUNC(vm_file_open),
		BIND_FUNC(vm_file_close),
		BIND_FUNC(vm_file_read),
		BIND_FUNC(vm_file_write),
		BIND_FUNC(vm_file_commit),
		BIND_FUNC(vm_file_seek),
		BIND_FUNC(vm_file_tell),
		BIND_FUNC(vm_file_is_eof),
		BIND_FUNC(vm_file_getfilesize),
		BIND_FUNC(vm_file_delete),
		BIND_FUNC(vm_file_rename),
		BIND_FUNC(vm_file_mkdir),
		BIND_FUNC(vm_file_set_attributes),
		BIND_FUNC(vm_file_get_attributes),
		BIND_FUNC(vm_find_first),
		BIND_FUNC(vm_find_next),
		BIND_FUNC(vm_find_close),
		BIND_FUNC(vm_find_first_ext),
		BIND_FUNC(vm_find_next_ext),
		BIND_FUNC(vm_find_close_ext),
		BIND_FUNC(vm_file_get_modify_time),
		BIND_FUNC(vm_get_removeable_driver),
		BIND_FUNC(vm_get_system_driver),
		BIND_FUNC(vm_get_disk_free_space),
		BIND_FUNC(vm_get_disk_info),
		BIND_FUNC(vm_is_support_keyborad),
		BIND_FUNC(vm_is_support_pen_touch),

		// SIM
		BIND_FUNC(vm_get_operator),
		BIND_FUNC(vm_has_sim_card),
		BIND_FUNC(vm_get_imei),
		BIND_FUNC(vm_get_imsi),
		BIND_FUNC(vm_sim_card_count),
		BIND_FUNC(vm_set_active_sim_card),
		BIND_FUNC(vm_get_sim_card_status),
		BIND_FUNC(vm_query_operator_code),
		BIND_FUNC(vm_sim_get_active_sim_card),
		BIND_FUNC(vm_sim_max_card_count),
		BIND_FUNC(vm_sim_get_prefer_sim_card),

		// Graphic
		BIND_FUNC(vm_graphic_get_screen_width),
		BIND_FUNC(vm_graphic_get_screen_height),
		BIND_FUNC(vm_graphic_create_layer),
		BIND_FUNC(vm_graphic_delete_layer),
		BIND_FUNC(vm_graphic_active_layer),
		BIND_FUNC(vm_graphic_get_layer_buffer),
		BIND_FUNC(vm_graphic_clear_layer_bg),
		BIND_FUNC(vm_graphic_flush_layer),
		BIND_FUNC(vm_graphic_flatten_layer),
		BIND_FUNC(vm_graphic_translate_layer),
		BIND_FUNC(vm_graphic_get_bits_per_pixel),
		BIND_FUNC(vm_graphic_create_layer_ex),
		BIND_FUNC(vm_graphic_create_layer_cf),
		BIND_FUNC(vm_graphic_get_img_property_ex),
		BIND_FUNC(vm_graphic_blt),
		BIND_FUNC(vm_graphic_blt_ex),
		BIND_FUNC(vm_graphic_rotate),
		BIND_FUNC(vm_graphic_mirror),
		BIND_FUNC(vm_graphic_get_pixel),
		BIND_FUNC(vm_graphic_set_pixel),
		BIND_FUNC(vm_graphic_set_pixel_ex),
		BIND_FUNC(vm_graphic_line),
		BIND_FUNC(vm_graphic_line_ex),
		BIND_FUNC(vm_graphic_fill_rect),
		BIND_FUNC(vm_graphic_fill_rect_ex),
		BIND_FUNC(vm_graphic_roundrect),
		BIND_FUNC(vm_graphic_roundrect_ex),
		BIND_FUNC(vm_graphic_fill_roundrect),
		BIND_FUNC(vm_graphic_fill_roundrect_ex),
		BIND_FUNC(vm_graphic_ellipse),
		BIND_FUNC(vm_graphic_fill_ellipse),
		BIND_FUNC(vm_graphic_rect),
		BIND_FUNC(vm_graphic_rect_ex),
		BIND_FUNC(vm_graphic_fill_polygon),
		BIND_FUNC(vm_graphic_get_layer_clip),
		BIND_FUNC(vm_graphic_set_layer_clip),
		BIND_FUNC(vm_graphic_set_clip),
		BIND_FUNC(vm_graphic_reset_clip),
		BIND_FUNC(vm_graphic_flush_screen),
		BIND_FUNC(vm_graphic_get_blt_layer_count),
		BIND_FUNC(vm_graphic_is_r2l_state),
		BIND_FUNC(vm_graphic_setcolor),
		{FUNCN(vm_graphic_gradient_paint_rect), [](uc_engine* uc) {
			vm_graphic_gradient_paint_rect(
				read_arg(uc, 0),
				read_arg(uc, 1),
				read_arg(uc, 2),
				read_arg(uc, 3),
				read_arg(uc, 4),
				(vm_graphic_color)read_arg_64(uc, 5, 6), // it makes all more difficult
				(vm_graphic_color)read_arg_64(uc, 7, 8),
				(vm_graphic_gp_style)read_arg(uc, 9));
		}},

		// Canvas
		BIND_FUNC_FIX(vm_graphic_create_canvas),
		BIND_FUNC_FIX(vm_graphic_create_canvas_cf),
		BIND_FUNC_FIX(vm_graphic_release_canvas),
		BIND_FUNC_FIX(vm_graphic_release_canvas_ex),
		BIND_FUNC_FIX(vm_graphic_get_canvas_buffer),
		BIND_FUNC_FIX(vm_graphic_get_canvas_buffer_size),
		BIND_FUNC_FIX(vm_graphic_get_img_buffer),
		BIND_FUNC_FIX(vm_graphic_get_frame_number),
		BIND_FUNC_FIX(vm_graphic_canvas_set_trans_color),

		// Image
		BIND_FUNC_FIX(vm_graphic_load_image),
		BIND_FUNC_FIX(vm_graphic_load_image_cf),
		BIND_FUNC_FIX(vm_graphic_load_image_resized),
		BIND_FUNC_FIX(vm_graphic_load_image_resized_cf),
		BIND_FUNC_FIX(vm_graphic_get_img_property),

		// Graphic Old
		BIND_FUNC(vm_graphic_lock),
		BIND_FUNC(vm_graphic_unlock),
		BIND_FUNC(vm_graphic_get_buffer),
		BIND_FUNC(vm_graphic_flush_buffer),
		BIND_FUNC(vm_initialize_screen_buffer),
		BIND_FUNC(vm_finalize_screen_buffer),
		BIND_FUNC(vm_graphic_drawtext),
		BIND_FUNC(vm_dd_initialize_clip_rect),
		BIND_FUNC(vm_dd_set_clip),
		BIND_FUNC(vm_dd_reset_clip),
		BIND_FUNC(vm_dd_load_image),
		BIND_FUNC(vm_dd_get_img_property),
		BIND_FUNC(vm_dd_get_frame_number),
		BIND_FUNC(vm_dd_clean),

		// Textout
		BIND_FUNC(vm_graphic_get_character_height),
		BIND_FUNC(vm_graphic_get_character_width),
		BIND_FUNC(vm_graphic_get_string_width),
		BIND_FUNC(vm_graphic_get_string_height),
		BIND_FUNC(vm_graphic_measure_character),
		BIND_FUNC(vm_graphic_get_character_info),
		BIND_FUNC(vm_graphic_set_font),
		BIND_FUNC(vm_graphic_textout),
		BIND_FUNC(vm_graphic_textout_by_baseline),
		BIND_FUNC(vm_font_set_font_size),
		BIND_FUNC(vm_font_set_font_style),
		BIND_FUNC(vm_graphic_textout_to_layer),
		BIND_FUNC(vm_graphic_get_string_baseline),
		BIND_FUNC(vm_graphic_is_use_vector_font),
		BIND_FUNC(vm_graphic_get_char_num_in_width),
		BIND_FUNC(vm_graphic_show_truncated_text),
		BIND_FUNC(vm_graphic_get_char_baseline_alllang),

		// Resources
		BIND_FUNC(vm_load_resource),
		BIND_FUNC(vm_resource_get_data),
		BIND_FUNC(vm_get_resource_offset),
		BIND_FUNC(vm_get_resource_offset_from_file),
		BIND_FUNC(vm_get_res_header),
		BIND_FUNC(vm_res_init),
		BIND_FUNC(vm_res_init_with_language),
		BIND_FUNC(vm_res_deinit),
		BIND_FUNC(vm_res_get_string),
		BIND_FUNC(vm_res_get_image),
		BIND_FUNC(vm_res_get_audio),
		BIND_FUNC(vm_res_get_image_and_size),
		BIND_FUNC(vm_res_delete),
		BIND_FUNC(vm_reg_res_provider),

		// CharSet
		BIND_FUNC(vm_ucs2_to_gb2312),
		BIND_FUNC(vm_gb2312_to_ucs2),
		BIND_FUNC(vm_ucs2_to_ascii),
		BIND_FUNC(vm_ascii_to_ucs2),
		BIND_FUNC(vm_chset_convert),
		BIND_FUNC(vm_get_language),
		BIND_FUNC(vm_get_language_ssc),
		BIND_FUNC(vm_ucs2_string),

		// STDLib
		BIND_FUNC(vm_create_dyn_array),
		BIND_FUNC(vm_dyn_array_add),
		BIND_FUNC(vm_dyn_array_del),
		BIND_FUNC(vm_dyn_array_del_all),
		BIND_FUNC(vm_free_dyn_array),
		BIND_FUNC(vm_safe_wstrcpy),
		BIND_FUNC(vm_wstrlen),
		BIND_FUNC(vm_wstrcpy),
		BIND_FUNC(vm_wstrcat),
		BIND_FUNC(vm_wstrncpy),
		BIND_FUNC(vm_wstrcmp),

		// Audio
		BIND_FUNC(vm_audio_play_bytes),
		BIND_FUNC(vm_audio_pause),
		BIND_FUNC(vm_audio_resume),
		BIND_FUNC(vm_audio_stop),
		BIND_FUNC(vm_set_volume),
		BIND_FUNC(vm_get_volume),
		BIND_FUNC(vm_midi_play),
		BIND_FUNC(vm_midi_play_ex),
		BIND_FUNC(vm_midi_play_by_bytes),
		BIND_FUNC(vm_midi_play_by_bytes_ex),
		BIND_FUNC(vm_midi_pause),
		BIND_FUNC(vm_midi_get_time),
		BIND_FUNC(vm_midi_stop),
		BIND_FUNC(vm_midi_stop_all),
		BIND_FUNC(vm_audio_resume_bg_play),
		BIND_FUNC(vm_audio_suspend_bg_play),

		// Bitstream
		BIND_FUNC(vm_bitstream_audio_open),
		BIND_FUNC(vm_bitstream_audio_open_pcm),
		BIND_FUNC(vm_bitstream_audio_finished),
		BIND_FUNC(vm_bitstream_audio_close),
		BIND_FUNC(vm_bitstream_audio_get_buffer_status),
		BIND_FUNC(vm_bitstream_audio_put_data),
		BIND_FUNC(vm_bitstream_audio_start),
		BIND_FUNC(vm_bitstream_audio_stop),
		BIND_FUNC(vm_bitstream_audio_get_play_time),

		// Bitstream Record
		BIND_FUNC(mremu_media_setbufer),
		{FUNCN(mremu_media_getreadbuffer), [](uc_engine* uc) {
			VMUINT16** buffer = (VMUINT16**)ADDRESS_FROM_EMU(read_arg(uc, 0));
			VMUINT16* host_buf = 0;

			mremu_media_getreadbuffer(
				&host_buf,
				(VMUINT32*)ADDRESS_FROM_EMU(read_arg(uc, 1))
			);

			if (host_buf && buffer)
				*buffer = (VMUINT16*)ADDRESS_TO_EMU(host_buf);
		}},
		BIND_FUNC(mremu_media_record),
		BIND_FUNC(mremu_media_readdatadone),
		BIND_FUNC(mremu_media_stop),

		// Sock
		BIND_FUNC(vm_is_support_wifi),
		BIND_FUNC(vm_wifi_is_connected),
		BIND_FUNC(vm_soc_get_host_by_name),
		BIND_FUNC(vm_tcp_connect),
		BIND_FUNC(vm_tcp_close),
		BIND_FUNC(vm_tcp_read),
		BIND_FUNC(vm_tcp_write),
		BIND_FUNC(vm_udp_create),
		BIND_FUNC(vm_udp_sendto),
		BIND_FUNC(vm_udp_recvfrom),
		BIND_FUNC(vm_udp_close),

		// Sensor
		BIND_FUNC(vm_srv_sensor_is_available),

		// Some
		BIND_FUNC(srand),
		BIND_FUNC(rand),
		BIND_FUNC(strtoi),

		// ARModule
		{"armodule_malloc", NULL, [](uc_engine* uc) {
			write_ret(uc, ADDRESS_TO_EMU(armodule.app_memory.malloc(read_arg(uc, 0))));
		}},
		{"armodule_realloc", NULL, [](uc_engine* uc) {
			write_ret(uc, ADDRESS_TO_EMU(
				armodule.app_memory.realloc(
					(size_t)ADDRESS_FROM_EMU(read_arg(uc, 0)),
					read_arg(uc, 1))));
		}},
		{"armodule_free", NULL, [](uc_engine* uc) {
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

		spdlog::log(ret ? spdlog::level::debug : spdlog::level::warn,
			"vm_get_sym_entry({}) -> {:08x}", symbol, (uint32_t)ret);

		return ret;
	}

	void* vm_get_sym_entry_native(const char* symbol) {
		std::string str = symbol;

		void* ret = 0;

		for (int i = 0; i < func_map.size(); ++i)
			if (func_map[i].name == str) {
				ret = func_map[i].p;
				break;
			}

		if (ret == 0)
			if (str == "vm_vsprintf")
				ret = (void*)vsprintf;
			else if (str == "vm_sprintf")
				ret = (void*)sprintf;
			else if(str == "vm_sscanf")
				ret = (void*)sscanf;

		spdlog::log(ret ? spdlog::level::debug : spdlog::level::warn,
			"vm_get_sym_entry_native({}) -> {:08x}", symbol, (size_t)ret);

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

		Cpu::add_hook(UC_HOOK_CODE, (void*)bridge_hoock, 0,
			ADDRESS_TO_EMU(func_ptr), ADDRESS_TO_EMU(func_ptr + func_count * 2 - 1));

		armodule.init(vm_get_sym_entry("armodule_malloc"),
			vm_get_sym_entry("armodule_realloc"),
			vm_get_sym_entry("armodule_free"));
	}

	int ads_start(uint32_t entry, uint32_t vm_get_sym_entry_p, uint32_t data_base, uint32_t heap_base, uint32_t heap_limit, uint32_t stack_size) {
		write_reg(uc, UC_ARM_REG_R9, data_base);

		uint32_t base_it = data_base - 0x80;

		put_to_reg(uc, (uint32_t)idle_p);
		put_to_reg(uc, 0);

		uint32_t current_sp = read_reg(uc, UC_ARM_REG_SP);

		uint32_t args[5] = {
			current_sp,
			vm_get_sym_entry_p,
			heap_base,
			heap_limit,
			stack_size
		};

		for (int i = 0; i < 5; i++) {
			*(uint32_t*)ADDRESS_FROM_EMU(base_it) = args[i];
			base_it += 4;
		}

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

#ifdef GDB
		if (!GDB::gdb_mode)
#endif
		{
			uc_err err = uc_emu_start(uc, adr, (uint64_t)idle_p & ~1, 0, 0);
			if (err) {
				spdlog::error("uc_emu_start returned {} ({})", (int)err, uc_strerror(err));
				Cpu::printREG(uc);
				while (1) {
					std::this_thread::sleep_for(std::chrono::milliseconds(1000));
				}
			}
		}
#ifdef GDB
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
					spdlog::error("uc_emu_start returned {} ({})", (int)err, uc_strerror(err));
				}
			}
		}
#endif
		return read_reg(uc, UC_ARM_REG_R0);
	}
}