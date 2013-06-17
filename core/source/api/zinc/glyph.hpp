/**
 * FILE : glyph.hpp
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
 * The Original Code is libZinc.
 *
 * The Initial Developer of the Original Code is
 * Auckland Uniservices Ltd, Auckland, New Zealand.
 * Portions created by the Initial Developer are Copyright (C) 2012
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
#ifndef ZINC_GLYPH_HPP
#define ZINC_GLYPH_HPP

#include "zinc/glyph.h"

namespace zinc
{

class Glyph
{
protected:
	Cmiss_glyph_id id;

public:

	Glyph() : id(0)
	{  }

	// takes ownership of C handle, responsibility for destroying it
	explicit Glyph(Cmiss_glyph_id in_glyph_id) :
		id(in_glyph_id)
	{  }

	Glyph(const Glyph& glyph) :
		id(Cmiss_glyph_access(glyph.id))
	{  }

	Glyph& operator=(const Glyph& glyph)
	{
		Cmiss_glyph_id temp_id = Cmiss_glyph_access(glyph.id);
		if (0 != id)
		{
			Cmiss_glyph_destroy(&id);
		}
		id = temp_id;
		return *this;
	}

	~Glyph()
	{
		if (0 != id)
		{
			Cmiss_glyph_destroy(&id);
		}
	}

	bool isValid()
	{
		return (0 != id);
	}

	Cmiss_glyph_id getId()
	{
		return id;
	}

	enum RepeatMode
	{
		REPEAT_MODE_INVALID = CMISS_GLYPH_REPEAT_MODE_INVALID,
		REPEAT_NONE = CMISS_GLYPH_REPEAT_NONE,
		REPEAT_AXES_2D = CMISS_GLYPH_REPEAT_AXES_2D,
		REPEAT_AXES_3D = CMISS_GLYPH_REPEAT_AXES_3D,
		REPEAT_MIRROR = CMISS_GLYPH_REPEAT_MIRROR
	};

	char *getName()
	{
		return Cmiss_glyph_get_name(id);
	}

	int setName(const char *name)
	{
		return Cmiss_glyph_set_name(id, name);
	}

	bool isManaged()
	{
		return (0 != Cmiss_glyph_is_managed(id));
	}

	int setManaged(bool value)
	{
		return Cmiss_glyph_set_managed(id, static_cast<int>(value));
	}
};

class GlyphModule
{
protected:
	Cmiss_glyph_module_id id;

public:

	GlyphModule() : id(0)
	{  }

	// takes ownership of C handle, responsibility for destroying it
	explicit GlyphModule(Cmiss_glyph_module_id in_glyph_module_id) :
		id(in_glyph_module_id)
	{  }

	GlyphModule(const GlyphModule& glyphModule) :
		id(Cmiss_glyph_module_access(glyphModule.id))
	{  }

	GlyphModule& operator=(const GlyphModule& glyphModule)
	{
		Cmiss_glyph_module_id temp_id = Cmiss_glyph_module_access(glyphModule.id);
		if (0 != id)
		{
			Cmiss_glyph_module_destroy(&id);
		}
		id = temp_id;
		return *this;
	}

	~GlyphModule()
	{
		if (0 != id)
		{
			Cmiss_glyph_module_destroy(&id);
		}
	}

	bool isValid()
	{
		return (0 != id);
	}

	Cmiss_glyph_module_id getId()
	{
		return id;
	}

	int beginChange()
	{
		return Cmiss_glyph_module_begin_change(id);
	}

	int endChange()
	{
		return Cmiss_glyph_module_end_change(id);
	}

	int createStandardGlyphs()
	{
		return Cmiss_glyph_module_create_standard_glyphs(id);
	}

	Glyph findGlyphByName(const char *name)
	{
		return Glyph(Cmiss_glyph_module_find_glyph_by_name(id, name));
	}

	Glyph getDefaultPointGlyph()
	{
		return Glyph(Cmiss_glyph_module_get_default_point_glyph(id));
	}

	int setDefaultPointGlyph(Glyph& glyph)
	{
		return Cmiss_glyph_module_set_default_point_glyph(id, glyph.getId());
	}

};

}  // namespace zinc

#endif /* ZINC_GLYPH_HPP */