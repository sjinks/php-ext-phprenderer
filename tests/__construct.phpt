--TEST--
__construct works as expected
--SKIPIF--
<?php include('skipif.inc'); ?>
--FILE--
<?php
$c1 = new WildWolf\Views\PhpRenderer();
$c2 = new WildWolf\Views\PhpRenderer("");
$c3 = new WildWolf\Views\PhpRenderer("path");
$c4 = new WildWolf\Views\PhpRenderer("path/");
$c5 = new WildWolf\Views\PhpRenderer("path\\");
echo $c1->getTemplatePath(), PHP_EOL;
echo $c2->getTemplatePath(), PHP_EOL;
echo $c3->getTemplatePath(), PHP_EOL;
echo $c4->getTemplatePath(), PHP_EOL;
echo $c5->getTemplatePath(), PHP_EOL;

$c1 = new WildWolf\Views\PhpRenderer();
$c2 = new WildWolf\Views\PhpRenderer('', []);
$c3 = new WildWolf\Views\PhpRenderer('', ['a' => 'b']);
var_dump($c1->getAttributes());
var_dump($c2->getAttributes());
var_dump($c3->getAttributes());

$a  = ['a' => 'b', 'c' => 'd', 'e' => 'f'];
$c1 = new WildWolf\Views\PhpRenderer('', $a);
unset($a['a']);
var_dump($c1->getAttributes());
?>
--EXPECT--
/
/
path/
path/
path/
array(0) {
}
array(0) {
}
array(1) {
  ["a"]=>
  string(1) "b"
}
array(3) {
  ["a"]=>
  string(1) "b"
  ["c"]=>
  string(1) "d"
  ["e"]=>
  string(1) "f"
}
