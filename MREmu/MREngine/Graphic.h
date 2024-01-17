#pragma once

#include "System.h"

namespace MREngine {
	class Graphic {
	public:
		int width = 240, height = 320;
		
		Graphic();

		void activate();

		~Graphic();
	};
}

// MRE API
VMINT vm_graphic_get_screen_width(void);
VMINT vm_graphic_get_screen_height(void);
