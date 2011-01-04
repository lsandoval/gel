#include <string.h>

#include <gelcontext.h>
#include <gelvalue.h>
#include <gelvalueprivate.h>
#include <gelclosure.h>
#include <geldebug.h>


/**
 * SECTION:gelcontext
 * @short_description: Class used to keep symbols and evaluate values.
 * @title: GelContext
 * @include: gel.h
 *
 * #GelContext is a class where symbols are stored and evaluated.
 */


struct _GelContext
{
    /*< private >*/
    GHashTable *symbols;
    /*< private >*/
    GelContext *outer;
};


static
void gel_value_array_to_string_transform(const GValue *src_value,
                                         GValue *dest_value)
{
    const GValueArray *array = (GValueArray*)g_value_get_boxed(src_value);

    GString *buffer = g_string_new("[");
    const guint n_values = array->n_values;
    if(n_values > 0)
    {
        const guint last = n_values - 1;
        const GValue *const array_values = array->values;
        register guint i;
        for(i = 0; i <= last; i++)
        {
            gchar *s = gel_value_to_string(array_values + i);
            g_string_append(buffer, s);
            if(i != last)
                g_string_append(buffer, " ");
            g_free(s);
        }
    }
    g_string_append(buffer, "]");

    g_value_take_string(dest_value, g_string_free(buffer, FALSE));
}


/**
 * gel_context_new:
 *
 * Creates a #GelContext, no outer context is assigned and the
 * default symbols are added with #gel_context_add_symbols.
 *
 * Returns: A new #GelContext, with no outer context assigned.
 *
 */
GelContext* gel_context_new(void)
{
    return gel_context_new_with_outer(NULL);
}


/**
 * gel_context_new_with_outer:
 * @outer: The outer context.
 *
 * Creates a #GelContext, using @outer as the outer context.
 * This method is used when invoking functions to have local variables.
 *
 * Returns: A new created #GelContext, with @outer as the outer context.
 *
 */
GelContext* gel_context_new_with_outer(GelContext *outer)
{
    static volatile gsize only_once = 0;
    if(g_once_init_enter(&only_once))
    {
        g_value_register_transform_func(
            G_TYPE_VALUE_ARRAY, G_TYPE_STRING,
            gel_value_array_to_string_transform);
        g_once_init_leave (&only_once, 1);
    }

    GelContext *self = g_slice_new0(GelContext);

    self->symbols = g_hash_table_new_full(
        g_str_hash, g_str_equal,
        (GDestroyNotify)g_free, (GDestroyNotify)gel_value_free);

    if((self->outer = outer) == NULL)
        gel_context_add_default_symbols(self);
    return self;
}


static
void gel_context_invoke_closure(GelContext *self, GClosure *closure,
                                guint n_values, const GValue *values,
                                GValue *dest_value)
{
    g_return_if_fail(self != NULL);
    g_return_if_fail(closure != NULL);
    g_return_if_fail(dest_value != NULL);

    if(gel_closure_is_gel(closure))
    {
        GValueArray *const array = g_value_array_new(n_values);
        register guint i;
        for(i = 0; i < n_values; i++)
        {
            GValue tmp_value = {0};
            g_value_array_append(array,
                gel_context_eval_value(self, values+i, &tmp_value));
            if(G_IS_VALUE(&tmp_value))
                g_value_unset(&tmp_value);
        }

        g_closure_invoke(
            closure, dest_value, n_values, array->values, self);
        g_value_array_free(array);
    }
    else
        g_closure_invoke(closure, dest_value, n_values , values, self);
}


static
void gel_context_invoke_type(GelContext *self, GType type,
                             guint n_values, const GValue *values,
                             GValue *dest_value)
{
    g_return_if_fail(self != NULL);
    g_return_if_fail(type != G_TYPE_INVALID);
    g_return_if_fail(dest_value != NULL);

    register guint i;
    for(i = 0; i < n_values; i++)
    {
        g_return_if_fail(G_VALUE_HOLDS_STRING(values+i));
        const gchar *var_name = g_value_get_string(values+i);
        GValue *var_value = gel_value_new_of_type(type);
        gel_context_add_symbol(self, var_name, var_value);
    }

    GValue count_value = {0};
    g_value_init(&count_value, G_TYPE_GTYPE);
    g_value_set_gtype(&count_value, type);
    gel_value_copy(&count_value, dest_value);
}


