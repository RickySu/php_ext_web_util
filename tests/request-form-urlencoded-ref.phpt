--TEST--
Check for request form urlencoded
--SKIPIF--
<?php include('skipif.inc')?>
--FILE--
<?php
use WebUtil\Parser\HttpParser;
$p = null;
$f = fopen(__DIR__.'/fixture/request-form-urlencoded.txt', 'r');
$parser = new HttpParser();
$parser->setOnHeaderParsedCallback(function($parsedData) use(&$p, $parser){
    print_r($parsedData);
    $p = $parser;
    return true;
});
$parser->setOnBodyParsedCallback(function($content) use(&$p, $parser){
    print_r($content); 
    $p = $parser;
});
while(!feof($f)){
    $data = fread($f, rand(10, 20));
    $parser->feed($data);
}
fclose($f);
var_dump($p === $parser);
?>
--EXPECT--
Array
(
    [Header] => Array
        (
            [Host] => localhost
            [User-Agent] => Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:40.0) Gecko/20100101 Firefox/40.0
            [Accept] => text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8
            [Accept-Language] => zh-TW,en-US;q=0.7,en;q=0.3
            [DNT] => 1
            [Content-Type] => application/x-www-form-urlencoded
            [Content-Length] => 49
        )

    [Request] => Array
        (
            [Method] => POST
            [Target] => /admin/test
            [Protocol] => HTTP
            [Protocol-Version] => 1.1
        )

    [Query] => Array
        (
            [Path] => /admin/test
            [Param] => 
        )

)
Array
(
    [foo] => bar
    [baz] => boom
    [cow] => milk
    [php] => hypertext processor
)
bool(true)