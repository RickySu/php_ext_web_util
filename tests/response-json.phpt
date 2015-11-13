--TEST--
Check for response json
--SKIPIF--
<?php include('skipif.inc')?>
--FILE--
<?php
use WebUtil\Parser\HttpParser;

$f = fopen(__DIR__.'/fixture/response-json.txt', 'r');
$parser = new HttpParser(HttpParser::TYPE_RESPONSE);
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
            [Date] => Wed, 28 Oct 2015 04:37:32 GMT
            [Server] => Apache/2.4.16 (Ubuntu)
            [Cache-Control] => no-cache
            [X-Frame-Options] => SAMEORIGIN
            [Vary] => Accept-Encoding
            [Transfer-Encoding] => chunked
            [Content-Type] => application/json; charset=UTF-8
        )

    [Response] => Array
        (
            [Status-Code] => 200
            [Protocol] => HTTP
            [Protocol-Version] => 1.1
        )

)
Array
(
    [0] => 1
    [1] => 2
    [2] => 3
    [3] => 4
    [4] => 5
)
