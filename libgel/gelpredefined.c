#include <girepository.h>
#include <gitypelib.h>

#include <gelcontext.h>
#include <gelcontextprivate.h>
#include <geldebug.h>
#include <gelvalue.h>
#include <gelvalueprivate.h>
#include <gelsymbol.h>
#include <gelclosure.h>


static
GClosure* new_closure(GelContext *context, const gchar *name,
                      guint n_vars, const GValue *var_values,
                      guint n_values, const GValue *values)
{
    gchar **vars = (gchar**)g_new0(gchar*, n_vars + 1);
    gchar *invalid_arg_name = NULL;

    guint i;
    for(i = 0; i < n_vars; i++)
    {
        const GValue *value = var_values + i;
        if(GEL_VALUE_HOLDS(value, GEL_TYPE_SYMBOL))
        {
            GelSymbol *symbol = (GelSymbol*)gel_value_get_boxed(value);
            vars[i] = g_strdup(gel_symbol_get_name(symbol));
        }
        else
        {
            invalid_arg_name = gel_value_to_string(value);
            break;
        }
    }

    GClosure *self;
    if(invalid_arg_name == NULL)
    {
        GValueArray *code = g_value_array_new(n_values);
        for(i = 0; i < n_values; i++)
            g_value_array_append(code, values + i);
        self = gel_closure_new(name, vars, code, context);
        g_closure_ref(self);
        g_closure_sink(self);
    }
    else
    {
        gel_warning_invalid_argument_name(__FUNCTION__, invalid_arg_name);
        g_free(invalid_arg_name);
        for(i = 0; i < n_vars; i++)
            if(vars[i] != NULL)
                g_free(vars[i]);
        g_free(vars);
        self = NULL;
    }

    return self;
}


static
void define_(GClosure *self, GValue *return_value,
             guint n_values, const GValue *values, GelContext *context)
{
    guint n_args = 2;
    if(n_values < n_args)
    {
        gel_warning_needs_at_least_n_arguments(__FUNCTION__, n_args);
        return;
    }

    GList *list = NULL;
    gchar *name = NULL;
    GValue *value = NULL;
    gboolean defined = FALSE;
    GClosure *closure = NULL;

    GType type = GEL_VALUE_TYPE(values + 0);

    if(type == GEL_TYPE_SYMBOL)
    {
        GValue *r_value = NULL;
        if(gel_context_eval_params(context, __FUNCTION__, &list,
                "sV", &n_values, &values, &name, &r_value))
        {
            if(gel_context_get_variable(context, name) == NULL)
                value = gel_value_dup(r_value);
            else
                defined = TRUE;
        }
    }
    else
    if(type == G_TYPE_VALUE_ARRAY)
    {
        GValueArray *array = NULL;
        if(gel_context_eval_params(context, __FUNCTION__, &list,
                "a*", &n_values, &values, &array))
        {
            guint array_n_values = array->n_values;
            const GValue *array_values = array->values;
            if(!gel_context_eval_params(context, __FUNCTION__, &list,
                    "s*", &array_n_values, &array_values, &name))
                type = G_TYPE_INVALID;
            else
                if(gel_context_lookup(context, name) == NULL)
                {
                    closure = new_closure(context, name,
                            array_n_values, array_values, n_values, values);
                    if(closure != NULL)
                    {
                        value = gel_value_new_of_type(G_TYPE_CLOSURE);
                        g_value_take_boxed(value, closure);
                    }
                }
                else
                    defined = FALSE;
        }
        else
            type = G_TYPE_INVALID;
    }

    if(type == G_TYPE_INVALID)
        g_warning("%s: Expected a symbol or array of symbols", __FUNCTION__);
    else
        if(defined)
            g_warning("%s: Symbol '%s' already exists", __FUNCTION__, name);
        else
            if(value != NULL)
            {
                gel_context_insert(context, name, value);
                if(closure != NULL)
                    gel_closure_bind(closure);
            }

    gel_value_list_free(list);
}


static
void lambda_(GClosure *self, GValue *return_value,
             guint n_values, const GValue *values, GelContext *context)
{
    GValueArray *array;
    GList *list = NULL;

    if(!gel_context_eval_params(context, __FUNCTION__, &list,
            "a*", &n_values, &values, &array))
        return;

    GClosure *closure = new_closure(context, "lambda",
            array->n_values, array->values, n_values, values);

    if(closure != NULL)
    {
        gel_closure_bind(closure);
        g_value_init(return_value, G_TYPE_CLOSURE);
        g_value_take_boxed(return_value, closure);
    }

    gel_value_list_free(list);
}


