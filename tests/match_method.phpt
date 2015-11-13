--TEST--
Check for match method
--SKIPIF--
<?php include('skipif.inc')?>
--FILE--
<?php
$r = new \WebUtil\R3();
$r->addRoute(
    '/test-{id:\\d+}.html', 
    $r::METHOD_GET|$r::METHOD_POST,
    'my-test-data'
);
$r->compile();
$match = $r->match('/test-1234.html', $r::METHOD_GET);
echo "{$match[0]} {$match[1]['id']}\n";
$match = $r->match('/test-1234.html', $r::METHOD_POST);
echo "{$match[0]} {$match[1]['id']}\n";
?>
--EXPECT--
my-test-data 1234
my-test-data 1234
