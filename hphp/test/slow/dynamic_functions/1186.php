<?hh
<<__DynamicallyCallable>>
function test($a, $b) :mixed{
 print $a.$b;
}

 <<__EntryPoint>>
function main_1186() :mixed{
$a = 'test';
 $y = varray['k','q','q'];
 $a('o',$y[0]);
}