static
void let_(GClosure *self, GValue *return_value,
            guint n_values, const GValue *values, GelContext *context)
{
    guint n_args = 2;
    if(n_values < n_args)
    {
        gel_warning_needs_at_least_n_arguments(__FUNCTION__, n_args);
        return;
    }

    GList *list = NULL;
    gchar *name = NULL;
    GValue *body = NULL;
    
    GType type = GEL_VALUE_TYPE(values + 0);

    if(type == GEL_TYPE_SYMBOL)
    {
        if(gel_context_eval_params(context, __FUNCTION__, &list,
             "s*", &n_values, &values, &name));
    }

    GValueArray *bindings = NULL;
    if(gel_context_eval_params(context, __FUNCTION__, &list,
           "a*", &n_values, &values, &bindings))
    {
        guint bindings_n_values = bindings->n_values;
        const GValue *bindings_values = bindings->values;
        guint n_vars =  bindings_n_values;
        gchar **var_names = g_new0(gchar *, n_vars+1);
        GValue **var_values = g_new(GValue *, n_vars);
        guint i;
        for(i = 0; i < n_vars; i++)
        {
            gchar *var_name = NULL;
            GValue *var_value = NULL;
            if(gel_context_eval_params(context, __FUNCTION__, &list,
                "(sV)", &bindings_n_values, &bindings_values,
                &var_name, &var_value))
            {
                var_names[i] = g_strdup(var_name);
                var_values[i] = var_value;
            }
        }

        if(gel_context_eval_params(context, __FUNCTION__, &list,
            "v", &n_values, &values, &body))
        {
            GelContext *let_context = gel_context_new_with_outer(context);
            if(name != NULL)
            {
                GValueArray *code = g_value_array_new(1);
                g_value_array_append(code, body);
                GClosure *closure =
                    gel_closure_new(name, var_names, code, let_context);
                if(closure != NULL)
                {
                    GValue *value = gel_value_new_of_type(G_TYPE_CLOSURE);
                    g_value_take_boxed(value, closure);
                    gel_context_insert(let_context, name, value);
                    gel_closure_bind(closure);
                }
            }

            guint i;
            for(i = 0; i < n_vars; i++)
            {
                gel_context_insert(let_context,
                    var_names[i], gel_value_dup(var_values[i]));
            }

            gel_context_eval_into_value(let_context, body, return_value);
            g_free(var_values);
            gel_context_free(let_context);
        }
    }
    gel_value_list_free(list);
}


static
void begin_(GClosure *self, GValue *return_value,
            guint n_values, const GValue *values, GelContext *context)
{
    guint n_args = 1;
    if(n_values < n_args)
    {
        gel_warning_needs_at_least_n_arguments(__FUNCTION__, n_args);
        return;
    }

    guint last = n_values-1;
    guint i;
    for(i = 0; i <= last && gel_context_get_running(context); i++)
    {
        GValue tmp_value = {0};
        const GValue *value =
            gel_context_eval_into_value(context, values + i, &tmp_value);
        if(i == last && GEL_IS_VALUE(value))
            gel_value_copy(value, return_value);
        if(GEL_IS_VALUE(&tmp_value))
            g_value_unset(&tmp_value);
    }
}


static
void set_(GClosure *self, GValue *return_value,
          guint n_values, const GValue *values, GelContext *context)
{
    const gchar *symbol = NULL;
    GValue *value = NULL;
    GList *list = NULL;

     if(!gel_context_eval_params(context, __FUNCTION__, &list,
            "sV", &n_values, &values, &symbol, &value))
        return;

    GValue *symbol_value = gel_context_lookup(context, symbol);
    if(symbol_value != NULL)
        gel_value_copy(value, symbol_value);
    else
        gel_warning_unknown_symbol(__FUNCTION__, symbol);

    gel_value_list_free(list);
}


static
void get_(GClosure *self, GValue *return_value,
          guint n_values, const GValue *values, GelContext *context)
{
    guint n_args = 1;
    if(n_values != n_args)
    {
        gel_warning_needs_n_arguments(__FUNCTION__, n_args);
        return;
    }

    GList *list = NULL;
    gchar *name;
    if(gel_context_eval_params(context, __FUNCTION__, &list,
            "s", &n_values, &values, &name))
    {
        GelVariable *variable = gel_context_lookup_variable(context, name);
        if(variable != NULL)
        {
            GelSymbol *symbol =
                gel_symbol_new(name, gel_variable_ref(variable));
            g_value_init(return_value, GEL_TYPE_SYMBOL);
            g_value_take_boxed(return_value, symbol);
        }
    }
    gel_value_list_free(list);
}


static
void if_(GClosure *self, GValue *return_value,
         guint n_values, const GValue *values, GelContext *context)
{
    guint n_args = 2;
    if(n_values < n_args)
    {
        gel_warning_needs_at_least_n_arguments(__FUNCTION__, n_args);
        return;
    }

    GValue tmp_value = {0};
    const GValue *cond_value =
        gel_context_eval_into_value(context, values + 0, &tmp_value);

    if(gel_value_to_boolean(cond_value))
        gel_context_eval(context, values + 1, return_value);
    else
        if(n_values > 2)
            gel_context_eval(context, values + 2, return_value);

    if(GEL_IS_VALUE(&tmp_value))
        g_value_unset(&tmp_value);
}


static
void cond_(GClosure *self, GValue *return_value,
           guint n_values, const GValue *values, GelContext *context)
{
    GList *list = NULL;

    gboolean running = TRUE;
    while(n_values > 0 && running)
    {
        GValueArray *array = NULL;
        if(gel_context_eval_params(context, __FUNCTION__, &list,
                "a*", &n_values, &values, &array))
        {
            guint array_n_values = array->n_values;
            GValue *array_values = array->values;
            gboolean predicate_is_true = FALSE;

            GType type = GEL_VALUE_TYPE(array_values + 0);
            if(type == GEL_TYPE_SYMBOL)
            {
                GelSymbol *symbol =
                    (GelSymbol*)g_value_get_boxed(array_values + 0);
                array_n_values--;
                array_values++;
                const gchar *name = gel_symbol_get_name(symbol);
                if(g_strcmp0(name, "else") == 0)
                    predicate_is_true = TRUE;
                else
                    gel_warning_invalid_argument_name(__FUNCTION__, name);
            }

            GValue *cond_value = NULL;
            if(!predicate_is_true)
            {
                const GValue *array_values = array->values;
                guint array_n_values = array->n_values;
                if(!gel_context_eval_params(context, __FUNCTION__, &list,
                        "V*", &array_n_values, &array_values, &cond_value))
                    running = FALSE;
                else
                    predicate_is_true = gel_value_to_boolean(cond_value);
            }

            if(running && predicate_is_true)
            {
                if(array_n_values > 0)
                    begin_(self, return_value,
                        array_n_values, array_values, context);
                else
                    if(cond_value != NULL)
                        gel_value_copy(cond_value, return_value);
                running = FALSE;
            }
        }
        else
            running = FALSE;
    }

    gel_value_list_free(list);
}


