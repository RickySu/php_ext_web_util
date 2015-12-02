#include "web_util_http_parser.h"

static const char s_application_x_www_form_urlencode[] = "application/x-www-form-urlencoded";
static const char s_application_json[] = "application/json";
static const char s_multipart_form_data[] = "multipart/form-data";
static const char s_boundary_equal[] = "boundary=";

#define SETTER_METHOD(ce, me, pn) \
PHP_METHOD(ce, me) { \
    zval *self = getThis(); \
    zval *cb; \
    zval *oldcb; \
    if(FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &cb)) { \
        return; \
    } \
    if(!zend_is_callable(cb, 0, NULL TSRMLS_CC)) { \
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "param cb is not callable"); \
    } \
    oldcb = zend_read_property(CLASS_ENTRY(ce), self, ZEND_STRL(#pn), 1 TSRMLS_CC); \
    RETVAL_ZVAL(oldcb, 1, 0); \
    zend_update_property(CLASS_ENTRY(ce), self, ZEND_STRL(#pn), cb TSRMLS_CC); \
}

static zend_always_inline void initFunctionCache(http_parser_ext *resource TSRMLS_DC){
}

static zend_always_inline void releaseFunctionCache(http_parser_ext *resource TSRMLS_DC){
    freeFunctionCache(&resource->onHeaderParsedCallback TSRMLS_CC);
    freeFunctionCache(&resource->onBodyParsedCallback TSRMLS_CC);
    freeFunctionCache(&resource->onContentPieceCallback TSRMLS_CC);
    freeFunctionCache(&resource->onMultipartCallback TSRMLS_CC);
    freeFunctionCache(&resource->parse_str TSRMLS_CC);
}

static zend_always_inline int multipartCallback(http_parser_ext *resource, bstring *data, int type TSRMLS_DC) {
    int ret = 0;
    zval retval;
    zval *params[2];
    zval *callback = zend_read_property(CLASS_ENTRY(WebUtil_http_parser), &resource->object, ZEND_STRL("onMultipartCallback"), 0 TSRMLS_CC);
    if(!ZVAL_IS_NULL(callback)){
        MAKE_STD_ZVAL(params[0]);
        MAKE_STD_ZVAL(params[1]);
        ZVAL_STRINGL(params[0], data->val, data->len, 1);
        ZVAL_LONG(params[1], type);
        call_user_function(CG(function_table), NULL, callback, &retval, 2, params TSRMLS_CC);
        ret = !zend_is_true(&retval);
        zval_dtor(&retval);
        zval_ptr_dtor(&params[0]);
        zval_ptr_dtor(&params[1]);
    }
    bstring_free(data);
    return ret;
}

static zend_always_inline int sendData(http_parser_ext *resource, bstring *data TSRMLS_DC) {
    int headerpos, ret;

    if(resource->parser_data.multipartHeader){
        bstring_append_p(&resource->parser_data.multipartHeaderData, data->val, data->len);
        if((headerpos = bstring_find_cstr(resource->parser_data.multipartHeaderData, ZEND_STRL("\r\n\r\n"))) >= 0){
            if(multipartCallback(resource, bstring_make(resource->parser_data.multipartHeaderData->val, headerpos), TYPE_MULTIPART_HEADER TSRMLS_CC)){
                bstring_free(data);
                return 1;
            }
            if(resource->parser_data.multipartHeaderData->len - headerpos - 4 > 0){
                if(multipartCallback(resource, bstring_make(&resource->parser_data.multipartHeaderData->val[headerpos + 4], resource->parser_data.multipartHeaderData->len - headerpos - 4), TYPE_MULTIPART_CONTENT TSRMLS_CC)){
                    bstring_free(data);
                    return 1;
                }
            }
            UNSET_BSTRING(resource->parser_data.multipartHeaderData);
            resource->parser_data.multipartHeader = 0;
        }
        bstring_free(data);
        return 0;
    }
    return multipartCallback(resource, data,  TYPE_MULTIPART_CONTENT TSRMLS_CC);
}

static zend_always_inline int flushBufferData(http_parser_ext *resource TSRMLS_DC) {
    int pos, end_pos;

    if(resource->parser_data.multipartEnd){
        UNSET_BSTRING(resource->parser_data.body);
        return 0;
    }

    while(1){
        if(resource->parser_data.body->len < resource->parser_data.delimiterClose->len){
            return 0;
        }

        pos = bstring_find(resource->parser_data.body, resource->parser_data.delimiter);
        end_pos = bstring_find(resource->parser_data.body, resource->parser_data.delimiterClose);

        if(end_pos >= 0){
            bstring_cuttail(resource->parser_data.body, end_pos);
            resource->parser_data.multipartEnd = 1;
        }

        if(pos >= 0){
            if(pos > 0){
                if(sendData(resource, bstring_make(resource->parser_data.body->val, pos) TSRMLS_CC)){
                    UNSET_BSTRING(resource->parser_data.body);
                    return 1;
                }
            }
            bstring_cuthead(resource->parser_data.body, pos + resource->parser_data.delimiter->len);

            resource->parser_data.multipartHeader = 1;
            if(bstring_find(resource->parser_data.body, resource->parser_data.delimiter) >= 0){
                continue;
            }
        }
        if(resource->parser_data.multipartEnd){
            if(sendData(resource, bstring_copy(resource->parser_data.body) TSRMLS_CC)){
                UNSET_BSTRING(resource->parser_data.body);
                return 1;
            }
            UNSET_BSTRING(resource->parser_data.body);
            break;
        }
        if(pos < 0 && resource->parser_data.body->len > resource->parser_data.delimiter->len){
            if(sendData(resource, bstring_make(resource->parser_data.body->val, resource->parser_data.body->len - resource->parser_data.delimiter->len) TSRMLS_CC)){
                UNSET_BSTRING(resource->parser_data.body);
                return 1;
            }
            bstring_cuthead(resource->parser_data.body, resource->parser_data.body->len - resource->parser_data.delimiter->len);
        }
        break;
    }

    return 0;
}

static zend_always_inline zval *parseBody(http_parser_ext *resource TSRMLS_DC) {
    zval retval;
    zval *params[2];
    zval *parsedContent;
    MAKE_STD_ZVAL(parsedContent);
    if(!resource->parser_data.body){
        ZVAL_NULL(parsedContent);
        return parsedContent;
    }
    switch(resource->parser_data.contentType){
        case CONTENT_TYPE_URLENCODE:
            MAKE_STD_ZVAL(params[0]);
            ZVAL_STRINGL(params[0], resource->parser_data.body->val, resource->parser_data.body->len, 1);
            MAKE_STD_ZVAL(params[1]);
            array_init(params[1]);
            fci_call_function(&resource->parse_str, &retval, 2, params TSRMLS_CC);
            ZVAL_ZVAL(parsedContent, params[1], 1, 0);
            zval_ptr_dtor(&params[0]);
            zval_ptr_dtor(&params[1]);
            zval_dtor(&retval);
            break;
        case CONTENT_TYPE_JSONENCODE:
            php_json_decode(parsedContent, resource->parser_data.body->val, resource->parser_data.body->len, 1, PHP_JSON_PARSER_DEFAULT_DEPTH TSRMLS_CC);
            break;
        default:
            ZVAL_STRINGL(parsedContent, resource->parser_data.body->val, resource->parser_data.body->len, 1);
    }
    return parsedContent;
}

static zend_always_inline zval *fetchArrayElement_ex(zval *arr, const char *str, size_t str_len, int init) {
    zval **tmp_p, *tmp = NULL;
    
    if(zend_hash_find(Z_ARRVAL_P(arr), str, str_len, (void **) &tmp_p) != SUCCESS){
        if(init){
            MAKE_STD_ZVAL(tmp);
            array_init(tmp);
            add_assoc_zval_ex(arr, str, str_len, tmp);
        }
        return tmp;
    }
    
    return *tmp_p;
}

static zend_always_inline zval *fetchArrayElement(zval *arr, const char *str, size_t str_len) {
    return fetchArrayElement_ex(arr, str, str_len, 1);
}

static zend_always_inline void resetHeaderParser(http_parser_ext *resource TSRMLS_DC){
    zval *parsedData = zend_read_property(CLASS_ENTRY(WebUtil_http_parser), &resource->object, ZEND_STRL("parsedData"), 0 TSRMLS_CC);
    zval *parsedData_header = fetchArrayElement(parsedData, ZEND_STRL("Header") + 1);
    if(resource->parser_data.header && resource->parser_data.field){
        add_assoc_stringl(parsedData_header, resource->parser_data.header->val, resource->parser_data.field->val, resource->parser_data.field->len, 1);
    }
    
    if(resource->parser_data.headerEnd){
        resource->parser_data.headerEnd = 0;
        UNSET_BSTRING(resource->parser_data.header);
        UNSET_BSTRING(resource->parser_data.field);
    }
}

static zend_always_inline void releaseParser(http_parser_ext *resource) {
    UNSET_BSTRING(resource->parser_data.url);
    UNSET_BSTRING(resource->parser_data.header);
    UNSET_BSTRING(resource->parser_data.field);
    UNSET_BSTRING(resource->parser_data.body);
    UNSET_BSTRING(resource->parser_data.multipartHeaderData);
    UNSET_BSTRING(resource->parser_data.delimiter);
    UNSET_BSTRING(resource->parser_data.delimiterClose);
}

static zend_always_inline void resetParserStatus(http_parser_ext *resource TSRMLS_DC) {
    zval *parsedData = zend_read_property(CLASS_ENTRY(WebUtil_http_parser), &resource->object, ZEND_STRL("parsedData"), 0 TSRMLS_CC);
    if(Z_TYPE_P(parsedData) != IS_ARRAY){
        array_init(parsedData);
        zend_update_property(CLASS_ENTRY(WebUtil_http_parser), &resource->object, ZEND_STRL("parsedData"), parsedData TSRMLS_CC);
    }
    http_parser_init(&resource->parser, resource->parserType);
    resource->parser_data.contentType = 0;
    resource->parser_data.headerEnd = 0;
    resource->parser_data.multipartHeader = 0;
    resource->parser_data.multipartEnd = 0;    
    releaseParser(resource);
}

static zend_always_inline void parseContentType(http_parser_ext *resource TSRMLS_DC) {
    zval *parsedData = zend_read_property(CLASS_ENTRY(WebUtil_http_parser), &resource->object, ZEND_STRL("parsedData"), 0 TSRMLS_CC);
    zval *parsedData_header = fetchArrayElement(parsedData, ZEND_STRL("Header") + 1);
    zval *z_content_type = fetchArrayElement_ex(parsedData_header, ZEND_STRL("Content-Type") + 1, 0);

    if(!z_content_type){
        return;
    }

    if(strncmp(Z_STRVAL_P(z_content_type), s_application_x_www_form_urlencode, sizeof(s_application_x_www_form_urlencode) - 1) == 0){
        resource->parser_data.contentType = CONTENT_TYPE_URLENCODE;
        return;
    }
    
    if(strncmp(Z_STRVAL_P(z_content_type), s_application_json, sizeof(s_application_json) - 1) == 0){
        resource->parser_data.contentType = CONTENT_TYPE_JSONENCODE;
        return;
    }
    
    if(strncmp(Z_STRVAL_P(z_content_type), s_multipart_form_data, sizeof(s_multipart_form_data) -1) == 0){
        resource->parser_data.contentType = CONTENT_TYPE_MULTIPART;
        char *boundary = php_memnstr(Z_STRVAL_P(z_content_type), (char *) s_boundary_equal, sizeof(s_boundary_equal) - 1, &Z_STRVAL_P(z_content_type)[Z_STRLEN_P(z_content_type) - 1]);
        size_t boundary_len;
        if(boundary){
            boundary_len = (Z_STRLEN_P(z_content_type) - (boundary - Z_STRVAL_P(z_content_type)) - sizeof(s_boundary_equal));
            resource->parser_data.delimiter = bstring_make(ZEND_STRL("\r\n--"));
            bstring_append_p(&resource->parser_data.delimiter, &boundary[sizeof(s_boundary_equal) - 1], boundary_len + 1);
            resource->parser_data.delimiterClose = bstring_copy(resource->parser_data.delimiter);
            bstring_append_p(&resource->parser_data.delimiter, ZEND_STRL("\r\n"));
            bstring_append_p(&resource->parser_data.delimiterClose, ZEND_STRL("--"));
            UNSET_BSTRING(resource->parser_data.multipartHeaderData);
            UNSET_BSTRING(resource->parser_data.body);
            resource->parser_data.body = bstring_make(ZEND_STRL("\r\n"));
            resource->parser_data.multipartHeader = resource->parser_data.multipartEnd = 0;
        }
        return;
    }
}

static zend_always_inline void parseRequest(http_parser_ext *resource TSRMLS_DC) {
    char buf[8];
    struct http_parser_url parser_url;
    zval *parsedData = zend_read_property(CLASS_ENTRY(WebUtil_http_parser), &resource->object, ZEND_STRL("parsedData"), 0 TSRMLS_CC);
    zval *parsedData_request;
    zval *parsedData_query;
    MAKE_STD_ZVAL(parsedData_request);
    MAKE_STD_ZVAL(parsedData_query);
    array_init(parsedData_request);
    array_init(parsedData_query);
    add_assoc_string(parsedData_request, "Method",  (char *) http_method_str((enum http_method)resource->parser.method), 1);
    add_assoc_stringl(parsedData_request, "Target", resource->parser_data.url->val, resource->parser_data.url->len, 1);
    add_assoc_string(parsedData_request, "Protocol",  "HTTP", 1);
    snprintf(buf, sizeof(buf), "%d.%d", resource->parser.http_major, resource->parser.http_minor);
    add_assoc_string(parsedData_request, "Protocol-Version",  buf, 1);

    http_parser_parse_url(resource->parser_data.url->val, resource->parser_data.url->len, 0, &parser_url);
    add_assoc_stringl(parsedData_query, "Path", &resource->parser_data.url->val[parser_url.field_data[UF_PATH].off], parser_url.field_data[UF_PATH].len, 1);

    if(parser_url.field_data[UF_QUERY].len){
        zval retval;
        zval *params[2];
        MAKE_STD_ZVAL(params[0]);
        ZVAL_STRINGL(params[0], &resource->parser_data.url->val[parser_url.field_data[UF_QUERY].off], parser_url.field_data[UF_QUERY].len, 1);
        MAKE_STD_ZVAL(params[1]);
        array_init(params[1]);
        fci_call_function(&resource->parse_str, &retval, 2, params TSRMLS_CC);
        Z_ADDREF_P(params[1]);
        add_assoc_zval(parsedData_query, "Param", params[1]);
        zval_ptr_dtor(&params[0]);
        zval_ptr_dtor(&params[1]);
        zval_dtor(&retval);
    }
    else{
        add_assoc_null(parsedData_query, "Param");
    }
    
    add_assoc_zval(parsedData, "Request", parsedData_request);
    add_assoc_zval(parsedData, "Query", parsedData_query);
}

static zend_always_inline void parseResponse(http_parser_ext *resource TSRMLS_DC) {
    char buf[8];
    struct http_parser_url parser_url;
    zval *parsedData = zend_read_property(CLASS_ENTRY(WebUtil_http_parser), &resource->object, ZEND_STRL("parsedData"), 0 TSRMLS_CC);
    zval *parsedData_response;
    MAKE_STD_ZVAL(parsedData_response);
    array_init(parsedData_response);
    add_assoc_long(parsedData_response, "Status-Code",  resource->parser.status_code);
    add_assoc_string(parsedData_response, "Protocol",  "HTTP", 1);
    snprintf(buf, sizeof(buf), "%d.%d", resource->parser.http_major, resource->parser.http_minor);
    add_assoc_string(parsedData_response, "Protocol-Version",  buf, 1);
    add_assoc_zval(parsedData, "Response", parsedData_response);
}


static zend_always_inline void parseCookie(http_parser_ext *resource, const char *cookie_field TSRMLS_DC) {
    zval *parsedData = zend_read_property(CLASS_ENTRY(WebUtil_http_parser), &resource->object, ZEND_STRL("parsedData"), 0 TSRMLS_CC);
    zval *parsedData_header = fetchArrayElement(parsedData, ZEND_STRL("Header") + 1);
    zval *s_cookie = fetchArrayElement_ex(parsedData_header, cookie_field, strlen(cookie_field) + 1, 0);
    const char *cookieString;
    size_t cookieString_len;
    bstring *key;
    uint token_equal_pos = 0, token_semi_pos = 0;
    uint field_start = 0;
    uint i = 0;

    if(!s_cookie || Z_TYPE_P(s_cookie) != IS_STRING){
        return;
    }

    cookieString = Z_STRVAL_P(s_cookie);
    cookieString_len = Z_STRLEN_P(s_cookie);
    
    zval *cookie;
    MAKE_STD_ZVAL(cookie);
    array_init(cookie);

    while(cookieString_len){
        if(cookieString[i] == '='){
            if(token_equal_pos <= token_semi_pos){
                token_equal_pos = i;
            }
        }
        if(cookieString[i] == ';' || i==cookieString_len){
            token_semi_pos = i;
            if(token_equal_pos < field_start){
                key = bstring_make(&cookieString[field_start], i - field_start);
                add_assoc_bool(cookie, key->val, 1);
                bstring_free(key);
            }
            else{
                key = bstring_make(&cookieString[field_start], token_equal_pos - field_start);
                add_assoc_stringl(cookie, key->val, (char *) &cookieString[token_equal_pos+1], token_semi_pos - token_equal_pos - 1, 1);
                bstring_free(key);
            }
            field_start = i + 2;
        }
        
        if(++i>cookieString_len){
            break;
        }        
    }
    add_assoc_zval(parsedData_header, cookie_field, cookie);
}

static http_parser_settings parser_response_settings = {
    (http_cb) on_message_begin,    //on_message_begin
    (http_data_cb) on_url,    //on_url
    (http_data_cb) on_status,    //on_status
    (http_data_cb) on_header_field,    //on_header_field
    (http_data_cb) on_header_value,    //on_header_value
    (http_cb) on_headers_complete_response,    //on_headers_complete
    (http_data_cb) on_response_body,    //on_body      
    (http_cb) on_message_complete     //on_message_complete
};

static http_parser_settings parser_request_settings = {
    (http_cb) on_message_begin,    //on_message_begin
    (http_data_cb) on_url,    //on_url
    (http_data_cb) on_status,    //on_status
    (http_data_cb) on_header_field,    //on_header_field
    (http_data_cb) on_header_value,    //on_header_value
    (http_cb) on_headers_complete_request,    //on_headers_complete
    (http_data_cb) on_request_body,    //on_body      
    (http_cb) on_message_complete     //on_message_complete
};

static int on_message_begin(http_parser_ext *resource){
    return 0;
}

static int on_message_complete(http_parser_ext *resource){
    TSRMLS_FETCH();
    int ret = 0;
    zval retval;
    zval *callback = zend_read_property(CLASS_ENTRY(WebUtil_http_parser), &resource->object, ZEND_STRL("onBodyParsedCallback"), 0 TSRMLS_CC);
    zval *parsedBody = parseBody(resource TSRMLS_CC);
    if(!ZVAL_IS_NULL(callback)){
        call_user_function(CG(function_table), NULL, callback, &retval, 1, &parsedBody TSRMLS_CC);
        ret = !zend_is_true(&retval);
        zval_dtor(&retval);
    }
    zval_ptr_dtor(&parsedBody);
    zval_dtor(&retval);
    releaseParser(resource);
    return ret;
}

static int on_headers_complete_request(http_parser_ext *resource){
    TSRMLS_FETCH();
    int ret = 0;
    zval retval;
    zval *parsedData = zend_read_property(CLASS_ENTRY(WebUtil_http_parser), &resource->object, ZEND_STRL("parsedData"), 0 TSRMLS_CC);
    zval *callback = zend_read_property(CLASS_ENTRY(WebUtil_http_parser), &resource->object, ZEND_STRL("onHeaderParsedCallback"), 0 TSRMLS_CC);
    zval *parsedData_header = fetchArrayElement(parsedData, ZEND_STRL("Header") + 1);
    
    
    resetHeaderParser(resource TSRMLS_CC);
    parseRequest(resource TSRMLS_CC);
    parseContentType(resource TSRMLS_CC);
    parseCookie(resource, "Cookie" TSRMLS_CC);

    if(!ZVAL_IS_NULL(callback)){
        call_user_function(CG(function_table), NULL, callback, &retval, 1, &parsedData TSRMLS_CC);
        ret = !zend_is_true(&retval);
        zval_dtor(&retval);
    }
    return ret;
}

static int on_headers_complete_response(http_parser_ext *resource){
    TSRMLS_FETCH();
    int ret = 0;
    zval retval;
    zval *parsedData = zend_read_property(CLASS_ENTRY(WebUtil_http_parser), &resource->object, ZEND_STRL("parsedData"), 0 TSRMLS_CC);
    zval *callback = zend_read_property(CLASS_ENTRY(WebUtil_http_parser), &resource->object, ZEND_STRL("onHeaderParsedCallback"), 0 TSRMLS_CC);
    zval *parsedData_header = fetchArrayElement(parsedData, ZEND_STRL("Header") + 1);
    
    
    resetHeaderParser(resource TSRMLS_CC);
    parseResponse(resource TSRMLS_CC);
    parseContentType(resource TSRMLS_CC);
    parseCookie(resource, "Set-Cookie" TSRMLS_CC);

    if(!ZVAL_IS_NULL(callback)){
        call_user_function(CG(function_table), NULL, callback, &retval, 1, &parsedData TSRMLS_CC);
        ret = !zend_is_true(&retval);
        zval_dtor(&retval);
    }
    return ret;
}

static int on_status(http_parser_ext *resource, const char *buf, size_t len){
    return 0;
}

static int on_url(http_parser_ext *resource, const char *buf, size_t len){
    bstring_append_p(&resource->parser_data.url, buf, len);
    return 0;
}

static int on_header_field(http_parser_ext *resource, const char *buf, size_t len){
    TSRMLS_FETCH();
    resetHeaderParser(resource TSRMLS_CC);
    bstring_append_p(&resource->parser_data.header, buf, len);
    return 0;
}

static int on_header_value(http_parser_ext *resource, const char *buf, size_t len){
    resource->parser_data.headerEnd = 1;
    bstring_append_p(&resource->parser_data.field, buf, len);
    return 0;
}

static int on_response_body(http_parser_ext *resource, const char *buf, size_t len){
    TSRMLS_FETCH();
    int ret = 0;
    zval retval;
    zval *param;
    zval *callback = zend_read_property(CLASS_ENTRY(WebUtil_http_parser), &resource->object, ZEND_STRL("onContentPieceCallback"), 0 TSRMLS_CC);

    if(!ZVAL_IS_NULL(callback)){
        MAKE_STD_ZVAL(param);
        ZVAL_STRINGL(param, buf, len, 1);
        call_user_function(CG(function_table), NULL, callback, &retval, 1, &param TSRMLS_CC);
        ret = !zend_is_true(&retval);
        zval_dtor(&retval);
        zval_ptr_dtor(&param);
    }
    
    if(ret){
        return 1;
    }

    bstring_append_p(&resource->parser_data.body, buf, len);
    
    return 0;
}

static int on_request_body(http_parser_ext *resource, const char *buf, size_t len){
    TSRMLS_FETCH();
    int ret = 0;
    zval retval;
    zval *param;
    zval *callback = zend_read_property(CLASS_ENTRY(WebUtil_http_parser), &resource->object, ZEND_STRL("onContentPieceCallback"), 0 TSRMLS_CC);

    if(!ZVAL_IS_NULL(callback)){
        MAKE_STD_ZVAL(param);
        ZVAL_STRINGL(param, buf, len, 1);
        call_user_function(CG(function_table), NULL, callback, &retval, 1, &param TSRMLS_CC);
        ret = !zend_is_true(&retval);
        zval_dtor(&retval);
        zval_ptr_dtor(&param);
    }
    
    if(ret){
        return 1;
    }

    bstring_append_p(&resource->parser_data.body, buf, len);
    
    if(resource->parser_data.contentType == CONTENT_TYPE_MULTIPART){
        return flushBufferData(resource TSRMLS_CC);
    }

    return 0;
}

CLASS_ENTRY_FUNCTION_D(WebUtil_http_parser){
    zval *array;
    REGISTER_CLASS_WITH_OBJECT_NEW(WebUtil_http_parser, "WebUtil\\Parser\\HttpParser", createWebUtil_http_parserResource);
    OBJECT_HANDLER(WebUtil_http_parser).clone_obj = NULL;
    zend_declare_property_null(CLASS_ENTRY(WebUtil_http_parser), ZEND_STRL("onHeaderParsedCallback"), ZEND_ACC_PRIVATE TSRMLS_CC);
    zend_declare_property_null(CLASS_ENTRY(WebUtil_http_parser), ZEND_STRL("onBodyParsedCallback"), ZEND_ACC_PRIVATE TSRMLS_CC);
    zend_declare_property_null(CLASS_ENTRY(WebUtil_http_parser), ZEND_STRL("onContentPieceCallback"), ZEND_ACC_PRIVATE TSRMLS_CC);
    zend_declare_property_null(CLASS_ENTRY(WebUtil_http_parser), ZEND_STRL("onMultipartCallback"), ZEND_ACC_PRIVATE TSRMLS_CC);
    zend_declare_property_null(CLASS_ENTRY(WebUtil_http_parser), ZEND_STRL("parsedData"), ZEND_ACC_PRIVATE TSRMLS_CC);
    REGISTER_CLASS_CONSTANT_LONG(WebUtil_http_parser, TYPE_REQUEST);
    REGISTER_CLASS_CONSTANT_LONG(WebUtil_http_parser, TYPE_RESPONSE);
}

static zend_object_value createWebUtil_http_parserResource(zend_class_entry *ce TSRMLS_DC) {
    zval *fn_parse_str;
    zend_object_value retval; 
    http_parser_ext *resource;
    resource = (http_parser_ext *) ecalloc(1, sizeof(http_parser_ext));
    zend_object_std_init(&resource->zo, ce TSRMLS_CC);
    object_properties_init(&resource->zo, ce);
    retval.handle = zend_objects_store_put(
        &resource->zo,
        (zend_objects_store_dtor_t) zend_objects_destroy_object,
        freeWebUtil_http_parserResource,
        NULL TSRMLS_CC);
    retval.handlers = &OBJECT_HANDLER(WebUtil_http_parser);
    initFunctionCache(resource TSRMLS_CC);
    MAKE_STD_ZVAL(fn_parse_str);
    ZVAL_STRING(fn_parse_str, "parse_str", 1);
    registerFunctionCache(&resource->parse_str, fn_parse_str TSRMLS_CC);
    zval_ptr_dtor(&fn_parse_str);
    return retval;
}

void freeWebUtil_http_parserResource(void *object TSRMLS_DC) {
    http_parser_ext *resource;
    resource = FETCH_RESOURCE(object, http_parser_ext);
    releaseParser(resource);
    releaseFunctionCache(resource TSRMLS_CC);
    zend_object_std_dtor(&resource->zo TSRMLS_CC);
    efree(resource);
}

PHP_METHOD(WebUtil_http_parser, reset){
    zval *self = getThis();
    http_parser_ext *resource = FETCH_OBJECT_RESOURCE(self, http_parser_ext);
    resetParserStatus(resource TSRMLS_CC);
}

PHP_METHOD(WebUtil_http_parser, feed){
    zval *self = getThis();
    const char *data = NULL;
    int data_len;
    http_parser_ext *resource = FETCH_OBJECT_RESOURCE(self, http_parser_ext);

    if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &data, &data_len)) {
        return;
    }
    http_parser_execute(&resource->parser, (resource->parserType == HTTP_REQUEST ? &parser_request_settings:&parser_response_settings), data, data_len);
}

PHP_METHOD(WebUtil_http_parser, __construct){
    long type = TYPE_REQUEST;
    zval *self = getThis();
    http_parser_ext *resource = FETCH_OBJECT_RESOURCE(self, http_parser_ext);
    if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|l", &type)) {
        return;
    }
    
    if(type == TYPE_REQUEST){
        resource->parserType = HTTP_REQUEST;
    }
    else{
        resource->parserType = HTTP_RESPONSE;
    }
    resource->object = *self;
    resetParserStatus(resource TSRMLS_CC);
}

SETTER_METHOD(WebUtil_http_parser, setOnHeaderParsedCallback, onHeaderParsedCallback)
SETTER_METHOD(WebUtil_http_parser, setOnBodyParsedCallback, onBodyParsedCallback)
SETTER_METHOD(WebUtil_http_parser, setOnContentPieceCallback, onContentPieceCallback)
SETTER_METHOD(WebUtil_http_parser, setOnMultipartCallback, onMultipartCallback)
