<?hh

function a() :mixed{ return varray[1,2,3]; }
function b() :mixed{ return varray[1,4,5]; }
function c($x) :mixed{
  $val = $x ? a() : b();
  return $val[0];
}
function main() :mixed{
  var_dump(c(true));
  var_dump(c(false));
}

<<__EntryPoint>>
function main_array_001() :mixed{
main();
}