static
void case_(GClosure *self, GValue *return_value,
            guint n_values, const GValue *values, GelContext *context)
{

    guint n_args = 2;
    if(n_values < n_args)
    {
        gel_warning_needs_at_least_n_arguments(__FUNCTION__, n_args);
        return;
    }

    GList *list = NULL;
    gboolean running = TRUE;

    GValue *probe_value = NULL;
    if(!gel_context_eval_params(context, __FUNCTION__, &list,
            "V*", &n_values, &values, &probe_value))
        return;

    while(n_values > 0 && running)
    {
        GValueArray *cases = NULL;
        if(gel_context_eval_params(context, __FUNCTION__, &list,
                "a*", &n_values, &values, &cases))
        {
            guint cases_n_values = cases->n_values;
            const GValue *cases_values = cases->values;
            if(cases_n_values > 0 && running)
            {
                gboolean are_equals = FALSE;
                GValueArray *tests = NULL;
                GType type = GEL_VALUE_TYPE(cases_values + 0);
                if(type == GEL_TYPE_SYMBOL)
                {
                    GelSymbol *symbol =
                        (GelSymbol*)gel_value_get_boxed(cases_values + 0);
                    cases_n_values--;
                    cases_values++;
                    const gchar *name = gel_symbol_get_name(symbol);
                    if(g_strcmp0(name, "else") == 0)
                        are_equals = TRUE;
                    else
                        gel_warning_invalid_argument_name(__FUNCTION__, name);
                }
                else
                if(gel_context_eval_params(context, __FUNCTION__, &list,
                    "a*", &cases_n_values, &cases_values, &tests))
                {
                    guint tests_n_values = tests->n_values;
                    const GValue *tests_values = tests->values;
                    while(tests_n_values > 0 && running)
                    {
                        GValue *test_value = 0;
                        if(gel_context_eval_params(context, __FUNCTION__,
                            &list, "v*", &tests_n_values, &tests_values,
                            &test_value))
                            if(gel_values_eq(probe_value, test_value))
                                are_equals = TRUE;
                    }
                }

                if(are_equals)
                {
                    begin_(self, return_value,
                        cases_n_values, cases_values, context);
                    running = FALSE;
                }
            }
        }
    }

    gel_value_list_free(list);
}


static
void while_(GClosure *self, GValue *return_value,
            guint n_values, const GValue *values, GelContext *context)
{
    guint n_args = 2;
    if(n_values < n_args)
    {
        gel_warning_needs_at_least_n_arguments(__FUNCTION__, n_args);
        return;
    }

    gboolean run = FALSE;
    GelContext *loop_context = gel_context_new_with_outer(context);
    while(gel_context_get_running(loop_context))
    {
        GValue tmp_value = {0};
        const GValue *cond_value =
            gel_context_eval_into_value(context, values + 0, &tmp_value);
        if(gel_value_to_boolean(cond_value))
        {
            run = TRUE;
            begin_(self, return_value, n_values-1, values+1, context);
        }
        else
        {
            gel_context_set_running(loop_context, FALSE);
            if(run == FALSE)
                gel_value_copy(cond_value, return_value);
        }
        if(GEL_IS_VALUE(&tmp_value))
            g_value_unset(&tmp_value);
    }
    gel_context_free(loop_context);
}


static
void for_(GClosure *self, GValue *return_value,
          guint n_values, const GValue *values, GelContext *context)
{
    const gchar *name;
    GValueArray *array;
    GList *list = NULL;

    if(!gel_context_eval_params(context, __FUNCTION__, &list,
            "sA*", &n_values, &values, &name, &array))
        return;

    guint last = array->n_values;
    const GValue *array_values = array->values;

    GelContext *loop_context = gel_context_new_with_outer(context);
    GValue *value = gel_value_new();
    gel_context_insert(loop_context, name, value);

    guint i;
    for(i = 0; i < last && gel_context_get_running(loop_context); i++)
    {
        gel_value_copy(array_values + i, value);
        begin_(self, return_value, n_values, values, loop_context);
        g_value_unset(value);
    }
    gel_context_free(loop_context);

    gel_value_list_free(list);
}


static
void break_(GClosure *self, GValue *return_value,
            guint n_values, const GValue *values, GelContext *context)
{
    GelContext *i;
    for(i = context; i != NULL; i = gel_context_get_outer(i))
        if(gel_context_get_running(i))
        {
            gel_context_set_running(i, FALSE);
            break;
        }
}


static
void print_(GClosure *self, GValue *return_value,
            guint n_values, const GValue *values, GelContext *context)
{
    guint n_args = 1;
    if(n_values < n_args)
    {
        gel_warning_needs_at_least_n_arguments(__FUNCTION__, n_args);
        return;
    }

    guint last = n_values - 1;
    guint i;
    for(i = 0; i <= last; i++)
    {
        GValue tmp_value = {0};
        const GValue *value =
            gel_context_eval_into_value(context, values + i, &tmp_value);
        if(GEL_IS_VALUE(value))
        {
            gchar *value_string = gel_value_to_string(value);
            g_print("%s", value_string);
            g_free(value_string);
        }
        if(GEL_IS_VALUE(&tmp_value))
            g_value_unset(&tmp_value);
    }
    g_print("\n");
}


