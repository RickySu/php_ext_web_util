#include "web_util_http_parser.h"

static const char s_application_x_www_form_urlencode[] = "application/x-www-form-urlencoded";
static const char s_application_json[] = "application/json";
static const char s_multipart_form_data[] = "multipart/form-data";
static const char s_boundary_equal[] = "boundary=";

static zend_always_inline void initFunctionCache(http_parser_ext *resource){
    ZVAL_NULL(&resource->onHeaderParsedCallback.func);
    ZVAL_NULL(&resource->onBodyParsedCallback.func);
    ZVAL_NULL(&resource->onContentPieceCallback.func);
    ZVAL_NULL(&resource->onMultipartCallback.func);
}

static zend_always_inline void releaseFunctionCache(http_parser_ext *resource){
    freeFunctionCache(&resource->onHeaderParsedCallback);
    freeFunctionCache(&resource->onBodyParsedCallback);
    freeFunctionCache(&resource->onContentPieceCallback);
    freeFunctionCache(&resource->onMultipartCallback);
    freeFunctionCache(&resource->parse_str);
}

#define SETTER_METHOD(ce, me, pn) \
PHP_METHOD(ce, me) { \
    zval *self = getThis(); \
    http_parser_ext *resource = FETCH_OBJECT_RESOURCE(self, http_parser_ext); \
    zval *cb; \
    if(FAILURE == zend_parse_parameters(ZEND_NUM_ARGS(), "z", &cb)) { \
        return; \
    } \
    registerFunctionCache(&resource->pn, cb); \
}

static zend_always_inline int multipartCallback(http_parser_ext *resource, bstring *data, int type) {
    int ret = 0;
    zval retval;
    zval params[2];
    if(!Z_ISNULL(resource->onMultipartCallback.func)){
        ZVAL_STRINGL(&params[0], data->val, data->len);
        ZVAL_LONG(&params[1], type);
        fci_call_function(&resource->onMultipartCallback, &retval, 2, params);
        ret = !zend_is_true(&retval);
        zval_dtor(&retval);
        zval_dtor(&params[0]);
        zval_dtor(&params[1]);
    }
    bstring_free(data);
    return ret;
}

