#ifndef __GEL_CONTEXT_H__
#define __GEL_CONTEXT_H__

#include <glib-object.h>


typedef struct _GelContext GelContext;

typedef void (*GelContextCallback)(gpointer user_data);

GelContext* gel_context_new(void);
GelContext* gel_context_new_with_outer(GelContext *outer);
void gel_context_free(GelContext *self);

GValue* gel_context_find_symbol(const GelContext *self, const gchar *name);
GelContext* gel_context_get_outer(const GelContext *self);

void gel_context_add_symbol(GelContext *self, const gchar *name, GValue *value);
void gel_context_add_object(GelContext *self, const gchar *name,
                            GObject *object);
void gel_context_add_callback(GelContext *self, const gchar *name,
                              GelContextCallback callback, gpointer user_data);
void gel_context_add_default_symbols(GelContext *self);
gboolean gel_context_remove_symbol(GelContext *self, const gchar *name);

gboolean gel_context_eval(GelContext *self,
                          const GValue *value, GValue *dest_value);
const GValue* gel_context_eval_value(GelContext *self,
                                     const GValue *value, GValue *tmp_value);
gboolean gel_context_eval_params(GelContext *self, const gchar *func,
                                 GList **list, const gchar *format,
                                 guint *n_values, const GValue **values, ...);


#endif

