--TEST--
Template path related methods work as expected
--SKIPIF--
<?php include('skipif.inc'); ?>
--FILE--
<?php
$c = new WildWolf\Views\PhpRenderer();
$c->setTemplatePath("");
echo $c->getTemplatePath(), PHP_EOL;

$c->setTemplatePath("path");
echo $c->getTemplatePath(), PHP_EOL;

$c->setTemplatePath("path/");
echo $c->getTemplatePath(), PHP_EOL;

$c->setTemplatePath("path\\");
echo $c->getTemplatePath(), PHP_EOL;
?>
--EXPECT--
/
path/
path/
path/