static
const GValue* gel_context_eval_array(GelContext *self, const GValueArray *array,
                                     GValue *dest_value)
{
    g_return_val_if_fail(self != NULL, NULL);
    g_return_val_if_fail(dest_value != NULL, NULL);
    g_return_val_if_fail(array != NULL, NULL);
    const guint array_n_values = array->n_values;
    g_return_val_if_fail(array_n_values > 0, NULL);

    const GValue *result_value = NULL;
    const GValue *const array_values = array->values;

    if(G_VALUE_HOLDS_STRING(array_values + 0))
    {
        const gchar *type_name = g_value_get_string(array_values + 0);
        GType type = g_type_from_name(type_name);
        if(type != G_TYPE_INVALID)
        {
            gel_context_invoke_type(self, type,
                array_n_values -1 , array_values + 1, dest_value);
            result_value = dest_value;
        }
    }

    if(result_value == NULL)
    {
        GValue tmp_value = {0};
        const GValue *first_value =
            gel_context_eval_value(self, array_values + 0, &tmp_value);

        if(G_VALUE_HOLDS(first_value, G_TYPE_CLOSURE))
        {
            GClosure *closure = (GClosure*)g_value_get_boxed(first_value);
            gel_context_invoke_closure(self, closure,
                array_n_values - 1, array_values + 1, dest_value);
            result_value = dest_value;
        }
        else
            gel_warning_value_not_of_type(__FUNCTION__,
                first_value, G_TYPE_CLOSURE);

        if(G_IS_VALUE(&tmp_value))
            g_value_unset(&tmp_value);
    }

    return result_value;
}


/**
 * gel_context_eval:
 * @self: context
 * @value: value to evaluate
 * @dest_value: destination value
 *
 * Evaluates @value, stores the result in @dest_value
 *
 * Returns: #TRUE if @dest_value was written, #FALSE otherwise.
 *
 */
gboolean gel_context_eval(GelContext *self, 
                          const GValue *value, GValue *dest_value)
{
    g_return_val_if_fail(self != NULL, FALSE);
    g_return_val_if_fail(value != NULL, FALSE);
    g_return_val_if_fail(dest_value != NULL, FALSE);

    const GValue *result_value =
        gel_context_eval_value(self, value, dest_value);
    if(G_IS_VALUE(result_value))
    {
        if(result_value != dest_value)
            gel_value_copy(result_value, dest_value);
        return TRUE;
    }
    return FALSE;
}


/**
 * gel_context_eval_value:
 * @self: context
 * @value: value to evaluate
 * @tmp_value: value where a result may be written.
 *
 * Evaluates the given value.
 * If @value matches a symbol, returns the corresponding value.
 * If @value is a literal (number or string), returns @value itself.
 * If not, then @value is evaluated and the result stored in @tmp_value.
 *
 * Returns: The value with the result.
 *
 */
const GValue* gel_context_eval_value(GelContext *self,
                                     const GValue *value, GValue *tmp_value)
{
    g_return_val_if_fail(self != NULL, NULL);
    g_return_val_if_fail(value != NULL, NULL);
    g_return_val_if_fail(tmp_value != NULL, NULL);

    const GValue *result_value = NULL;

    if(G_VALUE_HOLDS(value, G_TYPE_STRING))
    {
        const gchar *symbol_name = g_value_get_string(value);
        GValue *symbol_value = gel_context_find_symbol(self, symbol_name);
        if(symbol_value != NULL)
            result_value = symbol_value;
        else
            gel_warning_unknown_symbol(__FUNCTION__, symbol_name);
    }
    else
    if(G_VALUE_HOLDS(value, G_TYPE_VALUE_ARRAY))
    {
        GValueArray *array = (GValueArray*)g_value_get_boxed(value);
        result_value = gel_context_eval_array(self, array, tmp_value);
    }

    if(result_value == NULL)
        result_value = value;

    return result_value;
}


