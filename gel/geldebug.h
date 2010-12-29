#ifndef __GEL_DEBUG_H__
#define __GEL_DEBUG_H__

#include <glib-object.h>


void gel_warning_needs_at_least_n_arguments(const gchar *func, guint n);

void gel_warning_needs_n_arguments(const gchar *func, guint n);

void gel_warning_no_such_property(const gchar *func, const gchar *prop_name);

void gel_warning_value_not_of_type(const gchar *func,
                                   const GValue *value, GType type);

void gel_warning_unknown_symbol(const gchar *func, const gchar *symbol);

void gel_warning_type_not_instantiatable(const gchar *func, GType type);

void gel_warning_invalid_value_for_property(const gchar *func,
                                            const GValue *value,
                                            const GParamSpec *spec);

void gel_warning_type_name_invalid(const gchar *func, const gchar *name);

void gel_warning_invalid_argument_name(const gchar *func, const gchar *name);

void gel_warning_index_out_of_bounds(const gchar *func);


#endif
