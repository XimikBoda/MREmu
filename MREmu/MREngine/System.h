#pragma once

// MRE common data types
#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef NULL
#define NULL 0
#endif


/* start MRE environment normally */
#define VM_NORMAL_START_MRE_ENV 1


typedef unsigned char VMUINT8;
typedef unsigned short VMUINT16;
typedef unsigned int VMUINT;
typedef unsigned long VMUINT32;


typedef  unsigned long long VMUINT64;
typedef  long long VMINT64;


typedef char VMINT8;
typedef short VMINT16;
typedef int  VMINT;
typedef long  VMINT32;

typedef VMUINT8 VMUCHAR;
typedef VMUINT16 VMUWCHAR;
typedef VMUINT8* VMUSTR;
typedef VMUINT16* VMUWSTR;

typedef VMINT8 VMCHAR;
typedef VMINT16 VMWCHAR;
typedef VMINT8* VMSTR;
typedef VMINT16* VMWSTR;

typedef unsigned char VMBYTE;
typedef unsigned short VMUSHORT;
typedef short VMSHORT;

typedef VMINT VMFILE;
typedef VMINT VMBOOL;
#define VM_PROF_NORMAL_MODE     0   /* normal mode */
#define VM_PROF_MEETING_MODE    1   /* Meeting mode */
#define VM_PROF_SILENT_MODE     2   /* Silnet mode */
#define VM_PROF_HEADSET_MODE    3   /* Headset mode */
#define VM_PROF_BLUETOOTH_MODE  4   /* Bluetooth profile */

typedef struct vm_time_t {
    VMINT year;
    VMINT mon;		/* month, begin from 1	*/
    VMINT day;		/* day,begin from  1 */
    VMINT hour;		/* house, 24-hour	*/
    VMINT min;		/* minute	*/
    VMINT sec;		/* second	*/
} vm_time_t;

typedef enum
{
    VM_TOUCH_FEEDBACK_DOWN,
    VM_TOUCH_FEEDBACK_DOWN_VIBRATE,
    VM_TOUCH_FEEDBACK_DOWN_TONE,
    VM_TOUCH_FEEDBACK_HOLD,
    VM_TOUCH_FEEDBACK_SPECIAL,
    VM_TOUCH_FEEDBACK_TOTAL
} vm_touch_feedback_enum;


typedef enum
{
    VM_MAINLCD_BRIGHTNESS_LEVEL0 = 0,  /* TURN OFF*/
    VM_MAINLCD_BRIGHTNESS_LEVEL1,
    VM_MAINLCD_BRIGHTNESS_LEVEL2,
    VM_MAINLCD_BRIGHTNESS_LEVEL3,
    VM_MAINLCD_BRIGHTNESS_LEVEL4,
    VM_MAINLCD_BRIGHTNESS_LEVEL5,
    VM_MAINLCD_BRIGHTNESS_LEVEL6,
    VM_MAINLCD_BRIGHTNESS_LEVEL7,
    VM_MAINLCD_BRIGHTNESS_LEVEL8,
    VM_MAINLCD_BRIGHTNESS_LEVEL9,
    VM_MAINLCD_BRIGHTNESS_LEVEL10,
    VM_MAINLCD_BRIGHTNESS_LEVEL11,
    VM_MAINLCD_BRIGHTNESS_LEVEL12,
    VM_MAINLCD_BRIGHTNESS_LEVEL13,
    VM_MAINLCD_BRIGHTNESS_LEVEL14,
    VM_MAINLCD_BRIGHTNESS_LEVEL15,
    VM_MAINLCD_BRIGHTNESS_LEVEL16,
    VM_MAINLCD_BRIGHTNESS_LEVEL17,
    VM_MAINLCD_BRIGHTNESS_LEVEL18,
    VM_MAINLCD_BRIGHTNESS_LEVEL19,
    VM_MAINLCD_BRIGHTNESS_LEVEL20,
    VM_MAINLCD_BRIGHTNESS_20LEVEL_MAX /* DO NOT USE THIS */
} vm_mainlcd_brightness_level_enum;

namespace MREngine {
	
}

// MRE API
void* vm_malloc(int size);
void vm_free(void* ptr);
