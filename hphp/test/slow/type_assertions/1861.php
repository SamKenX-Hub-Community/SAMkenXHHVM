<?hh

function f($x) :mixed{
  var_dump(is_array($x), $x[0]);
}

<<__EntryPoint>>
function main_1861() :mixed{
f(varray[0]);
f('foo');
}
