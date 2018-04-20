--TEST--
fetch() filters out GLOBALS works as expected
--SKIPIF--
<?php include('skipif.inc'); ?>
--FILE--
<?php
$c = new WildWolf\Views\PhpRenderer(dirname(__FILE__));
echo $c->fetch('globals.tpl', ['GLOBALS' => 'string']), PHP_EOL;
?>
--EXPECT--
array
