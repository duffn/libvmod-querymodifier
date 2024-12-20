#include "config.h"
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cache/cache.h"
#include "vcc_querymodifier_if.h"
#include "vcl.h"
#include "vsb.h"

#define MAX_QUERY_PARAMS 100
#define MAX_FILTER_PARAMS 100

typedef struct query_param {
    char *name;
    char *value;
} query_param_t;

/**
 * Tokenize the query string into an array of query parameters.
 * @param ctx The Varnish context.
 * @param result The array of query parameters.
 * @param query_str The query string to tokenize.
 * @return The number of query parameters or -1 on error.
 */
static int tokenize_querystring(VRT_CTX, query_param_t **result,
                                char *query_str) {
    int no_param = 0;
    char *save_ptr;
    char *param_str;

    *result = NULL;

    CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
    CHECK_OBJ_NOTNULL(ctx->ws, WS_MAGIC);
    AN(query_str);

    query_param_t *params_array =
        WS_Alloc(ctx->ws, MAX_QUERY_PARAMS * sizeof(query_param_t));
    if (params_array == NULL) {
        VRT_fail(ctx, "WS_Alloc: params_array: out of workspace");
        *result = NULL;
        return -1;
    }

    for (param_str = strtok_r(query_str, "&", &save_ptr); param_str;
         param_str = strtok_r(NULL, "&", &save_ptr)) {

        if (no_param >= MAX_QUERY_PARAMS) {
            VRT_fail(ctx, "Exceeded maximum number of query parameters");
            *result = NULL;
            return -1;
        }

        char *eq = strchr(param_str, '=');
        if (eq != NULL) {
            *eq = '\0';
            params_array[no_param].name = param_str;
            params_array[no_param].value = eq + 1;
        } else {
            params_array[no_param].name = param_str;
            params_array[no_param].value = NULL;
        }
        no_param++;
    }

    *result = params_array;
    return no_param;
}

/**
 * Tokenize and parse the filter parameters from params_in into filter_params
 * array.
 * @param ctx Varnish context
 * @param params_in Comma-separated filter parameter names
 * @param filter_params Output array of parameter names
 * @param num_filter_params Output number of parsed filter parameters
 * @return 0 on success, -1 on error
 */
static int parse_filter_params(VRT_CTX, const char *params_in,
                               char **filter_params,
                               size_t *num_filter_params) {
    char *saveptr;
    char *params_copy;
    size_t count = 0;

    CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
    CHECK_OBJ_NOTNULL(ctx->ws, WS_MAGIC);

    if (params_in == NULL || *params_in == '\0') {
        *num_filter_params = 0;
        return 0;
    }

    params_copy = WS_Copy(ctx->ws, params_in, strlen(params_in) + 1);
    if (!params_copy) {
        VRT_fail(ctx, "WS_Copy: params_copy: out of workspace");
        return -1;
    }

    for (char *filter_name = strtok_r(params_copy, ",", &saveptr); filter_name;
         filter_name = strtok_r(NULL, ",", &saveptr)) {

        if (count >= MAX_FILTER_PARAMS) {
            VRT_fail(ctx, "Exceeded maximum number of filter parameters");
            return -1;
        }
        filter_params[count++] = filter_name;
    }

    *num_filter_params = count;
    return 0;
}

/**
 * Determine if a given parameter should be included based on the exclude_params
 * flag and the list of filtered parameters.
 * @param param_name The query parameter name to check
 * @param filter_params Array of filter parameter names
 * @param num_filter_params Number of filter parameters
 * @param exclude_params If true, parameters in filter_params are excluded; else
 * included
 * @return 1 if parameter should be included, 0 otherwise
 */
static int should_include_param(const char *param_name, char **filter_params,
                                size_t num_filter_params,
                                VCL_BOOL exclude_params) {
    int match = 0;

    for (size_t i = 0; i < num_filter_params; i++) {
        if (strcmp(param_name, filter_params[i]) == 0) {
            match = 1;
            break;
        }
    }

    return exclude_params ? !match : match;
}

/**
 * Rebuild the query string by including or excluding parameters as per filters.
 * @param ctx Varnish context
 * @param uri_base The portion of the URI before the query string
 * @param params The array of tokenized query parameters
 * @param param_count The number of query parameters
 * @param filter_params The array of filter parameter names
 * @param num_filter_params The number of filter parameters
 * @param exclude_params Whether to exclude or include params_in
 * @return A pointer to the rebuilt URI from workspace or NULL on error
 */
