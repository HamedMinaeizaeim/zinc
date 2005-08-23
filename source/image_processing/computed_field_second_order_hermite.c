/******************************************************************
  FILE: computed_field_second_order_hermite.c

  LAST MODIFIED: 23 August 2005

  DESCRIPTION:Implement local orientation detecting.
==================================================================*/
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
#include <math.h>
#include "computed_field/computed_field.h"
#include "computed_field/computed_field_find_xi.h"
#include "computed_field/computed_field_private.h"
#include "computed_field/computed_field_set.h"
#include "image_processing/image_cache.h"
#include "general/image_utilities.h"
#include "general/debug.h"
#include "general/mystring.h"
#include "user_interface/message.h"
#include "image_processing/computed_field_second_order_hermite.h"


#define my_Min(x,y) ((x) <= (y) ? (x) : (y))
#define my_Max(x,y) ((x) <= (y) ? (y) : (x))


struct Computed_field_second_order_hermite_package
/*******************************************************************************
LAST MODIFIED : 17 February 2004

DESCRIPTION :
A container for objects required to define fields in this module.
==============================================================================*/
{
	struct MANAGER(Computed_field) *computed_field_manager;
	struct Cmiss_region *root_region;
	struct Graphics_buffer_package *graphics_buffer_package;
};


struct Computed_field_second_order_hermite_type_specific_data
{
	double sigma;
	int *angle_from_x_axis; /* 2d vector (n,m) corresponding to angle value pi * (n/m). */
	float cached_time;
	/*int element_dimension;*/
	struct Cmiss_region *region;
	struct Graphics_buffer_package *graphics_buffer_package;
	struct Image_cache *image;
	struct MANAGER(Computed_field) *computed_field_manager;
	void *computed_field_manager_callback_id;
};

static char computed_field_second_order_hermite_type_string[] = "second_order_hermite";

int Computed_field_is_type_second_order_hermite(struct Computed_field *field)
/*******************************************************************************
LAST MODIFIED : 10 December 2003

DESCRIPTION :
==============================================================================*/
{
	int return_code;

	ENTER(Computed_field_is_type_second_order_hermite);
	if (field)
	{
		return_code =
		  (field->type_string == computed_field_second_order_hermite_type_string);
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"Computed_field_is_type_second_order_hermite.  Missing field");
		return_code = 0;
	}

	return (return_code);
} /* Computed_field_is_type_second_order_hermite */

static void Computed_field_second_order_hermite_field_change(
	struct MANAGER_MESSAGE(Computed_field) *message, void *field_void)
/*******************************************************************************
LAST MODIFIED : 5 December 2003

DESCRIPTION :
Manager function called back when either of the source fields change so that
we know to invalidate the image cache.
==============================================================================*/
{
	struct Computed_field *field;
	struct Computed_field_second_order_hermite_type_specific_data *data;

	ENTER(Computed_field_second_order_hermite_source_field_change);
	if (message && (field = (struct Computed_field *)field_void) && (data =
		(struct Computed_field_second_order_hermite_type_specific_data *)
		field->type_specific_data))
	{
		switch (message->change)
		{
			case MANAGER_CHANGE_OBJECT_NOT_IDENTIFIER(Computed_field):
			case MANAGER_CHANGE_OBJECT(Computed_field):
			{
				if (Computed_field_depends_on_Computed_field_in_list(
					field->source_fields[0], message->changed_object_list) ||
					Computed_field_depends_on_Computed_field_in_list(
					field->source_fields[1], message->changed_object_list))
				{
					if (data->image)
					{
						data->image->valid = 0;
					}
				}
			} break;
			case MANAGER_CHANGE_ADD(Computed_field):
			case MANAGER_CHANGE_REMOVE(Computed_field):
			case MANAGER_CHANGE_IDENTIFIER(Computed_field):
			{
				/* do nothing */
			} break;
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"Computed_field_second_order_hermite_source_field_change.  "
			"Invalid arguments.");
	}
	LEAVE;
} /* Computed_field_second_order_hermite_source_field_change */

static int Computed_field_second_order_hermite_clear_type_specific(
	struct Computed_field *field)
/*******************************************************************************
LAST MODIFIED : 28 July 2004

DESCRIPTION :
Clear the type specific data used by this type.
==============================================================================*/
{
	int return_code;
	struct Computed_field_second_order_hermite_type_specific_data *data;

	ENTER(Computed_field_second_order_hermite_clear_type_specific);
	if (field && (data =
		(struct Computed_field_second_order_hermite_type_specific_data *)
		field->type_specific_data))
	{
		if (data->region)
		{
			DEACCESS(Cmiss_region)(&data->region);
		}
		if (data->image)
		{
			DEACCESS(Image_cache)(&data->image);
		}
		if (data->computed_field_manager && data->computed_field_manager_callback_id)
		{
			MANAGER_DEREGISTER(Computed_field)(
				data->computed_field_manager_callback_id,
				data->computed_field_manager);
		}
		if (data->angle_from_x_axis)
		{
		        DEALLOCATE(data->angle_from_x_axis);
		}
		DEALLOCATE(field->type_specific_data);
		return_code = 1;
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"Computed_field_second_order_hermite_clear_type_specific.  "
			"Invalid arguments.");
		return_code = 0;
	}
	LEAVE;

	return (return_code);
} /* Computed_field_second_order_hermite_clear_type_specific */

