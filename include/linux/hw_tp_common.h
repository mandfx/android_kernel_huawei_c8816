/*Add for huawei TP*/
/*
 * Copyright (c) 2014 Huawei Device Company
 *
 * This file provide common requeirment for different touch IC.
 * 
 * 2014-01-04:Add "tp_get_touch_screen_obj" by sunlibin
 *
 */
#ifndef __HW_TP_COMMON__
#define __HW_TP_COMMON__

struct kobject* tp_get_touch_screen_obj(void);
struct kobject* tp_get_glove_func_obj(void);

#endif