static
void arithmetic(GClosure *self, GValue *return_value,
                guint n_values, const GValue *values,
                GelContext *context, GelValuesArithmetic values_function)
{
    guint n_args = 2;
    if(n_values < n_args)
    {
        gel_warning_needs_at_least_n_arguments(__FUNCTION__, n_args);
        return;
    }

    GValue tmp1 = {0};
    GValue tmp2 = {0};
    GValue tmp3 = {0};
    GValue *v2 = &tmp3;

    gboolean running = values_function(
            gel_context_eval_into_value(context, values + 0, &tmp1),
            gel_context_eval_into_value(context, values + 1, &tmp2),
            &tmp3);

    if(GEL_IS_VALUE(&tmp1))
        g_value_unset(&tmp1);
    if(GEL_IS_VALUE(&tmp2))
        g_value_unset(&tmp2);

    guint i;
    for(i = 2; i < n_values && running; i++)
    {
        GValue *v1;

        if(i%2 == 0)
            v1 = &tmp3, v2 = &tmp2;
        else
            v2 = &tmp3, v1 = &tmp2;

        running = values_function(
            v1, gel_context_eval_into_value(context, values + i, &tmp1), v2);

        if(GEL_IS_VALUE(v1))
            g_value_unset(v1);
        if(GEL_IS_VALUE(&tmp1))
            g_value_unset(&tmp1);
    }

    if(running)
        gel_value_copy(v2, return_value);
    if(GEL_IS_VALUE(v2))
        g_value_unset(v2);
}


static
void add_(GClosure *self, GValue *return_value,
          guint n_values, const GValue *values, GelContext *context)
{
    arithmetic(self, return_value, n_values, values, context, gel_values_add);
}


static
void sub_(GClosure *self, GValue *return_value,
          guint n_values, const GValue *values, GelContext *context)
{
    arithmetic(self, return_value, n_values, values, context, gel_values_sub);
}


static
void mul_(GClosure *self, GValue *return_value,
          guint n_values, const GValue *values, GelContext *context)
{
    arithmetic(self, return_value, n_values, values, context, gel_values_mul);
}


static
void div_(GClosure *self, GValue *return_value,
          guint n_values, const GValue *values, GelContext *context)
{
    arithmetic(self, return_value, n_values, values, context, gel_values_div);
}


static
void mod_(GClosure *self, GValue *return_value,
          guint n_values, const GValue *values, GelContext *context)
{
    arithmetic(self, return_value, n_values, values, context, gel_values_mod);
}


static
void logic(GClosure *self, GValue *return_value,
           guint n_values, const GValue *values,
           GelContext *context, GelValuesLogic values_function)
{
    guint n_args = 2;
    if(n_values < n_args)
    {
        gel_warning_needs_at_least_n_arguments(__FUNCTION__, n_args);
        return;
    }

    gboolean result = TRUE;
    guint last = n_values - 1;

    guint i;
    for(i = 0; i < last && result; i++)
    {
        GValue tmp1 = {0};
        GValue tmp2 = {0};
        const GValue *i_value = values + i;
        result = values_function(
            gel_context_eval_into_value(context, i_value, &tmp1),
            gel_context_eval_into_value(context, i_value+1, &tmp2));

        if(GEL_IS_VALUE(&tmp1))
            g_value_unset(&tmp1);
        if(GEL_IS_VALUE(&tmp2))
            g_value_unset(&tmp2);
    }

    g_value_init(return_value, G_TYPE_BOOLEAN);
    gel_value_set_boolean(return_value, result);
}


static
void and_(GClosure *self, GValue *return_value,
          guint n_values, const GValue *values, GelContext *context)
{
    guint n_args = 1;
    if(n_values < n_args)
    {
        gel_warning_needs_at_least_n_arguments(__FUNCTION__, n_args);
        return;
    }

    gboolean result = TRUE;
    guint last = n_values - 1;
    guint i;
    for(i = 0; i <= last && result == TRUE; i++)
    {
        GValue tmp_value = {0};
        const GValue *value =
            gel_context_eval_into_value(context, values + i, &tmp_value);
        result = gel_value_to_boolean(value);
        if(!result || i == last)
            gel_value_copy(value, return_value);
        if(GEL_IS_VALUE(&tmp_value))
            g_value_unset(&tmp_value);
    }
}


static
void or_(GClosure *self, GValue *return_value,
         guint n_values, const GValue *values, GelContext *context)
{
    guint n_args = 1;
    if(n_values < n_args)
    {
        gel_warning_needs_at_least_n_arguments(__FUNCTION__, n_args);
        return;
    }

    guint last =  n_values - 1;
    gboolean result = FALSE;
    guint i;
    for(i = 0; i <= last && result == FALSE; i++)
    {
        GValue tmp_value = {0};
        const GValue *value =
            gel_context_eval_into_value(context, values + i, &tmp_value);
        result = gel_value_to_boolean(value);
        if(result || i == last)
            gel_value_copy(value, return_value);
        if(GEL_IS_VALUE(&tmp_value))
            g_value_unset(&tmp_value);
    }
}


static
void gt_(GClosure *self, GValue *return_value,
         guint n_values, const GValue *values, GelContext *context)
{
    logic(self, return_value, n_values, values, context, gel_values_gt);
}


static
void ge_(GClosure *self, GValue *return_value,
         guint n_values, const GValue *values, GelContext *context)
{
    logic(self, return_value, n_values, values, context, gel_values_ge);
}


static
void eq_(GClosure *self, GValue *return_value,
         guint n_values, const GValue *values, GelContext *context)
{
    logic(self, return_value, n_values, values, context, gel_values_eq);
}


static
void le_(GClosure *self, GValue *return_value,
         guint n_values, const GValue *values, GelContext *context)
{
    logic(self, return_value, n_values, values, context, gel_values_le);
}


static
void lt_(GClosure *self, GValue *return_value,
         guint n_values, const GValue *values, GelContext *context)
{
    logic(self, return_value, n_values, values, context, gel_values_lt);
}