static void *Computed_field_second_order_hermite_copy_type_specific(
	struct Computed_field *source_field, struct Computed_field *destination_field)
/*******************************************************************************
LAST MODIFIED : 28 July 2004

DESCRIPTION :
Copy the type specific data used by this type.
==============================================================================*/
{
	struct Computed_field_second_order_hermite_type_specific_data *destination,
		*source;
	int i;

	ENTER(Computed_field_second_order_hermite_copy_type_specific);
	if (source_field && destination_field && (source =
		(struct Computed_field_second_order_hermite_type_specific_data *)
		source_field->type_specific_data))
	{
		if (ALLOCATE(destination,
			struct Computed_field_second_order_hermite_type_specific_data, 1))
		{
			destination->sigma = source->sigma;
			for (i = 0 ; i < 2 ; i++)
			{
				destination->angle_from_x_axis[i] = source->angle_from_x_axis[i];
			}
			destination->cached_time = source->cached_time;
			destination->region = ACCESS(Cmiss_region)(source->region);
			/*destination->element_dimension = source->element_dimension;*/
			destination->graphics_buffer_package = source->graphics_buffer_package;
			destination->computed_field_manager = source->computed_field_manager;
			destination->computed_field_manager_callback_id =
				MANAGER_REGISTER(Computed_field)(
				Computed_field_second_order_hermite_field_change, (void *)destination_field,
				destination->computed_field_manager);
			if (source->image)
			{
				destination->image = ACCESS(Image_cache)(CREATE(Image_cache)());
				Image_cache_update_dimension(destination->image,
					source->image->dimension, source->image->depth,
					source->image->sizes);
			}
			else
			{
				destination->image = (struct Image_cache *)NULL;
			}
		}
		else
		{
			display_message(ERROR_MESSAGE,
				"Computed_field_second_order_hermite_copy_type_specific.  "
				"Unable to allocate memory.");
			destination = NULL;
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"Computed_field_second_order_hermite_copy_type_specific.  "
			"Invalid arguments.");
		destination = NULL;
	}
	LEAVE;

	return (destination);
} /* Computed_field_second_order_hermite_copy_type_specific */

int Computed_field_second_order_hermite_clear_cache_type_specific
   (struct Computed_field *field)
/*******************************************************************************
LAST MODIFIED : 10 December 2003

DESCRIPTION :
==============================================================================*/
{
	int return_code;
	struct Computed_field_second_order_hermite_type_specific_data *data;

	ENTER(Computed_field_second_order_hermite_clear_type_specific);
	if (field && (data =
		(struct Computed_field_second_order_hermite_type_specific_data *)
		field->type_specific_data))
	{
		if (data->image)
		{
			/* data->image->valid = 0; */
		}
		return_code = 1;
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"Computed_field_second_order_hermite_clear_type_specific.  "
			"Invalid arguments.");
		return_code = 0;
	}
	LEAVE;

	return (return_code);
} /* Computed_field_second_order_hermite_clear_type_specific */

static int Computed_field_second_order_hermite_type_specific_contents_match(
	struct Computed_field *field, struct Computed_field *other_computed_field)
/*******************************************************************************
LAST MODIFIED : 28 July 2004

DESCRIPTION :
Compare the type specific data
==============================================================================*/
{
	int return_code;
	struct Computed_field_second_order_hermite_type_specific_data *data,
		*other_data;

	ENTER(Computed_field_second_order_hermite_type_specific_contents_match);
	if (field && other_computed_field && (data =
		(struct Computed_field_second_order_hermite_type_specific_data *)
		field->type_specific_data) && (other_data =
		(struct Computed_field_second_order_hermite_type_specific_data *)
		other_computed_field->type_specific_data))
	{
		if ((data->sigma == other_data->sigma) &&
			data->image && other_data->image &&
			(data->image->dimension == other_data->image->dimension) &&
			(data->image->depth == other_data->image->depth))
		{
			/* Check sizes and minimums and maximums */
			return_code = 1;
		}
		else
		{
			return_code = 0;
		}
	}
	else
	{
		return_code = 0;
	}
	LEAVE;

	return (return_code);
} /* Computed_field_second_order_hermite_type_specific_contents_match */

#define Computed_field_second_order_hermite_is_defined_in_element \
	Computed_field_default_is_defined_in_element
/*******************************************************************************
LAST MODIFIED : 10 December 2003

DESCRIPTION :
Check the source fields using the default.
==============================================================================*/

#define Computed_field_second_order_hermite_is_defined_at_node \
	Computed_field_default_is_defined_at_node
/*******************************************************************************
LAST MODIFIED : 10 December 2003

DESCRIPTION :
Check the source fields using the default.
==============================================================================*/

