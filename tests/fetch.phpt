--TEST--
fetch() works as expected
--SKIPIF--
<?php include('skipif.inc'); ?>
--FILE--
<?php
$c = new WildWolf\Views\PhpRenderer(dirname(__FILE__));
echo $c->fetch('hello.tpl', ['hello' => 'Hi']), PHP_EOL;
echo $c->getAttribute('hello'), PHP_EOL;
?>
--EXPECT--
Hi
Hi