static
void ne_(GClosure *self, GValue *return_value,
         guint n_values, const GValue *values, GelContext *context)
{
    logic(self, return_value, n_values, values, context, gel_values_ne);
}


static
void array_(GClosure *self, GValue *return_value,
            guint n_values, const GValue *values, GelContext *context)
{
    GValueArray *array = g_value_array_new(n_values);
    guint i;
    for(i = 0; i < n_values; i++)
    {
        GValue tmp_value = {0};
        const GValue *value =
            gel_context_eval_into_value(context, values + i, &tmp_value);
        g_value_array_append(array, value);
        if(GEL_IS_VALUE(&tmp_value))
            g_value_unset(&tmp_value);
    }

    g_value_init(return_value, G_TYPE_VALUE_ARRAY);
    g_value_take_boxed(return_value, array);
}


static
void array_append_(GClosure *self, GValue *return_value,
                   guint n_values, const GValue *values, GelContext *context)
{
    GList *list = NULL;
    GValueArray *array = NULL;

    if(!gel_context_eval_params(context, __FUNCTION__, &list,
            "A*", &n_values, &values, &array))
        return;

    guint i;
    for(i = 0; i < n_values; i++)
    {
        GValue tmp = {0};
        const GValue *value =
            gel_context_eval_into_value(context,values + i, &tmp);
        g_value_array_append(array, value);
        if(GEL_IS_VALUE(&tmp))
            g_value_unset(&tmp);
    }

    gel_value_list_free(list);
}


static
void array_get_(GClosure *self, GValue *return_value,
                guint n_values, const GValue *values, GelContext *context)
{
    GList *list = NULL;
    GValueArray *array = NULL;
    glong index = 0;

    if(!gel_context_eval_params(context, __FUNCTION__, &list,
            "AI", &n_values, &values, &array, &index))
        return;

    guint array_n_values = array->n_values;
    if(index < 0)
        index += array_n_values;

    if(index < 0 || index >= array_n_values)
    {
        gel_warning_index_out_of_bounds(__FUNCTION__);
        return;
    }

    gel_value_copy(array->values + index, return_value);
    gel_value_list_free(list);
}


static
void array_set_(GClosure *self, GValue *return_value,
                guint n_values, const GValue *values, GelContext *context)
{
    GList *list = NULL;
    GValueArray *array = NULL;
    glong index = 0;
    GValue *value = NULL;

    if(!gel_context_eval_params(context, __FUNCTION__, &list,
            "AIV", &n_values, &values, &array, &index, &value))
        return;

    guint array_n_values = array->n_values;
    if(index < 0)
        index += array_n_values;

    if(index < 0 || index >= array_n_values)
    {
        gel_warning_index_out_of_bounds(__FUNCTION__);
        return;
    }

    gel_value_copy(value, array->values + index);
    gel_value_list_free(list);
}


static
void array_remove_(GClosure *self, GValue *return_value,
                   guint n_values, const GValue *values, GelContext *context)
{
    GList *list = NULL;
    GValueArray *array = NULL;
    glong index = 0;

    if(!gel_context_eval_params(context, __FUNCTION__, &list,
            "AI", &n_values, &values, &array, &index))
        return;

    guint array_n_values = array->n_values;
    if(index < 0)
        index += array_n_values;

    if(index < 0 || index >= array_n_values)
    {
        gel_warning_index_out_of_bounds(__FUNCTION__);
        return;
    }

    g_value_array_remove(array, index);
    gel_value_list_free(list);
}


static
void array_size_(GClosure *self, GValue *return_value,
                 guint n_values, const GValue *values, GelContext *context)
{
    GList *list = NULL;
    GValueArray *array = NULL;

    if(!gel_context_eval_params(context, __FUNCTION__, &list,
            "A", &n_values, &values, &array))
        return;

    g_value_init(return_value, G_TYPE_UINT);
    gel_value_set_uint(return_value, array->n_values);
    gel_value_list_free(list);
}


static
void range_(GClosure *self, GValue *return_value,
            guint n_values, const GValue *values, GelContext *context)
{
    GList *list = NULL;
    glong first = 0;
    glong last = 0;

    if(!gel_context_eval_params(context, __FUNCTION__, &list,
            "II", &n_values, &values, &first, &last))
        return;

    GValueArray *array = g_value_array_new(ABS(last-first) + 1);
    GValue value = {0};
    g_value_init(&value, G_TYPE_LONG);

    glong i;
    if(last > first)
        for(i = first; i <= last; i++)
        {
            gel_value_set_long(&value, i);
            g_value_array_append(array, &value);
        }
    else
        for(i = first; i >= last; i--)
        {
            gel_value_set_long(&value, i);
            g_value_array_append(array, &value);
        }

    g_value_unset(&value);
    g_value_init(return_value, G_TYPE_VALUE_ARRAY);
    g_value_take_boxed(return_value, array);
}


static
void find_(GClosure *self, GValue *return_value,
           guint n_values, const GValue *values, GelContext *context)
{
    GList *list = NULL;
    GValueArray *array = NULL;
    GClosure *closure = NULL;

    if(!gel_context_eval_params(context, __FUNCTION__, &list,
            "AC", &n_values, &values, &array, &closure))
        return;

    const GValue *array_values = array->values;
    glong result = -1;
    guint i;
    for(i = 0; i < array->n_values && result == -1; i++)
    {
        GValue value = {0};
        g_closure_invoke(closure, &value, 1, array_values + i, context);
        if(gel_value_to_boolean(&value))
            result = i;
        g_value_unset(&value);
    }

    g_value_init(return_value, G_TYPE_LONG);
    gel_value_set_long(return_value, result);
    gel_value_list_free(list);
}


