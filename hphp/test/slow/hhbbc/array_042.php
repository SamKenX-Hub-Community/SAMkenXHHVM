<?hh

class C { function heh() :mixed{ echo "heh\n"; } }
function foo() :mixed{
  return mt_rand() ? varray[new C, new C] : varray[new C, new C, new C];
}
function bar() :mixed{
  $x = foo();
  $x[] = new C;
  return $x[0];
}
function main() :mixed{
  $x = bar();
  $x->heh();
}

<<__EntryPoint>>
function main_array_042() :mixed{
main();
}
