<?hh

function test($x) :mixed{
  apc_store('foo', varray['a'.$x, varray[$x]]);
  $a = __hhvm_intrinsics\apc_fetch_no_check('foo');
  var_dump($a);
}


// disable array -> "Array" conversion notice
<<__EntryPoint>>
function main_1826() :mixed{
test('foo');
}