#define Computed_field_second_order_hermite_has_numerical_components \
	Computed_field_default_has_numerical_components
/*******************************************************************************
LAST MODIFIED : 10 December 2003

DESCRIPTION :
Window projection does have numerical components.
==============================================================================*/

#define Computed_field_second_order_hermite_not_in_use \
	(Computed_field_not_in_use_function)NULL
/*******************************************************************************
LAST MODIFIED : 10 December 2003

DESCRIPTION :
No special criteria.
==============================================================================*/

static int Image_cache_second_order_hermite(struct Image_cache *image, 
          double sigma, int *angle_from_x_axis)
/*******************************************************************************
LAST MOErFIED : 15 September 2004

DESCRIPTION :
Perform orientation detection bsed on hermite filter.
==============================================================================*/
{
	char *storage;
	FE_value *data_index, *result_index, *max;
	int i, j, k, storage_size;
	FE_value  *Di, *g_kernel, g1, g2, g3;
	int filter_size;
	int *offsets;
	int radius, center;
	/* int image_step, kernel_step;*/
	int return_code, kernel_size;
	FE_value theta, x, y, sum;

	ENTER(Image_cache_second_order_hermite);
	if (image && (image->dimension > 1) && (image->dimension < 4) && (image->depth > 0)
	        && (angle_from_x_axis[1] > 0))
	{
		return_code = 1;
		radius = (int) ceil(2.5 * sigma);
		filter_size = 2 * radius + 1;
		theta = ((FE_value)angle_from_x_axis[0] / (FE_value)angle_from_x_axis[1]) * M_PI;
		
		/* We only need the one kernel as it is just a reordering for the other dimensions */
		kernel_size = 1;
		for (i = 0 ; i < image->dimension ; i++)
		{
			kernel_size *= filter_size;
		}
		
		/* Allocate a new storage block for our data */
		storage_size = image->depth;
		for (i = 0 ; i < image->dimension ; i++)
		{
			storage_size *= image->sizes[i];
		}
		if (ALLOCATE(offsets, int, kernel_size) &&
			ALLOCATE(g_kernel, FE_value, kernel_size) &&
			ALLOCATE(storage, char, storage_size * sizeof(FE_value)) &&
			ALLOCATE(Di, FE_value, image->depth) &&
			ALLOCATE(max, FE_value, image->depth))
		{
			result_index = (FE_value *)storage;
			for (i = 0 ; i < storage_size ; i++)
			{
				*result_index = 0.0;
				result_index++;
			}
			for (k = 0; k < image->depth; k++)
			{
		        	max[k] = 0.0;
			}
			/* Create a 1d gaussian kernel */
			center = filter_size / 2;
			sum = 0.0;
			for (i = 0; i < filter_size; i++)
			{
			        for (j = 0; j < filter_size; j++)
				{
				        x = (FE_value)(j - center);
					y = (FE_value)(i - center); 
				        g1 = 0.9213 * (2.0 *x*x - sigma * sigma) * pow(2.7, -(x*x + y*y)/(sigma*sigma)) / (sigma * sigma);
					g2 = 1.843 * x * y * pow(2.7, -(x*x + y*y)/(sigma*sigma)) / (sigma * sigma);
					g3 = 0.9213 * (2.0 *y*y - sigma * sigma) * pow(2.7, -(x*x + y*y)/(sigma*sigma)) / (sigma * sigma);
					g_kernel[i * filter_size + j] = cos(theta) * cos(theta) *g1 - 2.0 * sin(theta) * cos(theta) * g2 + sin(theta) * sin(theta) * g3;
					sum += fabs(g_kernel[i * filter_size + j]);
				}
			}
			
			for(j = 0; j < kernel_size; j++)
			{
			        g_kernel[j] /= sum;
			}
			data_index = (FE_value *)image->data;
			result_index = (FE_value *)storage;
			
			for (j = 0 ; j < kernel_size ; j++)
			{
				offsets[j] = 0;
			}
			/*for(j = 0; j < kernel_size; j++)
			{
			        kernel_step = 1;
				image_step = 1;
				for(m = 0; m < image->dimension; m++)
				{
				        k = ((int)((FE_value)j/((FE_value)kernel_step))) % filter_size;
					offsets[j] += (k - radius) * image_step * image->depth;
					kernel_step *= filter_size;
					image_step *= image->sizes[m];
				}
			}*/
			Filter_offsets(offsets, image->dimension, radius, image->sizes, image->depth);
			for (i = 0 ; i < storage_size / image->depth ; i++)
			{
				for (k = 0 ; k < image->depth ; k++)
				{
                                        Di[k] = 0.0;
                                        for(j = 0 ; j < kernel_size ; j++)
					{
						if(result_index + offsets[j] < ((FE_value *)storage))
						{
						          /*wrapping around */
						         Di[k] += *(data_index + offsets[j] + storage_size +k) * g_kernel[j];
						}
						else if (result_index + offsets[j] >= ((FE_value *)storage) + storage_size)
						{
                                                        /*wrapping back */
							 Di[k] += *(data_index + offsets[j]-storage_size + k) * g_kernel[j];
						}
						else
						{
							/*standard */
						        Di[k] += *(data_index + offsets[j] + k) * g_kernel[j];
						}
					}
					max[k] = my_Max(max[k],Di[k]);
					result_index[k] = Di[k];
				}
				data_index += image->depth;
				result_index += image->depth;
			}
			for (i = (storage_size/image->depth) - 1; i>= 0; i--)
			{
			        result_index -= image->depth;
				for (k = 0; k < image->depth; k++)
				{
				        if (max[k] == 0.0)
					{
					        result_index[k] = 0.0;
					}
					else
					{
					        result_index[k] /= max[k];
					}
				}
			}
			if (return_code)
			{
				DEALLOCATE(image->data);
				image->data = storage;
				image->valid = 1;
			}
			else
			{
				DEALLOCATE(storage);
			}
			DEALLOCATE(offsets);
			DEALLOCATE(Di);
			DEALLOCATE(max);
			DEALLOCATE(g_kernel);
		}
		else
		{
			display_message(ERROR_MESSAGE,
				"Image_cache_second_order_hermite.  Not enough memory");
			return_code = 0;
		}
	}
	else
	{
		display_message(ERROR_MESSAGE, "Image_cache_second_order_hermite.  "
			"Invalid arguments.");
		return_code=0;
	}
	LEAVE;

	return (return_code);
} /* Image_cache_second_order_hermite */

