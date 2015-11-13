--TEST--
Check for simple match
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
$match = $r->match('/test-1234.html', $r::METHOD_GET);
echo "{$match[0]} {$match[1]['id']}\n";
--EXPECT--
my-test-data 1234
