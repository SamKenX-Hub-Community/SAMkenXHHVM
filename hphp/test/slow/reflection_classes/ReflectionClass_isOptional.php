<?hh

function arguments_1_defaults($a = 1, $b, $c) :mixed{}
function arguments_2_defaults($a, $b = NULL, $c) :mixed{}
function arguments_3_defaults($a, $b, $c = varray[]) :mixed{}

function arguments_12_defaults($a = 1, $b = NULL, $c) :mixed{}
function arguments_13_defaults($a = 1, $b, $c = varray[]) :mixed{}
function arguments_23_defaults($a, $b = NULL, $c = varray[]) :mixed{}

function arguments_123_defaults($a = 1, $b = NULL, $c = varray[]) :mixed{}


<<__EntryPoint>>
function main_reflection_class_is_optional() :mixed{
$functions = varray[
  'arguments_1_defaults',
  'arguments_2_defaults',
  'arguments_3_defaults',
  'arguments_12_defaults',
  'arguments_13_defaults',
  'arguments_23_defaults',
  'arguments_123_defaults',
];

$results = darray[];
foreach ($functions as $function) {
  $reflection = new ReflectionFunction($function);

  $params_info = varray[];
  foreach ($reflection->getParameters() as $param) {
    $params_info[] = $param->isOptional();
  }

  $results[$function] = $params_info;
}

var_dump($results);
}