static zend_always_inline int sendData(http_parser_ext *resource, bstring *data) {
    int headerpos, ret;

    if(resource->parser_data.multipartHeader){
        bstring_append_p(&resource->parser_data.multipartHeaderData, data->val, data->len);
        if((headerpos = bstring_find_cstr(resource->parser_data.multipartHeaderData, ZEND_STRL("\r\n\r\n"))) >= 0){
            if(multipartCallback(resource, bstring_make(resource->parser_data.multipartHeaderData->val, headerpos), TYPE_MULTIPART_HEADER)){
                bstring_free(data);
                return 1;
            }
            if(resource->parser_data.multipartHeaderData->len - headerpos - 4 > 0){
                if(multipartCallback(resource, bstring_make(&resource->parser_data.multipartHeaderData->val[headerpos + 4], resource->parser_data.multipartHeaderData->len - headerpos - 4), TYPE_MULTIPART_CONTENT)){
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
    return multipartCallback(resource, data,  TYPE_MULTIPART_CONTENT);
}

static zend_always_inline int flushBufferData(http_parser_ext *resource) {
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
                if(sendData(resource, bstring_make(resource->parser_data.body->val, pos))){
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
            if(sendData(resource, bstring_copy(resource->parser_data.body))){
                UNSET_BSTRING(resource->parser_data.body);
                return 1;
            }
            UNSET_BSTRING(resource->parser_data.body);
            break;
        }
        if(pos < 0 && resource->parser_data.body->len > resource->parser_data.delimiter->len){
            if(sendData(resource, bstring_make(resource->parser_data.body->val, resource->parser_data.body->len - resource->parser_data.delimiter->len))){
                UNSET_BSTRING(resource->parser_data.body);
                return 1;
            }
            bstring_cuthead(resource->parser_data.body, resource->parser_data.body->len - resource->parser_data.delimiter->len);
        }
        break;
    }

    return 0;
}

static zend_always_inline zval parse_str(const char *data, size_t data_len, fcall_info_t *parse_str_func){
    zval retval;
    zval params[2];
    ZVAL_STRINGL(&params[0], data, data_len);
    array_init(&params[1]);
    ZVAL_MAKE_REF(&params[1]);
    fci_call_function(parse_str_func, &retval, 2, params);
    ZVAL_UNREF(&params[1]);
    zval_dtor(&params[0]);
    zval_dtor(&retval);
    return params[1];
}

static zend_always_inline zval parseBody(http_parser_ext *resource) {
    zval fn, retval;
    zval params[2];
    zval parsedContent;
    bstring *res;
    if(!resource->parser_data.body){
        ZVAL_NULL(&parsedContent);
        return parsedContent;
    }
    switch(resource->parser_data.contentType){
        case CONTENT_TYPE_URLENCODE:
            retval = parse_str(resource->parser_data.body->val, resource->parser_data.body->len, &resource->parse_str);
            ZVAL_COPY_VALUE(&parsedContent, &retval);
            break;
        case CONTENT_TYPE_JSONENCODE:
            php_json_decode(&parsedContent, resource->parser_data.body->val, resource->parser_data.body->len, 1, PHP_JSON_PARSER_DEFAULT_DEPTH);
            break;
        default:
            ZVAL_STRINGL(&parsedContent, resource->parser_data.body->val, resource->parser_data.body->len);
    }
    return parsedContent;
}

static zend_always_inline zval *fetchArrayElement_ex(zval *arr, const char *str, size_t str_len, int init) {
    zval *tmp;
    
    if(tmp = zend_hash_str_find(HASH_OF(arr), str, str_len)){
        return tmp;
    }

    if(init){
        zval tmp_arr;
        array_init(&tmp_arr);
        tmp = zend_hash_str_update(HASH_OF(arr), str, str_len, &tmp_arr);
    }
    return tmp;
}

static zend_always_inline zval *fetchArrayElement(zval *arr, const char *str, size_t str_len) {
    return fetchArrayElement_ex(arr, str, str_len, 1);
}

static zend_always_inline void resetHeaderParser(http_parser_ext *resource){
    zval rv;
    zval *parsedData = zend_read_property(CLASS_ENTRY(WebUtil_http_parser), &resource->object, ZEND_STRL("parsedData"), 0, &rv);
    zval *parsedData_header = fetchArrayElement(parsedData, ZEND_STRL("Header"));
    if(resource->parser_data.header && resource->parser_data.field){
        add_assoc_stringl(parsedData_header, resource->parser_data.header->val, resource->parser_data.field->val, resource->parser_data.field->len);
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

static zend_always_inline void resetParserStatus(http_parser_ext *resource) {
    zval rv;
    zval *parsedData = zend_read_property(CLASS_ENTRY(WebUtil_http_parser), &resource->object, ZEND_STRL("parsedData"), 0, &rv);
    if(Z_TYPE_P(parsedData) != IS_ARRAY){
        array_init(parsedData);
        zend_update_property(CLASS_ENTRY(WebUtil_http_parser), &resource->object, ZEND_STRL("parsedData"), parsedData);
    }
    http_parser_init(&resource->parser, resource->parserType);
    resource->parser_data.contentType = 0;
    resource->parser_data.headerEnd = 0;
    resource->parser_data.multipartHeader = 0;
    resource->parser_data.multipartEnd = 0;    
    releaseParser(resource);
}

static zend_always_inline void parseContentType(http_parser_ext *resource) {
    zval rv;
    zval *parsedData = zend_read_property(CLASS_ENTRY(WebUtil_http_parser), &resource->object, ZEND_STRL("parsedData"), 0, &rv);
    zval *parsedData_header = fetchArrayElement(parsedData, ZEND_STRL("Header"));
    zval *z_content_type = fetchArrayElement_ex(parsedData_header, ZEND_STRL("Content-Type"), 0);

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

static zend_always_inline void parseRequest(http_parser_ext *resource) {
    char buf[8];
    struct http_parser_url parser_url;
    zval rv;
    zval *parsedData = zend_read_property(CLASS_ENTRY(WebUtil_http_parser), &resource->object, ZEND_STRL("parsedData"), 0, &rv);
    zval parsedData_request;
    zval parsedData_query;

    array_init(&parsedData_request);
    array_init(&parsedData_query);
    add_assoc_zval(parsedData, "Request", &parsedData_request);
    add_assoc_zval(parsedData, "Query", &parsedData_query);    
    add_assoc_string(&parsedData_request, "Method",  (char *) http_method_str((enum http_method)resource->parser.method));
    add_assoc_stringl(&parsedData_request, "Target", resource->parser_data.url->val, resource->parser_data.url->len);
    add_assoc_string(&parsedData_request, "Protocol",  "HTTP");
    snprintf(buf, sizeof(buf), "%d.%d", resource->parser.http_major, resource->parser.http_minor);
    add_assoc_string(&parsedData_request, "Protocol-Version",  buf);
    http_parser_parse_url(resource->parser_data.url->val, resource->parser_data.url->len, 0, &parser_url);
    add_assoc_stringl(&parsedData_query, "Path", &resource->parser_data.url->val[parser_url.field_data[UF_PATH].off], parser_url.field_data[UF_PATH].len);

    if(parser_url.field_data[UF_QUERY].len){
        zval retval;
        retval = parse_str(&resource->parser_data.url->val[parser_url.field_data[UF_QUERY].off], parser_url.field_data[UF_QUERY].len, &resource->parse_str);
        add_assoc_zval(&parsedData_query, "Param", &retval);
    }
    else{
        add_assoc_null(&parsedData_query, "Param");
    }
}

static zend_always_inline void parseResponse(http_parser_ext *resource) {
    char buf[8];
    zval rv;
    struct http_parser_url parser_url;
    zval *parsedData = zend_read_property(CLASS_ENTRY(WebUtil_http_parser), &resource->object, ZEND_STRL("parsedData"), 0, &rv);
    zval parsedData_response;
    array_init(&parsedData_response);
    add_assoc_long(&parsedData_response, "Status-Code",  resource->parser.status_code);
    add_assoc_string(&parsedData_response, "Protocol",  "HTTP");
    snprintf(buf, sizeof(buf), "%d.%d", resource->parser.http_major, resource->parser.http_minor);
    add_assoc_string(&parsedData_response, "Protocol-Version",  buf);
    add_assoc_zval(parsedData, "Response", &parsedData_response);
}


static zend_always_inline void parseCookie(http_parser_ext *resource, const char *cookie_field) {
    zval rv;
    zval *parsedData = zend_read_property(CLASS_ENTRY(WebUtil_http_parser), &resource->object, ZEND_STRL("parsedData"), 0, &rv);
    zval *parsedData_header = fetchArrayElement(parsedData, ZEND_STRL("Header"));
    zval *s_cookie = fetchArrayElement_ex(parsedData_header, cookie_field, strlen(cookie_field), 0);
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

    zval cookie;
    array_init(&cookie);
    add_assoc_zval(parsedData_header, cookie_field, &cookie);

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
                add_assoc_bool(&cookie, key->val, 1);
                bstring_free(key);
            }
            else{
                zval s, *sp;
                key = bstring_make(&cookieString[field_start], token_equal_pos - field_start);
                add_assoc_stringl(&cookie, key->val, (char *) &cookieString[token_equal_pos + 1], token_semi_pos - token_equal_pos - 1);
                bstring_free(key);
            }
            field_start = i + 2;
        }
        
        if(++i>cookieString_len){
            break;
        }
    }
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
    int ret = 0;
    zval retval;
    zval parsedBody = parseBody(resource);
    if(!Z_ISNULL(resource->onBodyParsedCallback.func)){
        fci_call_function(&resource->onBodyParsedCallback, &retval, 1, &parsedBody);
        ret = !zend_is_true(&retval);
        zval_dtor(&retval);
    }
    zval_dtor(&parsedBody);
    zval_dtor(&retval);
    releaseParser(resource);
    return ret;
}

static int on_headers_complete_request(http_parser_ext *resource){
    int ret = 0;
    zval retval, rv;
    zval *parsedData = zend_read_property(CLASS_ENTRY(WebUtil_http_parser), &resource->object, ZEND_STRL("parsedData"), 0, &rv);
    zval *parsedData_header = fetchArrayElement(parsedData, ZEND_STRL("Header"));
    
    resetHeaderParser(resource);
    parseRequest(resource);
    parseContentType(resource);
    parseCookie(resource, "Cookie");
    if(!Z_ISNULL(resource->onHeaderParsedCallback.func)){
        fci_call_function(&resource->onHeaderParsedCallback, &retval, 1, parsedData);
        ret = !zend_is_true(&retval);
        zval_dtor(&retval);
    }
    return ret;
}

static int on_headers_complete_response(http_parser_ext *resource){
    int ret = 0;
    zval retval, rv;
    zval *parsedData = zend_read_property(CLASS_ENTRY(WebUtil_http_parser), &resource->object, ZEND_STRL("parsedData"), 0, &rv);
    zval *callback = zend_read_property(CLASS_ENTRY(WebUtil_http_parser), &resource->object, ZEND_STRL("onHeaderParsedCallback"), 0, &rv);
    zval *parsedData_header = fetchArrayElement(parsedData, ZEND_STRL("Header"));
    
    resetHeaderParser(resource);
    parseResponse(resource);
    parseContentType(resource);
    parseCookie(resource, "Set-Cookie");

    if(!Z_ISNULL(resource->onHeaderParsedCallback.func)){
        fci_call_function(&resource->onHeaderParsedCallback, &retval, 1, parsedData);
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
    resetHeaderParser(resource);
    bstring_append_p(&resource->parser_data.header, buf, len);
    return 0;
}

static int on_header_value(http_parser_ext *resource, const char *buf, size_t len){
    resource->parser_data.headerEnd = 1;
    bstring_append_p(&resource->parser_data.field, buf, len);
    return 0;
}

static int on_response_body(http_parser_ext *resource, const char *buf, size_t len){
    int ret = 0;
    zval retval;
    zval param;

    if(!Z_ISNULL(resource->onContentPieceCallback.func)){
        ZVAL_STRINGL(&param, buf, len);
        fci_call_function(&resource->onContentPieceCallback, &retval, 1, &param);
        ret = !zend_is_true(&retval);
        zval_dtor(&retval);
        zval_dtor(&param);
    }
    
    if(ret){
        return 1;
    }

    bstring_append_p(&resource->parser_data.body, buf, len);
    
    return 0;
}

static int on_request_body(http_parser_ext *resource, const char *buf, size_t len){
    int ret = 0;
    zval retval;
    zval param;

    if(!Z_ISNULL(resource->onContentPieceCallback.func)){
        ZVAL_STRINGL(&param, buf, len);
        fci_call_function(&resource->onContentPieceCallback, &retval, 1, &param);
        ret = !zend_is_true(&retval);
        zval_dtor(&retval);
        zval_dtor(&param);
    }
    
    if(ret){
        return 1;
    }

    bstring_append_p(&resource->parser_data.body, buf, len);
    
    if(resource->parser_data.contentType == CONTENT_TYPE_MULTIPART){
        return flushBufferData(resource);
    }

    return 0;
}

CLASS_ENTRY_FUNCTION_D(WebUtil_http_parser){
    REGISTER_CLASS_WITH_OBJECT_NEW(WebUtil_http_parser, "WebUtil\\Parser\\HttpParser", createWebUtil_http_parserResource);
    OBJECT_HANDLER(WebUtil_http_parser).offset = XtOffsetOf(http_parser_ext, zo);
    OBJECT_HANDLER(WebUtil_http_parser).clone_obj = NULL;
    OBJECT_HANDLER(WebUtil_http_parser).free_obj = freeWebUtil_http_parserResource;
    zend_declare_property_null(CLASS_ENTRY(WebUtil_http_parser), ZEND_STRL("onHeaderParsedCallback"), ZEND_ACC_PRIVATE);
    zend_declare_property_null(CLASS_ENTRY(WebUtil_http_parser), ZEND_STRL("onBodyParsedCallback"), ZEND_ACC_PRIVATE);
    zend_declare_property_null(CLASS_ENTRY(WebUtil_http_parser), ZEND_STRL("onContentPieceCallback"), ZEND_ACC_PRIVATE);
    zend_declare_property_null(CLASS_ENTRY(WebUtil_http_parser), ZEND_STRL("onMultipartCallback"), ZEND_ACC_PRIVATE);
    zend_declare_property_null(CLASS_ENTRY(WebUtil_http_parser), ZEND_STRL("parsedData"), ZEND_ACC_PRIVATE);
    REGISTER_CLASS_CONSTANT_LONG(WebUtil_http_parser, TYPE_REQUEST);
    REGISTER_CLASS_CONSTANT_LONG(WebUtil_http_parser, TYPE_RESPONSE);
}

static zend_object *createWebUtil_http_parserResource(zend_class_entry *ce) {
    zval fn_parse_str;
    http_parser_ext *resource;
    resource = ALLOC_RESOURCE(http_parser_ext);
    zend_object_std_init(&resource->zo, ce);
    object_properties_init(&resource->zo, ce);
    initFunctionCache(resource);
    resource->zo.handlers = &OBJECT_HANDLER(WebUtil_http_parser);
    ZVAL_STRING(&fn_parse_str, "parse_str");
    registerFunctionCache(&resource->parse_str, &fn_parse_str);
    zval_dtor(&fn_parse_str);
    return &resource->zo;
}

void freeWebUtil_http_parserResource(zend_object *object) {
    http_parser_ext *resource;
    resource = FETCH_RESOURCE(object, http_parser_ext);
    releaseParser(resource);
    releaseFunctionCache(resource);
    zend_object_std_dtor(&resource->zo);
    efree(resource);
}

PHP_METHOD(WebUtil_http_parser, reset){
    zval *self = getThis();
    http_parser_ext *resource = FETCH_OBJECT_RESOURCE(self, http_parser_ext);
    resetParserStatus(resource);
}

PHP_METHOD(WebUtil_http_parser, feed){
    zval *self = getThis();
    const char *data = NULL;
    size_t data_len;
    http_parser_ext *resource = FETCH_OBJECT_RESOURCE(self, http_parser_ext);

    if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS(), "s", &data, &data_len)) {
        return;
    }
    http_parser_execute(&resource->parser, (resource->parserType == HTTP_REQUEST ? &parser_request_settings:&parser_response_settings), data, data_len);
}

PHP_METHOD(WebUtil_http_parser, __construct){
    long type = TYPE_REQUEST;
    zval *self = getThis();
    http_parser_ext *resource = FETCH_OBJECT_RESOURCE(self, http_parser_ext);
    if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS(), "|l", &type)) {
        return;
    }

    if(type == TYPE_REQUEST){
        resource->parserType = HTTP_REQUEST;
    }
    else{
        resource->parserType = HTTP_RESPONSE;
    }
    resource->object = *self;
    resetParserStatus(resource);
}

SETTER_METHOD(WebUtil_http_parser, setOnHeaderParsedCallback, onHeaderParsedCallback)
SETTER_METHOD(WebUtil_http_parser, setOnBodyParsedCallback, onBodyParsedCallback)
SETTER_METHOD(WebUtil_http_parser, setOnContentPieceCallback, onContentPieceCallback)
SETTER_METHOD(WebUtil_http_parser, setOnMultipartCallback, onMultipartCallback)

