[CCode (cheader_filename = "gel.h")]
namespace Gel {

    [CCode (type_id = "GEL_CONTEXT_TYPE")]
    [Compact]
    public class Context {
        public Context();
        public Context.with_outer(Gel.Context outer);
        public unowned GLib.Value lookup_symbol(string name);
        public unowned Context outer {get;}
        public bool running {get; set;}
        public void insert_symbol(string name, owned GLib.Value value);
        public void insert_object(string name, owned GLib.Object object);
        public void insert_function(string name, GLib.Func function);
        public void insert_default_symbols();
        public bool remove_symbol(string name);
        public bool eval(GLib.Value value, out GLib.Value dest_value);
        public unowned GLib.Value eval_value(GLib.Value value, out GLib.Value tmp_value);
        GLib.Closure closure_new([CCode (array_length=false, array_null_terminated=true)] owned string[] args, owned GLib.ValueArray code);
    }

    GLib.ValueArray parse_file(string file) throws GLib.FileError;
    GLib.ValueArray parse_text(string text, uint text_len);

    namespace Value {
        string to_string(GLib.Value value);
        bool to_boolean(GLib.Value value);
        bool copy(GLib.Value src_value, out GLib.Value dest_value);
    }

    namespace ValueList {
        void free(GLib.List<GLib.Value> value_list);
    }

    namespace Values {
        bool add(GLib.Value v1, GLib.Value v2, out GLib.Value dest_value);
        bool sub(GLib.Value v1, GLib.Value v2, out GLib.Value dest_value);
        bool mul(GLib.Value v1, GLib.Value v2, out GLib.Value dest_Value);
        bool div(GLib.Value v1, GLib.Value v2, out GLib.Value dest_value);
        bool mod(GLib.Value v1, GLib.Value v2, out GLib.Value dest_value);

        int cmp(GLib.Value v1, GLib.Value v2);

        bool gt(GLib.Value v1, GLib.Value v2);
        bool ge(GLib.Value v1, GLib.Value v2);
        bool eq(GLib.Value v1, GLib.Value v2);
        bool le(GLib.Value v1, GLib.Value v2);
        bool lt(GLib.Value v1, GLib.Value v2);
        bool ne(GLib.Value v1, GLib.Value v2);
    }
}

