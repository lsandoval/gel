#include <gelvariable.h>
#include <gelvalueprivate.h>


G_DEFINE_BOXED_TYPE(GelVariable, gel_variable, gel_variable_ref, gel_variable_unref)


struct _GelVariable
{
    GValue *value;
    gboolean owned;
    volatile gint ref_count;
};


GelVariable* gel_variable_new(GValue *value, gboolean owned)
{
    g_return_val_if_fail(value != NULL, NULL);

    GelVariable *self = g_slice_new0(GelVariable);
    self->value = value;
    self->owned = owned;
    self->ref_count = 1;

    return self;
}


GelVariable* gel_variable_ref(GelVariable *self)
{
    g_return_val_if_fail(self != NULL, NULL);

    g_atomic_int_add(&self->ref_count, 1);
    return self;
}


void gel_variable_unref(GelVariable *self)
{
    g_return_if_fail(self != NULL);

    if(g_atomic_int_dec_and_test(&self->ref_count))
    {
        if(self->owned)
            gel_value_free(self->value);
        g_slice_free(GelVariable, self);
    }
}


GValue* gel_variable_get_value(GelVariable *self)
{
    g_return_val_if_fail(self != NULL, NULL);
    return self->value;
}


gboolean gel_variable_get_owned(GelVariable *self)
{
    g_return_val_if_fail(self != NULL, FALSE);
    return self->owned;
}
