--TEST--
render() works as expected
--SKIPIF--
<?php include('skipif.inc'); ?>
--FILE--
<?php
namespace Psr\Http\Message {

interface ResponseInterface
{
	public function getBody();
}

}

namespace Test {

class Body
{
	public function write($s)
	{
		echo $s;
	}
}

class Response implements \Psr\Http\Message\ResponseInterface
{
	public function getBody()
	{
		return new Body();
	}
}

$r = new Response;
$c = new \WildWolf\Views\PhpRenderer(dirname(__FILE__));
$c->render($r, 'hello.tpl', ['hello' => 'Hi']);

}
?>
--EXPECT--
Hi
