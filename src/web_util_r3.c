#include "web_util_r3.h"

CLASS_ENTRY_FUNCTION_D(WebUtil_R3){
    REGISTER_CLASS_WITH_OBJECT_NEW(WebUtil_R3, "WebUtil\\R3", createWebUtil_R3Resource);
    OBJECT_HANDLER(WebUtil_R3).offset = XtOffsetOf(web_util_r3_t, zo);
    OBJECT_HANDLER(WebUtil_R3).clone_obj = NULL;
    OBJECT_HANDLER(WebUtil_R3).free_obj = freeWebUtil_R3Resource;
    zend_declare_property_null(CLASS_ENTRY(WebUtil_R3), ZEND_STRL("routes"), ZEND_ACC_PRIVATE);
    REGISTER_CLASS_CONSTANT_LONG(WebUtil_R3, METHOD_GET);
    REGISTER_CLASS_CONSTANT_LONG(WebUtil_R3, METHOD_POST);
    REGISTER_CLASS_CONSTANT_LONG(WebUtil_R3, METHOD_PUT);
    REGISTER_CLASS_CONSTANT_LONG(WebUtil_R3, METHOD_DELETE);
    REGISTER_CLASS_CONSTANT_LONG(WebUtil_R3, METHOD_PATCH);
    REGISTER_CLASS_CONSTANT_LONG(WebUtil_R3, METHOD_HEAD);
    REGISTER_CLASS_CONSTANT_LONG(WebUtil_R3, METHOD_OPTIONS);
}

static zend_object *createWebUtil_R3Resource(zend_class_entry *ce) {
    web_util_r3_t *resource;
    resource = ALLOC_RESOURCE(web_util_r3_t);
    
    zend_object_std_init(&resource->zo, ce);
    object_properties_init(&resource->zo, ce);
    resource->zo.handlers = &OBJECT_HANDLER(WebUtil_R3);
    return &resource->zo;
}

void freeWebUtil_R3Resource(zend_object *object) {
    web_util_r3_t *resource;
    resource = FETCH_RESOURCE(object, web_util_r3_t);
    if(resource->n){
        r3_tree_free(resource->n);
    }
    zend_object_std_dtor(&resource->zo);
    efree(resource);
}

static inline void initRoutes(zval *routes){
    if(IS_NULL == Z_TYPE_P(routes)){
        array_init(routes);
    }
}

static inline void extractRoute(zval *route, zval **pattern, zval **method, zval **data, zval **params){
    *pattern = zend_hash_index_find(Z_ARRVAL_P(route), 0);
    *method = zend_hash_index_find(Z_ARRVAL_P(route), 1);
    *data = zend_hash_index_find(Z_ARRVAL_P(route), 2);
    *params = zend_hash_index_find(Z_ARRVAL_P(route), 3);
}

PHP_METHOD(WebUtil_R3, addRoute){
    zval *self = getThis();
    zval *routes, *data;
    zval rv;
    const char *pattern = NULL;
    size_t pattern_len;
    long method;
    zval array_element;

    routes = zend_read_property(CLASS_ENTRY(WebUtil_R3), self, ZEND_STRL("routes"), 0, &rv);
    initRoutes(routes);
    
    if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "slz", &pattern, &pattern_len, &method, &data)) {
        return;
    }
    
    array_init(&array_element);
    add_next_index_stringl(&array_element, pattern, pattern_len);
    add_next_index_long(&array_element, method);
    Z_ADDREF_P(data);
    add_next_index_zval(&array_element, data);
    add_next_index_zval(routes, &array_element);
    RETURN_LONG(zend_hash_num_elements(Z_ARRVAL_P(routes)));
}

void findSlug(char *pattern, size_t pattern_len, zval *names)
{
    char *err = NULL;
    char *placeholder;
    char *name;    
    size_t placeholder_len = 0, name_len = 0;
    int slug_cnt = r3_slug_count(pattern, pattern_len, &err);
    int i;
    if(slug_cnt <= 0){
        ZVAL_NULL(names);
        return;
    }
    array_init(names);
    placeholder = pattern;
    for(i=0; i<slug_cnt; i++){
        placeholder = r3_slug_find_placeholder(placeholder, &placeholder_len);
        name = r3_slug_find_name(placeholder, &name_len);
        add_next_index_stringl(names, name, name_len);
        placeholder++;
    }
}

PHP_METHOD(WebUtil_R3, compile){
    int i, nRoutes;
    zval *self = getThis();
    web_util_r3_t *resource = FETCH_OBJECT_RESOURCE(self, web_util_r3_t);
    zval rv;
    zval *routes, *route;
    zval *pattern, *method, *data, *params;
    zval names;
    routes = zend_read_property(CLASS_ENTRY(WebUtil_R3), self, ZEND_STRL("routes"), 0, &rv);
    
    if(resource->n){
        r3_tree_free(resource->n);
    }

    nRoutes = zend_hash_num_elements(Z_ARRVAL_P(routes));
    
    resource->n = r3_tree_create(nRoutes);
    
    for(i=0; i<nRoutes; i++){
        if(route = zend_hash_index_find(Z_ARRVAL_P(routes), i)){
            extractRoute(route, &pattern, &method, &data, &params);
            if(pattern && method && data){
                findSlug(Z_STRVAL_P(pattern), Z_STRLEN_P(pattern), &names);
                if(!Z_ISNULL(names)){
                    add_next_index_zval(route, &names);
                }
                if(!r3_tree_insert_routel(resource->n, Z_LVAL_P(method), Z_STRVAL_P(pattern), Z_STRLEN_P(pattern), (void *)route)){
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
    int i;
    zval *self = getThis();
    web_util_r3_t *resource = FETCH_OBJECT_RESOURCE(self, web_util_r3_t);
    zval *r_route;
    char *uri = NULL, *c_uri;
    size_t uri_len;
    long method;
    zval *r_pattern, *r_method, *r_data, *r_params;
    match_entry * entry;
    route *matched_route;
    zval params;
    zval retval;
    zval function_name;
    zval call_params[2];
       
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
        r_route = (zval *) matched_route->data;
        array_init(&params);        
        for(i=0;i<entry->vars->len;i++){
            add_next_index_string(&params, entry->vars->tokens[i]);
        }
        
        extractRoute(r_route, &r_pattern, &r_method, &r_data, &r_params);
        Z_ADDREF_P(r_data);
        add_next_index_zval(return_value, r_data);
        if(r_params){
            call_params[0] = *r_params;
            call_params[1] = params;
            ZVAL_STRING(&function_name, "array_combine");
            call_user_function(CG(function_table), NULL, &function_name, &retval, 2, call_params);
            zval_dtor(&function_name);
            add_next_index_zval(return_value, &retval);
        }
        zval_dtor(&params);
    }
    match_entry_free(entry);    
    efree(c_uri);
}