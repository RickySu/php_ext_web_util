--TEST--
Check for request simple
--SKIPIF--
<?php include('skipif.inc')?>
--FILE--
<?php
use WebUtil\Parser\HttpParser;

$f = fopen(__DIR__.'/fixture/request-simple.txt', 'r');
$parser = new HttpParser();
$parser->setOnHeaderParsedCallback(function($parsedData){
    print_r($parsedData);
    return true;
});
$parser->setOnBodyParsedCallback(function($content){
    echo $content; 
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
            [Accept] => text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8
            [Accept-Language] => zh-TW,en-US;q=0.7,en;q=0.3
        )

    [Request] => Array
        (
            [Method] => GET
            [Target] => /index.php?a=123&b=456&c=%30
            [Protocol] => HTTP
            [Protocol-Version] => 1.1
        )

    [Query] => Array
        (
            [Path] => /index.php
            [Param] => Array
                (
                    [a] => 123
                    [b] => 456
                    [c] => 0
                )

        )

)
