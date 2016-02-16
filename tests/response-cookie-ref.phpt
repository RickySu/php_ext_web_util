--TEST--
Check for response cookie
--SKIPIF--
<?php include('skipif.inc')?>
--FILE--
<?php
use WebUtil\Parser\HttpParser;
$p = null;
$f = fopen(__DIR__.'/fixture/response-cookie.txt', 'r');
$parser = new HttpParser(HttpParser::TYPE_RESPONSE);
$parser->setOnHeaderParsedCallback(function($parsedData) use(&$p, $parser){
    print_r($parsedData);
    $p = $parser;
    return true;
});
$parser->setOnBodyParsedCallback(function($content) use(&$p, $parser){
    echo $content; 
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
            [Date] => Tue, 03 Nov 2015 06:20:11 GMT
            [Server] => Apache
            [Set-Cookie] => Array
                (
                    [0] => Array
                        (
                            [PHPSESSION] => 93qc462usashkajh1312
                            [path] => /
                            [secure] => 1
                            [HttpOnly] => 1
                        )

                )

            [Content-Type] => text/html
            [Content-Length] => 10
        )

    [Response] => Array
        (
            [Status-Code] => 200
            [Protocol] => HTTP
            [Protocol-Version] => 1.1
        )

)
1234567890bool(true)