static int Computed_field_second_order_hermite_evaluate_cache_at_node(
	struct Computed_field *field, struct FE_node *node, FE_value time)
/*******************************************************************************
LAST MODIFIED : 28 July 2004

DESCRIPTION :
Evaluate the fields cache at the node.
==============================================================================*/
{
	int return_code;
	struct Computed_field_second_order_hermite_type_specific_data *data;

	ENTER(Computed_field_second_order_hermite_evaluate_cache_at_node);
	if (field && node &&
		(data = (struct Computed_field_second_order_hermite_type_specific_data *)field->type_specific_data))
	{
		return_code = 1;
		/* 1. Precalculate the Image_cache */
		if (!data->image->valid)
		{
			return_code = Image_cache_update_from_fields(data->image, field->source_fields[0],
				field->source_fields[1], data->region,
				data->graphics_buffer_package);
			/* 2. Perform image processing operation */
			return_code = Image_cache_second_order_hermite(data->image, data->sigma,
			        data->angle_from_x_axis);
		}
		/* 3. Evaluate texture coordinates and copy image to field */
		Computed_field_evaluate_cache_at_node(field->source_fields[1],
			node, time);
		Image_cache_evaluate_field(data->image,field);

	}
	else
	{
		display_message(ERROR_MESSAGE,
			"Computed_field_second_order_hermite_evaluate_cache_at_node.  "
			"Invalid argument(s)");
		return_code = 0;
	}
	LEAVE;

	return (return_code);
} /* Computed_field_second_order_hermite_evaluate_cache_at_node */

static int Computed_field_second_order_hermite_evaluate_cache_in_element(
	struct Computed_field *field, struct FE_element *element, FE_value *xi,
	FE_value time, struct FE_element *top_level_element,int calculate_derivatives)
/*******************************************************************************
LAST MODIFIED : 28 July 2004

DESCRIPTION :
Evaluate the fields cache at the node.
==============================================================================*/
{
	int return_code;
	struct Computed_field_second_order_hermite_type_specific_data *data;

	ENTER(Computed_field_second_order_hermite_evaluate_cache_in_element);
	USE_PARAMETER(calculate_derivatives);
	if (field && element && xi && (field->number_of_source_fields > 0) &&
		(field->number_of_components == field->source_fields[0]->number_of_components) &&
		(data = (struct Computed_field_second_order_hermite_type_specific_data *) field->type_specific_data) &&
		data->image && (field->number_of_components == data->image->depth))
	{
		return_code = 1;
		/* 1. Precalculate the Image_cache */
		if (!data->image->valid)
		{
			return_code = Image_cache_update_from_fields(data->image, field->source_fields[0],
				field->source_fields[1], data->region,
				data->graphics_buffer_package);
			/* 2. Perform image processing operation */
			return_code = Image_cache_second_order_hermite(data->image, data->sigma,
			        data->angle_from_x_axis);
		}
		/* 3. Evaluate texture coordinates and copy image to field */
		Computed_field_evaluate_cache_in_element(field->source_fields[1],
			element, xi, time, top_level_element, /*calculate_derivatives*/0);
		Image_cache_evaluate_field(data->image,field);

	}
	else
	{
		display_message(ERROR_MESSAGE,
			"Computed_field_second_order_hermite_evaluate_cache_in_element.  "
			"Invalid argument(s)");
		return_code = 0;
	}
	LEAVE;

	return (return_code);
} /* Computed_field_second_order_hermite_evaluate_cache_in_element */

