#ifndef GEL_TYPE_TYPEINFO
#define GEL_TYPE_TYPEINFO (gel_typeinfo_get_type())

#include <girepository.h>

typedef struct _GelTypeinfo GelTypeinfo;

GType gel_typeinfo_get_type(void) G_GNUC_CONST;
GelTypeinfo* gel_typeinfo_new(GIBaseInfo *info);
GelTypeinfo* gel_typeinfo_ref(GelTypeinfo *self);
void gel_typeinfo_unref(GelTypeinfo *self);

const gchar* gel_typeinfo_get_name(const GelTypeinfo *self);
const GelTypeinfo* gel_typeinfo_lookup(const GelTypeinfo *self,
                                       const gchar *name);

GelTypeinfo* gel_typeinfo_from_gtype(GType type);

#endif
