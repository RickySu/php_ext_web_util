--TEST--
Check for multiple match
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
$r->addRoute(
    '/test/{id:\\d+}/{id2:\\d+}.php', 
    $r::METHOD_GET,
    'my-test-data2'
);
$r->compile();
$match = $r->match('/test-1234.html', $r::METHOD_GET);
echo "{$match[0]} {$match[1]['id']}\n";
$match = $r->match('/test/1234/5678.php', $r::METHOD_GET);
echo "{$match[0]} {$match[1]['id']} {$match[1]['id2']}\n";
?>
--EXPECT--
my-test-data 1234
my-test-data2 1234 5678