#define Computed_field_second_order_hermite_evaluate_as_string_at_node \
	Computed_field_default_evaluate_as_string_at_node
/*******************************************************************************
LAST MODIFIED : 10 December 2003

DESCRIPTION :
Print the values calculated in the cache.
==============================================================================*/

#define Computed_field_second_order_hermite_evaluate_as_string_in_element \
	Computed_field_default_evaluate_as_string_in_element
/*******************************************************************************
LAST MODIFIED : 10 December 2003

DESCRIPTION :
Print the values calculated in the cache.
==============================================================================*/

#define Computed_field_second_order_hermite_set_values_at_node \
   (Computed_field_set_values_at_node_function)NULL
/*******************************************************************************
LAST MODIFIED : 10 December 2003

DESCRIPTION :
Not implemented yet.
==============================================================================*/

#define Computed_field_second_order_hermite_set_values_in_element \
   (Computed_field_set_values_in_element_function)NULL
/*******************************************************************************
LAST MODIFIED : 10 December 2003

DESCRIPTION :
Not implemented yet.
==============================================================================*/

#define Computed_field_second_order_hermite_get_native_discretization_in_element \
	Computed_field_default_get_native_discretization_in_element
/*******************************************************************************
LAST MODIFIED : 10 December 2003

DESCRIPTION :
Inherit result from first source field.
==============================================================================*/

#define Computed_field_second_order_hermite_find_element_xi \
   (Computed_field_find_element_xi_function)NULL
/*******************************************************************************
LAST MODIFIED : 10 December 2003

DESCRIPTION :
Not implemented yet.
==============================================================================*/

int Computed_field_second_order_hermite_get_native_resolution(struct Computed_field *field,
        int *dimension, int **sizes, 
	struct Computed_field **texture_coordinate_field)
/*******************************************************************************
LAST MODIFIED : 4 February 2005

DESCRIPTION :
Gets the <dimension>, <sizes>, <minimums>, <maximums> and <texture_coordinate_field> from
the <field>. These parameters will be used in image processing.

==============================================================================*/
{       
        int return_code;
	struct Computed_field_second_order_hermite_type_specific_data *data;
	
	ENTER(Computed_field_second_order_hermite_get_native_resolution);
	if (field && (data =
		(struct Computed_field_second_order_hermite_type_specific_data *)
		field->type_specific_data) && data->image)
	{
	        return_code = 1;
		Image_cache_get_native_resolution(data->image,
			dimension, sizes);
		/* Texture_coordinate_field from source fields */
		if (*texture_coordinate_field)
		{
			REACCESS(Computed_field)(&(*texture_coordinate_field), field->source_fields[1]); 
		}
		else
		{
		        *texture_coordinate_field = ACCESS(Computed_field)(field->source_fields[1]);
		}	 
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"Computed_field_second_order_hermite_get_native_resolution.  Missing field");
		return_code=0;
	}

	return (return_code);
} /* Computed_field_second_order_hermite_get_native_resolution */


static int list_Computed_field_second_order_hermite(
	struct Computed_field *field)
/*******************************************************************************
LAST MODIFIED : 28 July 2004

DESCRIPTION :
==============================================================================*/
{
	int return_code;
	struct Computed_field_second_order_hermite_type_specific_data *data;

	ENTER(List_Computed_field_second_order_hermite);
	if (field && (field->type_string==computed_field_second_order_hermite_type_string)
		&& (data = (struct Computed_field_second_order_hermite_type_specific_data *)
		field->type_specific_data))
	{
		display_message(INFORMATION_MESSAGE,
			"    source field : %s\n",field->source_fields[0]->name);
		display_message(INFORMATION_MESSAGE,
			"    texture coordinate field : %s\n",field->source_fields[1]->name);
		display_message(INFORMATION_MESSAGE,
			"    filter sigma : %f\n", data->sigma);
		return_code = 1;
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"list_Computed_field_second_order_hermite.  Invalid field");
		return_code = 0;
	}
	LEAVE;

	return (return_code);
} /* list_Computed_field_second_order_hermite */

static char *Computed_field_second_order_hermite_get_command_string(
	struct Computed_field *field)