static
void filter_(GClosure *self, GValue *return_value,
             guint n_values, const GValue *values, GelContext *context)
{
    GList *list = NULL;
    GClosure *closure = NULL;
    GValueArray *array = NULL;

    if(!gel_context_eval_params(context, __FUNCTION__, &list,
            "CA", &n_values, &values, &closure, &array))
        return;

    GValueArray *result_array = g_value_array_new(array->n_values);

    guint i;
    for(i = 0; i < array->n_values; i++)
    {
        GValue tmp_value = {0};
        g_closure_invoke(closure, &tmp_value, 1, array->values + i, context);
        if(gel_value_to_boolean(&tmp_value))
            g_value_array_append(result_array, array->values + i);
        g_value_unset(&tmp_value);
    }

    g_value_init(return_value, G_TYPE_VALUE_ARRAY);
    g_value_take_boxed(return_value, result_array);
    gel_value_list_free(list);
}


static
void apply_(GClosure *self, GValue *return_value,
            guint n_values, const GValue *values, GelContext *context)
{
    GList *list = NULL;
    GClosure *closure = NULL;
    GValueArray *array = NULL;

    if(!gel_context_eval_params(context, __FUNCTION__, &list,
            "CA", &n_values, &values, &closure, &array))
        return;

    g_closure_invoke(closure, return_value,
        array->n_values, array->values, context);
    gel_value_list_free(list);
}


static
void zip_(GClosure *self, GValue *return_value,
          guint n_values, const GValue *values, GelContext *context)
{
    GList *list = NULL;

    guint n_arrays = n_values;
    GValueArray **arrays = g_new0(GValueArray*, n_values);

    guint i;
    for(i = 0; i < n_arrays; i++)
        if(!gel_context_eval_params(context, __FUNCTION__, &list,
                "A*", &n_values, &values, arrays + i))
        break;

    if(i == n_arrays)
    {
        guint min_n_values = arrays[0]->n_values;
        for(i = 1; i < n_arrays; i++)
        {
            guint i_n_values = arrays[i]->n_values;
            if(i_n_values < min_n_values)
                min_n_values = i_n_values;
        }

        GValueArray *result_array = g_value_array_new(min_n_values);
        GValue *result_array_values = result_array->values;
        result_array->n_values = min_n_values;

        for(i = 0; i < min_n_values; i++)
        {
            GValueArray *array = g_value_array_new(n_arrays);
            guint j;
            for(j = 0; j < n_arrays; j++)
                g_value_array_append(array, arrays[j]->values + i);
            g_value_init(result_array_values + i, G_TYPE_VALUE_ARRAY);
            g_value_take_boxed(result_array_values + i, array);
        }
        g_value_init(return_value, G_TYPE_VALUE_ARRAY);
        g_value_take_boxed(return_value, result_array);
    }
    else
        g_warning("%s: Argument %u is not an array", __FUNCTION__, i);

    g_free(arrays);
    gel_value_list_free(list);
}


static
void map_(GClosure *self, GValue *return_value,
          guint n_values, const GValue *values, GelContext *context)
{
    GList *list = NULL;
    GClosure *closure = NULL;
    GValueArray *array = NULL;

    if(!gel_context_eval_params(context, __FUNCTION__, &list,
            "CA", &n_values, &values, &closure, &array))
        return;

    const GValue *array_values = array->values;
    const guint array_n_values = array->n_values;

    GValueArray *result_array = g_value_array_new(array_n_values);
    result_array->n_values = array_n_values;
    GValue *result_array_values = result_array->values;

    guint i;
    for(i = 0; i < array_n_values; i++)
        g_closure_invoke(closure,
            result_array_values + i, 1, array_values + i, context);

    g_value_init(return_value, G_TYPE_VALUE_ARRAY);
    g_value_take_boxed(return_value, result_array);
    gel_value_list_free(list);
}


static guint
gel_value_hash(const GValue *value)
{
    GType type = GEL_VALUE_TYPE(value);
    switch(type)
    {
        case G_TYPE_LONG:
        {
            gint i = (gint)gel_value_get_long(value);
            return g_int_hash(&i);
        }
        case G_TYPE_DOUBLE:
        {
            gdouble d = gel_value_get_double(value);
            return g_double_hash(&d);
        }
        case G_TYPE_STRING:
        {
            gchar *s = gel_value_get_string(value);
            return g_str_hash(s);
        }
        default:
        {
            gchar *s = gel_value_to_string(value);
            guint hash = g_str_hash(s);
            g_free(s);
            return hash;
        }
    }
}


static
void hash_(GClosure *self, GValue *return_value,
           guint n_values, const GValue *values, GelContext *context)
{
    GList *list = NULL;
    GHashTable *hash = g_hash_table_new_full(
            (GHashFunc)gel_value_hash, (GEqualFunc)gel_values_eq,
            (GDestroyNotify)gel_value_free, (GDestroyNotify)gel_value_free);

    while(n_values > 0)
    {
        GValue *key = NULL;
        GValue *value = NULL;
        gel_context_eval_params(context, __FUNCTION__, &list,
                "(VV)*", &n_values, &values, &key, &value);
        g_hash_table_insert(hash, gel_value_dup(key), gel_value_dup(value));
    }

    g_value_init(return_value, G_TYPE_HASH_TABLE);
    g_value_take_boxed(return_value, hash);
    gel_value_list_free(list);
}


static
void hash_get_(GClosure *self, GValue *return_value,
               guint n_values, const GValue *values, GelContext *context)
{
    GList *list = NULL;
    GHashTable *hash = NULL;
    GValue *key = NULL;
    if(!gel_context_eval_params(context, __FUNCTION__, &list,
            "HV", &n_values, &values, &hash, &key))
        return;

    GValue *value = g_hash_table_lookup(hash, key);
    if(value != NULL)
        gel_value_copy(value, return_value);

    gel_value_list_free(list);
}


