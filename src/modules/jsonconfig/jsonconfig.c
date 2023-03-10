#include "modules/jsonconfig/jsonconfig.h"
#include "common/config.h"
#include "common/printing.h"

#ifdef FF_HAVE_JSONC

#include "common/io/io.h"
#include "modules/modules.h"

#include <assert.h>

static inline bool parseModuleJsonObject(FFinstance* instance, const char* type, JSONCData* data, json_object* module)
{
    return
        ffParseTitleJsonObject(instance, type, data, module) ||
        ffParseBatteryJsonObject(instance, type, data, module) ||
        ffParseCommandJsonObject(instance, type, data, module) ||
        ffParseDateTimeJsonObject(instance, type, data, module) ||
        ffParseDisplayJsonObject(instance, type, data, module) ||
        ffParseHostJsonObject(instance, type, data, module) ||
        ffParseKernelJsonObject(instance, type, data, module) ||
        ffParseOSJsonObject(instance, type, data, module) ||
        ffParseSeparatorJsonObject(instance, type, data, module) ||
        false;
}

static inline void wrapJsoncFree(JSONCData* data)
{
    assert(data);
    if (data->root)
        data->ffjson_object_put(data->root);
}

static const char* parseModules(FFinstance* instance, JSONCData* data, json_object* modules)
{
    array_list* list = data->ffjson_object_get_array(modules);
    if (!list) return "modules must be an array of strings or objects";

    for (size_t idx = 0; idx < list->length; ++idx)
    {
        json_object* module = (json_object*) list->array[idx];
        const char* type = NULL;
        if (data->ffjson_object_is_type(module, json_type_string))
        {
            type = data->ffjson_object_get_string(module);
            module = NULL;
        }
        else if (data->ffjson_object_is_type(module, json_type_object))
        {
            type = data->ffjson_object_get_string(data->ffjson_object_object_get(module, "type"));
            if (!type) return "module object must contain a type key";
        }
        else
            return "modules must be an array of strings or objects";

        if(!parseModuleJsonObject(instance, type, data, module))
            return "Unknown module type";
    }

    return NULL;
}

static const char* printJsonConfig(FFinstance* instance)
{
    FF_LIBRARY_LOAD(libjsonc, &instance->config.libJSONC, "dlopen libjson-c" FF_LIBRARY_EXTENSION" failed",
        #ifdef _WIN32
            "libjson-c-5" FF_LIBRARY_EXTENSION, -1
        #else
            "libjson-c" FF_LIBRARY_EXTENSION, 5
        #endif
    )
    JSONCData __attribute__((__cleanup__(wrapJsoncFree))) data = {};
    FF_LIBRARY_LOAD_SYMBOL_VAR_MESSAGE(libjsonc, data, json_tokener_parse)
    FF_LIBRARY_LOAD_SYMBOL_VAR_MESSAGE(libjsonc, data, json_object_is_type)
    FF_LIBRARY_LOAD_SYMBOL_VAR_MESSAGE(libjsonc, data, json_object_get_array)
    FF_LIBRARY_LOAD_SYMBOL_VAR_MESSAGE(libjsonc, data, json_object_get_boolean)
    FF_LIBRARY_LOAD_SYMBOL_VAR_MESSAGE(libjsonc, data, json_object_get_double)
    FF_LIBRARY_LOAD_SYMBOL_VAR_MESSAGE(libjsonc, data, json_object_get_int)
    FF_LIBRARY_LOAD_SYMBOL_VAR_MESSAGE(libjsonc, data, json_object_get_string_len)
    FF_LIBRARY_LOAD_SYMBOL_VAR_MESSAGE(libjsonc, data, json_object_get_string)
    FF_LIBRARY_LOAD_SYMBOL_VAR_MESSAGE(libjsonc, data, json_object_get_object)
    FF_LIBRARY_LOAD_SYMBOL_VAR_MESSAGE(libjsonc, data, json_object_object_get)
    FF_LIBRARY_LOAD_SYMBOL_VAR_MESSAGE(libjsonc, data, json_object_put)

    FF_STRBUF_AUTO_DESTROY content;
    ffStrbufInit(&content);
    FF_LIST_FOR_EACH(FFstrbuf, filename, instance->state.platform.configDirs)
    {
        uint32_t oldLength = filename->length;
        ffStrbufAppendS(filename, "fastfetch/config.jsonc");
        bool success = ffAppendFileBuffer(filename->chars, &content);
        ffStrbufSubstrBefore(filename, oldLength);
        if (success) break;
    }

    data.root = data.ffjson_tokener_parse(content.chars);
    if (!data.root)
        return "Failed to parse JSON config file";

    lh_table* rootObject = data.ffjson_object_get_object(data.root);
    if (!rootObject)
        return "Invalid JSON config format. Root value must be an object";

    struct lh_entry* entry;
    lh_foreach(rootObject, entry)
    {
        const char* key = (const char *)lh_entry_k(entry);
        json_object* val = (struct json_object *)lh_entry_v(entry);

        if (strcmp(key, "modules") == 0)
        {
            const char* error = parseModules(instance, &data, val);
            if (error) return error;
        }
        else
            return "Unknown JSON config key in root object";
    }

    return NULL;
}

#else

static const char* printJsonConfig(FF_MAYBE_UNUSED FFinstance* instance)
{
    return "Fastfetch was compiled without json-c support";
}

#endif

void ffPrintJsonConfig(FFinstance* instance)
{
    const char* error = printJsonConfig(instance);
    if (error)
        ffPrintErrorString(instance, "JsonConfig", 0, NULL, NULL, "%s", error);
}