/*******************************************************************************
LAST MODIFIED : 28 July 2004

DESCRIPTION :
Returns allocated command string for reproducing field. Includes type.
==============================================================================*/
{
	char *command_string, *field_name, temp_string[40];
	int error;
	struct Computed_field_second_order_hermite_type_specific_data *data;

	ENTER(Computed_field_second_order_hermite_get_command_string);
	command_string = (char *)NULL;
	if (field&& (field->type_string==computed_field_second_order_hermite_type_string)
		&& (data = (struct Computed_field_second_order_hermite_type_specific_data *)
		field->type_specific_data) )
	{
		error = 0;
		append_string(&command_string,
			computed_field_second_order_hermite_type_string, &error);
		append_string(&command_string, " field ", &error);
		if (GET_NAME(Computed_field)(field->source_fields[0], &field_name))
		{
			make_valid_token(&field_name);
			append_string(&command_string, field_name, &error);
			DEALLOCATE(field_name);
		}
		append_string(&command_string, " texture_coordinate_field ", &error);
		if (GET_NAME(Computed_field)(field->source_fields[1], &field_name))
		{
			make_valid_token(&field_name);
			append_string(&command_string, field_name, &error);
			DEALLOCATE(field_name);
		}
		sprintf(temp_string, " dimension %d ", data->image->dimension);
		append_string(&command_string, temp_string, &error);

		sprintf(temp_string, " angle_from_x_axis %d %d ",
		                    data->angle_from_x_axis[0],data->angle_from_x_axis[1]);
		append_string(&command_string, temp_string, &error);

		sprintf(temp_string, " sigma %f", data->sigma);
		append_string(&command_string, temp_string, &error);

		sprintf(temp_string, " sizes %d %d",
		                    data->image->sizes[0],data->image->sizes[1]);
		append_string(&command_string, temp_string, &error);
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"Computed_field_second_order_hermite_get_command_string.  Invalid field");
	}
	LEAVE;

	return (command_string);
} /* Computed_field_second_order_hermite_get_command_string */

#define Computed_field_second_order_hermite_has_multiple_times \
	Computed_field_default_has_multiple_times
/*******************************************************************************
LAST MODIFIED : 4 December 2003

DESCRIPTION :
Works out whether time influences the field.
==============================================================================*/

int Computed_field_set_type_second_order_hermite(struct Computed_field *field,
	struct Computed_field *source_field,
	struct Computed_field *texture_coordinate_field,
	double sigma, int *angle_from_x_axis,
	int dimension, int *sizes,
	struct MANAGER(Computed_field) *computed_field_manager,
	struct Cmiss_region *region, struct Graphics_buffer_package *graphics_buffer_package)
/*******************************************************************************
LAST MODIFIED : 28 July 2004

DESCRIPTION :
Converts <field> to type COMPUTED_FIELD_second_order_hermite with the supplied
fields, <source_field> and <texture_coordinate_field>.  The <radius> specifies
half the width and height of the filter window.  The <dimension> is the
size of the <sizes>.
==============================================================================*/
{
	int i, depth, number_of_source_fields, return_code;
	struct Computed_field **source_fields;
	struct Computed_field_second_order_hermite_type_specific_data *data;

	ENTER(Computed_field_set_type_second_order_hermite);
	if (field && source_field && texture_coordinate_field &&
		(sigma > 0.0) && (depth = source_field->number_of_components) &&
		(dimension <= texture_coordinate_field->number_of_components) &&
		region && graphics_buffer_package)
	{
		return_code=1;
		/* 1. make dynamic allocations for any new type-specific data */
		number_of_source_fields=2;
		data = (struct Computed_field_second_order_hermite_type_specific_data *)NULL;
		if (ALLOCATE(source_fields, struct Computed_field *, number_of_source_fields) &&
			ALLOCATE(data, struct Computed_field_second_order_hermite_type_specific_data, 1) &&
			ALLOCATE(data->angle_from_x_axis, int, 2) &&
			(data->image = ACCESS(Image_cache)(CREATE(Image_cache)())) &&
			Image_cache_update_dimension(
			data->image, dimension, depth, sizes) &&
			Image_cache_update_data_storage(data->image))
		{
			/* 2. free current type-specific data */
			Computed_field_clear_type(field);
			/* 3. establish the new type */
			field->type_string = computed_field_second_order_hermite_type_string;
			field->number_of_components = source_field->number_of_components;
			source_fields[0]=ACCESS(Computed_field)(source_field);
			source_fields[1]=ACCESS(Computed_field)(texture_coordinate_field);
			field->source_fields=source_fields;
			field->number_of_source_fields=number_of_source_fields;
			data->sigma = sigma;
			for (i = 0 ; i < 2 ; i++)
			{
				data->angle_from_x_axis[i] = angle_from_x_axis[i];
			}
			/*data->element_dimension = element_dimension;*/
			data->region = ACCESS(Cmiss_region)(region);
			data->graphics_buffer_package = graphics_buffer_package;
			data->computed_field_manager = computed_field_manager;
			data->computed_field_manager_callback_id =
				MANAGER_REGISTER(Computed_field)(
				Computed_field_second_order_hermite_field_change, (void *)field,
				computed_field_manager);

			field->type_specific_data = data;

			/* Set all the methods */
			COMPUTED_FIELD_ESTABLISH_METHODS(second_order_hermite);
		}
		else
		{
			DEALLOCATE(source_fields);
			if (data)
			{
				if (data->image)
				{
					DESTROY(Image_cache)(&data->image);
				}
				DEALLOCATE(data);
			}
			return_code = 0;
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"Computed_field_set_type_second_order_hermite.  Invalid argument(s)");
		return_code = 0;
	}
	LEAVE;

	return (return_code);
} /* Computed_field_set_type_second_order_hermite */

