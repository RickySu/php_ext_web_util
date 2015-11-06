#include "web_util_http_parser.h"

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

zend_always_inline zval *fetchArrayElement_ex(zval *arr, const char *str, size_t str_len, int init) {
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

zend_always_inline zval *fetchArrayElement(zval *arr, const char *str, size_t str_len) {
    return fetchArrayElement_ex(arr, str, str_len, 1);
}

zend_always_inline void resetHeaderParser(http_parser_ext *resource){
    TSRMLS_FETCH();
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

zend_always_inline void releaseParser(http_parser_ext *resource) {
    UNSET_BSTRING(resource->parser_data.url);
    UNSET_BSTRING(resource->parser_data.header);
    UNSET_BSTRING(resource->parser_data.field);
}

zend_always_inline void resetParserStatus(http_parser_ext *resource) {
    TSRMLS_FETCH();
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

zend_always_inline zval *parseCookie(const char *cookieString, size_t cookieString_len) {
    bstring *key;
    uint token_equal_pos = 0, token_semi_pos = 0;
    uint field_start = 0;
    uint i = 0;
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
                add_assoc_bool(cookie, key->val, 0);
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
    return cookie;
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
    return 0;
}

static int on_headers_complete_request(http_parser_ext *resource){
    TSRMLS_FETCH();
    zval retval;
    zval *parsedData = zend_read_property(CLASS_ENTRY(WebUtil_http_parser), &resource->object, ZEND_STRL("parsedData"), 0 TSRMLS_CC);
    zval *callback = zend_read_property(CLASS_ENTRY(WebUtil_http_parser), &resource->object, ZEND_STRL("onHeaderParsedCallback"), 0 TSRMLS_CC);
    zval *parsedData_header = fetchArrayElement(parsedData, ZEND_STRL("Header") + 1);
    zval *cookie = fetchArrayElement_ex(parsedData_header, ZEND_STRL("Cookie") + 1, 0);
    if(cookie && Z_TYPE_P(cookie) == IS_STRING){
        cookie = parseCookie(Z_STRVAL_P(cookie), Z_STRLEN_P(cookie));
        add_assoc_zval(parsedData_header, "Cookie", cookie);
    }
    if(!ZVAL_IS_NULL(callback)){
        call_user_function(CG(function_table), NULL, callback, &retval, 1, &parsedData TSRMLS_CC);
    }
    return !zend_is_true(&retval);
}

static int on_headers_complete_response(http_parser_ext *resource){
    return 0;
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
    return 0;
}

static int on_request_body(http_parser_ext *resource, const char *buf, size_t len){
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
    return retval;
}

void freeWebUtil_http_parserResource(void *object TSRMLS_DC) {
    http_parser_ext *resource;
    resource = FETCH_RESOURCE(object, http_parser_ext);
    releaseParser(resource);
    zend_object_std_dtor(&resource->zo TSRMLS_CC);
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
    resetParserStatus(resource);
}

SETTER_METHOD(WebUtil_http_parser, setOnHeaderParsedCallback, onHeaderParsedCallback)
SETTER_METHOD(WebUtil_http_parser, setOnBodyParsedCallback, onBodyParsedCallback)
SETTER_METHOD(WebUtil_http_parser, setOnContentPieceCallback, oContentPieceCallback)
SETTER_METHOD(WebUtil_http_parser, setOnMultipartCallback, onMultipartCallback)