static char *rebuild_query_string(VRT_CTX, const char *uri_base,
                                  query_param_t *params, size_t param_count,
                                  char **filter_params,
                                  size_t num_filter_params,
                                  VCL_BOOL exclude_params) {
    struct vsb *vsb = VSB_new_auto();
    char sep = '?';

    CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
    CHECK_OBJ_NOTNULL(ctx->ws, WS_MAGIC);
    AN(uri_base);
    AN(params);
    AN(filter_params);

    if (vsb == NULL) {
        VRT_fail(ctx, "VSB_new_auto failed");
        return NULL;
    }

    VSB_cat(vsb, uri_base);

    for (size_t i = 0; i < param_count; i++) {
        query_param_t *current = &params[i];
        if (should_include_param(current->name, filter_params,
                                 num_filter_params, exclude_params)) {
            if (current->value && (*current->value) != '\0') {
                VSB_printf(vsb, "%c%s=%s", sep, current->name, current->value);
            } else {
                // Parameter with no value
                VSB_printf(vsb, "%c%s", sep, current->name);
            }
            sep = '&';
        }
    }

    if (VSB_finish(vsb) != 0) {
        VRT_fail(ctx, "VSB_finish failed");
        VSB_destroy(&vsb);
        return NULL;
    }

    const char *final_uri = VSB_data(vsb);
    size_t final_len = VSB_len(vsb);
    char *ws_uri = WS_Copy(ctx->ws, final_uri, final_len + 1);
    VSB_destroy(&vsb);

    if (ws_uri == NULL) {
        VRT_fail(ctx, "WS_Copy: out of workspace");
        return NULL;
    }

    return ws_uri;
}

/**
 * This function modifies the query string of a URL by including or excluding
 * query parameters based on the input parameters.
 * @param ctx The Varnish context.
 * @param uri The URL to modify.
 * @param params_in The query parameters to include or exclude.
 * @param exclude_params If true, exclude the parameters in params_in. If false,
 * include the parameters in params_in.
 */
VCL_STRING
vmod_modifyparams(VRT_CTX, VCL_STRING uri, VCL_STRING params_in,
                  VCL_BOOL exclude_params) {
    CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
    CHECK_OBJ_NOTNULL(ctx->ws, WS_MAGIC);

    // Return if the URI is NULL
    if (uri == NULL) {
        VRT_fail(ctx, "uri is NULL");
        return NULL;
    }

    char *uri_buf = WS_Copy(ctx->ws, uri, strlen(uri) + 1);
    if (!uri_buf) {
        VRT_fail(ctx, "WS_Copy: uri_buf: out of workspace");
        return NULL;
    }

    char *query_str = strchr(uri_buf, '?');
    // No query string present so return the base URI
    if (query_str == NULL) {
        return uri_buf;
    }

    // Move past the '?'
    *query_str = '\0';
    query_str++;

    // If no query params present, return just the base URI
    if (*query_str == '\0') {
        return uri_buf;
    }

    // If params_in is NULL or empty, remove all query params and just return
    // base URI
    if (params_in == NULL || *params_in == '\0') {
        return uri_buf;
    }

    char *filter_params[MAX_FILTER_PARAMS];
    size_t num_filter_params = 0;
    if (parse_filter_params(ctx, params_in, filter_params, &num_filter_params) <
        0) {
        return NULL;
    }

    query_param_t *head;
    int no_param = tokenize_querystring(ctx, &head, query_str);
    if (no_param < 0) {
        return NULL;
    }

    // No parameters after tokenization means just return the base URI
    if (no_param == 0) {
        return uri_buf;
    }

    return rebuild_query_string(ctx, uri_buf, head, (size_t)no_param,
                                filter_params, num_filter_params,
                                exclude_params);
}

/**
 * Include the specified query parameters in the URL.
 * @param ctx The Varnish context.
 * @param uri The URL to modify.
 * @param params The query parameters to include.
 * @return The modified URL.
 */
VCL_STRING
vmod_includeparams(VRT_CTX, VCL_STRING uri, VCL_STRING params) {
    return vmod_modifyparams(ctx, uri, params, 0);
}

/**
 * Exclude the specified query parameters from the URL.
 * @param ctx The Varnish context.
 * @param uri The URL to modify.
 * @param params The query parameters to exclude.
 * @return The modified URL.
 */
VCL_STRING
vmod_excludeparams(VRT_CTX, VCL_STRING uri, VCL_STRING params) {
    return vmod_modifyparams(ctx, uri, params, 1);
}