int Computed_field_get_type_second_order_hermite(struct Computed_field *field,
	struct Computed_field **source_field,
	struct Computed_field **texture_coordinate_field,
	double *sigma, int **angle_from_x_axis,
	int *dimension, int **sizes)
/*******************************************************************************
LAST MODIFIED : 28 July 2004

DESCRIPTION :
If the field is of type COMPUTED_FIELD_second_order_hermite, the
parameters defining it are returned.
==============================================================================*/
{
	int i, return_code;
	struct Computed_field_second_order_hermite_type_specific_data *data;

	ENTER(Computed_field_get_type_second_order_hermite);
	if (field && (field->type_string==computed_field_second_order_hermite_type_string)
		&& (data = (struct Computed_field_second_order_hermite_type_specific_data *)
		field->type_specific_data) && data->image)
	{
		*dimension = data->image->dimension;
		if (ALLOCATE(*sizes, int, *dimension)
		        && ALLOCATE(*angle_from_x_axis, int, 2))
		{
			*source_field = field->source_fields[0];
			*texture_coordinate_field = field->source_fields[1];
			*sigma = data->sigma;
			for (i = 0; i < 2; i++)
			{
			        (*angle_from_x_axis)[i] = data->angle_from_x_axis[i];
			}
			for (i = 0 ; i < *dimension ; i++)
			{
				(*sizes)[i] = data->image->sizes[i];
			}
			return_code=1;
		}
		else
		{
			display_message(ERROR_MESSAGE,
				"Computed_field_get_type_second_order_hermite.  Unable to allocate vectors.");
			return_code = 0;
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"Computed_field_get_type_second_order_hermite.  Invalid argument(s)");
		return_code = 0;
	}
	LEAVE;

	return (return_code);
} /* Computed_field_get_type_second_order_hermite */

static int define_Computed_field_type_second_order_hermite(struct Parse_state *state,
	void *field_void, void *computed_field_second_order_hermite_package_void)
