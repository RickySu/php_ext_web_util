#include "web_util_r3.h"

CLASS_ENTRY_FUNCTION_D(WebUtil_R3){
    zval *array;
    REGISTER_CLASS_WITH_OBJECT_NEW(WebUtil_R3, "WebUtil\\R3", createWebUtil_R3Resource);
    OBJECT_HANDLER(WebUtil_R3).clone_obj = NULL;
    zend_declare_property_null(CLASS_ENTRY(WebUtil_R3), ZEND_STRL("routes"), ZEND_ACC_PRIVATE TSRMLS_CC);
    REGISTER_CLASS_CONSTANT_LONG(WebUtil_R3, METHOD_GET);
    REGISTER_CLASS_CONSTANT_LONG(WebUtil_R3, METHOD_POST);
    REGISTER_CLASS_CONSTANT_LONG(WebUtil_R3, METHOD_PUT);
    REGISTER_CLASS_CONSTANT_LONG(WebUtil_R3, METHOD_DELETE);
    REGISTER_CLASS_CONSTANT_LONG(WebUtil_R3, METHOD_PATCH);
    REGISTER_CLASS_CONSTANT_LONG(WebUtil_R3, METHOD_HEAD);
    REGISTER_CLASS_CONSTANT_LONG(WebUtil_R3, METHOD_OPTIONS);
}

static zend_object_value createWebUtil_R3Resource(zend_class_entry *ce TSRMLS_DC) {
    zend_object_value retval; 
    web_util_r3_t *resource;
    resource = (web_util_r3_t *) emalloc(sizeof(web_util_r3_t));
    memset(resource, 0, sizeof(web_util_r3_t));
    
    zend_object_std_init(&resource->zo, ce TSRMLS_CC);
    object_properties_init(&resource->zo, ce);
    retval.handle = zend_objects_store_put(
        &resource->zo,
        (zend_objects_store_dtor_t) zend_objects_destroy_object,
        freeWebUtil_R3Resource,
        NULL TSRMLS_CC);
                                    
    retval.handlers = &OBJECT_HANDLER(WebUtil_R3);
    return retval;
}

void freeWebUtil_R3Resource(void *object TSRMLS_DC) {
    web_util_r3_t *resource;
    resource = FETCH_RESOURCE(object, web_util_r3_t);
    
    if(resource->n){
        r3_tree_free(resource->n);
    }
    
    zend_object_std_dtor(&resource->zo TSRMLS_CC);
    efree(resource);
}

zend_always_inline void initRoutes(zval *routes){
    if(IS_NULL == Z_TYPE_P(routes)){
        array_init(routes);
    }
}

zend_always_inline void extractRoute(zval *route, zval **pattern, zval **method, zval **data){
    zval **p_pattern, **p_method, **p_data;
    *pattern = *method = *data = NULL;
    if(zend_hash_index_find(Z_ARRVAL_P(route), 0, (void **) &p_pattern) == SUCCESS){
        *pattern = *p_pattern;
    }
    if(zend_hash_index_find(Z_ARRVAL_P(route), 1, (void **) &p_method) == SUCCESS){
        *method = *p_method;
    }
    if(zend_hash_index_find(Z_ARRVAL_P(route), 2, (void **) &p_data) == SUCCESS){
        *data = *p_data;
    }
}

PHP_METHOD(WebUtil_R3, addRoute){
    zval *self = getThis();
    zval *routes, *data;
    const char *pattern;
    int pattern_len;
    long method;
    zval *array_element;

    routes = zend_read_property(CLASS_ENTRY(WebUtil_R3), self, ZEND_STRL("routes"), 0 TSRMLS_CC);
    initRoutes(routes);
    
    if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "slz", &pattern, &pattern_len, &method, &data)) {
        return;
    }
    
    MAKE_STD_ZVAL(array_element);
    array_init(array_element);
    add_next_index_stringl(array_element, pattern, pattern_len, 1);
    add_next_index_long(array_element, method);
    Z_ADDREF_P(data);
    add_next_index_zval(array_element, data);
    add_next_index_zval(routes, array_element);
    RETURN_LONG(zend_hash_num_elements(Z_ARRVAL_P(routes)));
}

PHP_METHOD(WebUtil_R3, compile){
    int i, nRoutes;
    zval *self = getThis();
    web_util_r3_t *resource = FETCH_OBJECT_RESOURCE(self, web_util_r3_t);
    zval *routes, **route;
    zval *pattern, *method, *data;
    routes = zend_read_property(CLASS_ENTRY(WebUtil_R3), self, ZEND_STRL("routes"), 0 TSRMLS_CC);
    
    if(resource->n){
        r3_tree_free(resource->n);
    }

    nRoutes = zend_hash_num_elements(Z_ARRVAL_P(routes));
    
    resource->n = r3_tree_create(nRoutes);
    
    for(i=0; i<nRoutes; i++){
        if(zend_hash_index_find(Z_ARRVAL_P(routes), i, (void **) &route) == SUCCESS){
            extractRoute(*route, &pattern, &method, &data);
            if(pattern && method && data){
                if(!r3_tree_insert_routel(resource->n, Z_LVAL_P(method), Z_STRVAL_P(pattern), Z_STRLEN_P(pattern), (void *)i)){
                    RETURN_BOOL(0);
                }
            }
        }
    }
    
    if(r3_tree_compile(resource->n, NULL) != 0) {
        RETURN_BOOL(0);
    }
    
    RETURN_BOOL(1);
}

PHP_METHOD(WebUtil_R3, match){
    int matched, i;
    zval *self = getThis();
    web_util_r3_t *resource = FETCH_OBJECT_RESOURCE(self, web_util_r3_t);
    zval *routes, **r_route;
    char *uri, *c_uri;
    int uri_len;
    long method;
    zval *r_pattern, *r_method, *r_data;
    match_entry * entry;
    route *matched_route;
    zval *params;
       
    routes = zend_read_property(CLASS_ENTRY(WebUtil_R3), self, ZEND_STRL("routes"), 0 TSRMLS_CC);

    if(!resource->n){
        RETURN_FALSE;
    }
    
    if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sl", &uri, &uri_len, &method)) {
        return;
    }
    
    MAKE_C_STR(c_uri, uri, uri_len);
    entry = match_entry_create(c_uri);
    entry->request_method = method;
    matched_route = r3_tree_match_route(resource->n, entry); 
    array_init(return_value);
    
    if(matched_route != NULL){
        matched = (int) matched_route->data;
        MAKE_STD_ZVAL(params);
        array_init(params);

        for(i=0;i<entry->vars->len;i++){
            add_next_index_string(params, entry->vars->tokens[i], 1);
        }
        
        if(zend_hash_index_find(Z_ARRVAL_P(routes), matched, (void **) &r_route) == SUCCESS){
            extractRoute(*r_route, &r_pattern, &r_method, &r_data);
            Z_ADDREF_P(r_data);
            add_next_index_zval(return_value, r_data);
            add_next_index_zval(return_value, params);
        }
        

    }
    efree(entry);    
}