/**
 * gel_context_eval_params:
 * @self: context to use
 * @func: name of the invoker function, usually __FUNCTION__
 * @list: pointer to a list to keep temporary values
 * @format: string describing types to get
 * @n_values: pointer to the number of values in @values
 * @values: pointer to an array of values
 * @...: list of pointers to store the retrieved variables
 *
 * Convenience method to be used in implementation of new closures.
 *
 * Each character in @format indicates the type of variable you are parsing.
 * Posible formats are: asASIOCV
 *
 * <itemizedlist>
 *   <listitem><para>
 *     a: get a literal array (#GValueArray *).
 *   </para></listitem>
 *   <listitem><para>
 *     s: get a literal string (#gchararray).
 *   </para></listitem>
 *   <listitem><para>
 *     A: evaluate and get an array (#GValueArray *).
 *   </para></listitem>
 *   <listitem><para>
 *     S: evaluate and get a string (#gchararray).
 *   </para></listitem>
 *   <listitem><para>
 *     I: evaluate and get an integer (#glong).
 *   </para></listitem>
 *   <listitem><para>
 *     O: evaluate and get an object (#GObject *).
 *   </para></listitem>
 *   <listitem><para>
 *     C: evaluate and get a closure (#GClosure *).
 *   </para></listitem>
 *   <listitem><para>
 *     V: evaluate and get a value (#GValue *).
 *   </para></listitem>
 *   <listitem><para>
 *     *: Do not check for exact number or arguments.
 *   </para></listitem>
 * </itemizedlist>
 *
 * On success, @n_values will be decreased and @values repositioned
 * to match the first remaining argument,
 * @list must be disposed with #gel_value_list_free.
 *
 * On failure , @n_values and @values are left untouched.
 * 
 * Returns: #TRUE on sucess, #FALSE otherwise.
 *
 */