/*******************************************************************************
LAST MODIFIED : 28 July 2004

DESCRIPTION :
Converts <field> into type COMPUTED_FIELD_second_order_hermite (if it is not
already) and allows its contents to be modified.
==============================================================================*/
{
	char *current_token;
	int dim = 2;
	double sigma;
	int *angle_from_x_axis;
	int dimension,  return_code, *sizes;
	struct Computed_field *field, *source_field, *texture_coordinate_field;
	struct Computed_field_second_order_hermite_package
		*computed_field_second_order_hermite_package;
	struct Option_table *option_table;
	struct Set_Computed_field_conditional_data set_source_field_data,
		set_texture_coordinate_field_data;

	ENTER(define_Computed_field_type_second_order_hermite);
	if (state&&(field=(struct Computed_field *)field_void)&&
		(computed_field_second_order_hermite_package=
		(struct Computed_field_second_order_hermite_package *)
		computed_field_second_order_hermite_package_void))
	{
		return_code=1;
		source_field = (struct Computed_field *)NULL;
		texture_coordinate_field = (struct Computed_field *)NULL;
		dimension = 0;
		angle_from_x_axis = (int *)NULL;
		sizes = (int *)NULL;
		sigma = 1.0;
		/* field */
		set_source_field_data.computed_field_manager =
			computed_field_second_order_hermite_package->computed_field_manager;
		set_source_field_data.conditional_function =
			Computed_field_has_numerical_components;
		set_source_field_data.conditional_function_user_data = (void *)NULL;
		/* texture_coordinate_field */
		set_texture_coordinate_field_data.computed_field_manager =
			computed_field_second_order_hermite_package->computed_field_manager;
		set_texture_coordinate_field_data.conditional_function =
			Computed_field_has_numerical_components;
		set_texture_coordinate_field_data.conditional_function_user_data = (void *)NULL;

		if (computed_field_second_order_hermite_type_string ==
			Computed_field_get_type_string(field))
		{
			return_code = Computed_field_get_type_second_order_hermite(field,
				&source_field, &texture_coordinate_field, &sigma,
				&angle_from_x_axis, &dimension, &sizes);
		}
		if (return_code)
		{
			/* must access objects for set functions */
			if (source_field)
			{
				ACCESS(Computed_field)(source_field);
			}
			if (texture_coordinate_field)
			{
				ACCESS(Computed_field)(texture_coordinate_field);
			}

			if ((current_token=state->current_token) &&
				(!(strcmp(PARSER_HELP_STRING,current_token)&&
					strcmp(PARSER_RECURSIVE_HELP_STRING,current_token))))
			{
				option_table = CREATE(Option_table)();
				/* angle_from_x_axis */
				Option_table_add_int_vector_entry(option_table,
					"angle_from_x_axis", angle_from_x_axis, &dim);
				/* dimension */
				Option_table_add_int_positive_entry(option_table,
				        "dimension", &dimension);
				/* field */
				Option_table_add_Computed_field_conditional_entry(option_table,
					"field", &source_field, &set_source_field_data);
				/* radius */
				Option_table_add_double_entry(option_table,
					"sigma", &sigma);
				/* sizes */
				Option_table_add_int_vector_entry(option_table,
					"sizes", sizes, &dimension);
				/* texture_coordinate_field */
				Option_table_add_Computed_field_conditional_entry(option_table,
					"texture_coordinate_field", &texture_coordinate_field,
					&set_texture_coordinate_field_data);
				return_code=Option_table_multi_parse(option_table,state);
				DESTROY(Option_table)(&option_table);
			}
			/* parse the dimension ... */
			if (return_code && (current_token = state->current_token))
			{
				/* ... only if the "dimension" token is next */
				if (fuzzy_string_compare(current_token, "dimension"))
				{
					option_table = CREATE(Option_table)();
					/* dimension */
					Option_table_add_int_positive_entry(option_table, "dimension",
						&dimension);
					if (return_code = Option_table_parse(option_table, state))
					{
						if (!(REALLOCATE(sizes, sizes, int, dimension) &&
						        REALLOCATE(angle_from_x_axis, angle_from_x_axis, int, 2)))
						{
							return_code = 0;
						}
					}
					DESTROY(Option_table)(&option_table);
				}
			}
			/*if (return_code && (dimension < 1))
			{
				display_message(ERROR_MESSAGE,
					"define_Computed_field_type_scale.  Must specify a dimension first.");
				return_code = 0;
			}*/
			/* parse the rest of the table */
			if (return_code&&state->current_token)
			{
				option_table = CREATE(Option_table)();
				/* angle_from_x_axis */
				Option_table_add_int_vector_entry(option_table,
					"angle_from_x_axis", angle_from_x_axis, &dim);
				/* field */
				Option_table_add_Computed_field_conditional_entry(option_table,
					"field", &source_field, &set_source_field_data);
				/* radius */
				Option_table_add_double_entry(option_table,
					"sigma", &sigma);
				/* sizes */
				Option_table_add_int_vector_entry(option_table,
					"sizes", sizes, &dimension);
				/* texture_coordinate_field */
				Option_table_add_Computed_field_conditional_entry(option_table,
					"texture_coordinate_field", &texture_coordinate_field,
					&set_texture_coordinate_field_data);
				return_code=Option_table_multi_parse(option_table,state);
				DESTROY(Option_table)(&option_table);
			}
			if ((dimension < 1) && source_field)
			{
			        return_code = Computed_field_get_native_resolution(source_field,
				     &dimension,&sizes,&texture_coordinate_field);
			}
			/* no errors,not asking for help */
			if (return_code)
			{
				return_code = Computed_field_set_type_second_order_hermite(field,
					source_field, texture_coordinate_field, sigma,
					angle_from_x_axis,dimension, sizes, 
					computed_field_second_order_hermite_package->computed_field_manager,
					computed_field_second_order_hermite_package->root_region,
					computed_field_second_order_hermite_package->graphics_buffer_package);
			}
			if (!return_code)
			{
				if ((!state->current_token)||
					(strcmp(PARSER_HELP_STRING,state->current_token)&&
						strcmp(PARSER_RECURSIVE_HELP_STRING,state->current_token)))
				{
					/* error */
					display_message(ERROR_MESSAGE,
						"define_Computed_field_type_second_order_hermite.  Failed");
				}
			}
			if (source_field)
			{
				DEACCESS(Computed_field)(&source_field);
			}
			if (texture_coordinate_field)
			{
				DEACCESS(Computed_field)(&texture_coordinate_field);
			}
			if (angle_from_x_axis)
			{
				DEALLOCATE(angle_from_x_axis);
			}
			if (sizes)
			{
				DEALLOCATE(sizes);
			}
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"define_Computed_field_type_second_order_hermite.  Invalid argument(s)");
		return_code = 0;
	}
	LEAVE;

	return (return_code);
} /* define_Computed_field_type_second_order_hermite */

int Computed_field_register_types_second_order_hermite(
	struct Computed_field_package *computed_field_package,
	struct Cmiss_region *root_region, struct Graphics_buffer_package *graphics_buffer_package)
/*******************************************************************************
LAST MODIFIED : 28 July 2004

DESCRIPTION :
==============================================================================*/
{
	int return_code;
	static struct Computed_field_second_order_hermite_package
		computed_field_second_order_hermite_package;

	ENTER(Computed_field_register_types_second_order_hermite);
	if (computed_field_package)
	{
		computed_field_second_order_hermite_package.computed_field_manager =
			Computed_field_package_get_computed_field_manager(
				computed_field_package);
		computed_field_second_order_hermite_package.root_region = root_region;
		computed_field_second_order_hermite_package.graphics_buffer_package = graphics_buffer_package;
		return_code = Computed_field_package_add_type(computed_field_package,
			            computed_field_second_order_hermite_type_string,
			            define_Computed_field_type_second_order_hermite,
			            &computed_field_second_order_hermite_package);
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"Computed_field_register_types_second_order_hermite.  Invalid argument(s)");
		return_code = 0;
	}
	LEAVE;

	return (return_code);
} /* Computed_field_register_types_second_order_hermite */

