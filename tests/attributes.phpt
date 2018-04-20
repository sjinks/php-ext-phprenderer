--TEST--
Attribute-related methods work as expected
--SKIPIF--
<?php include('skipif.inc'); ?>
--FILE--
<?php
$c = new WildWolf\Views\PhpRenderer();
var_dump($c->getAttributes());
var_dump($c->getAttribute('x'));
$c->addAttribute('a', 'b');
var_dump($c->getAttribute('a'));
var_dump($c->getAttributes());

$attrs = ['a' => 'a', 'b' => 'b', 'c' => 'c'];
$c->setAttributes($attrs);
var_dump($c->getAttribute('a'));
unset($attrs['a']);
var_dump($c->getAttribute('a'));
$attrs = $c->getAttributes();
var_dump($attrs);
unset($attrs['c']);
var_dump($c->getAttribute('c'));
var_dump($c->getAttributes());
?>
--EXPECT--
array(0) {
}
bool(false)
string(1) "b"
array(1) {
  ["a"]=>
  string(1) "b"
}
string(1) "a"
string(1) "a"
array(3) {
  ["a"]=>
  string(1) "a"
  ["b"]=>
  string(1) "b"
  ["c"]=>
  string(1) "c"
}
string(1) "c"
array(3) {
  ["a"]=>
  string(1) "a"
  ["b"]=>
  string(1) "b"
  ["c"]=>
  string(1) "c"
}
