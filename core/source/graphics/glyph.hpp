/**
 * FILE : glyph.hpp
 *
 * Internal header for private glyph functions.
 */
 /* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is cmgui.
 *
 * The Initial Developer of the Original Code is
 * Auckland Uniservices Ltd, Auckland, New Zealand.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#if !defined (GLYPH_H)
#define GLYPH_H

#include "zinc/glyph.h"
#include "general/enumerator.h"
#include "graphics/auxiliary_graphics_types.h"
#include "graphics/graphics_object.h"

PROTOTYPE_ENUMERATOR_FUNCTIONS(Cmiss_glyph_repeat_mode);

int Cmiss_glyph_repeat_mode_get_number_of_glyphs(
	enum Cmiss_glyph_repeat_mode glyph_repeat_mode);

bool Cmiss_glyph_repeat_mode_glyph_number_has_label(
	enum Cmiss_glyph_repeat_mode glyph_repeat_mode, int glyph_number);

/**
 * @param glyph_repeat_mode  NONE|MIRROR|AXES_2D|AXES_3D
 * @param glyph_number  From 0 to one less than number for glyph_repeat_mode.
 * @param base_size  Fixed minimum size to add to scale (after scale factors)
 * @param scale_factors  Scale factors to apply to scale.
 * @param offset  Offset of base point in axes*scale units.
 * Multiplies the three axes by their <scale> to give the final axes, reversing
 * <final_axis3> if necessary to produce a right handed coordinate system.
 */
void resolve_glyph_axes(
	enum Cmiss_glyph_repeat_mode glyph_repeat_mode, int glyph_number,
	Triple base_size, Triple scale_factors, Triple offset,
	Triple point, Triple axis1, Triple axis2, Triple axis3, Triple scale,
	Triple final_point, Triple final_axis1, Triple final_axis2, Triple final_axis3);

/** internal only */
Cmiss_glyph_module_id Cmiss_glyph_module_create();

/* internal only */
struct MANAGER(GT_object) *Cmiss_glyph_module_get_glyph_manager(
	Cmiss_glyph_module_id glyph_module);

/**
 * Create extra glyphs such as line_ticks, diamond, only used in cmgui
 */
int Cmiss_glyph_module_create_standard_cmgui_glyphs(
	Cmiss_glyph_module_id glyph_module);

#endif /* !defined (GLYPH_H) */