<?hh

class C { function heh() :mixed{ echo "heh\n"; } }
function foo() :mixed{ return varray[new C]; }
function bar() :mixed{
  $x = foo();
  $x[] = new C;
  return varray[$x[0], $x[1]];
}
function main() :mixed{
  list($y, $yy) = bar();
  $y->heh();
  $yy->heh();
}

<<__EntryPoint>>
function main_array_035() :mixed{
main();
}
