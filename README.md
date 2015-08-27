php_ext_web_util
==================================================

[![Build Status](https://travis-ci.org/RickySu/php_ext_web_util.svg?branch=master)](https://travis-ci.org/RickySu/php_ext_web_util)

hhvm-ext-web-util is a powerful collection of web utilities.

Collections
-----------

* [R3](https://github.com/c9s/r3) a high performance URL router library.


### Build Requirement

* autoconf
* automake
* check
* pkg-config

#### EXAMPLE

```php
<?php
use WebUtil\R3;
$r = new R3();
$r->addRoute(
    '/{id:\\d+}.html',               // match condition 
    R3::METHOD_GET|R3::METHOD_POST,  // match method GET or POST
    'match /{id}.html'              // custom define data
);
$m = $r->match('/123.html', R3::METHOD_GET);  // request url with GET method
print_r($m);
```

### Output

```
Array
(
    [0] => match /{id}.html
    [1] => Array
        (
            [id] => 123                // id parameters
        )

)
```

## LICENSE


This software is released under MIT License.