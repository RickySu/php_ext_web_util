--TEST--
Check for request multipart
--SKIPIF--
<?php include('skipif.inc')?>
--FILE--
<?php
use WebUtil\Parser\HttpParser;
function extractHeaders($header)
{
    $headers = array();
    foreach(explode("\r\n", $header) as $raw){
        list($name, $value) = explode(':', $raw);
        $headers[$name] = trim($value);
    }
    foreach(explode(';', $headers['Content-Disposition']) as $rawCol){
        $cols = explode('=', $rawCol);
        if(count($cols) == 1){
            continue;
        }
        $headers['__extra'][trim($cols[0])] = str_replace('"', '', $cols[1]); 
    }
    
    return $headers;
}

$f = fopen(__DIR__.'/fixture/request-multipart.txt', 'r');
$parser = new HttpParser();
$parser->setOnHeaderParsedCallback(function($parsedData){
    print_r($parsedData);
    return true;
});
$headers = array();
$content = array(
    'f1' => '',
    'f2' => '',
    'f3' => '',
);
$parser->setOnMultipartCallback(function($piece, $type) use(&$headers, &$content){
    if($type == 0){
        $headers = extractHeaders($piece);
        $md5 = null;
        return true;
    }
    $content[$headers['__extra']['name']].=$piece;
    return true;
});
while(!feof($f)){
    $data = fread($f, rand(10, 20));
    $parser->feed($data);
}
fclose($f);
echo 'f1:', (md5_file(__DIR__.'/fixture/250x100.png') === md5($content['f1']))?"ok":"fail", "\n";
echo 'f2:', (md5_file(__DIR__.'/fixture/640x480.png') === md5($content['f2']))?"ok":"fail", "\n";
echo 'f3:', $content['f3'], "\n";?>
--EXPECT--
Array
(
    [Header] => Array
        (
            [Host] => localhost:8888
            [User-Agent] => Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:41.0) Gecko/20100101 Firefox/41.0
            [Accept] => text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8
            [Accept-Language] => zh-TW,en-US;q=0.7,en;q=0.3
            [Accept-Encoding] => gzip, deflate
            [DNT] => 1
            [Referer] => http://localhost/~ricky/a.html
            [Connection] => keep-alive
            [Content-Type] => multipart/form-data; boundary=---------------------------1652693014104906404602549995
            [Content-Length] => 12627
        )

    [Request] => Array
        (
            [Method] => POST
            [Target] => /
            [Protocol] => HTTP
            [Protocol-Version] => 1.1
        )

    [Query] => Array
        (
            [Path] => /
            [Param] => 
        )

)
f1:ok
f2:ok
f3:this is a test!
