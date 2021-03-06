#ifndef GEL_TYPE_SYMBOL
#define GEL_TYPE_SYMBOL (gel_symbol_get_type())

#include <glib-object.h>
#include <gelvariable.h>

typedef struct _GelSymbol GelSymbol;
GType gel_symbol_get_type(void) G_GNUC_CONST;

GelSymbol* gel_symbol_new(const gchar *name, GelVariable *variable);
GelSymbol* gel_symbol_dup(const GelSymbol *self);
void gel_symbol_free(GelSymbol *self);

const gchar* gel_symbol_get_name(const GelSymbol *self);
GelVariable* gel_symbol_get_variable(const GelSymbol *self);
void gel_symbol_set_variable(GelSymbol *self, GelVariable *variable);
GValue* gel_symbol_get_value(const GelSymbol *self);

#endif
