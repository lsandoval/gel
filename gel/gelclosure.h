#ifndef __GEL_CLOSURE_H__
#define __GEL_CLOSURE_H__

#include <glib-object.h>

#include <gelcontext.h>


typedef struct _GelClosure GelClosure;

GClosure* gel_closure_new(GelContext *context, gchar **args, GValueArray *code);

gboolean gel_closure_is_gel(GClosure *closure);


#endif