static
void hash_set_(GClosure *self, GValue *return_value,
               guint n_values, const GValue *values, GelContext *context)
{
    GList *list = NULL;
    GHashTable *hash = NULL;
    GValue *key = NULL;
    GValue *value = NULL;

    if(!gel_context_eval_params(context, __FUNCTION__, &list,
            "HVV", &n_values, &values, &hash, &key, &value))
        return;
    g_hash_table_insert(hash, gel_value_dup(key), gel_value_dup(value));

    gel_value_list_free(list);
}


static
void hash_remove_(GClosure *self, GValue *return_value,
                  guint n_values, const GValue *values, GelContext *context)
{
    GList *list = NULL;
    GHashTable *hash = NULL;
    GValue *key = NULL;

    if(!gel_context_eval_params(context, __FUNCTION__, &list,
            "HV", &n_values, &values, &hash, &key))
        return;
    g_hash_table_remove(hash, key);

    gel_value_list_free(list);
}


static
void hash_size_(GClosure *self, GValue *return_value,
                guint n_values, const GValue *values, GelContext *context)
{
    GList *list = NULL;
    GHashTable *hash = NULL;

    if(!gel_context_eval_params(context, __FUNCTION__, &list,
            "H", &n_values, &values, &hash))
        return;

    g_value_init(return_value, G_TYPE_UINT);
    gel_value_set_uint(return_value, g_hash_table_size(hash));
    gel_value_list_free(list);
}


static
void hash_keys_(GClosure *self, GValue *return_value,
                guint n_values, const GValue *values, GelContext *context)
{
    GList *list = NULL;
    GHashTable *hash = NULL;

    if(gel_context_eval_params(context, __FUNCTION__, &list,
            "H", &n_values, &values, &hash))
    {
        guint size = g_hash_table_size(hash);
        GValueArray *array = g_value_array_new(size);
        GList *keys = g_hash_table_get_keys(hash);
        GList *iter;
        for(iter = keys; iter != NULL; iter = g_list_next(iter))
            g_value_array_append(array, (GValue*)iter->data);
        g_value_init(return_value, G_TYPE_VALUE_ARRAY);
        g_value_take_boxed(return_value, array);
        g_list_free(keys);
    }

    gel_value_list_free(list);
}


static
void require_(GClosure *self, GValue *return_value,
              guint n_values, const GValue *values, GelContext *context)
{
    const gchar *namespace_ = NULL;
    const gchar *version = NULL;
    GList *list = NULL;
    gboolean result = FALSE;

     if(!gel_context_eval_params(context, __FUNCTION__, &list,
            "sS", &n_values, &values, &namespace_, &version))
        return;

    void *typelib = g_irepository_require(NULL, namespace_, version, 0, NULL);

    if(typelib != NULL)
    {
        guint n = g_irepository_get_n_infos (NULL, namespace_);
        guint i;
        for(i = 0; i < n; i++)
        {
            GIBaseInfo *info = g_irepository_get_info (NULL, namespace_, i);
            GIInfoType info_type = g_base_info_get_type(info);
            switch(info_type)
            {
                case GI_INFO_TYPE_OBJECT:
                    g_registered_type_info_get_g_type(info);
                    break;
                default:
                    break;
            }
            g_base_info_unref(info);
        }

        result = TRUE;
    }

    g_value_init(return_value, G_TYPE_BOOLEAN);
    gel_value_set_boolean(return_value, result);

    gel_value_list_free(list);
}


static
void object_new(GClosure *self, GValue *return_value,
                guint n_values, const GValue *values, GelContext *context)
{
    const gchar *type_name = NULL;
    GList *list = NULL;

    if(!gel_context_eval_params(context, __FUNCTION__, &list,
            "S", &n_values, &values, &type_name))
        return;

    GType type = g_type_from_name(type_name);
    if(type != G_TYPE_INVALID)
        if(G_TYPE_IS_INSTANTIATABLE(type))
        {
            GObject *new_object = (GObject*)g_object_new(type, NULL);
            if(G_IS_INITIALLY_UNOWNED(new_object))
                g_object_ref_sink(new_object);

            g_value_init(return_value, type);
            g_value_take_object(return_value, new_object);
        }
        else
            gel_warning_type_not_instantiatable(__FUNCTION__, type);
    else
        gel_warning_type_name_invalid(__FUNCTION__, type_name);

    gel_value_list_free(list);
}


static
void object_get(GClosure *self, GValue *return_value,
                guint n_values, const GValue *values, GelContext *context)
{
    GObject *object = NULL;
    const gchar *prop_name = NULL;
    GList *list = NULL;

    if(!gel_context_eval_params(context, __FUNCTION__, &list,
            "OS", &n_values, &values, &object, &prop_name))
        return;

    if(G_IS_OBJECT(object))
    {
        GParamSpec *prop_spec =
            g_object_class_find_property(G_OBJECT_GET_CLASS(object), prop_name);
        if(prop_spec != NULL)
        {
            g_value_init(return_value, prop_spec->value_type);
            g_object_get_property(object, prop_name, return_value);
        }
        else
            gel_warning_no_such_property(__FUNCTION__, prop_name);
    }

    gel_value_list_free(list);
}


