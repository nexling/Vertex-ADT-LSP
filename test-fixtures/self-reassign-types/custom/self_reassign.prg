ar = get_struct_core(src)
obj = get_panel(ar)
obj = get_panel_core(obj)
use = get_panel_core(obj)
num = get_obj_param(obj, "x")
num = get_panel_core(num)
vec_to(pt) 1, 2, 3
vec_unit(dv) pt
vec_add(dv) dv, pt
vec_to(vec) 1, 2, 3
vec = vec_get_x(vec)
fresh = get_panel_core(fresh)
other = get_obj_param(obj, "y")
use = get_panel_core(other)
other = get_struct_core(src)
