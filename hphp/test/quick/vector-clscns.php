<?hh


class c {
}

function main($a) :mixed{
  return $a[c::BAR];
}
<<__EntryPoint>> function main_entry(): void {
var_dump(main(darray['hello there' => 'success']));
}