gboolean gel_context_eval_params(GelContext *self, const gchar *func,
                                 GList **list, const gchar *format,
                                 guint *n_values, const GValue **values, ...)
{
    g_return_val_if_fail(self != NULL, FALSE);
    g_return_val_if_fail(n_values != NULL, FALSE);
    g_return_val_if_fail(values != NULL, FALSE);

    guint n_args = 0;
    register guint i;
    for(i = 0; format[i] != 0; i++)
        if(strchr("asASIOCV", format[i]) != NULL)
            n_args++;

    gboolean exact = (strchr(format, '*') == NULL);

    if(exact && n_args != *n_values)
    {
        gel_warning_needs_n_arguments(func, n_args);
        return FALSE;
    }

    if(!exact && n_args > *n_values)
    {
        gel_warning_needs_at_least_n_arguments(func, n_args);
        return FALSE;
    }

    register gboolean parsed = TRUE;
    register gboolean pending = FALSE;
    va_list args;

    GList *o_list = *list;
    guint o_n_values = *n_values;
    const GValue *o_values = *values;

    va_start(args, values);
    while(*format != 0 && *n_values >= 0 && parsed && !pending)
    {
        GValue *value = NULL;
        const GValue *result_value = NULL;

        switch(*format)
        {
            case 'a':
                if(G_VALUE_HOLDS(*values, G_TYPE_VALUE_ARRAY))
                {
                    GValueArray **a = va_arg(args, GValueArray **);
                    *a = (GValueArray*)g_value_get_boxed(*values);
                }
                else
                {
                    gel_warning_value_not_of_type(func,
                        value, G_TYPE_VALUE_ARRAY);
                    parsed = FALSE;
                }
                break;
            case 's':
                if(G_VALUE_HOLDS_STRING(*values))
                {
                    const gchar **s = va_arg(args, const gchar **);
                    *s = g_value_get_string(*values);
                }
                else
                {
                    gel_warning_value_not_of_type(func,
                        value, G_TYPE_STRING);
                    parsed = FALSE;
                }
                break;
            case 'O':
                value = gel_value_new();
                result_value = gel_context_eval_value(self, *values, value);
                if(G_VALUE_HOLDS_OBJECT(result_value))
                {
                    GObject **obj = va_arg(args, GObject **);
                    *obj = (GObject*)g_value_get_object(result_value);
                }
                else
                {
                    gel_warning_value_not_of_type(func,
                        result_value, G_TYPE_OBJECT);
                    parsed = FALSE;
                }
                break;
            case 'A':
                value = gel_value_new();
                result_value = gel_context_eval_value(self, *values, value);
                if(G_VALUE_HOLDS(result_value, G_TYPE_VALUE_ARRAY))
                {
                    GValueArray **a = va_arg(args, GValueArray **);
                    *a = (GValueArray*)g_value_get_boxed(result_value);
                }
                else
                {
                    gel_warning_value_not_of_type(func,
                        result_value, G_TYPE_VALUE_ARRAY);
                    parsed = FALSE;
                }
                break;
            case 'S':
                value = gel_value_new();
                result_value = gel_context_eval_value(self, *values, value);
                if(G_VALUE_HOLDS_STRING(result_value))
                {
                    const gchar **s = va_arg(args, const gchar **);
                    *s = g_value_get_string(result_value);
                }
                else
                {
                    gel_warning_value_not_of_type(func,
                        result_value, G_TYPE_STRING);
                    parsed = FALSE;
                }
                break;
            case 'I':
                value = gel_value_new();
                result_value = gel_context_eval_value(self, *values, value);
                if(G_VALUE_HOLDS_LONG(result_value))
                {
                    glong *i = va_arg(args, glong *);
                    *i = g_value_get_long(result_value);
                }
                else
                {
                    gel_warning_value_not_of_type(func,
                        result_value, G_TYPE_LONG);
                    parsed = FALSE;
                }
                break;
            case 'C':
                value = gel_value_new();
                result_value = gel_context_eval_value(self, *values, value);
                if(G_VALUE_HOLDS(result_value, G_TYPE_CLOSURE))
                {
                    GClosure **closure = va_arg(args, GClosure **);
                    *closure = (GClosure*)g_value_get_boxed(result_value);
                }
                else
                {
                    gel_warning_value_not_of_type(func,
                        result_value, G_TYPE_STRING);
                    parsed = FALSE;
                }
                break;
            case 'V':
                value = gel_value_new();
                result_value = gel_context_eval_value(self, *values, value);
                if(G_IS_VALUE(result_value))
                {
                    const GValue **v = va_arg(args, const GValue **);
                    *v = result_value;
                }
                else
                {
                    gel_warning_value_not_of_type(func,
                        result_value, G_TYPE_VALUE);
                    parsed = FALSE;
                }
                break;
            case '*':
                pending = TRUE;
                break;
        }

        if(value != NULL)
        {
            if(parsed && result_value == value)
                *list = g_list_append(*list, value);
            else
                gel_value_free(value);
        }

        if(parsed && !pending)
        {
            format++;
            (*values)++;
            (*n_values)--;
        }
    }
    va_end(args);

    if(!parsed)
    {
        gel_value_list_free(*list);
        *list = o_list;
        *n_values = o_n_values;
        *values = o_values;
    }
    return parsed;
}


/**
 * gel_context_find_symbol:
 * @self: context
 * @name: name of the symbol to lookup
 *
 * If @context has a definition for @name, then returns its value.
 * If not, then it queries the outer context.
 * The returned value is owned by the context so it should not be freed.
 *
 * Returns: The value corresponding to @name, or #NULL if could not find it.
 */
GValue* gel_context_find_symbol(const GelContext *self, const gchar *name)
{
    g_return_val_if_fail(self != NULL, NULL);
    g_return_val_if_fail(name != NULL, NULL);

    GValue *symbol = (GValue*)g_hash_table_lookup(
        self->symbols, name);
    if(symbol == NULL && self->outer != NULL)
        symbol = gel_context_find_symbol(self->outer, name);
    return symbol;
}


