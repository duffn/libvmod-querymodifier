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
 * @return The number of query parameters.
 */
static int tokenize_querystring(VRT_CTX, query_param_t **result,
                                char *query_str) {
    int no_param = 0;
    char *save_ptr;
    char *param_str;

    *result = NULL;

    if (query_str == NULL) {
        VRT_fail(ctx, "query_str is NULL");
        *result = NULL;
        return -1;
    }

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
 * This function modifies the query string of a URL by including or excluding
 * query parameters based on the input parameters.
 * @param ctx The Varnish context.
 * @param uri The URL to modify.
 * @param params_in The query parameters to include or exclude.
 * @param exclude_params If true, exclude the parameters in params_in. If false,
 * include the parameters in params_in.
 */
VCL_STRING vmod_modifyparams(VRT_CTX, VCL_STRING uri, VCL_STRING params_in,
                             VCL_BOOL exclude_params) {
    char *saveptr;
    char *new_uri;
    char *new_uri_end;
    char *query_str;
    char *params;
    query_param_t *head;
    query_param_t *current;
    char *filter_params[MAX_FILTER_PARAMS];
    int num_filter_params = 0;
    int i;
    int no_param;
    char sep = '?';

    CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);

    // Return if the URI is NULL.
    if (uri == NULL) {
        VRT_fail(ctx, "uri is NULL");
        return NULL;
    }

    // Check if there is a query string.
    query_str = strchr(uri, '?');
    if (query_str == NULL) {
        char *ws_uri = WS_Copy(ctx->ws, uri, strlen(uri) + 1);
        if (ws_uri == NULL) {
            VRT_fail(ctx,
                     "WS_Copy: out of workspace when returning unmodified URI");
            return NULL;
        }
        return ws_uri;
    }

    size_t base_uri_len = query_str - uri;
    size_t query_str_len = strlen(query_str + 1); // +1 to skip '?'
    size_t new_uri_max_len =
        base_uri_len + query_str_len + 2; // +2 for '?' and '\0'

    new_uri = WS_Alloc(ctx->ws, new_uri_max_len);
    if (new_uri == NULL) {
        VRT_fail(ctx, "WS_Alloc: new_uri: out of workspace");
        return NULL;
    }

    memcpy(new_uri, uri, base_uri_len);
    new_uri[base_uri_len] = '\0';
    new_uri_end = new_uri + base_uri_len;

    // Skip past the '?' to get the query string.
    query_str = query_str + 1;

    // If there are no query params, return the base URI from workspace.
    if (*query_str == '\0') {
        return new_uri;
    }

    // If params_in is NULL or empty, remove all query params.
    if (params_in == NULL || *params_in == '\0') {
        return new_uri;
    }

    // Copy the query string to the workspace.
    char *query_str_copy = WS_Copy(ctx->ws, query_str, strlen(query_str) + 1);
    if (!query_str_copy) {
        VRT_fail(ctx, "WS_Copy: query_str_copy: out of workspace");
        return NULL;
    }

    // Copy the params_in to the workspace.
    params = WS_Copy(ctx->ws, params_in, strlen(params_in) + 1);
    if (!params) {
        VRT_fail(ctx, "WS_Copy: params: out of workspace");
        return NULL;
    }

    // Tokenize params_in into filter_params array.
    num_filter_params = 0;
    for (char *filter_name = strtok_r(params, ",", &saveptr); filter_name;
         filter_name = strtok_r(NULL, ",", &saveptr)) {
        if (num_filter_params >= MAX_FILTER_PARAMS) {
            VRT_fail(ctx, "Exceeded maximum number of filter parameters");
            return NULL;
        }
        filter_params[num_filter_params++] = filter_name;
    }

    // Tokenize the query string into parameters.
    no_param = tokenize_querystring(ctx, &head, query_str_copy);
    if (no_param < 0) {
        VRT_fail(ctx, "tokenize_querystring failed");
        return NULL;
    }

    if (no_param == 0) {
        return new_uri;
    }

    struct vsb *vsb = VSB_new_auto();
    if (vsb == NULL) {
        VRT_fail(ctx, "VSB_new_auto failed");
        return NULL;
    }

    VSB_bcat(vsb, uri, base_uri_len);

    // Iterate through the query parameters.
    for (i = 0, current = head; i < no_param; ++i, ++current) {
        int match = 0;
        for (int j = 0; j < num_filter_params; ++j) {
            if (strcmp(current->name, filter_params[j]) == 0) {
                match = 1;
                break;
            }
        }

        // Include or exclude parameters based upon the argument.
        int include = exclude_params ? !match : match;
        if (include) {
            if (current->value && (*current->value) != '\0') {
                VSB_printf(vsb, "%c%s=%s", sep, current->name, current->value);
            } else {
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

    // Copy the final URI from the VSB into the workspace
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
 * Include the specified query parameters in the URL.
 * @param ctx The Varnish context.
 * @param uri The URL to modify.
 * @param params The query parameters to include.
 * @return The modified URL.
 */
VCL_STRING vmod_includeparams(VRT_CTX, VCL_STRING uri, VCL_STRING params) {
    return vmod_modifyparams(ctx, uri, params, 0);
}

/**
 * Exclude the specified query parameters from the URL.
 * @param ctx The Varnish context.
 * @param uri The URL to modify.
 * @param params The query parameters to exclude.
 * @return The modified URL.
 */
VCL_STRING vmod_excludeparams(VRT_CTX, VCL_STRING uri, VCL_STRING params) {
    return vmod_modifyparams(ctx, uri, params, 1);
}
