--TEST--
Check for response json
--SKIPIF--
<?php include('skipif.inc')?>
--FILE--
<?php
use WebUtil\Parser\HttpParser;
$p = null;
$f = fopen(__DIR__.'/fixture/response-json.txt', 'r');
$parser = new HttpParser(HttpParser::TYPE_RESPONSE);
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
bool(true)