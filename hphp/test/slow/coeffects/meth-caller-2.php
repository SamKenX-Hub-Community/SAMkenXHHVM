<?hh

class A {
  function f()[rx] :mixed{
  }
}

<<__EntryPoint>>
function main()[write_props] :mixed{
  $f = meth_caller('A', 'f');
  $f(new A);
}
