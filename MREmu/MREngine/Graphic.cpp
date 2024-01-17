#include "Graphic.h"

static MREngine::Graphic *graphic = 0;

MREngine::Graphic::Graphic()
{
	activate();
}

void MREngine::Graphic::activate()
{
	graphic = this;
}

MREngine::Graphic::~Graphic()
{
	if (graphic == this)
		graphic = 0;
}

//MRE API

VMINT vm_graphic_get_screen_width(void)
{
	if (graphic)
		return graphic->width;
	else
		return 0;
}

VMINT vm_graphic_get_screen_height(void) 
{
	if (graphic)
		return graphic->height;
	else
		return 0;
}


