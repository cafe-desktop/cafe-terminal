/* LIBCAFE - The CAFE Library
 * Copyright (C) 1995-1997 Peter Mattis and Spencer Kimball
 *
 * cafevaluearray.h ported from GValueArray
 *
 * This library is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see
 * <https://www.gnu.org/licenses/>.
 */

#ifndef __CAFE_VALUE_ARRAY_H__
#define __CAFE_VALUE_ARRAY_H__

typedef struct _CafeValueArray   CafeValueArray;

G_BEGIN_DECLS

/**
 * CAFE_TYPE_VALUE_ARRAY:
 *
 * The type ID of the "CafeValueArray" type which is a boxed type,
 * used to pass around pointers to CafeValueArrays.
 *
 * Since: 2.10
 */
#define CAFE_TYPE_VALUE_ARRAY (cafe_value_array_get_type ())


GType            cafe_value_array_get_type (void) G_GNUC_CONST;

CafeValueArray * cafe_value_array_new      (gint                  n_prealloced);
CafeValueArray * cafe_value_array_new_from_types
                                           (gchar               **error_msg,
                                            GType                 first_type,
                                            ...);
CafeValueArray * cafe_value_array_new_from_types_valist
                                           (gchar               **error_msg,
                                            GType                 first_type,
                                            va_list               va_args);

CafeValueArray * cafe_value_array_ref      (CafeValueArray       *value_array);
void             cafe_value_array_unref    (CafeValueArray       *value_array);

gint             cafe_value_array_length   (const CafeValueArray *value_array);

GValue         * cafe_value_array_index    (const CafeValueArray *value_array,
                                            gint                  index);

CafeValueArray * cafe_value_array_prepend  (CafeValueArray       *value_array,
                                            const GValue         *value);
CafeValueArray * cafe_value_array_append   (CafeValueArray       *value_array,
                                            const GValue         *value);
CafeValueArray * cafe_value_array_insert   (CafeValueArray       *value_array,
                                            gint                  index,
                                            const GValue         *value);

CafeValueArray * cafe_value_array_remove   (CafeValueArray       *value_array,
                                            gint                  index);
void             cafe_value_array_truncate (CafeValueArray       *value_array,
                                            gint                  n_values);


/*
 * CAFE_TYPE_PARAM_VALUE_ARRAY
 */

#define CAFE_TYPE_PARAM_VALUE_ARRAY           (cafe_param_value_array_get_type ())
#define CAFE_IS_PARAM_SPEC_VALUE_ARRAY(pspec) (G_TYPE_CHECK_INSTANCE_TYPE ((pspec), CAFE_TYPE_PARAM_VALUE_ARRAY))
#define CAFE_PARAM_SPEC_VALUE_ARRAY(pspec)    (G_TYPE_CHECK_INSTANCE_CAST ((pspec), CAFE_TYPE_PARAM_VALUE_ARRAY, CafeParamSpecValueArray))

typedef struct _CafeParamSpecValueArray CafeParamSpecValueArray;

/**
 * CafeParamSpecValueArray:
 * @parent_instance:  private #GParamSpec portion
 * @element_spec:     the #GParamSpec of the array elements
 * @fixed_n_elements: default length of the array
 *
 * A #GParamSpec derived structure that contains the meta data for
 * value array properties.
 **/
struct _CafeParamSpecValueArray
{
  GParamSpec  parent_instance;
  GParamSpec *element_spec;
  gint        fixed_n_elements;
};

GType        cafe_param_value_array_get_type (void) G_GNUC_CONST;

GParamSpec * cafe_param_spec_value_array     (const gchar    *name,
                                              const gchar    *nick,
                                              const gchar    *blurb,
                                              GParamSpec     *element_spec,
                                              GParamFlags     flags);


G_END_DECLS

#endif /* __CAFE_VALUE_ARRAY_H__ */