static
void object_set(GClosure *self, GValue *return_value,
                guint n_values, const GValue *values, GelContext *context)
{
    GObject *object = NULL;
    gchar *prop_name = NULL;
    GValue *value = NULL;
    GList *list = NULL;

    if(!gel_context_eval_params(context, __FUNCTION__, &list,
            "OSV", &n_values, &values, &object, &prop_name, &value))
        return;

    if(G_IS_OBJECT(object))
    {
        GParamSpec *prop_spec =
            g_object_class_find_property(G_OBJECT_GET_CLASS(object), prop_name);
        if(prop_spec != NULL)
        {
            GValue result_value = {0};
            g_value_init(&result_value, prop_spec->value_type);

            if(g_value_transform(value, &result_value))
            {
                g_object_set_property(object, prop_name, &result_value);
                g_value_unset(&result_value);
            }
            else
                gel_warning_invalid_value_for_property(__FUNCTION__,
                    value, prop_spec);
        }
        else
            gel_warning_no_such_property(__FUNCTION__, prop_name);
    }

    gel_value_list_free(list);
}


static
void object_connect(GClosure *self, GValue *return_value,
                    guint n_values, const GValue *values, GelContext *context)
{
    GObject *object = NULL;
    gchar *signal = NULL;
    GClosure *callback = NULL;
    GList *list = NULL;

    if(!gel_context_eval_params(context, __FUNCTION__, &list,
            "OSC", &n_values, &values, &object, &signal, &callback))
        return;

    if(G_IS_OBJECT(object))
    {
        g_value_init(return_value, G_TYPE_UINT);
        gel_value_set_uint(return_value,
            g_signal_connect_closure(object,
                signal, g_closure_ref(callback), FALSE));
    }
    else
        gel_warning_value_not_of_type(__FUNCTION__, values + 0, G_TYPE_OBJECT);

    gel_value_list_free(list);
}


#define CLOSURE_NAME(N, S) {N, (GClosureMarshal)S}
#define CLOSURE(S) {#S, (GClosureMarshal)S##_}


static
GHashTable* gel_make_default_symbols(void)
{
    struct {const gchar *name; GClosureMarshal marshal;} *c, closures[] =
    {
        /* definitions */
        CLOSURE(define),
        CLOSURE(lambda),

        /* blocks */
        CLOSURE(let),
        CLOSURE(begin),

        /* imperative */
        CLOSURE_NAME("set!", set_),
        CLOSURE_NAME("get&", get_),

        /* conditional */
        CLOSURE(if),
        CLOSURE(cond),
        CLOSURE(case),

        /* loop */
        CLOSURE(while),
        CLOSURE(for),
        CLOSURE(break),

        /* output */
        CLOSURE(print),

        /* arithmetic */
        CLOSURE_NAME("+", add_),
        CLOSURE_NAME("-", sub_),
        CLOSURE_NAME("*", mul_),
        CLOSURE_NAME("/", div_),
        CLOSURE_NAME("%", mod_),

        /* logic */
        CLOSURE(and),
        CLOSURE(or),
        CLOSURE_NAME(">", gt_),
        CLOSURE_NAME(">=", ge_),
        CLOSURE_NAME("=", eq_),
        CLOSURE_NAME("<", lt_),
        CLOSURE_NAME("<=", le_),
        CLOSURE_NAME("!=", ne_),

        /* array */
        CLOSURE(array),
        CLOSURE_NAME("array-append!", array_append_),
        CLOSURE_NAME("array-get", array_get_),
        CLOSURE_NAME("array-set!", array_set_),
        CLOSURE_NAME("array-remove!", array_remove_),
        CLOSURE_NAME("array-size", array_size_),

        /* array management */
        CLOSURE(range),
        CLOSURE(find),
        CLOSURE(filter),
        CLOSURE(apply),
        CLOSURE(zip),
        CLOSURE(map),

        /* hash */
        CLOSURE(hash),
        CLOSURE_NAME("hash-get", hash_get_),
        CLOSURE_NAME("hash-set!", hash_set_),
        CLOSURE_NAME("hash-remove!", hash_remove_),
        CLOSURE_NAME("hash-size", hash_size_),
        CLOSURE_NAME("hash-keys", hash_keys_),

        /* introspection */
        CLOSURE(require),

        /* objects */
        CLOSURE_NAME("object-new", object_new),
        CLOSURE_NAME("object-get", object_get),
        CLOSURE_NAME("object-set", object_set),
        CLOSURE_NAME("object-connect", object_connect),

        {NULL,NULL}
    };

    GValue *value;
    GHashTable *symbols = g_hash_table_new_full(
            g_str_hash, g_str_equal,
            (GDestroyNotify)g_free, (GDestroyNotify)gel_value_free);

    for(c = closures; c->name != NULL; c++)
    {
        value = gel_value_new_of_type(G_TYPE_CLOSURE);
        GClosure *closure = gel_closure_new_native(c->name, c->marshal);
        g_value_set_boxed(value, closure);
        g_hash_table_insert(symbols, g_strdup(c->name), value);
    }

    value = gel_value_new_of_type(G_TYPE_BOOLEAN);
    gel_value_set_boolean(value, TRUE);
    g_hash_table_insert(symbols, g_strdup("#t"), value);

    value = gel_value_new_of_type(G_TYPE_BOOLEAN);
    gel_value_set_boolean(value, FALSE);
    g_hash_table_insert(symbols, g_strdup("#f"), value);

    value = gel_value_new_of_type(G_TYPE_POINTER);
    gel_value_set_pointer(value, NULL);
    g_hash_table_insert(symbols, g_strdup("NULL"), value);

    return symbols;
}

/**
 * gel_value_lookup_predefined:
 * @name: name of the predefined value to lookup
 *
 * Lookups the table of predefined values and retrieves
 * the value that correspond to @name.
 *
 * Returns: The #GValue that correspond to @name
 */
GValue *gel_value_lookup_predefined(const gchar *name)
{
    static GHashTable *symbols = NULL;
    static volatile gsize once = 0;
    if(g_once_init_enter(&once))
    {
        symbols = gel_make_default_symbols();
        g_once_init_leave(&once, 1);
    }
    return (GValue*)g_hash_table_lookup(symbols, name);
}