/**
 * gel_context_add_symbol:
 * @self: context
 * @name: name of the symbol to add
 * @value: value of the symbol to add
 *
 * Adds a new symbol to @context with the name given by @name.
 * @self takes ownership of @value so it should not be freed or unset.
 */
void gel_context_add_symbol(GelContext *self, const gchar *name, GValue *value)
{
    g_return_if_fail(self != NULL);
    g_return_if_fail(name != NULL);
    g_return_if_fail(value != NULL);
    g_hash_table_insert(self->symbols, g_strdup(name), value);
}


/**
 * gel_context_add_object:
 * @self: context
 * @name: name of the symbol to add
 * @object: object to add
 *
 * A wrapper for #gel_context_add_symbol.
 * @self takes ownership of @object so it should not be unreffed.
 */
void gel_context_add_object(GelContext *self, const gchar *name,
                            GObject *object)
{
    g_return_if_fail(self != NULL);
    g_return_if_fail(name != NULL);
    g_return_if_fail(object != NULL);
    g_return_if_fail(G_IS_OBJECT(object));

    GValue *value = gel_value_new_of_type(G_OBJECT_TYPE(object));
    g_value_take_object(value, object);
    gel_context_add_symbol(self, name, value);
}

static
void gel_context_marshal(GClosure *closure, GValue *return_value,
                         guint n_param_values, const GValue *param_values,
                         gpointer invocation_hint, gpointer marshal_data)
{
    typedef void (*MarshalCallback)(gpointer data1, gpointer data2);
    register GCClosure *cc = (GCClosure*)closure;
    register MarshalCallback callback = (MarshalCallback)cc->callback;

    if(n_param_values > 1)
        callback(g_value_peek_pointer(param_values + 0), closure->data);
    else
        callback(closure->data, NULL);
}


/**
 * gel_context_add_callback:
 * @self: context
 * @name: name of the symbol to add
 * @callback: a #GelContextCallback to invoke
 * @user_data: extra data to pass to @callback
 *
 * A wrapper for #gel_context_add_symbol that calls @callback when
 * @self evaluates a call to a function named @name.
 */
void gel_context_add_callback(GelContext *self, const gchar *name,
                              GelContextCallback callback, gpointer user_data)
{
    g_return_if_fail(self != NULL);
    g_return_if_fail(name != NULL);
    g_return_if_fail(callback != NULL);

    GValue *value = gel_value_new_of_type(G_TYPE_CLOSURE);
    GClosure *closure = g_cclosure_new(G_CALLBACK(callback), user_data, NULL);
    g_closure_set_marshal(closure, (GClosureMarshal)gel_context_marshal);
    g_value_take_boxed(value, closure);
    gel_context_add_symbol(self, name, value);
}


/**
 * gel_context_remove_symbol:
 * @self: context
 * @name: name of the symbol to remove
 *
 * Removes the symbol gived by @name.
 *
 * Returns: #TRUE is the symbol was removed, #FALSE otherwise.
 */
gboolean gel_context_remove_symbol(GelContext *self, const gchar *name)
{
    g_return_val_if_fail(self != NULL, FALSE);
    g_return_val_if_fail(name != NULL, FALSE);
    return g_hash_table_remove(self->symbols, name);
}


/**
 * gel_context_get_outer:
 * @self: context
 *
 * Retrieves @self's outer context
 *
 * Returns: the outer context, or #NULL if @self is the outermost context.
 */
GelContext* gel_context_get_outer(const GelContext* self)
{
    g_return_val_if_fail(self != NULL, NULL);
    return self->outer;
}


/**
 * gel_context_free:
 * @self: context
 *
 * Frees resources allocated by @self
 *
 */
void gel_context_free(GelContext *self)
{
    g_hash_table_destroy(self->symbols);
    g_slice_free(GelContext, self);
}

