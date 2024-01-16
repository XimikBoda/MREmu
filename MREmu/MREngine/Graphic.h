#pragma once


namespace MREngine {
	class Graphic {
	public:
		int width = 240, height = 320;
		
		Graphic();

		void activate();

		~Graphic();
	};
}