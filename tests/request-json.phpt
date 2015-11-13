--TEST--
Check for request json
--SKIPIF--
<?php include('skipif.inc')?>
--FILE--
<?php
use WebUtil\Parser\HttpParser;

$f = fopen(__DIR__.'/fixture/request-json.txt', 'r');
$parser = new HttpParser();
$parser->setOnHeaderParsedCallback(function($parsedData){
    print_r($parsedData);
    return true;
});
$parser->setOnBodyParsedCallback(function($content){
    print_r($content); 
});
while(!feof($f)){
    $data = fread($f, rand(10, 20));
    $parser->feed($data);
}
fclose($f);
?>
--EXPECT--
Array
(
    [Header] => Array
        (
            [Host] => localhost
            [User-Agent] => Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:40.0) Gecko/20100101 Firefox/40.0
            [Accept] => application/json, text/plain, */*
            [Content-Type] => application/json;charset=utf-8
            [Content-Length] => 51
        )

    [Request] => Array
        (
            [Method] => PUT
            [Target] => /admin/category/2
            [Protocol] => HTTP
            [Protocol-Version] => 1.1
        )

    [Query] => Array
        (
            [Path] => /admin/category/2
            [Param] => 
        )

)
Array
(
    [name] => test
    [key] => family
    [description] => test
)
