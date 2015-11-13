--TEST--
Check simple not match
--SKIPIF--
<?php include('skipif.inc')?>
--FILE--
<?php
$r = new \WebUtil\R3();
$r->addRoute(
    '/test-{id:\\d+}.html', 
    $r::METHOD_GET,
    'my-test-data'
);
$r->compile();
$match = $r->match('/test-1234abc.html', $r::METHOD_GET);
var_dump($match);
--EXPECT--
array(0) {
}